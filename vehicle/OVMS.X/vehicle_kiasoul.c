////////////////////////////////////////////////////////////////////////////////
// Project:       Open Vehicle Monitor System
// Vehicle:       Kia Soul EV
// 
// CAN documentation:
//  CAN reverse engineering done by Tyrel Haveman and AlKl NoFx
//  Google Docs spreadsheet:
//  https://docs.google.com/spreadsheets/d/1YYlZ-IcTQlz-LzaYkHO-7a4SFM8QYs2BGNXiSU5_EwI/
// 
// History:
// 
// 0.1  7 Oct 2015  (Michael Balzer)
//  - Initial release, untested
//  - VIN, car status, doors & charging status, SOC, estimated range, speed
// 
// 0.2  16 Oct 2015  (Michael Balzer)
//  - OBD diag pages 01 & 05
//  - SMS command "DEBUG"
// 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.


#include <stdlib.h>
#include <delays.h>
#include <string.h>
#include "ovms.h"
#include "params.h"
#include "net_msg.h"
#include "net_sms.h"

// Module version:
rom char vehicle_kiasoul_version[] = "0.2";


////////////////////////////////////////////////////////////////////////////////
// Kia Soul state variables
// 

#pragma udata overlay vehicle_overlay_data

UINT8 ks_busactive;           // indication that bus is active
#define KS_BUS_TIMEOUT 10     // bus activity timeout in seconds

car_doors1bits_t ks_doors1;   // Main status flags

UINT ks_soc;                  // SOC [1/10 %]
UINT ks_estrange;             // Estimated range remaining [km]
UINT ks_chargepower;          // in [1/256 kW]
UINT ks_speed;                // in [kph]

UINT8 ks_diag_01[0x3D-3];     // diag data page 01
UINT8 ks_diag_05[0x2C-3];     // diag data page 05

// test/debug:
UINT8 ks_diag_01_len;
UINT8 ks_diag_05_len;

#pragma udata


////////////////////////////////////////////////////////////////////////////////
// OBDII polling table:
// 
//    typedef struct
//    {
//      unsigned int moduleid; // send request to (0 = end of list)
//      unsigned int rmoduleid; // expect response from (0 = broadcast)
//      unsigned char type; // see vehicle.h
//      unsigned int pid; // the PID to request
//      unsigned int polltime[VEHICLE_POLL_NSTATES]; // poll frequency
//    } vehicle_pid_t;
//
// Polling types ("modes") supported: see vehicle.h
// 
// polltime[state] = poll period in seconds, i.e. 10 = poll every 10 seconds
//
// state: 0..2; set by vehicle_poll_setstate()
//      i.e. 0=parking, 1=driving
//

rom vehicle_pid_t vehicle_kiasoul_polls[] = 
{
  { 0x7e2, 0, VEHICLE_POLL_TYPE_OBDIIVEHICLE, 0x02, {  120, 120, 0 } }, // VIN
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIISESSION, 0x90, {  60, 10, 0 } }, // Start diag session
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x01, {  60, 10, 0 } }, // Diag page 01
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x05, {  60, 10, 0 } }, // Diag page 05
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIISESSION, 0x00, {  60, 10, 0 } }, // End diag session (?)
  { 0, 0, 0x00, 0x00, { 0, 0, 0 } }
};


// ISR optimization, see http://www.xargs.com/pic/c18-isr-optim.pdf
#pragma tmpdata high_isr_tmpdata

////////////////////////////////////////////////////////////////////////////////
// Framework hook: _poll0 (ISR; process CAN buffer 0)
// 
// ATT: avoid complex calculations and data processing in ISR code!
//      store in state variables, process in _ticker or _idlepoll hooks
// 
// Kia Soul: buffer 0 = process OBDII polling result
// 
// Available framework variables:
//  can_id            = CAN RX ID (poll -> rmoduleid)
//  can_filter        = CAN RX filter number triggered (see init)
//  can_databuffer    = CAN RX data buffer (8 bytes)
//  can_datalength    = CAN RX data length (in data buffer)
//
// OBDII/UDS polling response:
//  vehicle_poll_type = last poll -> type
//  vehicle_poll_pid  = last poll -> pid
//
// Multi frame response: hook called once per frame
//  vehicle_poll_ml_frame   = frame counter (0..n)
//  can_datalength          = frame length
//                            (3 bytes in frame 0, max 7 bytes in frames 1..n)
//  vehicle_poll_ml_offset  = msg length including this frame
//  vehicle_poll_ml_remain  = msg length remaining after this frame
//

BOOL vehicle_kiasoul_poll0(void)
  {
  UINT8 i;
  UINT base;

  // process OBDII data:
  switch (can_id)
    {
    
    case 0x7ea:
      switch (vehicle_poll_pid)
        {
        case 0x02:
          // VIN (multi-line response):
          // VIN length is 20 on Kia => skip first frame (3 bytes):
          if (vehicle_poll_ml_frame > 0)
            {
            base = vehicle_poll_ml_offset - can_datalength - 3;
            for (i=0; (i<can_datalength) && ((base+i)<(sizeof(car_vin)-1)); i++)
              car_vin[base+i] = can_databuffer[i];
            if (vehicle_poll_ml_remain == 0)
              car_vin[base+i] = 0;
            }
          break;
        }
      break;
  
    case 0x7ec:
      switch (vehicle_poll_pid)
        {
        case 0x01:
          // diag page 01: skip first frame (no data)
          if (vehicle_poll_ml_frame > 0)
            {
            base = vehicle_poll_ml_offset - can_datalength - 3;
            for (i=0; (i<can_datalength) && ((base+i)<sizeof(ks_diag_01)); i++)
              ks_diag_01[base+i] = can_databuffer[i];
            ks_diag_01_len = vehicle_poll_ml_offset;
            }
          break;
        case 0x05:
          // diag page 05: skip first frame (no data)
          if (vehicle_poll_ml_frame > 0)
            {
            base = vehicle_poll_ml_offset - can_datalength - 3;
            for (i=0; (i<can_datalength) && ((base+i)<sizeof(ks_diag_05)); i++)
              ks_diag_05[base+i] = can_databuffer[i];
            ks_diag_05_len = vehicle_poll_ml_offset;
            }
          break;
        }
      break;
  
    }

  return TRUE; 
  }


////////////////////////////////////////////////////////////////////////////////
// Framework hook: _poll1 (ISR; process CAN buffer 1)
// 
// ATT: avoid complex calculations and data processing in ISR code!
//      store in state variables, process in _ticker or _idlepoll hooks
// 
// Kia Soul: buffer 1 = direct CAN bus processing
// 
// Available framework variables:
//  can_id            = CAN RX ID
//  can_filter        = CAN RX filter number triggered (see init)
//  can_databuffer    = CAN RX data buffer (8 bytes)
//  can_datalength    = CAN RX data length (in data buffer)
//

BOOL vehicle_kiasoul_poll1(void)
  {
  UINT val1;
  
  // reset bus activity timeout:
  ks_busactive = KS_BUS_TIMEOUT;

  // process CAN data:
  switch (can_id)
    {
    
    case 0x018:
      // Car is ON:
      ks_doors1.CarON = 1;
      ks_doors1.Charging = 0;
      // Doors status:
      ks_doors1.FrontLeftDoor = ((CAN_BYTE(0) & 0x10) > 0);
      ks_doors1.FrontRightDoor = ((CAN_BYTE(0) & 0x80) > 0);
      break;
      
    case 0x200:
      // Estimated range:
      val1 = (UINT)CAN_BYTE(2) << 1;
      if (val1 > 0)
        ks_estrange = val1;
      break;
      
    case 0x4f2:
      // Speed:
      ks_speed = ((UINT)CAN_BYTE(1) << 1) + ((CAN_BYTE(2) & 0x80) >> 7);
      break;
      
    case 0x581:
      // Car is CHARGING:
      ks_doors1.CarON = 0;
      ks_doors1.Charging = (CAN_BYTE(3) != 0);
      ks_chargepower = ((UINT)CAN_BYTE(6) << 8) + CAN_BYTE(7);
      break;
      
    case 0x594:
      // SOC:
      val1 = (UINT)CAN_BYTE(5) * 5 + CAN_BYTE(6);
      if (val1 > 0)
        ks_soc = val1;
      break;
      
    }
  
  return TRUE;
  }

#pragma tmpdata


////////////////////////////////////////////////////////////////////////////////
// Framework hook: _ticker1 (called about once per second)
// 
// Kia Soul: 
// 

BOOL vehicle_kiasoul_ticker1(void)
  {
  
  // 
  // Check CAN bus activity timeout:
  //
  
  if (ks_busactive > 0)
    ks_busactive--;
  
  if (ks_busactive == 0)
    {
    // CAN bus is inactive, switch OFF:
    ks_doors1.CarON = 0;
    ks_doors1.Charging = 0;
    ks_chargepower = 0;
    ks_speed = 0;
    }
  

  // 
  // Convert KS status data to OVMS framework:
  //
  
  car_SOC = ks_soc / 10; // no rounding to avoid 99.5 = 100
  
  car_estrange = MiFromKm(ks_estrange);
  car_idealrange = car_estrange; // todo

  if (can_mileskm == 'M')
    car_speed = MiFromKm(ks_speed); // mph
  else
    car_speed = ks_speed; // kph
  
  
  //
  // Check for car status changes:
  //
  
  car_doors1bits.FrontLeftDoor = ks_doors1.FrontLeftDoor;
  car_doors1bits.FrontRightDoor = ks_doors1.FrontRightDoor;
  
  if (ks_doors1.CarON)
    {
    // Car is ON
    //vehicle_poll_setstate(1);
    car_doors1bits.ChargePort = 0;
    car_doors1bits.HandBrake = 0;
    car_doors1bits.CarON = 1;
    if (car_parktime != 0)
      {
      car_parktime = 0; // No longer parking
      net_req_notification(NET_NOTIFY_ENV);
      }
    }
  else
    {
    // Car is OFF
    //vehicle_poll_setstate(0);
    car_doors1bits.HandBrake = 1;
    car_doors1bits.CarON = 0;
    car_speed = 0;
    if (car_parktime == 0)
      {
      car_parktime = car_time-1; // Record it as 1 second ago, so non zero report
      net_req_notification(NET_NOTIFY_ENV);
      }
    }
  
  
  //
  // Check for charging status changes:
  //
  
  if (ks_doors1.Charging)
    {
    if (!car_doors1bits.Charging)
      {
      // Charging started:
      car_doors1bits.PilotSignal = 1;
      car_doors1bits.ChargePort = 1;
      car_doors1bits.Charging = 1;
      car_doors5bits.Charging12V = 1;
      car_chargeduration = 0; // Reset charge duration
      car_chargestate = 1; // Charging state
      net_req_notification(NET_NOTIFY_STAT);
      }
    else
      {
      // Charging continues:
      car_chargeduration++;
      if (car_SOC >= 95)
        {
        car_chargestate = 2; // Topping off
        net_req_notification(NET_NOTIFY_ENV);
        }
      }
    }
  else if (car_doors1bits.Charging)
    {
    // Charge completed or interrupted:
    car_doors1bits.PilotSignal = 0;
    car_doors1bits.Charging = 0;
    car_doors5bits.Charging12V = 0;
    if (car_SOC == 100)
      car_chargestate = 4; // Charge DONE
    else
      car_chargestate = 21; // Charge STOPPED
    net_req_notification(NET_NOTIFY_STAT);
    }
  

  }


////////////////////////////////////////////////////////////////////////
// Vehicle specific SMS command: DEBUG
//

BOOL vehicle_kiasoul_debug_sms(BOOL premsg, char *caller, char *command, char *arguments)
  {
  char *s;
  int i;
  
  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  if (!caller || !*caller)
    {
    caller = par_get(PARAM_REGPHONE);
    if (!caller || !*caller)
      return FALSE;
    }

  // Start SMS:
  if (premsg)
    net_send_sms_start(caller);

  // Format SMS:
  s = net_scratchpad;

  if (arguments && (arguments[0]=='1'))
    {
    // output ks_diag_01:
    s = stp_i(s, "DIAG_01:", ks_diag_01_len);
    for (i=0; i<sizeof(ks_diag_01); i++)
      s = stp_sx(s, (i%7)?"":"\n", ks_diag_01[i]);
    }
  else if (arguments && (arguments[0]=='5'))
    {
    // output ks_diag_05:
    s = stp_i(s, "DIAG_05:", ks_diag_05_len);
    for (i=0; i<sizeof(ks_diag_05); i++)
      s = stp_sx(s, (i%7)?"":"\n", ks_diag_05[i]);
    }
  else
    {
    // output status vars:
    s = stp_rom(s, "VARS:");
    s = stp_sx(s, " doors1=", *((UINT8*)&ks_doors1));
    s = stp_i(s, " soc=", ks_soc);
    s = stp_i(s, " estrange=", ks_estrange);
    s = stp_i(s, " chargepower=", ks_chargepower);
    s = stp_i(s, " speed=", ks_speed);
    }

  // Send SMS:
  net_puts_ram(net_scratchpad);

  return TRUE;
  }


////////////////////////////////////////////////////////////////////////
// This is the Kia Soul SMS command table
// (for implementation notes see net_sms::sms_cmdtable comment)
//
// First char = auth mode of command:
//   1:     the first argument must be the module password
//   2:     the caller must be the registered telephone
//   3:     the caller must be the registered telephone, or first argument the module password

BOOL vehicle_kiasoul_help_sms(BOOL premsg, char *caller, char *command, char *arguments);

rom char vehicle_kiasoul_sms_cmdtable[][NET_SMS_CMDWIDTH] =
  {
  "3DEBUG",       // Output debug data
  "3HELP",        // extend HELP output
  ""
  };

rom far BOOL(*vehicle_kiasoul_sms_hfntable[])(BOOL premsg, char *caller, char *command, char *arguments) =
  {
  &vehicle_kiasoul_debug_sms,
  &vehicle_kiasoul_help_sms
  };


// SMS COMMAND DISPATCHERS:
// premsg: TRUE=may replace, FALSE=may extend standard handler
// returns TRUE if handled

BOOL vehicle_kiasoul_fn_smshandler(BOOL premsg, char *caller, char *command, char *arguments)
  {
  // called to extend/replace standard command: framework did auth check for us

  UINT8 k;

  // Command parsing...
  for (k = 0; vehicle_kiasoul_sms_cmdtable[k][0] != 0; k++)
    {
    if (starts_with(command, (char const rom far*)vehicle_kiasoul_sms_cmdtable[k] + 1))
      {
      // Call sms handler:
      k = (*vehicle_kiasoul_sms_hfntable[k])(premsg, caller, command, arguments);

      if ((premsg) && (k))
        {
        // we're in charge + handled it; finish SMS:
        net_send_sms_finish();
        }

      return k;
      }
    }

  return FALSE; // no vehicle command
  }

BOOL vehicle_kiasoul_fn_smsextensions(char *caller, char *command, char *arguments)
  {
  // called for specific command: we need to do the auth check

  UINT8 k;

  // Command parsing...
  for (k = 0; vehicle_kiasoul_sms_cmdtable[k][0] != 0; k++)
    {
    if (starts_with(command, (char const rom far*)vehicle_kiasoul_sms_cmdtable[k] + 1))
      {
      // we need to check the caller authorization:
      arguments = net_sms_initargs(arguments);
      if (!net_sms_checkauth(vehicle_kiasoul_sms_cmdtable[k][0], caller, &arguments))
        return FALSE; // failed

      // Call sms handler:
      k = (*vehicle_kiasoul_sms_hfntable[k])(TRUE, caller, command, arguments);

      if (k)
        {
        // we're in charge + handled it; finish SMS:
        net_send_sms_finish();
        }

      return k;
      }
    }

  return FALSE; // no vehicle command
  }


////////////////////////////////////////////////////////////////////////
// Vehicle specific SMS command output extension: HELP
//

BOOL vehicle_kiasoul_help_sms(BOOL premsg, char *caller, char *command, char *arguments)
  {
  int k;

  if (premsg)
    return FALSE; // run only in extend mode

  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  net_puts_rom("\nKIA:");

  for (k = 0; vehicle_kiasoul_sms_cmdtable[k][0] != 0; k++)
    {
    net_puts_rom(" ");
    net_puts_rom(vehicle_kiasoul_sms_cmdtable[k] + 1);
    }

  return TRUE;
  }


////////////////////////////////////////////////////////////////////////////////
// Framework hook: _initialise (initialise vars & CAN interface)
// 
// Kia Soul: 
// 

BOOL vehicle_kiasoul_initialise(void)
  {

  //
  // Initialize vehicle status:
  // 
  
  car_type[0] = 'K'; // Car is type KS - Kia Soul
  car_type[1] = 'S';
  car_type[2] = 0;
  car_type[3] = 0;
  car_type[4] = 0;

  ks_busactive = 0;

  ks_doors1.CarON = 0;
  ks_doors1.Charging = 0;
  ks_doors1.FrontLeftDoor = 0;
  ks_doors1.FrontRightDoor = 0;
  
  ks_soc = 500; // avoid low SOC alerts
  
  ks_estrange = 0;
  ks_chargepower = 0;
  
  ks_speed = 0;
  
  memset(ks_diag_01, 0, sizeof(ks_diag_01));
  memset(ks_diag_05, 0, sizeof(ks_diag_05));

  ks_diag_01_len = 0;
  ks_diag_05_len = 0;
  
  
  //
  // Initialize CAN interface:
  // 
  
  CANCON = 0b10010000;
  while (!CANSTATbits.OPMODE2); // Wait for Configuration mode

  // We are now in Configuration Mode

  // Filters: low byte bits 7..5 are the low 3 bits of the filter, and bits 4..0 are all zeros
  //          high byte bits 7..0 are the high 8 bits of the filter
  //          So total is 11 bits

  // Buffer 0 (filters 0, 1) for OBDII responses
  RXB0CON = 0b00000000;

  RXM0SIDL = 0b00000000;        // Mask0   = 111 1111 1000
  RXM0SIDH = 0b11111111;

  RXF0SIDL = 0b00000000;        // Filter0 = 111 1110 1000 (0x7E8 .. 0x7EF)
  RXF0SIDH = 0b11111101;        // (OBDII responses)

  RXF1SIDL = 0b00000000;        // Filter1 = 111 1111 1000 (0x7F8 .. 0x7FF)
  RXF1SIDH = 0b11111111;        // (should not be triggered)
  
  
  // Buffer 1 (filters 2, 3, 4 and 5) for direct can bus messages
  RXB1CON  = 0b00000000;	// RX buffer1 uses Mask RXM1 and filters RXF2, RXF3, RXF4, RXF5

  RXM1SIDL = 0b00000000;
  RXM1SIDH = 0b11100000;	// Set Mask1 to 111 0000 0000

  RXF2SIDL = 0b00000000;	// Filter2 = CAN ID 0x500 - 0x5FF
  RXF2SIDH = 0b10100000;

  RXF3SIDL = 0b00000000;	// Filter3 = CAN ID 0x200 - 0x2FF
  RXF3SIDH = 0b01000000;

  RXF4SIDL = 0b00000000;        // Filter4 = CAN ID 0x400 - 0x4FF
  RXF4SIDH = 0b10000000;

  RXF5SIDL = 0b00000000;        // Filter5 = CAN ID 0x000 - 0x0FF
  RXF5SIDH = 0b00000000;

  // CAN bus baud rate

  BRGCON1 = 0x01; // SET BAUDRATE to 500 Kbps
  BRGCON2 = 0xD2;
  BRGCON3 = 0x02;

  CIOCON = 0b00100000; // CANTX pin will drive VDD when recessive
  if (sys_features[FEATURE_CANWRITE]>0)
    {
    CANCON = 0b00000000;  // Normal mode
    }
  else
    {
    CANCON = 0b01100000; // Listen only mode, Receive bufer 0
    }

  // Hook in...
  
  vehicle_version = vehicle_kiasoul_version;
  
  vehicle_fn_poll0 = &vehicle_kiasoul_poll0;
  vehicle_fn_poll1 = &vehicle_kiasoul_poll1;
  vehicle_fn_ticker1 = &vehicle_kiasoul_ticker1;
  vehicle_fn_smshandler = &vehicle_kiasoul_fn_smshandler;
  vehicle_fn_smsextensions = &vehicle_kiasoul_fn_smsextensions;
  
  if (sys_features[FEATURE_CANWRITE]>0)
    vehicle_poll_setpidlist(vehicle_kiasoul_polls);
  vehicle_poll_setstate(0);

  net_fnbits |= NET_FN_INTERNALGPS;   // Require internal GPS
  net_fnbits |= NET_FN_12VMONITOR;    // Require 12v monitor
  net_fnbits |= NET_FN_SOCMONITOR;    // Require SOC monitor
  net_fnbits |= NET_FN_CARTIME;       // Get real UTC time from modem

  return TRUE;
  }

