/*
;    Project:       Open Vehicle Monitor System
;    Date:          6 May 2012
;
;    Changes:
;    2.0  19.08.2013 (Haakon)
;           Migration of SMS from standard handler to vehicle_thinkcity.c. All functions are based on the vehicle_twizy.c
;           - SMS cmd "STAT": Moved specific output to vehicle_thinkcity.c
;             (SOC, batt temp, PCU temp, aux batt volt, traction batt volt and current)
;           - SMS cmd "DEBUG": Output Think City specific flags/variables
;             (SOC, Charge Enable, Open Circuit Voltage Measurement, End of Charge, Doors1 status and Charge Status)
;           - SMS cmd "HELP": adds commands help. "Help" is currently not working!
;    1.4  15.08.2013 (Haakon)
;         - Chargestate detection at EOC improved by testing state of the EOC-flag and if car_linevoltage > 100
;           (I have seen that line voltage is slightly above 0V some times, 100V is chosed to be sure)
;         - car_tpem in App is fixed by setting car_stale_temps > 0. (PID for car_tmotor is unknown yet and is not implemented)
;         - Added car_chargelimit, appears in the app during charging.
;           The text is a little hard to read in the app due to color and alignment and should be moved slightly to the left.
;
;    1.3  14.08.2013 (Haakon)
;         - Added car_tpem, PCU ambient temp, showing up in SMS but not in App
;         - Fix in determation of car_chargestate / car_doors1. Have not found any bits on the CAN showing "on plug detect" or "pilot signal set"
;         - Temp meas on Zebra-Think does not work as long as car_tbattery is 'signed char' (8 bits), Zebra operates at 270 deg.C and needs 9 bits.
;           E.g. temp of 264C (1 0000 1000) will be presented as 8C (0000 0000)
;           Request made to project for chang car_tbattery to ing signed int16.
;           Temporarily resolved by adding new global variable "unsigned int thinkcity_pack_temp" in ovms.c and ovms.h
;         - Battery current and voltage added to the SMS "STAT?" by adding new global variables
;           "unsigned int thinkcity_pack_voltage" and signed "int thinkcity_pack_current" to ovms.c and ovms.h files
;
;    1.2  13.08.2013 (Haakon)
;         - car_idealrange and car_estimatedrange hardcoded by setting 180km (ideal) and 150km (estimated).
;           This applies for Think City with Zebra-batterty and gen0/gen1 PowerControlUnit (PCU) with 180km range, VIN J004001-J005198.
;           Other Think City with EnerDel battery and Zebra with gen2 PCU have 160km range ideal and maybe 120km estimated.
;           Driving style and weather is not beeing considered.
;         - car_linevoltage and car_chargecurrent added to vehicle_thinkcity_poll1
;         - Mask/Filter RXM1 and RXF2 changed to pull 0x26_ messages
;         - net_msg.c changed in net_prep_stat to tweek SMS "STAT?" printout. Changes from line 1035.
;           Line 1013 and 1019 changed to "if (car_chargelimit_soclimit > 0)" and "if (car_chargelimit_rangelimit > 0)" respectively.
;
;    1.1  04.08.2013 (Haakon)
;         - Added more global parameters 'car_*'
;         - Added filters - RXF2 RXF3 and mask - RXM1 for can receive buffer 1 - RXB1, used in poll1
;
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
#include <stdio.h>
#include <math.h>
#include "ovms.h"
#include "params.h"
#include "led.h"
#include "utils.h"
#include "net_sms.h"
#include "net_msg.h"



/***************************************************************
 * Think City definitions
 */

// Think City specific features:


// Think City specific commands:
#define CMD_Debug                   200 // ()


#pragma udata overlay vehicle_overlay_data
unsigned int thinkcity_pack_voltage = 0; // Zebra battery pack voltage
signed int   thinkcity_pack_current = 0;  // Zebra battery pack current
unsigned int thinkcity_eoc_bit;
unsigned int thinkcity_chrgen_bit;
unsigned int thinkcity_ocvmeas_bit;

#pragma udata

////////////////////////////////////////////////////////////////////////
// vehicle_thinkcity_ticker1()
// This function is an entry point from the main() program loop, and
// gives the CAN framework a ticker call approximately once per second
//
BOOL vehicle_thinkcity_ticker1(void)
  {
  if (car_stale_ambient>0) car_stale_ambient--;
  if (car_stale_temps>0) car_stale_temps--;

  car_time++;


  return FALSE;
  }

////////////////////////////////////////////////////////////////////////
// can_poll()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to poll for data.
//
BOOL vehicle_thinkcity_poll0(void)
  {
  unsigned int id = ((unsigned int)RXB0SIDL >>5)
                  + ((unsigned int)RXB0SIDH <<3);

  can_datalength = RXB0DLC & 0x0F; // number of received bytes
  can_databuffer[0] = RXB0D0;
  can_databuffer[1] = RXB0D1;
  can_databuffer[2] = RXB0D2;
  can_databuffer[3] = RXB0D3;
  can_databuffer[4] = RXB0D4;
  can_databuffer[5] = RXB0D5;
  can_databuffer[6] = RXB0D6;
  can_databuffer[7] = RXB0D7;

  RXB0CONbits.RXFUL = 0; // All bytes read, Clear flag

  // Check for charging state
  // car_door1 Bits used: 0-7
  // Function net_prep_stat in net_msg.c uses car_doors1bits.ChargePort to determine SMS-printout
  //
  // 0x04 Chargeport open, bit 2 set
  // 0x08 Pilot signal on, bit 3 set
  // 0x10 Vehicle charging, bit 4 set
  // 0x0C Chargeport open and pilot signal, bits 2,3 set
  // 0x1C Bits 2,3,4 set

  switch (id)
    {
    case 0x301:
      car_SOC = 100 - ((((unsigned int)can_databuffer[4]<<8) + can_databuffer[5])/10);
      car_idealrange = (111.958773 * car_SOC / 100);
      car_estrange = (93.205678 * car_SOC / 100 );
      car_tbattery = (((signed int)can_databuffer[6]<<8) + can_databuffer[7])/10;
      thinkcity_pack_voltage = (((unsigned int) can_databuffer[2] << 8) + can_databuffer[3]) / 10;
      thinkcity_pack_current = (((int) can_databuffer[0] << 8) + can_databuffer[1]) / 10;
      car_stale_temps = 60;
      break;

    case 0x304:
      thinkcity_eoc_bit = (can_databuffer[3] & 0x01) ;
      if (thinkcity_eoc_bit == 1) // Is EOC_bit (bit 0) is set?
        {
        car_chargestate = 4; //Done
        car_chargemode = 0;
        if (car_linevoltage > 100)  // Is AC line voltage > 100 ?
          {
          car_doors1 = 0x0C;  // Charge connector connected
          }
        else
          car_doors1 = 0x00;  // Charge connector disconnected
        }
      break;
	
    case 0x305:
      thinkcity_chrgen_bit = (can_databuffer[3] & 0x01);
      if (thinkcity_chrgen_bit == 1) // Is charge_enable_bit (bit 0) is set (turns to 0 during 0CV-measuring at 80% SOC)?
        {
        car_chargestate = 1; //Charging
        car_chargemode = 0;
        car_doors1 = 0x1C;
        }
      thinkcity_ocvmeas_bit = (can_databuffer[3] & 0x02);
      if (thinkcity_ocvmeas_bit == 2) // Is ocv_meas_in_progress (bit 1) is  set?
        {
        car_chargestate = 2; //Top off
        car_chargemode = 0;
        car_doors1 = 0x0C;
        }
      break;

    case 0x311:
      car_chargelimit =  ((unsigned char) can_databuffer[1]) * 0.2 ;  // Charge limit, controlled by the "power charge button", usually 9 or 15A.
      break;
    }

  return TRUE;
  }

BOOL vehicle_thinkcity_poll1(void)
  {
  unsigned int id = ((unsigned int)RXB1SIDL >>5)
                  + ((unsigned int)RXB1SIDH <<3);

  can_datalength = RXB1DLC & 0x0F; // number of received bytes
  can_databuffer[0] = RXB1D0;
  can_databuffer[1] = RXB1D1;
  can_databuffer[2] = RXB1D2;
  can_databuffer[3] = RXB1D3;
  can_databuffer[4] = RXB1D4;
  can_databuffer[5] = RXB1D5;
  can_databuffer[6] = RXB1D6;
  can_databuffer[7] = RXB1D7;

  RXB1CONbits.RXFUL = 0; // All bytes read, Clear flag

  switch (id)
    {
    case 0x263:
      car_stale_temps = 60;
      car_chargecurrent =  ((signed int) can_databuffer[0]) * 0.2 ;
      car_linevoltage = (unsigned int) can_databuffer[1] ;
      if (car_linevoltage < 100)  // AC line voltage < 100
        {
        car_doors1 = 0x00;  // charging connector unplugged
        }
      car_tpem = ((signed char) can_databuffer[2]) * 0.5 ; // PCU abmbient temp
      break;
    }

  return TRUE;
  }


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/*******************************************************************************
 * COMMAND PLUGIN CLASSES: CODE DESIGN PATTERN
 *
 * A command class handles a group of related commands (functions).
 *
 * STANDARD METHODS:
 *
 *  MSG status output:
 *      char newstat = _msgp( char stat, int cmd, ... )
 *      stat: chaining status pushes with optional crc diff checking
 *
 *  MSG command handler:
 *      BOOL handled = _cmd( BOOL msgmode, int cmd, char *arguments )
 *      msgmode: FALSE=just execute / TRUE=also output reply
 *      arguments: "," separated (see MSG protocol)
 *
 *  SMS command handler:
 *      BOOL handled = _sms( BOOL premsg, char *caller, char *command, char *arguments )
 *      premsg: TRUE=replace system handler / FALSE=append to system handler
 *          (framework will first try TRUE, if not handled fall back to system
 *          handler and afterwards try again with FALSE)
 *      arguments: " " separated / free form
 *
 * STANDARD BEHAVIOUR:
 *
 *  cmd=0 / command=NULL shall map to the default action (push status).
 *      (if a specific MSG protocol push ID has been assigned,
 *       _msgp() shall use that for cmd=0)
 *
 * PLUGGING IN:
 *
 *  - add _cmd() to vehicle fn_commandhandler()
 *  - add _sms() to vehicle sms_cmdtable[]/sms_hfntable[]
 *
 */
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


/***********************************************************************
 * COMMAND CLASS: DEBUG
 *
 *  MSG: CMD_Debug ()
 *  SMS: DEBUG
 *      - output internal state dump for debugging
 *
 */

char vehicle_thinkcity_debug_msgp(char stat, int cmd)
{
  //static WORD crc; // diff crc for push msgs
  char *s;

  stat = net_msgp_environment(stat);

  s = stp_rom(net_scratchpad, "MP-0 ");
  s = stp_i(s, "c", cmd ? cmd : CMD_Debug);
  //s = stp_x(s, ",0,", thinkcity_status);
  s = stp_x(s, ",", car_doors1);
  //s = stp_x(s, ",", car_doors5);
  //s = stp_i(s, ",", car_chargestate);
  //s = stp_i(s, ",", thinkcity_speed);
  //s = stp_i(s, ",", thinkcity_power);
  //s = stp_ul(s, ",", thinkcity_odometer);
  s = stp_i(s, ",", car_SOC);
  //s = stp_i(s, ",", thinkcity_soc_min);
  //s = stp_i(s, ",", thinkcity_soc_max);
  //s = stp_i(s, ",", thinkcity_range);
  //s = stp_i(s, ",", thinkcity_soc_min_range);
  s = stp_i(s, ",", car_estrange);
  s = stp_i(s, ",", car_idealrange);
  s = stp_i(s, ",", can_minSOCnotified);

  net_msg_encode_puts();
  return (stat == 2) ? 1 : stat;
}

BOOL vehicle_thinkcity_debug_cmd(BOOL msgmode, int cmd, char *arguments)
{
#ifdef OVMS_DIAGMODULE
  if (arguments && *arguments)
  {
    // Run simulator:
    //vehicle_thinkcity_simulator_run(atoi(arguments));
  }
#endif // OVMS_DIAGMODULE

  if (msgmode)
    vehicle_thinkcity_debug_msgp(0, cmd);

  return TRUE;
}

BOOL vehicle_thinkcity_debug_sms(BOOL premsg, char *caller, char *command, char *arguments)
{
  char *s;

#ifdef OVMS_DIAGMODULE
  if (arguments && *arguments)
  {
    // Run simulator:
    //vehicle_thinkcity_simulator_run(atoi(arguments));
  }
#endif // OVMS_DIAGMODULE

  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  if (!caller || !*caller)
  {
    caller = par_get(PARAM_REGPHONE);
    if (!caller || !*caller)
      return FALSE;
  }

  if (premsg)
    net_send_sms_start(caller);

  // SMS PART:

  s = net_scratchpad;
  s = stp_rom(s, "Debug:");
  s = stp_i(s, "\r SOC=", car_SOC);
  s = stp_i(s, "\r CEN= ", thinkcity_chrgen_bit);  // Charge Enable
  s = stp_i(s, "\r OCV= ", thinkcity_ocvmeas_bit); // Open Circuit Voltage Meas
  s = stp_i(s, "\r EOC= ", thinkcity_eoc_bit);  // End of Charge
  s = stp_x(s, "\r DS1= ", car_doors1); // Car doors1
  s = stp_i(s, "\r CHS= ", car_chargestate);  // Charge state
  net_puts_ram(net_scratchpad);

#ifdef OVMS_DIAGMODULE
  // ...MORE IN DIAG MODE (serial port):
  if (net_state == NET_STATE_DIAGMODE)
  {
    s = net_scratchpad;
    s = stp_i(s, "\n# FIX=", car_gpslock);
    s = stp_lx(s, " LAT=", car_latitude);
    s = stp_lx(s, " LON=", car_longitude);
    s = stp_i(s, " ALT=", car_altitude);
    s = stp_i(s, " DIR=", car_direction);
    net_puts_ram(net_scratchpad);
  }
#endif // OVMS_DIAGMODULE

#ifdef OVMS_THINKCITY_BATTMON
  // battery bit fields:
  s = net_scratchpad;
  s = stp_x(s, "\n# vw=", thinkcity_batt[0].volt_watches);
  s = stp_x(s, " va=", thinkcity_batt[0].volt_alerts);
  s = stp_x(s, " lva=", thinkcity_batt[0].last_volt_alerts);
  s = stp_x(s, " tw=", thinkcity_batt[0].temp_watches);
  s = stp_x(s, " ta=", thinkcity_batt[0].temp_alerts);
  s = stp_x(s, " lta=", thinkcity_batt[0].last_temp_alerts);
  s = stp_x(s, " ss=", thinkcity_batt_sensors_state);
  net_puts_ram(net_scratchpad);
#endif // OVMS_THINKCITY_BATTMON


  return TRUE;
}

/***********************************************************************
 * COMMAND CLASS: CHARGE STATUS / ALERT
 *
 *  MSG: CMD_Alert()
 *  SMS: STAT
 *      - output charge status
 *
 */

// prepmsg: Generate STAT message for SMS & MSG mode
// - Charge state
// - Temp battery and PCU
// - Aux battery- and traction battery voltage
// - Traction battery current
//
// => cat to net_scratchpad (to be sent as SMS or MSG)
//    line breaks: default \r for MSG mode >> cr2lf >> SMS

void vehicle_thinkcity_stat_prepmsg(void)
{
  char *s;

  // append to net_scratchpad:
  s = strchr(net_scratchpad, 0);

  if (car_doors1bits.ChargePort)
  {
    // Charge port door is open, we are charging
    switch (car_chargestate)
    {
    case 0x01:
      s = stp_rom(s, "Charging");
      break;
    case 0x02:
      s = stp_rom(s, "Charging, Topping off");
      break;
    case 0x04:
      s = stp_rom(s, "Charging Done");
      break;
    default:
      s = stp_rom(s, "Charging Stopped");
    }

  }
  else
  {
    // Charge port door is closed, not charging
    s = stp_rom(s, "Not charging");
  }

/*
  // Estimated + Ideal Range:
  if (can_mileskm == 'M')
  {
    s = stp_i(s, "\r Range: ", car_estrange);
    s = stp_i(s, " - ", car_idealrange);
    s = stp_rom(s, " mi");
  }
  else
  {
    s = stp_i(s, "\r Range: ", KmFromMi(car_estrange));
    s = stp_i(s, " - ", KmFromMi(car_idealrange));
    s = stp_rom(s, " km");
  }

*/

  s = stp_i(s, "\r SOC: ", car_SOC);
  s = stp_rom(s, "%");
  s = stp_i(s, "\r Batt temp: ", car_tbattery);
  s = stp_rom(s, " oC");
  s = stp_i(s, "\r PCU temp: ", car_tpem);
  s = stp_rom(s, " oC");
  s = stp_l2f(s, "\r Aux batt: ", car_12vline, 1);
  s = stp_rom(s, "V");
  s = stp_i(s, "\r Batt volt: ", thinkcity_pack_voltage);
  s = stp_rom(s, "V");
  s = stp_i(s, "\r Batt curr: ", thinkcity_pack_current);
  s = stp_rom(s, "A");


}

BOOL vehicle_thinkcity_stat_sms(BOOL premsg, char *caller, char *command, char *arguments)
{
  // check for replace mode:
  if (!premsg)
    return FALSE;

  // check SMS notifies:
  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  if (!caller || !*caller)
  {
    caller = par_get(PARAM_REGPHONE);
    if (!caller || !*caller)
      return FALSE;
  }

  // prepare message:
  net_scratchpad[0] = 0;
  vehicle_thinkcity_stat_prepmsg();
  cr2lf(net_scratchpad);

  // OK, start SMS:
  delay100(2);
  net_send_sms_start(caller);
  net_puts_ram(net_scratchpad);

  return TRUE; // handled
}

BOOL vehicle_thinkcity_alert_cmd(BOOL msgmode, int cmd, char *arguments)
{
  // prepare message:
  strcpypgm2ram(net_scratchpad, (char const rom far*)"MP-0 PA");
  vehicle_thinkcity_stat_prepmsg();

  net_msg_encode_puts();

  return TRUE;
}


////////////////////////////////////////////////////////////////////////
// This is the Think City SMS command table
// (for implementation notes see net_sms::sms_cmdtable comment)
//
// First char = auth mode of command:
//   1:     the first argument must be the module password
//   2:     the caller must be the registered telephone
//   3:     the caller must be the registered telephone, or first argument the module password

BOOL vehicle_thinkcity_help_sms(BOOL premsg, char *caller, char *command, char *arguments);

rom char vehicle_thinkcity_sms_cmdtable[][NET_SMS_CMDWIDTH] = {
  "3DEBUG", // Think City: output internal state dump for debug
  "3STAT", // override standard STAT
  "3HELP", // extend HELP output
  // - - "3RANGE", // Think City: set/query max ideal range
  // - - "3CA", // Think City: set/query charge alerts

#ifdef OVMS_THINKCITY_BATTMON
  "3BATT", // Think City: battery status
#endif // OVMS_THINKCITY_BATTMON

  // - - "3POWER", // Think City: power usage statistics

  ""
};

rom far BOOL(*vehicle_thinkcity_sms_hfntable[])(BOOL premsg, char *caller, char *command, char *arguments) = {
  &vehicle_thinkcity_debug_sms,
  &vehicle_thinkcity_stat_sms,
  &vehicle_thinkcity_help_sms,
  // - - &vehicle_thinkcity_range_sms,
  // - - &vehicle_thinkcity_ca_sms,

#ifdef OVMS_THINKCITY_BATTMON
  &vehicle_thinkcity_battstatus_sms,
#endif // OVMS_THINKCITY_BATTMON

  //- - - &vehicle_thinkcity_power_sms
};

// SMS COMMAND DISPATCHER:
// premsg: TRUE=may replace, FALSE=may extend standard handler
// returns TRUE if handled

BOOL vehicle_thinkcity_fn_sms(BOOL checkauth, BOOL premsg, char *caller, char *command, char *arguments)
{
  int k;

  // Command parsing...
  for (k = 0; vehicle_thinkcity_sms_cmdtable[k][0] != 0; k++)
  {
    if (memcmppgm2ram(command,
                      (char const rom far*)vehicle_thinkcity_sms_cmdtable[k] + 1,
                      strlenpgm((char const rom far*)vehicle_thinkcity_sms_cmdtable[k]) - 1) == 0)
    {
      BOOL result;

      if (checkauth)
      {
        // we need to check the caller authorization:
        arguments = net_sms_initargs(arguments);
        if (!net_sms_checkauth(vehicle_thinkcity_sms_cmdtable[k][0], caller, &arguments))
          return FALSE; // failed
      }

      // Call sms handler:
      result = (*vehicle_thinkcity_sms_hfntable[k])(premsg, caller, command, arguments);

      if ((premsg) && (result))
      {
        // we're in charge + handled it; finish SMS:
        net_send_sms_finish();
      }

      return result;
    }
  }

  return FALSE; // no vehicle command
}

BOOL vehicle_thinkcity_fn_smshandler(BOOL premsg, char *caller, char *command, char *arguments)
{
  // called to extend/replace standard command: framework did auth check for us
  return vehicle_thinkcity_fn_sms(FALSE, premsg, caller, command, arguments);
}

BOOL vehicle_thinkcity_fn_smsextensions(char *caller, char *command, char *arguments)
{
  // called for specific command: we need to do the auth check
  return vehicle_thinkcity_fn_sms(TRUE, TRUE, caller, command, arguments);
}


////////////////////////////////////////////////////////////////////////
// Think City specific SMS command output extension: HELP
// - add Think City commands
//

BOOL vehicle_thinkcity_help_sms(BOOL premsg, char *caller, char *command, char *arguments)
{
  int k;

  if (premsg)
    return FALSE; // run only in extend mode

  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  net_puts_rom(" \r\nThink City Commands:");

  for (k = 0; vehicle_thinkcity_sms_cmdtable[k][0] != 0; k++)
  {
    net_puts_rom(" ");
    net_puts_rom(vehicle_thinkcity_sms_cmdtable[k] + 1);
  }

  return TRUE;
}




////////////////////////////////////////////////////////////////////////
// vehicle_thinkcity_initialise()
// This function is the initialisation entry point should we be the
// selected vehicle.
//
BOOL vehicle_thinkcity_initialise(void)
  {
  char *p;

  car_type[0] = 'T'; // Car is type TCxx (with xx replaced later)
  car_type[1] = 'C';
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
  RXB0CON  = 0b00000000;
  // Mask0 = 0b11111111000 (0x7F8), filterbit 0,1,2 deactivated
  RXM0SIDL = 0b00000000;
  RXM0SIDH = 0b11111100;

  // Filter0 0b01100000000 (0x300..0x3E0)
  RXF0SIDL = 0b00000000;
  RXF0SIDH = 0b01100000;

  // Buffer 1 (filters 2,3, etc) for direct can bus messages
  RXB1CON  = 0b00000000;	    // RX buffer1 uses Mask RXM1 and filters RXF2. Filters RXF3, RXF4 and RXF5 are not used in this version

  // Mask1 = 0b11111110000 (0x7f0), filterbit 0,1,2,3 deactivated for low volume IDs
  RXM1SIDL = 0b00000000;
  RXM1SIDH = 0b11111110;

  // Filter2 0b01001100000 (0x260..0x26F) = GROUP 0x26_:
  RXF2SIDL = 0b00000000;
  RXF2SIDH = 0b01001100;

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
  vehicle_fn_poll0 = &vehicle_thinkcity_poll0;
  vehicle_fn_poll1 = &vehicle_thinkcity_poll1;
  vehicle_fn_ticker1 = &vehicle_thinkcity_ticker1;
  vehicle_fn_smshandler = &vehicle_thinkcity_fn_smshandler;
  vehicle_fn_smsextensions = &vehicle_thinkcity_fn_smsextensions;



  net_fnbits |= NET_FN_INTERNALGPS;   // Require internal GPS
  net_fnbits |= NET_FN_12VMONITOR;    // Require 12v monitor
  net_fnbits |= NET_FN_SOCMONITOR;    // Require SOC monitor

  return TRUE;
  }
