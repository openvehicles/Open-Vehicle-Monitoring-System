/*
;    Project:       Open Vehicle Monitor System
;    Date:          6 May 2012
;
;
;    Changes:
;    2.8  11.11.2013 (Haakon)
;         - Added charge alert when interruped, when 80% (Ocvm) and when done
;
;    2.7  30.10.2013 (Haakon)
;         - Added function for stale temps
;
;    2.6  25.09.2013 (Haakon)
;         - Added car_parktime for the app
;
;    2.5  24.09.2013 (Haakon)
;         - Added support for switching on/off external fuelheater.Activated via Valet Mode button in the app
;           RC3 is high for 20 minutes if not "unvalet".
;    2.4  22.09.2013 (Haakon)
;         - Added central lock/unlock. Assigned pin 4 at 9X2 to RC1 (lock) and pin 6 to RC2 (unlock)
;         - Added msg 0x460, SRS status and assigned pin 2 at 9X2 to RC0 (SRS OK).
;
;    2.3  02.09.2013 (Haakon)
;         - Bugfix to get GPS streaming (FEATURE 9 = 1) working
;         - Added correct value to car_speed and set car_doors1bits.CarON = 1 when tc_bit_dischgenbl > 0
;           (car has to be moving and 'car on' is detected when discharge is allowed)
;         - Added car_SOCalertlimit = 10 to set a system alert when SOC < 10
;
;    2.2  27.08.2013 (Haakon)
;         - TX to can-bus introdused to request car_tpem, car_tmotor_, car_ambient_temp and tc_charger_temp in k-line style.
;           TX is located in idlepoll()
;         - SMS fault printout gives list of active faults only and the count
;         - SMS code "DEBUG" is renamed to "FLAG" and the result is a list of active flags only
;         - STAT printout on SMS is rearranged
;         - Cleanup of Twizy leftovers
;         - Tested with master v2.5.2
;
;    2.1  19.08.2013 (Haakon)
;         - SMS command "HELP"fixed. Function net_send_sms_start(caller) added to the _help_sms part and premsg is ignored
;           "HELP" returns "STAT DEBUG FAULT HELP"
;         - Hook vehicle_thinkcity_ticker1 modified to vehicle_thinkcity_state_ticker1
;         - Detection of charge state and door1 moved to state_ticker vehicle_thinkcity_state_ticker10
;         - Chargemode is always 0 (standard charge) and is moved to state_ticker1
;         - New Think City specific parameters added.
;         - SMS handler for "FAULT" added. Errors, warnings and notification are grouped, but there is no logic in selecting activ ones yet.
;
;    2.0  18.08.2013 (Haakon)
;         Migration of SMS from standard handler to vehicle_thinkcity.c. All functions are based on the vehicle_twizy.c
;         - SMS cmd "STAT": Moved specific output to vehicle_thinkcity.c
;           (SOC, batt temp, PCU temp, aux batt volt, traction batt volt, trac current and AC-line voltage/current during charging)
;         - SMS cmd "DEBUG": Output Think City specific flags/variables
;           (SOC, Charge Enable, Open Circuit Voltage Measurement, End of Charge, Doors1 status and Charge Status)
;         - SMS cmd "HELP": adds commands help. "Help" is currently not working!
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
;           Temporarily resolved by adding new global variable "unsigned int tc_pack_temp" in ovms.c and ovms.h
;         - Battery current and voltage added to the SMS "STAT?" by adding new global variables
;           "unsigned int tc_pack_voltage" and signed "int tc_pack_current" to ovms.c and ovms.h files
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
#include "inputs.h"
#include "net_sms.h"
#include "net_msg.h"



/***************************************************************
 * Think City definitions
 */

// Think City specific commands:

#pragma udata overlay vehicle_overlay_data
unsigned int  tc_pack_voltage;
signed int    tc_pack_current;
signed int    tc_pack_maxchgcurr;
unsigned int  tc_pack_maxchgvolt;
unsigned int  tc_pack_failedcells;
signed int    tc_pack_temp1;
signed int    tc_pack_temp2;
unsigned int  tc_pack_batteriesavail;
unsigned int  tc_pack_rednumbatteries;
unsigned int  tc_pack_mindchgvolt;
signed int    tc_pack_maxdchgamps;
signed int    tc_charger_temp = 0;
signed int    tc_slibatt_temp = 0;
unsigned int  tc_charger_pwm;
unsigned int  tc_sys_voltmaxgen;
unsigned char tc_srs_stat;
signed int    tc_heater_count = 0;

//Status flags:
unsigned int tc_bit_eoc;
unsigned int tc_bit_socgreater102;
unsigned int tc_bit_chrgen;
unsigned int tc_bit_ocvmeas;
unsigned int tc_bit_mainsacdet;
unsigned int tc_bit_syschgenbl;
unsigned int tc_bit_fastchgenbl;
unsigned int tc_bit_dischgenbl;
unsigned int tc_bit_isotestinprog;
unsigned int tc_bit_acheatrelaystat;
unsigned int tc_bit_acheatswitchstat;
unsigned int tc_bit_regenbrkenbl;
unsigned int tc_bit_dcdcenbl;
unsigned int tc_bit_fanactive;

//Errors:
unsigned int tc_bit_epoemerg;
unsigned int tc_bit_crash;
unsigned int tc_bit_generalerr;
unsigned int tc_bit_intisoerr;
unsigned int tc_bit_extisoerr;
unsigned int tc_bit_thermalisoerr;
unsigned int tc_bit_isoerr;
unsigned int tc_bit_manyfailedcells;

//Notifications:
unsigned int tc_bit_chgwaittemp;
unsigned int tc_bit_reacheocplease;
unsigned int tc_bit_waitoktmpdisch;
unsigned int tc_bit_chgwaitttemp2;

//Warnings:
unsigned int tc_bit_chgcurr;
unsigned int tc_bit_chgovervolt;
unsigned int tc_bit_chgovercurr;



#pragma udata
////////////////////////////////////////////////////////////////////////
// vehicle_thinkcity_ticker1()
// This function is an entry point from the main() program loop, and
// gives the CAN framework a ticker call approximately once per second
//

BOOL vehicle_thinkcity_state_ticker1(void)
  {
  if (tc_srs_stat == 0)
  {
    output_gpo0(1); // Set digital out RC0 high, pin2 header 9X2.
  }
  else
    output_gpo0(0); // Set digital out RC0 low, pin2 header 9X2.

  car_time++;

  car_chargemode = 0;
  car_SOCalertlimit = 10;
  car_doors3bits.CoolingPump= (car_stale_temps<=1)?0:1;

  return FALSE;
  }

////////////////////////////////////////////////////////////////////////
// tc_state_ticker10()
// State Model: 10 second ticker
// This function is called approximately once every 10 seconds (since state
// was first entered), and gives the state a timeslice for activity.
//

BOOL vehicle_thinkcity_state_ticker10(void)
  {


// Check for charging state
// car_door1 Bits used: 0-7
// Function net_prep_stat in net_msg.c uses car_doors1bits.ChargePort to determine SMS-printout
//
// 0x04 Chargeport open, bit 2 set
// 0x08 Pilot signal on, bit 3 set
// 0x10 Vehicle charging, bit 4 set
// 0x0C Chargeport open and pilot signal, bits 2,3 set
// 0x1C Bits 2,3,4 set

// Charge enable:
  if (tc_bit_chrgen == 1) // Is charge_enable_bit (bit 0) set (turns to 0 during 0CV-measuring at 80% SOC)?
  {
    car_chargestate = 1;
    car_chargesubstate = 3; // (by request)
    car_doors1 = 0x1C; //Charging
  }

// Open Circuit Measurement at 80% SOC
  if ((tc_bit_ocvmeas == 2) && (car_chargestate == 1))  // Is ocv_meas_in_progress (bit 1) set?
  {
    car_chargestate = 2; //Top off
    car_chargesubstate = 3; // (by request)
    car_doors1 = 0x0C;
    net_req_notification(NET_NOTIFY_CHARGE);
  }


// End of Charge
  if ((tc_bit_eoc == 1) && (car_chargestate == 1)) // Is EOC_bit (bit 0) set?
  {
    car_chargestate = 4; //Done
    car_chargesubstate = 3; // (by request)
    net_req_notification(NET_NOTIFY_CHARGE);

    if (car_linevoltage > 100)  // Is AC line voltage > 100 ?
    {
      car_doors1 = 0x0C;  // Charge connector connected
    }
    else
      car_doors1 = 0x00;  // Charge connector disconnected
   }

// Charging unplugged or power disconnected
  if (car_linevoltage < 100)  // AC line voltage < 100
  {
    car_doors1 = 0x00;  // charging connector unplugged
  }

//Charging interrupted (assumed)
  if ((car_chargestate == 1) && (tc_bit_eoc != 1) && (tc_bit_ocvmeas !=2) && (car_chargecurrent == 0) && (car_SOC < 97))
  {
    car_chargestate = 21;    // Charge STOPPED
    car_chargesubstate = 14; // Charge INTERRUPTED
    net_req_notification(NET_NOTIFY_CHARGE);
  }



// Parking timer
  if (tc_bit_dischgenbl > 0) //Discharge allowed, car is on
  {
    car_doors1bits.CarON = 1;  // Car is on 0x80
    if (car_parktime != 0)
    {
      car_parktime = 0; // No longer parking
      net_req_notification(NET_NOTIFY_ENV);
    }
  }

  if (tc_bit_dischgenbl == 0) //Discharge not allowed, car is off
  {
    car_doors1bits.CarON = 0;  // Car off
    if (car_parktime == 0)
    {
      car_parktime = car_time-1;    // Record it as 1 second ago, so non zero report
      net_req_notification(NET_NOTIFY_ENV);
    }
  }

// Heater handeling
  if (tc_heater_count == 0)
  {
    output_gpo3(0);
    net_req_notification(NET_NOTIFY_ENV);
    car_doors2bits.ValetMode = 0;  // Unvalet activated or heater/aux off

  }
  if (tc_heater_count < 0)
  {
    tc_heater_count++;
  }

  return FALSE;
  }


////////////////////////////////////////////////////////////////////////
// can_poll()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to poll for data.
//
BOOL vehicle_thinkcity_poll0(void)
  {
  switch (can_id)
    {
    case 0x301:
      tc_pack_current = (((int) can_databuffer[0] << 8) + can_databuffer[1]) / 10;
      tc_pack_voltage = (((unsigned int) can_databuffer[2] << 8) + can_databuffer[3]) / 10;
      car_SOC = 100 - ((((unsigned int)can_databuffer[4]<<8) + can_databuffer[5])/10);
      car_tbattery = (((signed int)can_databuffer[6]<<8) + can_databuffer[7])/10;
      car_idealrange = (111.958773 * car_SOC / 100);
      car_estrange = (93.205678 * car_SOC / 100);
      car_stale_temps = 60;
    break;

    case 0x302:
      tc_bit_generalerr = (can_databuffer[0] & 0x01);
      tc_bit_isoerr = (can_databuffer[2] & 0x01);
      tc_pack_mindchgvolt = (((unsigned int) can_databuffer[4] << 8) + can_databuffer[5]) / 10;
      tc_pack_maxdchgamps = (((unsigned int) can_databuffer[6] << 8) + can_databuffer[7]) / 10;
    break;

    case 0x303:
      tc_pack_maxchgcurr = (((signed int) can_databuffer[0] << 8) + can_databuffer[1]) / 10;
      tc_pack_maxchgvolt = (((unsigned int) can_databuffer[2] << 8) + can_databuffer[3]) / 10;
      tc_bit_syschgenbl = (can_databuffer[4] & 0x01);
      tc_bit_regenbrkenbl = (can_databuffer[4] & 0x02);
      tc_bit_dischgenbl = (can_databuffer[4] & 0x04);
      tc_bit_fastchgenbl = (can_databuffer[4] & 0x08);
      tc_bit_dcdcenbl = (can_databuffer[4] & 0x10);
      tc_bit_mainsacdet = (can_databuffer[4] & 0x20);
      tc_pack_batteriesavail = can_databuffer[5];
      tc_pack_rednumbatteries = (can_databuffer[6] & 0x01);
      tc_bit_epoemerg = (can_databuffer[6] & 0x08);
      tc_bit_crash = (can_databuffer[6] & 0x10);
      tc_bit_fanactive = (can_databuffer[6] & 0x20);
      tc_bit_socgreater102 = (can_databuffer[6] & 0x40);
      tc_bit_isotestinprog = (can_databuffer[6] & 0x80);
      tc_bit_chgwaittemp = (can_databuffer[7] & 0x01);
    break;

    case 0x304:
      tc_sys_voltmaxgen = (((unsigned int) can_databuffer[0] << 8) + can_databuffer[1]) / 10;
      tc_bit_eoc = (can_databuffer[3] & 0x01);
      tc_bit_reacheocplease = (can_databuffer[3] & 0x02);
      tc_bit_chgwaitttemp2 = (can_databuffer[3] & 0x04);
      tc_bit_manyfailedcells = (can_databuffer[3] & 0x08);
      tc_bit_acheatrelaystat = (can_databuffer[3] & 0x10);
      tc_bit_acheatswitchstat = (can_databuffer[3] & 0x20);
      tc_pack_temp1 = (((signed int) can_databuffer[4] << 8) + can_databuffer[5]) / 10;
      tc_pack_temp2 = (((signed int) can_databuffer[6] << 8) + can_databuffer[7]) / 10;
    break;

    case 0x305:
      tc_charger_pwm = (((unsigned int) can_databuffer[0] << 8) + can_databuffer[1]) / 10;
      tc_bit_intisoerr = (can_databuffer[2] & 0x10);
      tc_bit_extisoerr = (can_databuffer[2] & 0x20);
      tc_bit_chrgen = (can_databuffer[3] & 0x01);
      tc_bit_ocvmeas = (can_databuffer[3] & 0x02);
      tc_bit_chgcurr = (can_databuffer[3] & 0x04);
      tc_bit_chgovervolt = (can_databuffer[3] & 0x08);
      tc_bit_chgovercurr = (can_databuffer[3] & 0x10);
      tc_pack_failedcells = ((unsigned int)can_databuffer[4]<<8) + can_databuffer[5];
      tc_bit_waitoktmpdisch = (can_databuffer[6] & 0x40);
      tc_bit_thermalisoerr = (can_databuffer[6] & 0x20);
    break;

    case 0x311:
      car_chargelimit =  ((unsigned char) can_databuffer[1]) * 0.2 ;  // Charge limit, controlled by the "power charge button", usually 9 or 15A.
    break;
    }

  return TRUE;
  }

BOOL vehicle_thinkcity_poll1(void)
  {
  switch (can_id)
    {
    case 0x263:
      car_stale_ambient = 60;
      car_chargecurrent =  ((signed int) can_databuffer[0]) * 0.2;
      car_linevoltage = (unsigned int) can_databuffer[1];
      car_ambient_temp = ((signed char) can_databuffer[2]) * 0.5; // PCU abmbient temp
      car_speed = ((unsigned char) can_databuffer[5]) / 2;
    break;

    case 0x460:
	  tc_srs_stat = (unsigned char) can_databuffer[4] ;
	break;

    case 0x75B:
      car_stale_temps = 60;
      if (can_databuffer[3] == 0x65)
      {
        tc_charger_temp = (((signed int) can_databuffer[4] << 8) + can_databuffer[5]) / 100;
      }
      else if (can_databuffer[3] == 0x66)
      {
        car_tpem = (((signed int) can_databuffer[4] << 8) + can_databuffer[5]) / 100;
      }
      else if (can_databuffer[3] == 0x67)
      {
        car_tmotor = (((signed int) can_databuffer[4] << 8) + can_databuffer[5]) / 100;
      }
      else if (can_databuffer[3] == 0x68)
      {
        tc_slibatt_temp = (((signed int) can_databuffer[4] << 8) + can_databuffer[5]) / 100;
      }
    break;

    default:
        tc_srs_stat = 34;
    break;

    }

  return TRUE;
  }

////////////////////////////////////////////////////////////////////////
// vehicle_thinkcity_idlepoll()
// This function is an entry point from the main() program loop, and
// gives the CAN framework a call whenever there is idle time
//
BOOL vehicle_thinkcity_idlepoll(void)
{

  if (sys_features[FEATURE_CANWRITE]>0)
  {
    while (TXB0CONbits.TXREQ) {} // Loop until TX is done
    TXB0CON = 0;
    TXB0SIDL = 0b01100000;
    TXB0SIDH = 0b11101010;
    TXB0D0 = 0x03;
    TXB0D1 = 0x22;
    TXB0D2 = 0x49;
    TXB0D3 = 0x65;
    TXB0DLC = 0b00000100; // data length (4)
    TXB0CON = 0b00001000; // mark for transmission
    delay100b(); // Delay a little... (100ms, approx)

    while (TXB0CONbits.TXREQ) {} // Loop until TX is done
    TXB0CON = 0;
    TXB0SIDL = 0b01100000;
    TXB0SIDH = 0b11101010;
    TXB0D0 = 0x03;
    TXB0D1 = 0x22;
    TXB0D2 = 0x49;
    TXB0D3 = 0x66;
    TXB0DLC = 0b00000100; // data length (4)
    TXB0CON = 0b00001000; // mark for transmission
    delay100b(); // Delay a little... (100ms, approx)

    while (TXB0CONbits.TXREQ) {} // Loop until TX is done
    TXB0CON = 0;
    TXB0SIDL = 0b01100000;
    TXB0SIDH = 0b11101010;
    TXB0D0 = 0x03;
    TXB0D1 = 0x22;
    TXB0D2 = 0x49;
    TXB0D3 = 0x67;
    TXB0DLC = 0b00000100; // data length (4)
    TXB0CON = 0b00001000; // mark for transmission
    delay100b(); // Delay a little... (100ms, approx)

    while (TXB0CONbits.TXREQ) {} // Loop until TX is done
    TXB0CON = 0;
    TXB0SIDL = 0b01100000;
    TXB0SIDH = 0b11101010;
    TXB0D0 = 0x03;
    TXB0D1 = 0x22;
    TXB0D2 = 0x49;
    TXB0D3 = 0x68;
    TXB0DLC = 0b00000100; // data length (4)
    TXB0CON = 0b00001000; // mark for transmission
    delay100b(); // Delay a little... (100ms, approx)
  }



return FALSE;
}

void vehicle_thinkcity_tx_lockunlockcar(unsigned char mode, char *pin)
  {
  // Mode is 0=valet, 1=novalet, 2=lock, 3=unlock
  long lpin;
  lpin = atol(pin);

  if ((mode == 0x02)&&(car_doors1 & 0x80))
    return; // Refuse to lock a car that is turned on
  // Check if RB4 is low, set RB4 high for 500ms and back to low

  if (mode == 0x02) //lock
  {
    if (PORTCbits.RC1 == 0)
    {
      PORTCbits.RC1 = 1;
      delay100(5);
      PORTCbits.RC1= 0;
    }
    net_req_notification(NET_NOTIFY_ENV);
    car_lockstate = 4;  // Car is locked
    car_doors2bits.CarLocked = 1;  // Car is locked
  }
  else if (mode == 0x03) //unlock
  {
    if (PORTCbits.RC2 == 0)
    {
      PORTCbits.RC2 = 1;
      delay100(5);
      PORTCbits.RC2= 0;
    }
    net_req_notification(NET_NOTIFY_ENV);
    car_lockstate = 5;  // Car unlocked
    car_doors2bits.CarLocked = 0;  // Car is unlocked

  }
  else if (mode == 0x00) //valet or external heater or auxilary on
  {
    if (PORTCbits.RC3 == 0)
    {
      PORTCbits.RC3 = 1;
      tc_heater_count = -120;
    }
    net_req_notification(NET_NOTIFY_ENV);
    car_doors2bits.ValetMode = 1;  // Valet activated or heater/aux on

  }
  else if (mode == 0x01) //unvalet or external heater or auxilary off
  {
    if (PORTCbits.RC3 == 1)
    {
      PORTCbits.RC3= 0;
    }
    net_req_notification(NET_NOTIFY_ENV);
    car_doors2bits.ValetMode = 0;  // Unvalet activated or heater/aux off

  }

  }



BOOL vehicle_thinkcity_commandhandler(BOOL msgmode, int code, char* msg)
  {
  char *p;
  BOOL sendenv = FALSE;

  switch (code)
    {
    case 20: // Lock car (params pin)
        vehicle_thinkcity_tx_lockunlockcar(2, net_msg_cmd_msg);
        STP_OK(net_scratchpad, code);
      sendenv=TRUE;
      break;


    case 22: // Unlock car (params pin)
        vehicle_thinkcity_tx_lockunlockcar(3, net_msg_cmd_msg);
        STP_OK(net_scratchpad, code);
      sendenv=TRUE;
      break;

    case 21: // Activate valet mode (params pin)
        vehicle_thinkcity_tx_lockunlockcar(0, net_msg_cmd_msg);
        STP_OK(net_scratchpad, code);
      sendenv=TRUE;
      break;

    case 23: // Deactivate valet mode (params pin)
        vehicle_thinkcity_tx_lockunlockcar(1, net_msg_cmd_msg);
        STP_OK(net_scratchpad, code);
      sendenv=TRUE;
      break;


    default:
      return FALSE;
    }

  if (msgmode)
    {
    net_msg_encode_puts();
    delay100(2);
    net_msgp_environment(0);
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
 * COMMAND CLASS: FLAG
 *
 *  SMS: FLAG
 *      - output internal state flags for debugging
 *
 */


BOOL vehicle_thinkcity_flag_sms(BOOL premsg, char *caller, char *command, char *arguments)
{
  char *s;

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
  s = stp_rom(s, "Flags:");
  s = stp_i(s, "\r SOC=", car_SOC);
  if ( tc_bit_socgreater102 != 0 ) { s = stp_rom(s, "\r SOC>102"); }
  if ( tc_bit_chrgen != 0 ) { s = stp_rom(s, "\r ChgEnbl"); } // Charge Enable
  if ( tc_bit_ocvmeas != 0 ) { s = stp_rom(s, "\r OcvMeas"); }// Open Circuit Voltage Meas
  if ( tc_bit_eoc != 0 ) { s = stp_rom(s, "\r EOC"); } // End of Charge
  if ( tc_bit_mainsacdet != 0 ) { s = stp_rom(s, "\r MainsAcDetect"); } // Mains AC detect
  if ( tc_bit_syschgenbl != 0 ) { s = stp_rom(s, "\r SysChgDetect"); } // Sys charge enable
  if ( tc_bit_fastchgenbl != 0 ) { s = stp_rom(s, "\r FastChgEnbl"); } // Fast charge enable
  if ( tc_bit_dischgenbl != 0 ) { s = stp_rom(s, "\r DisChgEnbl"); } // Discharge enable
  if ( tc_bit_isotestinprog != 0 ) { s = stp_rom(s, "\r IsoTestInPrg"); } // ISO test in progress
  if ( tc_bit_acheatrelaystat != 0 ) { s = stp_rom(s, "\r AcHeatRlyStat"); } // AC heater relay status
  if ( tc_bit_acheatswitchstat != 0 ) { s = stp_rom(s, "\r AcHeatSwStat"); } // AC heater switch status
  if ( tc_bit_regenbrkenbl != 0 ) { s = stp_rom(s, "\r RegBrkEnbl"); } // Regen break enable
  if ( tc_bit_dcdcenbl != 0 ) { s = stp_rom(s, "\r DcDcEnbl"); } // DC-DC enable
  if ( tc_bit_fanactive != 0 ) { s = stp_rom(s, "\r FansAct"); } // Fans active
  s = stp_x(s, "\r Door1Stat= ", car_doors1 ); // Car doors1
  s = stp_i(s, "\r CrgStat= ", car_chargestate);  // Charge state


  net_puts_ram(net_scratchpad);

  return TRUE;
}

/***********************************************************************
 * COMMAND CLASS: CHARGE STATUS
 *
 *  SMS: STAT
 *      - output charge status
 *
 */

// prepmsg: Generate STAT message for SMS
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
    char fShowVA = TRUE;
    // Charge port door is open, we are charging
    switch (car_chargestate)
    {
    case 0x01:
      s = stp_rom(s, "Charging"); // Charge State Charging
    break;
    case 0x02:
      s = stp_rom(s, "80% OCV-Meas"); // Open Circuit Voltage Measurement
    break;
    case 0x04:
      s = stp_rom(s, "Charging Done"); // Done
      fShowVA = FALSE;
    break;
    case 0x0d:
      s = stp_rom(s, "Preparing"); // Preparing
    break;
    case 0x0f:
      s = stp_rom(s, "Charging, Heating"); // Heating
    break;
    default:
      s = stp_rom(s, "Charging Stopped"); // Stopped
      fShowVA = FALSE;
    break;
    }
    car_doors1bits.ChargePort = 0; // MJ Close ChargePort, will open next CAN Reading
    if (fShowVA)
    {
      s = stp_i(s, "\r ", car_linevoltage);
      s = stp_i(s, "V/", car_chargecurrent);
      s = stp_rom(s, "A");
    }
  }
  else if (car_speed > 0)
  {
  s = stp_i(s, "Driving @ ", car_speed);
  s = stp_rom(s, "km/h");
  }
  else
  {
    s = stp_rom(s, "Not Charging");
  }

  s = stp_i(s, "\r SOC: ", car_SOC);
  s = stp_rom(s, "%");
  s = stp_i(s, "\r PackV: ", tc_pack_voltage);
  s = stp_rom(s, "V");
  s = stp_i(s, "\r PackC: ", tc_pack_current);
  s = stp_rom(s, "A");
  s = stp_i(s, "\r PackTp: ", car_tbattery);
  s = stp_rom(s, " oC");
  s = stp_i(s, "\r ChgTp: ", tc_charger_temp);
  s = stp_rom(s, " oC");
  s = stp_i(s, "\r AmbTp: ", tc_slibatt_temp);
  s = stp_rom(s, " oC");
  s = stp_l2f(s, "\r AuxBatt: ", car_12vline, 1);
  s = stp_rom(s, "V");
  if (tc_charger_pwm > 0)
  {
    s = stp_i(s, "\r PWM: ", tc_charger_pwm);
  s = stp_rom(s, "%");
  }

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

/***********************************************************************
 * COMMAND CLASS: Fault
 *
 *  SMS: FAULT
 *      - output errors, warning and notificatons
 *
 */

void vehicle_thinkcity_fault_prepmsg(void)
{
  char *s;
  int errc = 0;

  // append to net_scratchpad:
  s = strchr(net_scratchpad, 0);
  s = stp_rom(s, "Act err: ");
  if (tc_pack_failedcells != 0) { s = stp_i(s, "\rFail cells:", tc_pack_failedcells); errc++; }
  if (tc_bit_manyfailedcells != 0) { s = stp_rom(s, "\rMnyFailCel"); errc++; }
  if (tc_bit_crash != 0) { s = stp_rom(s, "\rCrash");errc++; }
  if (tc_bit_generalerr != 0) { s = stp_rom(s, "\rGenErr"); errc++; }
  if (tc_bit_isoerr != 0) { s = stp_rom(s, "\rIsoErr"); errc++; }
  if (tc_bit_intisoerr != 0) { s = stp_rom(s, "\rIntIso"); errc++; }
  if (tc_bit_extisoerr != 0) { s = stp_rom(s, "\rExtIso"); errc++; }
  if (tc_bit_thermalisoerr != 0) { s = stp_rom(s, "\rThermIso"); errc++; }
  if (tc_bit_epoemerg != 0) { s = stp_rom(s, "\rEmgEPO"); errc++; }
  if (tc_bit_chgwaittemp != 0) { s = stp_rom(s, "\rChgWaitTemp"); errc++; }
  if (tc_bit_reacheocplease != 0) { s = stp_rom(s, "\rReachEOC"); errc++; }
  if (tc_bit_waitoktmpdisch != 0) { s = stp_rom(s, "\rWaitOkTpDcg"); errc++; }
  if (tc_bit_chgwaitttemp2 != 0) { s = stp_rom(s, "\rCrgWaitTp2"); errc++; }
  if (tc_bit_chgcurr != 0) { s = stp_rom(s, "\rNoChgCur"); errc++; }
  if (tc_bit_chgovervolt != 0) { s = stp_rom(s, "\rChgOverVolt"); errc++; }
  if (tc_bit_chgovercurr != 0) { s = stp_rom(s, "\rChgOverCur"); errc++; }
  if (errc == 0)
  {
    s = stp_rom(s, "0");
  }
  else
    s = stp_i(s, "\rNo err: ", errc);

}

BOOL vehicle_thinkcity_fault_sms(BOOL premsg, char *caller, char *command, char *arguments)
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
  vehicle_thinkcity_fault_prepmsg();
  cr2lf(net_scratchpad);

  // OK, start SMS:
  delay100(2);
  net_send_sms_start(caller);
  net_puts_ram(net_scratchpad);

  return TRUE; // handled
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
  "3STAT", // override standard STAT
  "3FLAG", // Think City: output internal flag state for debug
  "3FAULT", // Think City: output internal errors, warnings and notofications
  "3HELP", // extend HELP output
  ""
};

rom far BOOL(*vehicle_thinkcity_sms_hfntable[])(BOOL premsg, char *caller, char *command, char *arguments) = {
  &vehicle_thinkcity_stat_sms,
  &vehicle_thinkcity_flag_sms,
  &vehicle_thinkcity_fault_sms,
  &vehicle_thinkcity_help_sms,

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

  if (!premsg)
    return FALSE; // run only in extend mode

  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  net_send_sms_start(caller);
  net_puts_rom("Think Commands:\r");

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

  car_type[0] = 'T'; // Car is type Think City
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

  // Filter3 0b10001100000 (0x460..0x46F) = GROUP 0x46_: (SRS-modele)
  RXF3SIDL = 0b00000000;
  RXF3SIDH = 0b10001100;

  // Filter4 0b11101010000 (0x750..0x75F) = GROUP 0x75_: (motor_temp, heat sink temp)
  RXF4SIDL = 0b00000000;
  RXF4SIDH = 0b11101010;


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

  PORTCbits.RC0 = 0;
  PORTCbits.RC1 = 0;
  PORTCbits.RC2 = 0;
  PORTCbits.RC3 = 0;
  // Hook in...
  vehicle_fn_poll0 = &vehicle_thinkcity_poll0;
  vehicle_fn_poll1 = &vehicle_thinkcity_poll1;
  vehicle_fn_ticker1 = &vehicle_thinkcity_state_ticker1;
  vehicle_fn_ticker10 = &vehicle_thinkcity_state_ticker10;
  vehicle_fn_idlepoll = &vehicle_thinkcity_idlepoll;
  vehicle_fn_commandhandler = &vehicle_thinkcity_commandhandler;
  vehicle_fn_smshandler = &vehicle_thinkcity_fn_smshandler;
  vehicle_fn_smsextensions = &vehicle_thinkcity_fn_smsextensions;



  net_fnbits |= NET_FN_INTERNALGPS;   // Require internal GPS
  net_fnbits |= NET_FN_12VMONITOR;    // Require 12v monitor
  net_fnbits |= NET_FN_SOCMONITOR;    // Require SOC monitor

  return TRUE;
  }
