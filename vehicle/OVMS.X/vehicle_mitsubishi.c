/*
;    Project:       Open Vehicle Monitor System
;    Date:          6 May 2012
;
;    Changes:
;    1.1  21.09.13 (Thomas)
;         - Fixed ODO
;         - Fixed filter and mask for poll0 (thanks to Matt Beard)
;         - Verified ideal range
;         - Verified SOC
;         - Verified Charge state
;         - Added battery temperature reading (thanks to Matt Beard)
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

// Mitsubishi state variables

#pragma udata overlay vehicle_overlay_data
signed char mi_batttemps[24]; // Temperature buffer, holds first two temps from the 0x6e1 messages (24 of the 64 values)

#pragma udata

////////////////////////////////////////////////////////////////////////
// vehicle_mitsubishi_ticker1()
// This function is an entry point from the main() program loop, and
// gives the CAN framework a ticker call approximately once per second
//
BOOL vehicle_mitsubishi_ticker1(void)
  {
  return FALSE;
  }

  ////////////////////////////////////////////////////////////////////////
// tc_state_ticker10()
// State Model: 10 second ticker
// This function is called approximately once every 10 seconds (since state
// was first entered), and gives the state a timeslice for activity.
//

BOOL vehicle_mitsubishi_ticker10(void)
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


  if ((car_linevoltage > 100) && (car_chargecurrent < 1))
    {
    car_chargestate = 4; //Done
    car_doors1 = 0x0C;  // Charge connector connected
    }

  if ((car_linevoltage > 100) && (car_chargecurrent > 1))
    {
    car_chargestate = 1; //Charging
    car_doors1 = 0x1C;
    }

  if (car_linevoltage < 100)  // AC line voltage < 100
    {
    car_doors1 = 0x00;  // charging connector unplugged
    }

  return FALSE;
  }


////////////////////////////////////////////////////////////////////////
// can_poll()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to poll for data.
//
BOOL vehicle_mitsubishi_poll0(void)
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

  switch (id)
  {
    case 0x346: //Range
	{
	  if (can_mileskm == 'K') 
	  {
        car_estrange = (unsigned int)(MiFromKm((unsigned long)can_databuffer[7]));
	  }
	  else
	  {
	    car_estrange = (unsigned int)can_databuffer[7]; 
	  }
      break;
	}
    
    /*
    case 0x373:
      // BatCurr & BatVolt
    break;
    */

    case 0x374: //SOC
	{
      car_SOC = (unsigned char)(((unsigned int)can_databuffer[1] - 10) / 2); 
	  
	  if (can_mileskm == 'K') 
	  {
        car_idealrange = ((((unsigned int)car_SOC) * 150) / 100);
	  }
	  else
	  {
		car_idealrange = ((((unsigned int)car_SOC) * 80) / 100);
      }
      break;
	}

    
    case 0x389: //charge voltage & current
	{
      car_linevoltage = (unsigned char)can_databuffer[1];
      car_chargecurrent = (unsigned char)((unsigned int)can_databuffer[6] / 10);
      break;
	}
  }

  return TRUE;
  }

BOOL vehicle_mitsubishi_poll1(void)
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

  RXB1CONbits.RXFUL = 0;        // All bytes read, Clear flag

  switch (id)
  {
    
    case 0x285:
	{
      if (can_databuffer[6] == 0x0C) // Car in park
      {
		//car_doors1 |= 0x40;     //  PARK
        car_doors1 &= ~0x80;    // CAR OFF
      }
      if (can_databuffer[6] == 0x0E) // Car not in park
      {
		//car_doors1 &= ~0x40;     //  NOT PARK
        car_doors1 |= 0x80;     // CAR ON
      }
      break;
	}
    
    case 0x412: //Speed & Odo
    {
	  if (can_databuffer[1] > 200) //Speed
      {
        car_speed = (unsigned char)((unsigned int)can_databuffer[1] - 255);
      }
      else
      {
        car_speed = (unsigned char) can_databuffer[1];
      }
      
	  if (can_mileskm == 'K') // Odo
      {
	    car_odometer = MiFromKm((((unsigned long) can_databuffer[2] << 16) + ((unsigned long) can_databuffer[3] << 8) + can_databuffer[4]) * 10);
      }
	  else
	  {
		car_odometer = ((((unsigned long) can_databuffer[2] << 16) + ((unsigned long) can_databuffer[3] << 8) + can_databuffer[4]) * 10);
	  }
      break;
	}
    
	case 0x6e1: 
    {
       // Calculate average battery pack temperature based on 24 of the 64 temperature values
       // Message 0x6e1 carries two temperatures for each of 12 banks, bank number (1..12) in byte 0,
       // temperatures in bytes 2 and 3, offset by 50C
       int idx = can_databuffer[0] - 1;
       if((idx >= 0) && (idx <= 11))
       {
         int i;
         int tbattery = 0;
         idx <<= 2;
         mi_batttemps[idx] = (signed char)(can_databuffer[2] - 50);
         mi_batttemps[idx + 1] = (signed char)(can_databuffer[3] - 50);

         car_tpem = 100;     // Min cell temp
         car_tmotor = 0;     // Max cell temp
         for(i=0; i<24; i++)
         {
           signed char t = mi_batttemps[i];
           tbattery += t;
           if(t < car_tpem) car_tpem = t;
           if(t > car_tmotor) car_tmotor = t;
         }
         car_tbattery = tbattery / 24;
         car_stale_temps = 120; // Reset stale indicator
       }
      break;
	}
  }
  
  return TRUE;
  }



////////////////////////////////////////////////////////////////////////
// vehicle_mitsubishi_initialise()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to initialise itself.
//
BOOL vehicle_mitsubishi_initialise(void)
  {
  char *p;
  int i;
  
  car_type[0] = 'M'; // Car is type MI - Mitsubishi iMiev
  car_type[1] = 'I';
  car_type[2] = 0;
  car_type[3] = 0;
  car_type[4] = 0;

  // Vehicle specific data initialisation
  car_stale_timer = -1; // Timed charging is not supported for OVMS MI
  car_time = 0;
  
   // Clear the battery temperatures
  for(i=0; i<12; i++) mi_batttemps[i] = 0;

  CANCON = 0b10010000; // Initialize CAN
  while (!CANSTATbits.OPMODE2); // Wait for Configuration mode

  // We are now in Configuration Mode

  // Filters: low byte bits 7..5 are the low 3 bits of the filter, and bits 4..0 are all zeros
  //          high byte bits 7..0 are the high 8 bits of the filter
  //          So total is 11 bits

  // Buffer 0 (filters 0, 1) for extended PID responses
  RXB0CON = 0b00000000;
  // Mask0 = 0b11110000000 (0x700), filterbit 0-7 deactivated
  RXM0SIDL = 0b00000000;
  RXM0SIDH = 0b11100000;


  // Filter0 0b01100000000 (0x300..0x3F8)
  RXF0SIDL = 0b00000000;
  RXF0SIDH = 0b01100000;


  // Buffer 1 (filters 2, 3, 4 and 5) for direct can bus messages
  RXB1CON  = 0b00000000;	// RX buffer1 uses Mask RXM1 and filters RXF2, RXF3, RXF4, RXF5

   // Mask1 = 0b11111111111 (0x7FF)
  RXM1SIDL = 0b11100000;
  RXM1SIDH = 0b11111111;

  // Filter2 0b01010000101 (0x285)
  RXF2SIDL = 0b10100000;
  RXF2SIDH = 0b01010000;

  // Filter3 0b10000010010 (0x412)
  RXF3SIDL = 0b01000000;
  RXF3SIDH = 0b10000010;

  // Filter4 0b11011100001 (0x6E1)
  // Sample the first of the battery values
  RXF4SIDL = 0b00100000;
  RXF4SIDH = 0b11011100;

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
  vehicle_fn_poll0 = &vehicle_mitsubishi_poll0;
  vehicle_fn_poll1 = &vehicle_mitsubishi_poll1;
  vehicle_fn_ticker1 = &vehicle_mitsubishi_ticker1;
  vehicle_fn_ticker10 = &vehicle_mitsubishi_ticker10;

  net_fnbits |= NET_FN_INTERNALGPS;   // Require internal GPS
  net_fnbits |= NET_FN_12VMONITOR;    // Require 12v monitor

  return TRUE;
  }
