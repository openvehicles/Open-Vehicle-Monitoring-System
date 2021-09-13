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

// Nissan Leaf module version:
rom char nissanleaf_version[] = "1.6";

// Nissan Leaf capabilities:
// - CMD_StartCharge (11)
// - CMD_Homelink (24)
// - CMD_ClimateControl (26)
rom char nissanleaf_capabilities[] = "C11,C24,C26";

// When the the Nissan TCU recieves a request from CARWINGS, it sends the
// command CAN bus message 25 times at 10Hz. We send it once during the command
// handler then ...ticker10th sends the rest.

#define REMOTE_COMMAND_REPEAT_COUNT 24 // number of times to send the remote command after the first time
#define ACTIVATION_REQUEST_TIME 10 // tenths of a second to hold activation request signal
#define REMOTE_CC_TIME_GRID 1800 // seconds to run remote climate control on grid power
#define REMOTE_CC_TIME_BATTERY 900 // seconds to run remote climate control on battery power

#if (REMOTE_CC_TIME_GRID < REMOTE_CC_TIME_BATTERY)
#error "Climate Control on Grid must be longer than Climate Control on Battery"
#endif

#define GEN_1_NEW_CAR_GIDS 281
#define GEN_1_NEW_CAR_GIDS_S "281"
#define GEN_1_NEW_CAR_RANGE_MILES 84

#define PARAM_SOC_CONFIGURATION 21

// Nissan Leaf state variables

#pragma udata overlay vehicle_overlay_data

typedef enum
  {
  CHARGER_STATUS_IDLE,
  CHARGER_STATUS_PLUGGED_IN_TIMER_WAIT,
  CHARGER_STATUS_CHARGING,
  CHARGER_STATUS_QUICK_CHARGING,
  CHARGER_STATUS_FINISHED
  } ChargerStatus;

typedef enum
  {
  ENABLE_CLIMATE_CONTROL,
  DISABLE_CLIMATE_CONTROL,
  START_CHARGING
  } RemoteCommand;

typedef enum
  {
  CAN_READ_ONLY,
  INVALID_SYNTAX,
  OK
  } CommandResult;

typedef enum
  {
  ZERO = 0,
  ONE = 1,
  TWO = 2,
  THREE = 3,
  FOUR = 4,
  FIVE = 5,
  IDLE = 6
  } PollState;

UINT8 nl_busactive; // non-zero if we recently received data
UINT8 nl_abs_active; // non-zero if we recently received data from the ABS system

RemoteCommand nl_remote_command; // command to send, see ticker10th()
UINT8 nl_remote_command_ticker; // number of tenths remaining to send remote command frames
UINT16 nl_cc_off_ticker; // seconds before we send the climate control off command

UINT16 nl_gids; // current gids in the battery
UINT16 nl_max_gids; // maximum gids, used for SOC calculation

ChargerStatus nl_charger_status; // the current charger status

INT16 nl_battery_current; // battery current from LBC, 0.5A per bit
UINT16 nl_battery_voltage; // battery voltage from LBC, 0.5V per bit

PollState nl_poll_state = IDLE;

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
// Send the specified can frame. Does nothing if FEATURE_CANWRITE is not set,
// does nothing if length > 8.
//
// TODO move to util?

void vehicle_nissanleaf_send_can_message(short id, unsigned char length, unsigned char *data)
  {
  volatile far unsigned char *txb0 = &TXB0D0;
  UINT8 i;

  if (sys_features[FEATURE_CANWRITE] == 0 || length > 8)
    {
    return;
    }
  while (TXB0CONbits.TXREQ)
    {
    } // Loop until TX is done
  TXB0CON = 0;
  TXB0SIDL = (id & 0x7) << 5;
  TXB0SIDH = id >> 3;
  for (i = 0; i < length; i++)
    {
    txb0[i] = data[i];
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
    case CHARGER_STATUS_QUICK_CHARGING:
    case CHARGER_STATUS_CHARGING:
      car_doors1bits.Charging = 1;
      car_chargestate = 1;
      car_chargesubstate = 3; // Charging by request
      // TODO only use battery current for Quick Charging, for regular charging
      // we should return AC line current and voltage, not battery
      // TODO does the leaf know the AC line current and voltage?
      if (nl_battery_current < 0)
        {
        // battery current can go negative when climate control draws more than
        // is available from the charger. We're abusing the line current which
        // is unsigned, so don't underflow it
        //
        // TODO quick charging can draw current from the vehicle
        car_chargecurrent = 0;
        }
      else
        {
        car_chargecurrent = (nl_battery_current + 1) / 2;
        }
      car_linevoltage = (nl_battery_voltage + 1) / 2;

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
  if (status != CHARGER_STATUS_CHARGING && status != CHARGER_STATUS_QUICK_CHARGING)
    {
    car_chargecurrent = 0;
    // TODO the charger probably knows the line voltage, when we find where it's
    // coded, don't zero it out when we're plugged in but not charging
    car_linevoltage = 0;
    }
  if (nl_charger_status != status)
    {
    net_req_notification(NET_NOTIFY_STAT);
    }
  nl_charger_status = status;
  }

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_poll_start()
// Send the initial message to poll for data. Further data is requested
// in vehicle_nissanleaf_poll_continue() after the recept of each page.
//

void vehicle_nissanleaf_poll_start(void)
  {
  // Request Group 1
  char data[] = {0x02, 0x21, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
  nl_poll_state = ZERO;
  vehicle_nissanleaf_send_can_message(0x79b, 8, data);
  }

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_poll_continue()
// Process a 0x7bb polling response and request the next page
//

void vehicle_nissanleaf_poll_continue(void)
  {
  UINT16 hx;
  UINT32 ah;
  if (nl_poll_state == IDLE)
    {
    // we're not expecting anything, maybe something else is polling?
    return;
    }
  if ((can_databuffer[0] & 0x0f) != nl_poll_state)
    {
    // not the page we were expecting, abort
    nl_poll_state = IDLE;
    return;
    }

  switch (nl_poll_state)
    {
    case ZERO:
    case ONE:
    case TWO:
    case THREE:
      nl_poll_state++;
      break;
    case FOUR:
      hx = can_databuffer[2];
      hx = hx << 8;
      hx = hx | can_databuffer[3];
      hx = hx / 100;
      // LeafSpy calculates SOH by dividing Ah by the nominal capacity.
      // Since SOH is derived from Ah, we don't bother storing it separately.
      // We store Ah in CAC (below) and store Hx in SOH.
      // TODO store full precision of Hx?
      if (car_soh != hx)
        {
        car_soh = hx;
        net_req_notification(NET_NOTIFY_STAT);
        }
      nl_poll_state++;
      break;
    case FIVE:
      ah = can_databuffer[2];
      ah = ah << 8;
      ah = ah | can_databuffer[3];
      ah = ah << 8;
      ah = ah | can_databuffer[4];
      ah = ah / 100;
      if (car_cac100 != ah)
        {
        car_cac100 = ah;
        net_req_notification(NET_NOTIFY_STAT);
        }
      nl_poll_state = IDLE;
      break;
    }
  if (nl_poll_state != IDLE)
    {
    // request the next page of data
    char next[] = {0x30, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    vehicle_nissanleaf_send_can_message(0x79b, 8, next);
    }
  }

BOOL vehicle_nissanleaf_poll1(void)
  {
  nl_busactive = 10; // Reset the per-second charge timer

  switch (can_id)
    {
    case 0x1db:
      // sent by the LBC, measured inside the battery box
      // current is 11 bit twos complement big endian starting at bit 0
      nl_battery_current = ((INT16) can_databuffer[0] << 3) | (can_databuffer[1] & 0xe0) >> 5;
      if (nl_battery_current & 0x0400)
        {
        // negative so extend the sign bit
        nl_battery_current |= 0xf800;
        }
      // voltage is 10 bits unsigned big endian starting at bit 16
      nl_battery_voltage = ((UINT16) can_databuffer[2] << 2) | (can_databuffer[3] & 0xc0) >> 6;
      // can bus data is 0.5v per bit, car_battvoltage is 0.1v per bit, so multiply by 5
      car_battvoltage = nl_battery_voltage * 5;
      break;
    case 0x284:
      // this is apparently data relayed from the ABS system, if we have
      // that then we assume car is on.
      nl_abs_active = 10;
      vehicle_nissanleaf_car_on(TRUE);
    {
      UINT16 car_speed16 = can_databuffer[4];
      car_speed16 = car_speed16 << 8;
      car_speed16 = car_speed16 | can_databuffer[5];
      // this ratio determined by comparing with the dashboard speedometer
      // it is approximately correct and converts to km/h on my car with km/h speedo
      car_speed = car_speed16 / 92;
    }
      break;
    case 0x390:
      // Gen 2 Charger
      //
      // When the data is valid, can_databuffer[6] is the J1772 maximum
      // available current if we're plugged in, and 0 when we're not.
      // can_databuffer[3] seems to govern when it's valid, and is probably a
      // bit field, but I don't have a full decoding
      //
      // specifically the last few frames before shutdown to wait for the charge
      // timer contain zero in can_databuffer[6] so we ignore them and use the
      // valid data in the earlier frames
      //
      // During plug in with charge timer activated, byte 3 & 6:
      //
      // 0x390 messages start
      // 0x00 0x21
      // 0x60 0x21
      // 0x60 0x00
      // 0xa0 0x00
      // 0xb0 0x00
      // 0xb2 0x15 -- byte 6 now contains valid j1772 pilot amps
      // 0xb3 0x15
      // 0xb1 0x00
      // 0x71 0x00
      // 0x69 0x00
      // 0x61 0x00
      // 0x390 messages stop
      //
      // byte 3 is 0xb3 during charge, and byte 6 contains the valid pilot amps
      //
      // so far, except briefly during startup, when byte 3 is 0x00 or 0x03,
      // byte 6 is 0x00, correctly indicating we're unplugged, so we use that
      // for unplugged detection.
      //
      if (can_databuffer[3] == 0xb3 ||
        can_databuffer[3] == 0x00 ||
        can_databuffer[3] == 0x03)
        {
        // can_databuffer[6] is the J1772 pilot current, 0.5A per bit
        car_chargetype = J1772_TYPE1;
        car_chargelimit = (can_databuffer[6] + 1) / 2;
        car_doors1bits.ChargePort = car_chargelimit == 0 ? 0 : 1;
        car_doors1bits.PilotSignal = car_chargelimit == 0 ? 0 : 1;
        }
      switch (can_databuffer[5])
        {
        case 0x80:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_IDLE);
          break;
        case 0x83:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_QUICK_CHARGING);
          break;
        case 0x88:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_CHARGING);
          break;
        case 0x98:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_PLUGGED_IN_TIMER_WAIT);
          break;
        }
      break;
    case 0x54b:
    {
      BOOL hvac_candidate;
      // this might be a bit field? So far these 6 values indicate HVAC on
      hvac_candidate = can_databuffer[1] == 0x0a || // Gen 1 Remote
        can_databuffer[1] == 0x48 || // Manual Heating or Fan Only
        can_databuffer[1] == 0x4b || // Gen 2 Remote Heating
        can_databuffer[1] == 0x71 || // Gen 2 Remote Cooling
        can_databuffer[1] == 0x76 || // Auto
        can_databuffer[1] == 0x78; // Manual A/C on
      if (car_doors5bits.HVAC != hvac_candidate)
        {
        car_doors5bits.HVAC = hvac_candidate;
        net_req_notification(NET_NOTIFY_ENV);
        }
    }
      break;
    case 0x54c:
      if (can_databuffer[6] == 0xff)
        {
        break;
        }
      // TODO this temperature isn't quite right
      car_ambient_temp = ((int) can_databuffer[6]) - 56; // Fahrenheit
      car_ambient_temp = (car_ambient_temp - 32) / 1.8f; // Celsius
      car_stale_ambient = 10;
      break;
    case 0x5bc:
    {
      UINT16 nl_gids_candidate = ((UINT16) can_databuffer[0] << 2) | ((can_databuffer[1] & 0xc0) >> 6);
      if (nl_gids_candidate == 1023)
        {
        // ignore invalid data seen during startup
        break;
        }
      nl_gids = nl_gids_candidate;
      car_SOC = (nl_gids * 100 + (nl_max_gids / 2)) / nl_max_gids;
      car_idealrange = (nl_gids * GEN_1_NEW_CAR_RANGE_MILES + (GEN_1_NEW_CAR_GIDS / 2)) / GEN_1_NEW_CAR_GIDS;
    }
      break;
    case 0x5bf:
      // Gen 1 Charger
      if (can_databuffer[4] == 0xb0)
        {
        // Quick Charging
        car_chargetype = CHADEMO;
        car_chargelimit = 120;
        car_doors1bits.ChargePort = 1;
        car_doors1bits.PilotSignal = 1;
        }
      else
        {
        // Maybe J1772 is connected
        // can_databuffer[2] is the J1772 maximum available current, 0 if we're not plugged in
        car_chargetype = J1772_TYPE1;
        car_chargelimit = can_databuffer[2] / 5;
        car_doors1bits.ChargePort = car_chargelimit == 0 ? 0 : 1;
        car_doors1bits.PilotSignal = car_chargelimit == 0 ? 0 : 1;
        }

      switch (can_databuffer[4])
        {
        case 0x28:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_IDLE);
          break;
        case 0x30:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_PLUGGED_IN_TIMER_WAIT);
          break;
        case 0xb0: // Quick Charging
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_QUICK_CHARGING);
          break;
        case 0x60: // L2 Charging
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_CHARGING);
          break;
        case 0x40:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_FINISHED);
          break;
        }
      break;
    case 0x5c0:
      if (can_databuffer[0] == 0x40)
        {
        car_tbattery = can_databuffer[2] / 2 - 40;
        car_stale_temps = 10;
        }
      break;
    case 0x7bb:
      vehicle_nissanleaf_poll_continue();
      break;
    }
  return TRUE;
  }

#pragma tmpdata

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_remote_command()
// Send a RemoteCommand on the CAN bus.
//
// Does nothing if transmit isn't enabled or if @command is out of range

void vehicle_nissanleaf_send_command(RemoteCommand command)
  {
  unsigned char data;
  switch (command)
    {
    case ENABLE_CLIMATE_CONTROL:
      data = 0x4e;
      break;
    case DISABLE_CLIMATE_CONTROL:
      data = 0x56;
      break;
    case START_CHARGING:
      data = 0x66;
      break;
    default:
      // shouldn't be possible, but lets not send random data on the bus
      return;
    }
  vehicle_nissanleaf_send_can_message(0x56e, 1, &data);
  }

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_ticker10th()
// implements the repeated sending of remote commands and releases
// EV SYSTEM ACTIVATION REQUEST after an appropriate amount of time

BOOL vehicle_nissanleaf_ticker10th(void)
  {
  if (nl_remote_command_ticker > 0)
    {
    nl_remote_command_ticker--;
    vehicle_nissanleaf_send_command(nl_remote_command);

    // nl_remote_command_ticker is set to REMOTE_COMMAND_REPEAT_COUNT in
    // vehicle_nissanleaf_remote_command() and we decrement it every 10th of a
    // second, hence the following if statement evaluates to true
    // ACTIVATION_REQUEST_TIME tenths after we start
    if (nl_remote_command_ticker == REMOTE_COMMAND_REPEAT_COUNT - ACTIVATION_REQUEST_TIME)
      {
      // release EV SYSTEM ACTIVATION REQUEST
      output_gpo3(FALSE);
      }
    }
  return TRUE;
  }

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_init_default_soc_configuration()
// Store the default soc calculation configuration (fixed 281 gids) in
// the PARAM_SOC_CONFIGURATION parameter.
//
//

void vehicle_nissanleaf_init_default_soc_configuration(void)
  {
  char s[4];
  stp_rom(s, GEN_1_NEW_CAR_GIDS_S);
  par_set(PARAM_SOC_CONFIGURATION, s);
  }

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_load_soc_configuration()
// Loads the soc calculation configuration out of the
// PARAM_SOC_CONFIGURATIONparameter. Sets a default if the data is
// invalid.
//

void vehicle_nissanleaf_load_soc_configuration(void)
  {
  char *s;
  UINT16 max_gids_candidate;

  // set default to override if the data is valid
  nl_max_gids = GEN_1_NEW_CAR_GIDS;

  s = par_get(PARAM_SOC_CONFIGURATION);
  if (!s)
    {
    vehicle_nissanleaf_init_default_soc_configuration();
    return;
    }

  max_gids_candidate = atoi(s);
  if (max_gids_candidate == 0)
    {
    vehicle_nissanleaf_init_default_soc_configuration();
    return;
    }
  nl_max_gids = max_gids_candidate;
  }

BOOL vehicle_nissanleaf_ticker60(void)
  {
  if (car_doors1bits.CarON)
    {
    // we only poll while the car is on, as polling at other times causes a
    // relay to click
    vehicle_nissanleaf_poll_start();
    }
  return TRUE;
  }

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_remote_command()
// Wake up the car & send Climate Control or Remote Charge message to VCU,
// replaces Nissan's CARWINGS and TCU module, see
// http://www.mynissanleaf.com/viewtopic.php?f=44&t=4131&hilit=open+CAN+discussion&start=416
//
// On Generation 1 Cars, TCU pin 11's "EV system activation request signal" is
// driven to 12V to wake up the VCU. This function drives RC3 high to
// activate the "EV system activation request signal". Without a circuit
// connecting RC3 to the activation signal wire, remote climate control will
// only work during charging and for obvious reasons remote charging won't
// work at all.
//
// On Generation 2 Cars, a CAN bus message is sent to wake up the VCU. This
// function sends that message even to Generation 1 cars which doesn't seem to
// cause any problems.
//

CommandResult vehicle_nissanleaf_remote_command(RemoteCommand command)
  {
  unsigned char data;

  if (sys_features[FEATURE_CANWRITE] == 0)
    {
    // we don't want to transmit when CAN write is disabled.
    return CAN_READ_ONLY;
    }

  // Use GPIO to wake up GEN 1 Leaf with EV SYSTEM ACTIVATION REQUEST
  output_gpo3(TRUE);
  // send GEN 2 wakeup frame
  data = 0;
  vehicle_nissanleaf_send_can_message(0x68c, 1, &data);

  // the GEN 2 sends the command 50ms after the wakeup message, so wait
  delay5(10);

  // send the command once right now
  vehicle_nissanleaf_send_command(command);

  // The GEN 2 Nissan TCU module sends the command repeatedly, so we instruct
  // vehicle_nissanleaf_ticker10th() to do this
  // EV SYSTEM ACTIVATION REQUEST will be released in ticker10th() too
  nl_remote_command = command;
  nl_remote_command_ticker = REMOTE_COMMAND_REPEAT_COUNT;
  if (command == ENABLE_CLIMATE_CONTROL)
    {
    nl_cc_off_ticker = REMOTE_CC_TIME_GRID;
    }


  return OK;
  }

BOOL vehicle_nissanleaf_ticker1(void)
  {
  car_time++;
  if (nl_busactive > 0) nl_busactive--;
  if (nl_abs_active > 0) nl_abs_active--;
  if (car_stale_temps > 0) car_stale_temps--;
  if (car_stale_ambient > 0) car_stale_ambient--;
  if (nl_cc_off_ticker > 0) nl_cc_off_ticker--;

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
    car_doors5bits.HVAC = FALSE;
    }

  if (nl_cc_off_ticker < (REMOTE_CC_TIME_GRID - REMOTE_CC_TIME_BATTERY)
    && nl_cc_off_ticker > 1
    && !car_doors1bits.Charging)
    {
    // we're not on grid power so switch off early
    nl_cc_off_ticker = 1;
    }
  if (nl_cc_off_ticker > 1 && car_doors1bits.CarON)
    {
    // car has turned on during climate control, switch climate control off
    nl_cc_off_ticker = 1;
    }
  if (nl_cc_off_ticker == 1)
    {
    vehicle_nissanleaf_remote_command(DISABLE_CLIMATE_CONTROL);
    }
  }

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_fn_commandhandler()
// Remote Command Handler, decodes command and delegates actual work elsewhere

BOOL vehicle_nissanleaf_fn_commandhandler(BOOL msgmode, int cmd, char *msg)
  {
  CommandResult result;

  // Temporary support via homelink command
  // TODO remove when the app supports CMD_ClimateControl
  if (cmd == CMD_Homelink)
    {
    int button = atoi(msg);
    if (button == 0)
      {
      cmd = CMD_ClimateControl;
      strcpypgm2ram(msg, "1");
      }
    else if (button == 1)
      {
      cmd = CMD_ClimateControl;
      strcpypgm2ram(msg, "0");
      }
    }
  // end of temporary support

  switch (cmd)
    {
    case CMD_StartCharge:
      result = vehicle_nissanleaf_remote_command(START_CHARGING);
      break;
    case CMD_ClimateControl:
      if (strcmppgm2ram(msg, "1") == 0)
        {
        result = vehicle_nissanleaf_remote_command(ENABLE_CLIMATE_CONTROL);
        }
      else if (strcmppgm2ram(msg, "0") == 0)
        {
        result = vehicle_nissanleaf_remote_command(DISABLE_CLIMATE_CONTROL);
        }
      else
        {
        result = INVALID_SYNTAX;
        }
      break;
    default:
      // not one of our commands, return FALSE and the framework will handle it
      return FALSE;
    }

  // we need to put the appropriate message in the buffer and do the sending
  // dance to keep the framework happy
  switch (result)
    {
    case CAN_READ_ONLY:
      STP_NOCANWRITE(net_scratchpad, cmd);
      break;
    case INVALID_SYNTAX:
      STP_INVALIDSYNTAX(net_scratchpad, cmd);
      break;
    case OK:
      STP_OK(net_scratchpad, cmd);
      break;
    }

  if (msgmode)
    {
    net_msg_encode_puts();
    }

  // we handled this command so return TRUE
  return TRUE;
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
  nl_remote_command_ticker = 0;

  vehicle_nissanleaf_load_soc_configuration();

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

  // Msk0 111 1111 1000
  RXM0SIDH = 0b11111111;
  RXM0SIDL = 0b00000000;

  // Flt0 111 1001 1xxx (0x798 .. 0x79f)
  RXF0SIDH = 0b11110011;
  RXF0SIDL = 0b00000000;

  // Flt1 000 0000 0xxx (-)
  RXF1SIDH = 0b00000000;
  RXF1SIDL = 0b00000000;

  // Buffer 1 (filters 2, 3, 4 and 5) for direct can bus messages
  RXB1CON = 0b00000000;

  // Msk1 000 0000 0000 (accept all frames)
  RXM1SIDH = 0b00000000;
  RXM1SIDL = 0b00000000;

  // SET CAN bus BAUDRATE to 500 Kbps
  BRGCON1 = 0x01;
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

  vehicle_version = nissanleaf_version;
  can_capabilities = nissanleaf_capabilities;

  vehicle_fn_poll0 = &vehicle_nissanleaf_poll0;
  vehicle_fn_poll1 = &vehicle_nissanleaf_poll1;
  vehicle_fn_ticker10th = &vehicle_nissanleaf_ticker10th;
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

