/*
;    Project:       Open Vehicle Monitor System
;    Date:          6 May 2012
;
;    Changes:
;    1.0  Initial release
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
 */

#include <stdlib.h>
#include <delays.h>
#include <string.h>
#include "ovms.h"
#include "params.h"
#include "net_msg.h"
#include "inputs.h"

// Nissan Leaf state variables

#pragma udata overlay vehicle_overlay_data

typedef enum
  {
  CHARGER_STATUS_IDLE,
  CHARGER_STATUS_PLUGGED_IN_TIMER_WAIT,
  CHARGER_STATUS_CHARGING,
  CHARGER_STATUS_FINISHED
  } ChargerStatus;

UINT8 nl_busactive; // non-zero if we recently received data
UINT8 nl_abs_active; // non-zero if we recently received data from the ABS system
UINT16 nl_gids; // current gids in the battery
ChargerStatus nl_charger_status; // the current charger status

#pragma udata

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_polls
// This rom table records the PIDs that need to be polled

rom vehicle_pid_t vehicle_nissanleaf_polls[]
  = {
  { 0x797, 0x79a, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x0d,
    { 10, 10, 0}}, // Speed
  { 0x797, 0x79a, VEHICLE_POLL_TYPE_OBDIIVEHICLE, 0x02,
    { 10, 10, 0}}, // VIN
  { 0, 0, 0x00, 0x00,
    { 0, 0, 0}}
  };

// ISR optimization, see http://www.xargs.com/pic/c18-isr-optim.pdf
#pragma tmpdata high_isr_tmpdata

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_send_can_message()
// Send the specified can frame, does nothing if FEATURE_CANWRITE is not set
//
// TODO move to util?

void vehicle_nissanleaf_send_can_message(short id, unsigned char length, unsigned char *data)
  {
  if (sys_features[FEATURE_CANWRITE] == 0)
    {
    return;
    }
  while (TXB0CONbits.TXREQ)
    {
    } // Loop until TX is done
  TXB0CON = 0;
  TXB0SIDL = (id & 0x7) << 5;
  TXB0SIDH = id >> 3;
  // TODO it would be nicer to copy in a loop here, can we assume the CAN tx
  // buffer is sequential? Would the code be smaller or faster?
  if (length > 0)
    {
    TXB0D0 = data[0];
    }
  if (length > 1)
    {
    TXB0D1 = data[1];
    }
  if (length > 2)
    {
    TXB0D2 = data[2];
    }
  if (length > 3)
    {
    TXB0D3 = data[3];
    }
  if (length > 4)
    {
    TXB0D4 = data[4];
    }
  if (length > 5)
    {
    TXB0D5 = data[5];
    }
  if (length > 6)
    {
    TXB0D6 = data[6];
    }
  if (length > 7)
    {
    TXB0D7 = data[7];
    }
  TXB0DLC = length; // data length
  TXB0CON = 0b00001000; // mark for transmission
  }

////////////////////////////////////////////////////////////////////////
// can_poll()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to poll for data.
//

BOOL vehicle_nissanleaf_poll0(void)
  {
  unsigned char value1;
  unsigned int value2;

  value1 = can_databuffer[3];
  value2 = ((unsigned int) can_databuffer[3] << 8) + (unsigned int) can_databuffer[4];

  switch (vehicle_poll_pid)
    {
    case 0x02: // VIN (multi-line response)
      for (value1 = 0; value1 < can_datalength; value1++)
        {
        car_vin[value1 + (vehicle_poll_ml_offset - can_datalength)] = can_databuffer[value1];
        }
      if (vehicle_poll_ml_remain == 0)
        car_vin[value1 + vehicle_poll_ml_offset] = 0;
      break;
    case 0x0d: // Speed
      if (can_mileskm == 'K')
        car_speed = value1;
      else
        car_speed = (unsigned char) MiFromKm((unsigned long) value1);
      break;
    }

  return TRUE;
  }

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_car_on()
// Takes care of setting all the state appropriate when the car is on
// or off. Centralized so we can more easily make on and off mirror
// images.
//

void vehicle_nissanleaf_car_on(BOOL isOn)
  {
  car_doors1bits.CarON = isOn ? 1 : 0;
  car_doors3bits.CarAwake = isOn ? 1 : 0;
  // We don't know if handbrake is on or off, but it's usually off when the car is on.
  car_doors1bits.HandBrake = isOn ? 0 : 1;
  if (isOn && car_parktime != 0)
    {
    car_parktime = 0; // No longer parking
    net_req_notification(NET_NOTIFY_ENV);
    car_doors1bits.ChargePort = 0;
    car_doors1bits.Charging = 0;
    car_doors1bits.PilotSignal = 0;
    car_chargestate = 0;
    car_chargesubstate = 0;
    }
  else if (!isOn && car_parktime == 0)
    {
    car_parktime = car_time - 1; // Record it as 1 second ago, so non zero report
    net_req_notification(NET_NOTIFY_ENV);
    }
  }

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_charger_status()
// Takes care of setting all the charger state bit when the charger
// switches on or off. Separate from vehicle_nissanleaf_poll1() to make
// it clearer what is going on.
//

void vehicle_nissanleaf_charger_status(ChargerStatus status)
  {
  switch (status)
    {
    case CHARGER_STATUS_IDLE:
      car_doors1bits.Charging = 0;
      car_chargestate = 0;
      car_chargesubstate = 0;
      break;
    case CHARGER_STATUS_PLUGGED_IN_TIMER_WAIT:
      car_doors1bits.Charging = 0;
      car_chargestate = 0;
      car_chargesubstate = 0;
      break;
    case CHARGER_STATUS_CHARGING:
      car_doors1bits.Charging = 1;
      car_chargestate = 1;
      car_chargesubstate = 3; // Charging by request
      if (nl_charger_status != status)
        {
        car_chargeduration = 0; // Reset charge duration
        car_chargekwh = 0; // Reset charge kWh
        }
      break;
    case CHARGER_STATUS_FINISHED:
      // Charging finished
      car_chargecurrent = 0;
      car_doors1bits.Charging = 0;
      car_chargestate = 4; // Charge DONE
      car_chargesubstate = 3; // Charging by request
      break;
    }
  if (nl_charger_status != status)
    {
    net_req_notification(NET_NOTIFY_STAT);
    }
  nl_charger_status = status;
  }

BOOL vehicle_nissanleaf_poll1(void)
  {
  nl_busactive = 10; // Reset the per-second charge timer

  switch (can_id)
    {
    case 0x284:
      // this is apparently data relayed from the ABS system, if we have
      // that then we assume car is on.
      nl_abs_active = 10;
      vehicle_nissanleaf_car_on(TRUE);
      // vehicle_poll_setstate(1);
      break;
    case 0x5bc:
      nl_gids = ((unsigned int) can_databuffer[0] << 2) +
        ((can_databuffer[1] & 0xc0) >> 6);
      car_SOC = (nl_gids * 100 + 140) / 281;
      car_idealrange = (nl_gids * 84 + 140) / 281;
      break;
    case 0x5bf:
      // this is the J1772 maximum available current, 0 if we're not plugged in
      car_chargelimit = can_databuffer[2] / 5;
      car_doors1bits.ChargePort = car_chargelimit == 0 ? 0 : 1;
      car_doors1bits.PilotSignal = car_chargelimit == 0 ? 0 : 1;

      switch (can_databuffer[4])
        {
        case 0x28:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_IDLE);
          break;
        case 0x30:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_PLUGGED_IN_TIMER_WAIT);
          break;
        case 0x60:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_CHARGING);
          break;
        case 0x40:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_FINISHED);
          break;
        }
      break;
    }
  return TRUE;
  }

#pragma tmpdata

BOOL vehicle_nissanleaf_ticker1(void)
  {
  car_time++;
  if (nl_busactive > 0) nl_busactive--;
  if (nl_abs_active > 0) nl_abs_active--;

  // have the messages from the ABS system stopped?
  if (nl_abs_active == 0)
    {
    vehicle_nissanleaf_car_on(FALSE);
    car_speed = 0;
    }

  // has the whole CAN bus gone quiet?
  if (nl_busactive == 0)
    {
    vehicle_nissanleaf_car_on(FALSE);
    car_speed = 0;
    }
  }

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_cc()
// Wake up the car & send Climate Control message to VCU, replaces Nissan's
// CARWINGS and TCU module, see
// http://www.mynissanleaf.com/viewtopic.php?f=44&t=4131&hilit=open+CAN+discussion&start=416
//
// On Generation 1 Cars, TCU pin 11's "EV system activation request signal" is
// driven to 12V to wake up the VCU. This function drives RC3 high to
// activate the "EV system activation request signal". Without a circuit
// connecting RC3 to the activation signal wire, remote climate control will
// only work during charging.
//
// On Generation 2 Cars, a CAN bus message is sent to wake up the VCU. This
// function sends that message even to Generation 1 cars which doesn't seem to
// cause any problems.
//

void vehicle_nissanleaf_cc(BOOL enable_cc)
  {
  unsigned char data;
  if (sys_features[FEATURE_CANWRITE] == 0)
    {
    // we don't want to transmit when CAN write is disabled.
    return;
    }

  // Use GPIO to wake up GEN 1 Leaf with EV SYSTEM ACTIVATION REQUEST
  output_gpo3(TRUE);
  // send GEN 2 wakeup frame
  data = 0;
  vehicle_nissanleaf_send_can_message(0x68c, 1, &data);

  // wait 50 ms for things to wake up
  delay5(10);

  // turn off EV SYSTEM ACTIVATION REQUEST
  output_gpo3(FALSE);

  // send climate control command
  data = enable_cc ? 0x4e : 0x56;
  vehicle_nissanleaf_send_can_message(0x56e, 1, &data);
  }

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_fn_commandhandler()
// Remote Command Handler, decodes command and delegates actual work elsewhere

BOOL vehicle_nissanleaf_fn_commandhandler(BOOL msgmode, int cmd, char *msg)
  {
  // TODO climate control shouldn't be hooked to these commands
  switch (cmd)
    {
    case CMD_Homelink:
      vehicle_nissanleaf_cc(TRUE);
      break;
    case CMD_Alert:
      vehicle_nissanleaf_cc(FALSE);
      break;
    }
  // we return false even on success so the framework takes care of the response
  return FALSE;
  }

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_initialise()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to initialise itself.
//

BOOL vehicle_nissanleaf_initialise(void)
  {
  char *p;

  car_type[0] = 'N'; // Car is type NL - Nissan Leaf
  car_type[1] = 'L';
  car_type[2] = 0;
  car_type[3] = 0;
  car_type[4] = 0;

  // Vehicle specific data initialisation
  car_stale_timer = -1; // Timed charging is not supported for OVMS NL
  car_time = 0;

  // initialize SOC and SOC alert limit to avoid SMS on startup
  car_SOC = car_SOCalertlimit = 2;

  CANCON = 0b10010000; // Initialize CAN
  while (!CANSTATbits.OPMODE2); // Wait for Configuration mode

  // We are now in Configuration Mode

  // Filters: low byte bits 7..5 are the low 3 bits of the filter, and bits 4..0 are all zeros
  //          high byte bits 7..0 are the high 8 bits of the filter
  //          So total is 11 bits

  // Buffer 0 (filters 0, 1) for extended PID responses
  RXB0CON = 0b00000000;

  RXM0SIDL = 0b00000000; // Mask   11111111000
  RXM0SIDH = 0b11111111;

  RXF0SIDL = 0b00000000; // Filter 11111101000 (0x798 .. 0x79f)
  RXF0SIDH = 0b11110011;

  // Buffer 1 (filters 2, 3, 4 and 5) for direct can bus messages
  RXB1CON = 0b00000000; // RX buffer1 uses Mask RXM1 and filters RXF2, RXF3, RXF4, RXF5

  RXM1SIDL = 0b00000000;
  RXM1SIDH = 0b11100000; // Set Mask1 to 11100000000

  RXF2SIDL = 0b00000000; // Setup Filter2 so that CAN ID 0x500 - 0x5FF will be accepted
  RXF2SIDH = 0b10100000;

  RXF3SIDL = 0b00000000; // Setup Filter3 so that CAN ID 0x200 - 0x2FF will be accepted
  RXF3SIDH = 0b01000000;

  RXF4SIDL = 0b00000000; // Setup Filter4 so that CAN ID 0x300 - 0x3FF will be accepted
  RXF4SIDH = 0b01100000;

  RXF5SIDL = 0b00000000; // Setup Filter5 so that CAN ID 0x000 - 0x0FF will be accepted
  RXF5SIDH = 0b00000000;

  // CAN bus baud rate

  BRGCON1 = 0x01; // SET BAUDRATE to 500 Kbps
  BRGCON2 = 0xD2;
  BRGCON3 = 0x02;

  CIOCON = 0b00100000; // CANTX pin will drive VDD when recessive
  if (sys_features[FEATURE_CANWRITE] > 0)
    {
    CANCON = 0b00000000; // Normal mode
    }
  else
    {
    CANCON = 0b01100000; // Listen only mode, Receive bufer 0
    }

  nl_busactive = 10;

  // Hook in...
  vehicle_fn_poll0 = &vehicle_nissanleaf_poll0;
  vehicle_fn_poll1 = &vehicle_nissanleaf_poll1;
  vehicle_fn_ticker1 = &vehicle_nissanleaf_ticker1;
  vehicle_fn_commandhandler = &vehicle_nissanleaf_fn_commandhandler;

  // TODO this causes a relay to click every 20 seconds or so while charging
  // TODO and on my gen 1 car at least, does not get the VIN number or speed
  //if (sys_features[FEATURE_CANWRITE] > 0)
  //vehicle_poll_setpidlist(vehicle_nissanleaf_polls);
  //
  //vehicle_poll_setstate(0);

  net_fnbits |= NET_FN_INTERNALGPS; // Require internal GPS
  net_fnbits |= NET_FN_12VMONITOR; // Require 12v monitor
  net_fnbits |= NET_FN_SOCMONITOR; // Require SOC monitor

  return TRUE;
  }

