////////////////////////////////////////////////////////////////////////////////
// Project:       Open Vehicle Monitor System
// Vehicle:       Renault Zoe
// 
// Based on and with support by:
//  CanZE
//  http://canze.fisch.lu/
//  https://github.com/fesch/CanZE
// 
// History:
// 
// 0.1  5 May 2017  (Michael Balzer)
//  - Initial kickoff release, untested
//  - SOC, odometer, DEBUG command
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

// Module metadata:
rom char vehicle_zoe_version[] = "0.1";
rom char vehicle_zoe_capabilities[] = "";


////////////////////////////////////////////////////////////////////////////////
// Renault Zoe state variables
// 

#pragma udata overlay vehicle_overlay_data

UINT zoe_soc;                   // SOC [2/100%]
UINT32 zoe_odometer;            // Odometer [km]

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
//     0=off, 1=on, 2=charging
// 
// Use broadcasts to query all ECUs. The request is sent to ID 0x7df (broadcast
// listener) and the poller accepts any response ID from 0x7e8..0x7ef.
// The moduleid is not used for broadcasts, just needs to be != 0.
//
// To poll a specific ECU, send to its specific ID (one of 0x7e0..0x7e7).
// ECUs will respond using their specific ID plus 8 (= range 0x7e8..0x7ef).
//

rom vehicle_pid_t vehicle_zoe_polls[] = {
  
  // SOC (also serves for "car awake" detection, so polled while off)
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2002, { 10, 10, 10 } },
  
  // Odometer
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2006, {  0, 60,  0 } },
  
  // -END-
  { 0, 0, 0x00, 0x00, { 0, 0, 0 } }
  
};

// Use a response timeout like this if you cannot detect the car state
// from passive listening but need to poll for a response.
// As SOC is polled every 10 seconds, we assume car is off after 15 seconds.
UINT8 zoe_busactive;
#define ZOE_BUS_TIMEOUT 15     // bus activity timeout in seconds


////////////////////////////////////////////////////////////////////////////////
// Framework hook: _poll0 (ISR; process CAN buffer 0)
// 
// ATT: avoid complex calculations and data processing in ISR code!
//      store in state variables, process in _ticker or _idlepoll hooks
// 
// Renault Zoe: buffer 0 = process OBDII polling result
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

// ISR optimization, see http://www.xargs.com/pic/c18-isr-optim.pdf
#pragma tmpdata high_isr_tmpdata

BOOL vehicle_zoe_poll0(void) {

  // Reset timeout:
  zoe_busactive = ZOE_BUS_TIMEOUT;
  
  // Process OBDII data:
  switch (can_id) {
    
    case 0x7ec:
      switch (vehicle_poll_pid) {
        
        case 0x2002:
          zoe_soc = CAN_UINT(3);
          break;
        
        case 0x2006:
          zoe_odometer = CAN_UINT24(3);
          break;
        
      }
      break;

  }
  
  return TRUE;
}

#pragma tmpdata


////////////////////////////////////////////////////////////////////////////////
// Framework hook: _ticker1 (called about once per second)
// 
// Renault Zoe:
// 

BOOL vehicle_zoe_ticker1(void) {
  
  // Check for response timeout:
  if (zoe_busactive > 0)
    zoe_busactive--;
  if (zoe_busactive == 0) {
    // Timeout: switch OFF
    car_doors3bits.CarAwake = 0;
  }
  
  
  //
  // Translate Zoe data to OVMS car model:
  //
  
  car_SOC = (zoe_soc * 2) / 100;
  car_odometer = MiFromKm(zoe_odometer);
  
  
  //
  // Perform state changes:
  //

  if (car_doors3bits.CarAwake) {
    // Car is ON:
    if (car_parktime != 0) {
      // Car has just turned ON:
      car_parktime = 0; // No longer parking
      net_req_notification(NET_NOTIFY_STAT);
    }
  } else {
    // Car is OFF:
    if (car_parktime == 0) {
      // Car has just turned OFF:
      car_parktime = car_time - 1; // Record it as 1 second ago, so non zero report
      net_req_notification(NET_NOTIFY_STAT);
    }
  }

  // Change polling state:
  vehicle_poll_setstate(car_doors1bits.Charging ? 2 : (car_doors3bits.CarAwake ? 1 : 0));
  // Force active polling:
  vehicle_poll_busactive = 60;

  return TRUE;
}


////////////////////////////////////////////////////////////////////////
// Vehicle specific SMS command: DEBUG
//

BOOL vehicle_zoe_debug_sms(BOOL premsg, char *caller, char *command, char *arguments) {
  char *s;

  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;
  
  caller = net_assert_caller(caller);

  // Start SMS:
  if (premsg)
    net_send_sms_start(caller);

  // Format SMS:
  s = net_scratchpad;
  s = stp_rom(s, "VARS:");
  s = stp_sx(s, "\nds3=", car_doors3);
  s = stp_i(s, "\nvps=", vehicle_poll_state);
  s = stp_i(s, "\nsoc=", zoe_soc);
  s = stp_l(s, "\nodo=", zoe_odometer);

  // Send SMS:
  net_puts_ram(net_scratchpad);

  return TRUE;
}



////////////////////////////////////////////////////////////////////////
// This is the Renault Zoe SMS command table
// (for implementation notes see net_sms::sms_cmdtable comment)
//
// First char = auth mode of command:
//   1:     the first argument must be the module password
//   2:     the caller must be the registered telephone
//   3:     the caller must be the registered telephone, or first argument the module password

BOOL vehicle_zoe_help_sms(BOOL premsg, char *caller, char *command, char *arguments);

rom char vehicle_zoe_sms_cmdtable[][NET_SMS_CMDWIDTH] = {
  "3HELP", // extend HELP output
  "3DEBUG", // Output debug data
  ""
};

rom far BOOL(*vehicle_zoe_sms_hfntable[])(BOOL premsg, char *caller, char *command, char *arguments) = {
  &vehicle_zoe_help_sms,
  &vehicle_zoe_debug_sms
};


// SMS COMMAND DISPATCHERS:
// premsg: TRUE=may replace, FALSE=may extend standard handler
// returns TRUE if handled

BOOL vehicle_zoe_fn_smshandler(BOOL premsg, char *caller, char *command, char *arguments) {
  // called to extend/replace standard command: framework did auth check for us

  UINT8 k;

  // Command parsing...
  for (k = 0; vehicle_zoe_sms_cmdtable[k][0] != 0; k++) {
    if (starts_with(command, (char const rom far*) vehicle_zoe_sms_cmdtable[k] + 1)) {
      // Call sms handler:
      k = (*vehicle_zoe_sms_hfntable[k])(premsg, caller, command, arguments);

      if ((premsg) && (k)) {
        // we're in charge + handled it; finish SMS:
        net_send_sms_finish();
      }

      return k;
    }
  }

  return FALSE; // no vehicle command
}

BOOL vehicle_zoe_fn_smsextensions(char *caller, char *command, char *arguments) {
  // called for specific command: we need to do the auth check

  UINT8 k;

  // Command parsing...
  for (k = 0; vehicle_zoe_sms_cmdtable[k][0] != 0; k++) {
    if (starts_with(command, (char const rom far*) vehicle_zoe_sms_cmdtable[k] + 1)) {
      // we need to check the caller authorization:
      arguments = net_sms_initargs(arguments);
      if (!net_sms_checkauth(vehicle_zoe_sms_cmdtable[k][0], caller, &arguments))
        return FALSE; // failed

      // Call sms handler:
      k = (*vehicle_zoe_sms_hfntable[k])(TRUE, caller, command, arguments);

      if (k) {
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

BOOL vehicle_zoe_help_sms(BOOL premsg, char *caller, char *command, char *arguments) {
  int k;

  //if (premsg)
  //  return FALSE; // run only in extend mode

  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  net_puts_rom("\nZOE:");

  // Start at k=1 to skip HELP command:
  for (k = 1; vehicle_zoe_sms_cmdtable[k][0] != 0; k++) {
    net_puts_rom(" ");
    net_puts_rom(vehicle_zoe_sms_cmdtable[k] + 1);
  }

  return TRUE;
}


////////////////////////////////////////////////////////////////////////////////
// Framework hook: _initialise (initialise vars & CAN interface)
// 
// Renault Zoe: 
// 

BOOL vehicle_zoe_initialise(void) {

  //
  // Initialize vehicle status:
  // 
  // Hint: you can keep SRAM values (uninitialized variables) over crash/reset.
  //    To detect a crash/reset, check the car_type before setting it.
  //    See vehicle_twizy.c for an example.
  //

  car_type[0] = 'R'; // Car is type RZ - Renault Zoe
  car_type[1] = 'Z';
  car_type[2] = 0;
  car_type[3] = 0;
  car_type[4] = 0;

  zoe_busactive = 0;

  zoe_soc = 2500; // = 50%
  zoe_odometer = 0;

  
  //
  // Initialize CAN interface:
  // 

  CANCON = 0b10010000;
  while (!CANSTATbits.OPMODE2); // Wait for Configuration mode

  // We are now in Configuration Mode

  // Filters: low byte bits 7..5 are the low 3 bits of the filter, and bits 4..0 are all zeros
  //          high byte bits 7..0 are the high 8 bits of the filter
  //          So total is 11 bits

  // Buffer 0 (filters 0, 1) for extended PID responses
  RXB0CON = 0b00000000;
  RXM0SIDH = 0b11111111; RXM0SIDL = 0b00000000; // Msk0 111 1111 1000
  RXF0SIDH = 0b11111101; RXF0SIDL = 0b00000000; // Flt0 111 1110 1xxx (0x7e8 .. 0x7ef)
  RXF1SIDH = 0b00000000; RXF1SIDL = 0b00000000; // Flt1 000 0000 0xxx (-)

  // Buffer 1 (filters 2 - 5) currently unused here.
  // These can be used to listen passively to regular process data frames.
  // See other vehicles for mask & filter configuration examples.
  // Hint: you can use open filters if your ISR code is fast enough
  //    and/or there are few/infrequent process data frames.
  RXB1CON = 0b00000000;
  RXM1SIDH = 0b11111111; RXM1SIDL = 0b11100000; // Msk1 111 1111 1111
  RXF2SIDH = 0b00000000; RXF2SIDL = 0b00000000; // Flt2 000 0000 0000 (-)
  RXF3SIDH = 0b00000000; RXF3SIDL = 0b00000000; // Flt3 000 0000 0000 (-)
  RXF4SIDH = 0b00000000; RXF4SIDL = 0b00000000; // Flt4 000 0000 0000 (-)
  RXF5SIDH = 0b00000000; RXF5SIDL = 0b00000000; // Flt5 000 0000 0000 (-)
  
  
  // CAN bus baud rate

  BRGCON1 = 0x01; // SET BAUDRATE to 500 Kbps
  BRGCON2 = 0xD2;
  BRGCON3 = 0x02;

  CIOCON = 0b00100000; // CANTX pin will drive VDD when recessive
  
  if (sys_features[FEATURE_CANWRITE]>0)
    {
    // Normal mode (read/write):
    CANCON = 0b00000000;
    
    // Activate polling:
    vehicle_poll_setpidlist(vehicle_zoe_polls);
    vehicle_poll_setstate(0);
    }
  else
    {
    // Listen only mode:
    CANCON = 0b01100000;
    }

  
  // Hook in...
  
  vehicle_version = vehicle_zoe_version;
  can_capabilities = vehicle_zoe_capabilities;

  vehicle_fn_poll0 = &vehicle_zoe_poll0;
  vehicle_fn_ticker1 = &vehicle_zoe_ticker1;
  vehicle_fn_smshandler = &vehicle_zoe_fn_smshandler;
  vehicle_fn_smsextensions = &vehicle_zoe_fn_smsextensions;

  net_fnbits |= NET_FN_INTERNALGPS; // Require internal GPS
  net_fnbits |= NET_FN_12VMONITOR; // Require 12v monitor
  net_fnbits |= NET_FN_SOCMONITOR; // Require SOC monitor
  net_fnbits |= NET_FN_CARTIME; // Get real UTC time from modem

  sys_features[FEATURE_CARBITS] |= FEATURE_CB_SSMSTIME; // Disable SMS timestamps
  
  return TRUE;
}

