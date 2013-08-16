
/*
;    Project:       Open Vehicle Monitor System
;    Date:          6 May 2012
;
;    Changes:
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
#include "ovms.h"
#include "params.h"


#pragma udata


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
    //
  switch (id)
    {
    case 0x301:
      car_SOC = 100 - ((((unsigned int)can_databuffer[4]<<8) + can_databuffer[5])/10);
      car_idealrange = (111.958773 * car_SOC / 100);
      car_estrange = (93.205678 * car_SOC / 100 );
      tc_pack_temp = (((unsigned int)can_databuffer[6]<<8) + can_databuffer[7])/10;
      tc_pack_voltage = (((unsigned int) can_databuffer[2] << 8) + can_databuffer[3]) / 10;
      tc_pack_current = (((int) can_databuffer[0] << 8) + can_databuffer[1]) / 10;
      car_stale_temps = 60;
    break;

    case 0x304:
      
      if ((can_databuffer[3] & 0x01) == 1) // Is EOC_bit (bit 0) is set?
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
      if ((can_databuffer[3] & 0x01) == 1) // Is charge_enable_bit (bit 0) is set (turns to 0 during 0CV-measuring at 80% SOC)?
      {
        car_chargestate = 1; //Charging
        car_chargemode = 0;
        car_doors1 = 0x1C;
      }

      if ((can_databuffer[3] & 0x02) == 2) // Is ocv_meas_in_progress (bit 1) is  set?
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
      car_chargecurrent =  ((signed int) can_databuffer[0]) * 0.2 ;
      car_linevoltage = (unsigned int) can_databuffer[1] ;
      if (car_linevoltage < 100)  // AC line voltage < 100
        {
          car_doors1 = 0x00;  // charging connector unplugged
        }
      car_tpem = ((signed char) can_databuffer[2]) * 0.5 ; // PCU abmbient temp
      car_stale_temps = 60;
    break;
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

  net_fnbits |= NET_FN_INTERNALGPS;   // Require internal GPS
  net_fnbits |= NET_FN_12VMONITOR;    // Require 12v monitor
  net_fnbits |= NET_FN_SOCMONITOR;    // Require SOC monitor

  return TRUE;
  }
