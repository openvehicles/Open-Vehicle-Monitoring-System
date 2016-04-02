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

// Nissan Leaf state variables

#pragma udata overlay vehicle_overlay_data

unsigned char nl_busactive; // An indication that bus is active

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

////////////////////////////////////////////////////////////////////////
// can_poll()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to poll for data.
//

// ISR optimization, see http://www.xargs.com/pic/c18-isr-optim.pdf
#pragma tmpdata high_isr_tmpdata

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

BOOL vehicle_nissanleaf_poll1(void)
  {
  nl_busactive = 10; // Reset the per-second charge timer

  switch (can_id)
    {
    case 0x56e:
      if (can_databuffer[0] == 0x86)
        { // Car is driving
        //        vehicle_poll_setstate(1);
        car_doors1 &= ~0x40; // NOT PARK
        car_doors1 |= 0x80; // CAR ON
        if (car_parktime != 0)
          {
          car_parktime = 0; // No longer parking
          net_req_notification(NET_NOTIFY_ENV);
          }
        }
      else
        { // Car is not driving
        //        vehicle_poll_setstate(0);
        car_doors1 |= 0x40; // PARK
        car_doors1 &= ~0x80; // CAR OFF
        car_speed = 0;
        if (car_parktime == 0)
          {
          car_parktime = car_time - 1; // Record it as 1 second ago, so non zero report
          net_req_notification(NET_NOTIFY_ENV);
          }
        }
      break;
    case 0x5bc:
      car_idealrange = ((int) can_databuffer[0] << 2) +
        ((can_databuffer[1]&0xc0) >> 6);
      car_SOC = (car_idealrange * 100 + 140) / 281; // convert Gids to percent
      car_idealrange = (car_idealrange * 84 + 140) / 281;
      break;
    case 0x5bf:
      car_chargelimit = can_databuffer[2] / 5;
      if ((car_chargelimit != 0)&&(can_databuffer[3] != 0))
        { // Car is charging
        if (car_chargestate != 1)
          {
          car_doors1bits.ChargePort = 1;
          car_doors1bits.Charging = 1;
          car_doors1bits.PilotSignal = 1;
          car_chargemode = 0; // Standard charge mode
          car_charge_b4 = 0; // Not required
          car_chargestate = 1; // Charging state
          car_chargesubstate = 3; // Charging by request
          car_chargeduration = 0; // Reset charge duration
          car_chargekwh = 0; // Reset charge kWh
          net_req_notification(NET_NOTIFY_STAT);
          }
        }
      /*      else if ((car_chargelimit==0)&&(car_chargestate==1))
              { // Charge has been interrupted
              car_doors1bits.ChargePort = 0;
              car_doors1bits.Charging = 0;
              car_doors1bits.PilotSignal = 0;
              car_chargemode = 0;     // Standard charge mode
              car_charge_b4 = 0;      // Not required
              car_chargestate = 21;    // Charge STOPPED
              car_chargesubstate = 14; // Charge INTERRUPTED
              net_req_notification(NET_NOTIFY_CHARGE);
              net_req_notification(NET_NOTIFY_STAT);
              } */
      break;
    }
  return TRUE;
  }

#pragma tmpdata

BOOL vehicle_nissanleaf_ticker1(void)
  {
  car_time++;
  if (nl_busactive > 0) nl_busactive--;
  if (nl_busactive == 0)
    {
    if (car_chargestate == 1)
      { // Charge has completed
      car_doors1bits.ChargePort = 0;
      car_doors1bits.Charging = 0;
      car_doors1bits.PilotSignal = 0;
      car_chargemode = 0; // Standard charge mode
      car_charge_b4 = 0; // Not required
      car_chargestate = 4; // Charge DONE
      car_chargesubstate = 3; // Leave it as is
      net_req_notification(NET_NOTIFY_STAT);
      }
    }
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
  if (sys_features[FEATURE_CANWRITE] > 0)
    vehicle_poll_setpidlist(vehicle_nissanleaf_polls);
  vehicle_poll_setstate(0);

  net_fnbits |= NET_FN_INTERNALGPS; // Require internal GPS
  net_fnbits |= NET_FN_12VMONITOR; // Require 12v monitor
  net_fnbits |= NET_FN_SOCMONITOR; // Require SOC monitor

  return TRUE;
  }

