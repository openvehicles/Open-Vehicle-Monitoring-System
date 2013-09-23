/*
;    Project:       Open Vehicle Monitor System
;    Date:          6 May 2012
;
;    Changes:
;    1.3  23.09.13 (Thomas)
;         - Fixed charging notification when battery is full, SOC=100%
;         - TODO: Fix battery temperature reading. 
;    1.2  22.09.13 (Thomas)
;         - Fixed Ideal range
;         - Added parking timer
;         - Added detection of car is asleep.
;    1.1  21.09.13 (Thomas)
;         - Fixed ODO
;         - Fixed filter and mask for poll0 (thanks to Matt Beard)
;         - Verified estimated range
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
unsigned char mi_charge_timer;   // A per-second charge timer
unsigned long mi_charge_wm;      // A per-minute watt accumulator
unsigned char mi_candata_timer;  // A per-second timer for CAN bus data

signed char mi_batttemps[24]; // Temperature buffer, holds first two temps from the 0x6e1 messages (24 of the 64 values)

#pragma udata

////////////////////////////////////////////////////////////////////////
// vehicle_mitsubishi_ticker1()
// This function is an entry point from the main() program loop, and
// gives the CAN framework a ticker call approximately once per second
//
BOOL vehicle_mitsubishi_ticker1(void)
  {
  
  ////////////////////////////////////////////////////////////////////////
  // Stale tickers
  ////////////////////////////////////////////////////////////////////////
  
  if (car_stale_temps>0) car_stale_temps--;
  if (mi_candata_timer > 0)
    {
    if (--mi_candata_timer == 0) 
      { // Car has gone to sleep
      car_doors3 &= ~0x01;  // Car is asleep
      }
    else
      {
      car_doors3 |= 0x01;   // Car is awake
      }
    }
  car_time++;
  
  
  
  ////////////////////////////////////////////////////////////////////////
  // Charge state determination
  ////////////////////////////////////////////////////////////////////////
  // 0x04 Chargeport open, bit 2 set
  // 0x08 Pilot signal on, bit 3 set
  // 0x10 Vehicle charging, bit 4 set
  // 0x0C Chargeport open and pilot signal, bits 2,3 set
  // 0x1C Bits 2,3,4 set
  
  if ((car_chargecurrent!=0)&&(car_linevoltage>100))
    { // CAN says the car is charging
    car_doors5bits.Charging12V = 1;  //MJ
    if ((car_doors1 & 0x08)==0)
      { // Charge has started
      car_doors1 |= 0x1c;     // Set charge, door and pilot bits
      //car_doors1 |= 0x0c;     // Set charge, and pilot bits
      car_chargemode = 0;     // Standard charge mode
      car_chargestate = 1;    // Charging state
      car_chargesubstate = 3; // Charging by request
      car_chargelimit = 13;   // Hard-code 13A charge limit
      car_chargeduration = 0; // Reset charge duration
      car_chargekwh = 0;      // Reset charge kWh
      mi_charge_timer = 0;    // Reset the per-second charge timer
      mi_charge_wm = 0;       // Reset the per-minute watt accumulator
      net_req_notification(NET_NOTIFY_STAT);
      }
    else
      { // Charge is ongoing
      car_doors1bits.ChargePort = 1;  //MJ
      mi_charge_timer++;
      if (mi_charge_timer>=60)
        { // One minute has passed
        mi_charge_timer=0;
        car_chargeduration++;
        mi_charge_wm += (car_chargecurrent*car_linevoltage);
        if (mi_charge_wm >= 60000L)
          { // Let's move 1kWh to the virtual car
          car_chargekwh += 1;
          mi_charge_wm -= 60000L;
          }
        }
      }
    }

  else if ((car_chargecurrent==0)&&(car_linevoltage>100))
    { // CAN says the car is not charging
    if ((car_doors1 & 0x08)&&(car_SOC==100))
      { // Charge has completed 
      car_doors1 &= ~0x18;    // Clear charge and pilot bits
      //car_doors1 &= ~0x0c;    // Clear charge and pilot bits
      car_doors1bits.ChargePort = 1;  //MJ
      car_chargemode = 0;     // Standard charge mode
      car_chargestate = 4;    // Charge DONE
      car_chargesubstate = 3; // Leave it as is
      net_req_notification(NET_NOTIFY_CHARGE);  // Charge done Message MJ
      mi_charge_timer = 0;       // Reset the per-second charge timer
      mi_charge_wm = 0;          // Reset the per-minute watt accumulator
      net_req_notification(NET_NOTIFY_STAT);
      }
    car_doors5bits.Charging12V = 0;  // MJ
    }

  else if ((car_chargecurrent==0)&&(car_linevoltage<100))
    { // CAN says the car is not charging
    if (car_doors1 & 0x08)
      { // Charge has completed / stopped
      car_doors1 &= ~0x18;    // Clear charge and pilot bits
      //car_doors1 &= ~0x0c;    // Clear charge and pilot bits
      car_doors1bits.ChargePort = 1;  //MJ
      car_chargemode = 0;     // Standard charge mode
      if (car_SOC < 95)
        { // Assume charge was interrupted
        car_chargestate = 21;    // Charge STOPPED
        car_chargesubstate = 14; // Charge INTERRUPTED
        net_req_notification(NET_NOTIFY_CHARGE);
        }
      else
        { // Assume charge completed normally
        car_chargestate = 4;    // Charge DONE
        car_chargesubstate = 3; // Leave it as is
        net_req_notification(NET_NOTIFY_CHARGE);  // Charge done Message MJ
        }
      mi_charge_timer = 0;       // Reset the per-second charge timer
      mi_charge_wm = 0;          // Reset the per-minute watt accumulator
      net_req_notification(NET_NOTIFY_STAT);
      }
    car_doors5bits.Charging12V = 0;  // MJ
    car_doors1bits.ChargePort = 0;   // Charging cable unplugged, charging door closed.
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

  mi_candata_timer = 60;  // Reset the timer
  
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
      car_idealrange = ((((unsigned int)car_SOC) * 93) / 100); //Ideal range: i-Miev - 93 miles (150 Km). C-Zero - 80 miles?
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

  mi_candata_timer = 60;  // Reset the timer
  
  switch (id)
    {
    
    case 0x285:
      {
      if (can_databuffer[6] == 0x0C) // Car in park
        {
        car_doors1 |= 0x40;     //  PARK
        car_doors1 &= ~0x80;    // CAR OFF
        if (car_parktime == 0)
          {
          car_parktime = car_time-1;    // Record it as 1 second ago, so non zero report
          net_req_notification(NET_NOTIFY_ENV);
          }
        }
      if (can_databuffer[6] == 0x0E) // Car not in park
        {
        car_doors1 &= ~0x40;     //  NOT PARK
        car_doors1 |= 0x80;      // CAR ON
        if (car_parktime != 0)
          {
          car_parktime = 0; // No longer parking
          net_req_notification(NET_NOTIFY_ENV);
          }
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
  mi_candata_timer = 0;
  
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

  net_fnbits |= NET_FN_INTERNALGPS;   // Require internal GPS
  net_fnbits |= NET_FN_12VMONITOR;    // Require 12v monitor

  return TRUE;
  }
