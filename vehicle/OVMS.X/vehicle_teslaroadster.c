/*
;    Project:       Open Vehicle Monitor System
;    Date:          16 October 2011
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011  Michael Stegen / Stegen Electronics
;    (C) 2011  Mark Webb-Johnson
;    (C) 2011  Sonny Chen @ EPRO/DX
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

/*
; CREDIT
; Thanks to Scott451 for figuring out many of the Roadster CAN bus messages used by the OVMS,
; and the pinout of the CAN bus socket in the Roadster.
; http://www.teslamotorsclub.com/showthread.php/4388-iPhone-app?p=49456&viewfull=1#post49456"]iPhone app
; Thanks to fuzzylogic for further analysis and messages such as door status, unlock/lock, speed, VIN, etc.
; Thanks to markwj for further analysis and messages such as Trip, Odometer, TPMS, etc.
*/

#include <stdlib.h>
#include <delays.h>
#include <string.h>
#include <stdio.h>
#include "ovms.h"
#include "params.h"
#include "net_msg.h"

#define FEATURE_SPEEDO_REPEATS 5 // Number of times to repeat speedo updates

// Capabilities for Tesla Roadster
rom char teslaroadster_capabilities[] = "C10-12,C15-24";

#pragma udata overlay vehicle_overlay_data
signed char tr_cooldown_recycle;             // Ticker counter for cooldown recycle
unsigned char can_lastspeedmsg[8];           // A buffer to store the last speed message
unsigned char can_lastspeedrpt;              // A mechanism to repeat the tx of last speed message
unsigned char tr_requestcac;                 // Request CAC

#pragma udata

BOOL vehicle_teslaroadster_ticker60(void);

////////////////////////////////////////////////////////////////////////
// can_poll()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to poll for data.
//

// ISR optimization, see http://www.xargs.com/pic/c18-isr-optim.pdf
#pragma tmpdata high_isr_tmpdata

BOOL vehicle_teslaroadster_poll0(void)                // CAN ID 100 and 102
  {
  unsigned char k;
  unsigned int k1;
  unsigned long k2;

  if (can_id == 0x100)
    {
    switch (can_databuffer[0])
      {
      case 0x06: // Charge timer mode
        if (can_databuffer[1] == 0x1b)
          {
          car_timermode = can_databuffer[4];
          car_stale_timer = 1; // Reset stale indicator
          }
        else if (can_databuffer[1] == 0x1a)
          {
          car_timerstart = (can_databuffer[4]<<8)+can_databuffer[5];
          car_stale_timer = 1; // Reset stale indicator
          }
        break;
      case 0x80: // Range / State of Charge
        car_SOC = can_databuffer[1];
        car_idealrange = can_databuffer[2]+((unsigned int) can_databuffer[3] << 8);
        car_estrange = can_databuffer[6]+((unsigned int) can_databuffer[7] << 8);
        if (car_idealrange>6000) car_idealrange=0; // Sanity check (limit rng->std)
        if (car_estrange>6000)   car_estrange=0; // Sanity check (limit rng->std)
        break;
      case 0x81: // Time/ Date UTC
        car_time = can_databuffer[4]
                   + ((unsigned long) can_databuffer[5] << 8)
                   + ((unsigned long) can_databuffer[6] << 16)
                   + ((unsigned long) can_databuffer[7] << 24);
        break;
      case 0x82: // Ambient Temperature
        car_ambient_temp = (signed char)can_databuffer[1];
        car_stale_ambient = 120; // Reset stale indicator
        break;
      case 0x83: // GPS Latitude
        car_latitude = can_databuffer[4]
                       + ((unsigned long) can_databuffer[5] << 8)
                       + ((unsigned long) can_databuffer[6] << 16)
                       + ((unsigned long) can_databuffer[7] << 24);
        break;
      case 0x84: // GPS Longitude
        car_longitude = can_databuffer[4]
                        + ((unsigned long) can_databuffer[5] << 8)
                        + ((unsigned long) can_databuffer[6] << 16)
                        + ((unsigned long) can_databuffer[7] << 24);
        break;
      case 0x85: // GPS direction and altitude
        car_gpslock = can_databuffer[1];
        if (car_gpslock)
          {
          car_direction = ((unsigned int)can_databuffer[3]<<8)+(can_databuffer[2]);
          if (car_direction==360) car_direction=0; // Bug-fix for Tesla VMS bug
          if (can_databuffer[5]&0xf0)
            car_altitude = 0;
          else
            car_altitude = ((unsigned int)can_databuffer[5]<<8)+(can_databuffer[4]);
          car_stale_gps = 120; // Reset stale indicator
          }
        else
          {
          car_stale_gps = 0; // Reset stale indicator
          }
        break;
      case 0x88: // Charging Current / Duration
        if (can_databuffer[6] != car_chargelimit)
          { // If the charge limit has changed, notify it
          net_req_notification(NET_NOTIFY_STAT);
          }
        car_chargecurrent = can_databuffer[1];
        car_chargelimit = can_databuffer[6];
        car_chargeduration = ((unsigned int)can_databuffer[3]<<8)+(can_databuffer[2]);
        break;
      case 0x89: // Charging Voltage / Iavailable
        if (can_mileskm=='M')
          car_speed = can_databuffer[1];     // speed in miles/hour
        else
          car_speed = (unsigned char) ((((unsigned long)can_databuffer[1] * 1609)+500)/1000);     // speed in km/hour
        car_linevoltage = can_databuffer[2]
                          + ((unsigned int) can_databuffer[3] << 8);
        break;
      case 0x93: // VDS Vehicle Error
        k = can_databuffer[1];
        k1 = ((unsigned int)can_databuffer[3]<<8)+(can_databuffer[2]);
        k2 = (((unsigned long)can_databuffer[7]<<24) +
              ((unsigned long)can_databuffer[6]<<16) +
              ((unsigned long)can_databuffer[5]<<8) +
              (can_databuffer[4]));
        if (k1 != 0xffff)
          {
          if ((k == 0x14)&&(k1 == 25))
            {
            // Special case of 100% vehicle logs
            net_req_notification_error(0, 0);
            net_req_notification_error(k1, k2); // Notify the 100%
            }
          else if (k & 0x01)
            {
            // An error code is being raised
            net_req_notification_error(k1, k2);
            }
          else
            {
            // An error code is being cleared
            net_req_notification_error(0, 0);
            }
          }
        break;
      case 0x8F: // HVAC#1 message
        k1 = ((unsigned int)can_databuffer[7]<<8)+(can_databuffer[6]);
        if (k1 > 0)
          {
          car_doors5bits.HVAC = 1;
          tr_cooldown_recycle = -1;  // Stop the recycle attempts
          }
        else
          {
          if ((car_coolingdown>=0)&&(car_doors5bits.HVAC))
            {
            // Car is cooling down, and HVAC has just gone off - end of a cycle
            car_coolingdown++;
            tr_cooldown_recycle = 60;  // Try to recycle cooling in 60 seconds
            net_req_notification(NET_NOTIFY_STAT);
            }
          car_doors5bits.HVAC = 0;
          }
        break;
      case 0x95: // Charging mode
        if ((can_databuffer[1] != car_chargestate)&&            // Charge state has changed AND
            ((car_chargestate<=2)||(car_chargestate==0x0f))&&   // was (Charging or Heating) AND
            (can_databuffer[1]!=0x04))                          // new state is not done
          {
          // We've moved from charging/heating to something other than DONE
          // Let's treat this as a notifiable alert
          net_req_notification(NET_NOTIFY_CHARGE);
          }
        if ((can_databuffer[1] != car_chargestate)||
            (can_databuffer[2] != car_chargesubstate))
          { // If the state or sub-state has changed, notify it
          net_req_notification(NET_NOTIFY_STAT);
          if (can_databuffer[1]==1)
            tr_requestcac=2; // Request CAC when charge starts
          }
        car_chargestate = can_databuffer[1];
        car_chargesubstate = can_databuffer[2];
        if (sys_features[FEATURE_CARBITS]&FEATURE_CB_2008) // A 2010+ roadster?
          k = (can_databuffer[4]) & 0x0F;  // for 2008 roadsters
        else
          k = (can_databuffer[5] >> 4) & 0x0F; // for 2010 roadsters
        if (k != car_chargemode)
          { // If the charge mode has changed, notify it
          car_chargemode = k;
          net_req_notification(NET_NOTIFY_STAT);
          }
        car_charge_b4 = can_databuffer[3];
        car_chargekwh = can_databuffer[7];
        break;
      case 0x96: // Doors / Charging yes/no
        if (car_chargestate == 0x0f) can_databuffer[1] |= 0x10; // Fudge for heating state, to be charging=on
        if ((car_doors1 != can_databuffer[1])||
            (car_doors2 != can_databuffer[2])||
            (car_doors3 != can_databuffer[3])||
            (car_doors4 != can_databuffer[4]))
          net_req_notification(NET_NOTIFY_ENV);

        if (((car_doors2&0x80)==0)&&(can_databuffer[2]&0x80)&&(can_databuffer[2]&0x10))
          net_req_notification(NET_NOTIFY_TRUNK); // Valet mode is active, and trunk was opened

        if (((car_doors4&0x02)==0)&&((can_databuffer[4]&0x02)!=0))
          net_req_notification(NET_NOTIFY_ALARM); // Alarm has been triggered

        car_doors1 = can_databuffer[1]; // Doors #1
        car_doors2 = can_databuffer[2]; // Doors #2
        car_doors3 = can_databuffer[3]; // Doors #3
        car_doors4 = can_databuffer[4]; // Doors #4
        if (((car_doors1 & 0x80)==0)&&  // Car is not ON
            (car_parktime == 0)&&       // Parktime was not previously set
            (car_time != 0))            // We know the car time
          {
          tr_requestcac=2; // Request CAC when car stops
          car_parktime = car_time-1;    // Record it as 1 second ago, so non zero report
          net_req_notification(NET_NOTIFY_ENV);
          }
        else if ((car_doors1 & 0x80)&&  // Car is ON
                 (car_parktime != 0))   // Parktime was previously set
          {
          tr_requestcac=2; // Request CAC when car starts
          car_parktime = 0;
          net_req_notification(NET_NOTIFY_ENV);
          }
        break;
      case 0x9E: // CAC
        if (tr_requestcac == 1)
          tr_requestcac = 3; // Turn off CAC streaming
        car_cac100 = ((unsigned int)can_databuffer[3]*100)+
                     ((((unsigned int)can_databuffer[2]*100)+128)/256);
        break;
      case 0xA3: // Temperatures
        car_tpem = (signed char)can_databuffer[1]; // Tpem
        car_tmotor = (unsigned char)can_databuffer[2]; // Tmotor
        car_tbattery = (signed char)can_databuffer[6]; // Tbattery
        car_stale_temps = 120; // Reset stale indicator
        break;
      case 0xA4: // 7 VIN bytes i.e. "SFZRE2B"
        for (k=0;k<7;k++)
          car_vin[k] = can_databuffer[k+1];
        break;
      case 0xA5: // 7 VIN bytes i.e. "39A3000"
        for (k=0;k<7;k++)
          car_vin[k+7] = can_databuffer[k+1];
        if ((can_databuffer[3] == 'A')||(can_databuffer[3] == 'B'))
          car_type[2] = '2';
        else
          car_type[2] = '1';
        if (can_databuffer[3] == '8')
          sys_features[FEATURE_CARBITS] |= FEATURE_CB_2008; // Auto-enable 1.5 support
        if (can_databuffer[1] == '3')
          car_type[3] = 'S';
        else
          car_type[3] = 'N';
        break;
      case 0xA6: // 3 VIN bytes i.e. "359"
        car_vin[14] = can_databuffer[1];
        car_vin[15] = can_databuffer[2];
        car_vin[16] = can_databuffer[3];
        break;
      }
    }
  else if (can_id == 0x102)
    {
    switch (can_databuffer[0])
      {
      case 0x0E: // Lock/Unlock state on ID#102
        if (car_lockstate != can_databuffer[1])
          net_req_notification(NET_NOTIFY_ENV);
        car_lockstate = can_databuffer[1];
        break;
      }
    }

  return TRUE;
  }

BOOL vehicle_teslaroadster_poll1(void)                // CAN ID 344 and 402
{
  if (can_id == 0x400)
    {
    // Speedometer feature - replace Range->Dash with speed
    if ((can_databuffer[0]==0x02)&&         // The SPEEDO AMPS message
        (sys_features[FEATURE_OPTIN]&FEATURE_OI_SPEEDO)&& // Digital speedo
        ((sys_features[FEATURE_CARBITS]&FEATURE_CB_2008)==0)&& // A 2010+ roadster?
        (car_doors1 & 0x80)&&               // The car is on
        (car_speed != can_databuffer[2])&&  // The speed != Amps
        (sys_features[FEATURE_CANWRITE]>0)) // The CAN bus can be written to
      {
      can_lastspeedmsg[0] = can_databuffer[0];
      can_lastspeedmsg[1] = can_databuffer[1];
      can_lastspeedmsg[2] = car_speed;
      can_lastspeedmsg[3] = can_databuffer[3] & 0xf0; // Mask lower nibble (speed always <256)
      can_lastspeedmsg[4] = can_databuffer[4];
      can_lastspeedmsg[5] = can_databuffer[5];
      can_lastspeedmsg[6] = can_databuffer[6];
      can_lastspeedmsg[7] = can_databuffer[7];
      while (TXB0CONbits.TXREQ) {} // Loop until TX is done
      TXB0CON = 0;
      TXB0SIDL = 0b00000000; // Setup Filter and Mask so that only CAN ID 0x100 will be accepted
      TXB0SIDH = 0b10000000; // Set Filter to 0x400
      TXB0D0 = can_lastspeedmsg[0];
      TXB0D1 = can_lastspeedmsg[1];
      TXB0D2 = can_lastspeedmsg[2];
      TXB0D3 = can_lastspeedmsg[3];
      TXB0D4 = can_lastspeedmsg[4];
      TXB0D5 = can_lastspeedmsg[5];
      TXB0D6 = can_lastspeedmsg[6];
      TXB0D7 = can_lastspeedmsg[7];
      TXB0DLC = 0b00001000; // data length (8)
      TXB0CON = 0b00001000; // mark for transmission
      can_lastspeedrpt = FEATURE_SPEEDO_REPEATS; // Force re-transmissions
      if (can_lastspeedrpt>10) can_lastspeedrpt=10;
      }
    }
  else if (can_id == 0x344)
    {
    // TPMS code here
    if (can_databuffer[3]>0) // front-right
      {
      car_tpms_p[0] = can_databuffer[2];
      car_tpms_t[0] = (signed char)can_databuffer[3];
      }
    if (can_databuffer[7]>0) // rear-right
      {
      car_tpms_p[1] = can_databuffer[6];
      car_tpms_t[1] = (signed char)can_databuffer[7];
      }
    if (can_databuffer[1]>0) // front-left
      {
      car_tpms_p[2] = can_databuffer[0];
      car_tpms_t[2] = (signed char)can_databuffer[1];
      }
    if (can_databuffer[5]>0)
      {
      car_tpms_p[3] = can_databuffer[4];
      car_tpms_t[3] = (signed char)can_databuffer[5];
      }
    car_stale_tpms = 120; // Reset stale indicator
    }
  else if (can_id == 0x402)
    {
    switch (can_databuffer[0])
      {
      case 0xFA:			// ODOMETER
        car_odometer = can_databuffer[3]
		+ ((unsigned long) can_databuffer[4] << 8)
                + ((unsigned long) can_databuffer[5] << 16);		// Miles /10
	car_trip = can_databuffer[6] + ((unsigned int) can_databuffer[7] << 8);	// Miles /10
	break;
      }
    }

  return TRUE;
}

#pragma tmpdata


////////////////////////////////////////////////////////////////////////
// vehicle_teslaroadster_ticker10th()
// This function is an entry point from the main() program loop, and
// gives the CAN framework a ticker call approximately ten times per
// second.
//
BOOL vehicle_teslaroadster_ticker10th(void)
  {
  if (can_lastspeedrpt==0) can_lastspeedrpt=FEATURE_SPEEDO_REPEATS;
  return FALSE;
  }

////////////////////////////////////////////////////////////////////////
// vehicle_teslaroadster_ticker10th()
// This function is an entry point from the main() program loop, and
// gives the CAN framework a call whenever there is idle time
//
BOOL vehicle_teslaroadster_idlepoll(void)
  {
  if (tr_requestcac > 1)
    {
    if (sys_features[FEATURE_CANWRITE]==0)
      {
      tr_requestcac = 0;
      }
    else if (tr_requestcac == 2)
      {
      // Request CAC streaming...
      // 102 06 D0 07 00 00 00 00 40
      tr_requestcac = 1; // Wait for cac, then cancel
      while (TXB0CONbits.TXREQ) {} // Loop until TX is done
      TXB0CON = 0;
      TXB0SIDL = 0b01000000; // Setup 0x102
      TXB0SIDH = 0b00100000; // Setup 0x102
      TXB0D0 = 0x06;
      TXB0D1 = 0xd0;
      TXB0D2 = 0x07;
      TXB0D3 = 0x00;
      TXB0D4 = 0x00;
      TXB0D5 = 0x00;
      TXB0D6 = 0x00;
      TXB0D7 = 0x40;
      TXB0DLC = 0b00001000; // data length (8)
      TXB0CON = 0b00001000; // mark for transmission
      }
    else if (tr_requestcac == 3)
      {
      // Cancel CAC streaming...
      // 102 06 00 00 00 00 00 00 40
      tr_requestcac = 0; // CAC done
      while (TXB0CONbits.TXREQ) {} // Loop until TX is done
      TXB0CON = 0;
      TXB0SIDL = 0b01000000; // Setup 0x102
      TXB0SIDH = 0b00100000; // Setup 0x102
      TXB0D0 = 0x06;
      TXB0D1 = 0x00;
      TXB0D2 = 0x00;
      TXB0D3 = 0x00;
      TXB0D4 = 0x00;
      TXB0D5 = 0x00;
      TXB0D6 = 0x00;
      TXB0D7 = 0x40;
      TXB0DLC = 0b00001000; // data length (8)
      TXB0CON = 0b00001000; // mark for transmission
      vehicle_teslaroadster_ticker60();      // To calculate charge mins remaining, now we have CAC
      net_req_notification(NET_NOTIFY_STAT); // Notify it (in particular, the charge time estimate
      }
    }

  if (can_lastspeedrpt == 0) return FALSE;

  // Speedometer feature - replace Range->Dash with speed
  if ((can_lastspeedmsg[0]==0x02)&&        // It is a valid AMPS message
      (sys_features[FEATURE_OPTIN]&FEATURE_OI_SPEEDO)&& // Digital speedo
      ((sys_features[FEATURE_CARBITS]&FEATURE_CB_2008)==0)&& // A 2010+ roadster?
      (car_doors1 & 0x80)&&                // The car is on
      (sys_features[FEATURE_CANWRITE]>0))  // The CAN bus can be written to
    {
    Delay1KTCYx(1);
    while (TXB0CONbits.TXREQ) {} // Loop until TX is done
    TXB0CON = 0;
    TXB0SIDL = 0b00000000; // Setup Filter and Mask so that only CAN ID 0x100 will be accepted
    TXB0SIDH = 0b10000000; // Set Filter to 0x400
    TXB0D0 = can_lastspeedmsg[0];
    TXB0D1 = can_lastspeedmsg[1];
    TXB0D2 = can_lastspeedmsg[2];
    TXB0D3 = can_lastspeedmsg[3];
    TXB0D4 = can_lastspeedmsg[4];
    TXB0D5 = can_lastspeedmsg[5];
    TXB0D6 = can_lastspeedmsg[6];
    TXB0D7 = can_lastspeedmsg[7];
    TXB0DLC = 0b00001000; // data length (8)
    TXB0CON = 0b00001000; // mark for transmission
    }
  can_lastspeedrpt--;

  return FALSE;
  }

void vehicle_teslaroadster_tx_wakeup(void)
  {
  while (TXB0CONbits.TXREQ) {} // Loop until TX is done
  TXB0CON = 0;
  TXB0SIDL = 0b01000000; // Setup 0x102
  TXB0SIDH = 0b00100000; // Setup 0x102
  TXB0D0 = 0x0a;
  TXB0DLC = 0b00000001; // data length (1)
  TXB0CON = 0b00001000; // mark for transmission
  while (TXB0CONbits.TXREQ) {} // Loop until TX is done
  }

void vehicle_teslaroadster_tx_wakeuptemps(void)
  {
  while (TXB0CONbits.TXREQ) {} // Loop until TX is done
  TXB0CON = 0;
  TXB0SIDL = 0b01000000; // Setup 0x102
  TXB0SIDH = 0b00100000; // Setup 0x102
  TXB0D0 = 0x06;
  TXB0D1 = 0x2c;
  TXB0D2 = 0x01;
  TXB0D3 = 0x00;
  TXB0D4 = 0x00;
  TXB0D5 = 0x09;
  TXB0D6 = 0x10;
  TXB0D7 = 0x00;
  TXB0DLC = 0b00001000; // data length (8)
  TXB0CON = 0b00001000; // mark for transmission
  while (TXB0CONbits.TXREQ) {} // Loop until TX is done
  }

void vehicle_teslaroadster_tx_wakeuphvac(void)
  {
  while (TXB0CONbits.TXREQ) {} // Loop until TX is done
  TXB0CON = 0;
  TXB0SIDL = 0b01000000; // Setup 0x102
  TXB0SIDH = 0b00100000; // Setup 0x102
  TXB0D0 = 0x06;
  TXB0D1 = 0xd0;
  TXB0D2 = 0x07;
  TXB0D3 = 0x00;
  TXB0D4 = 0x00;
  TXB0D5 = 0x80;
  TXB0D6 = 0x00;
  TXB0D7 = 0x08;
  TXB0DLC = 0b00001000; // data length (8)
  TXB0CON = 0b00001000; // mark for transmission
  while (TXB0CONbits.TXREQ) {} // Loop until TX is done
  }

void vehicle_teslaroadster_tx_setchargemode(unsigned char mode)
  {
  vehicle_teslaroadster_tx_wakeup(); // Also, wakeup the car if necessary

  while (TXB0CONbits.TXREQ) {} // Loop until TX is done
  TXB0CON = 0;
  TXB0SIDL = 0b01000000; // Setup 0x102
  TXB0SIDH = 0b00100000; // Setup 0x102
  TXB0D0 = 0x05;
  TXB0D1 = 0x19;
  TXB0D2 = 0x00;
  TXB0D3 = 0x00;
  TXB0D4 = mode;
  TXB0D5 = 0x00;
  TXB0D6 = 0x00;
  TXB0D7 = 0x00;
  TXB0DLC = 0b00001000; // data length (8)
  TXB0CON = 0b00001000; // mark for transmission
  while (TXB0CONbits.TXREQ) {} // Loop until TX is done
  }

void vehicle_teslaroadster_tx_setchargecurrent(unsigned char current)
  {
  vehicle_teslaroadster_tx_wakeup(); // Also, wakeup the car if necessary

  while (TXB0CONbits.TXREQ) {} // Loop until TX is done
  TXB0CON = 0;
  TXB0SIDL = 0b01000000; // Setup 0x102
  TXB0SIDH = 0b00100000; // Setup 0x102
  TXB0D0 = 0x05;
  TXB0D1 = 0x02;
  TXB0D2 = 0x00;
  TXB0D3 = 0x00;
  TXB0D4 = current;
  TXB0D5 = 0x00;
  TXB0D6 = 0x00;
  TXB0D7 = 0x00;
  TXB0DLC = 0b00001000; // data length (8)
  TXB0CON = 0b00001000; // mark for transmission
  while (TXB0CONbits.TXREQ) {} // Loop until TX is done
  }

void vehicle_teslaroadster_tx_startstopcharge(unsigned char start)
  {
  vehicle_teslaroadster_tx_wakeup(); // Also, wakeup the car if necessary

  while (TXB0CONbits.TXREQ) {} // Loop until TX is done
  TXB0CON = 0;
  TXB0SIDL = 0b01000000; // Setup 0x102
  TXB0SIDH = 0b00100000; // Setup 0x102
  TXB0D0 = 0x05;
  TXB0D1 = 0x03;
  TXB0D2 = 0x00;
  TXB0D3 = 0x00;
  TXB0D4 = start;
  TXB0D5 = 0x00;
  TXB0D6 = 0x00;
  TXB0D7 = 0x00;
  TXB0DLC = 0b00001000; // data length (8)
  TXB0CON = 0b00001000; // mark for transmission
  while (TXB0CONbits.TXREQ) {} // Loop until TX is done
  }

void vehicle_teslaroadster_tx_lockunlockcar(unsigned char mode, char *pin)
  {
  // Mode is 0=valet, 1=novalet, 2=lock, 3=unlock
  long lpin;
  lpin = atol(pin);

  if ((mode == 0x02)&&(car_doors1 & 0x80))
    return; // Refuse to lock a car that is turned on

  while (TXB0CONbits.TXREQ) {} // Loop until TX is done
  TXB0CON = 0;
  TXB0SIDL = 0b01000000; // Setup 0x102
  TXB0SIDH = 0b00100000; // Setup 0x102
  TXB0D0 = 0x0B;
  TXB0D1 = mode;
  TXB0D2 = 0x00;
  TXB0D3 = 0x00;
  TXB0D4 = lpin & 0xff;
  TXB0D5 = (lpin>>8) & 0xff;
  TXB0D6 = (lpin>>16) & 0xff;
  TXB0D7 = (strlen(pin)<<4) + ((lpin>>24) & 0x0f);
  TXB0DLC = 0b00001000; // data length (8)
  TXB0CON = 0b00001000; // mark for transmission
  while (TXB0CONbits.TXREQ) {} // Loop until TX is done
  }

void vehicle_teslaroadster_tx_timermode(unsigned char mode, unsigned int starttime)
  {
  vehicle_teslaroadster_tx_wakeup(); // Also, wakeup the car if necessary

  while (TXB0CONbits.TXREQ) {} // Loop until TX is done
  TXB0CON = 0;
  TXB0SIDL = 0b01000000; // Setup 0x102
  TXB0SIDH = 0b00100000; // Setup 0x102
  TXB0D0 = 0x05;
  TXB0D1 = 0x1B;
  TXB0D2 = 0x00;
  TXB0D3 = 0x00;
  TXB0D4 = mode;
  TXB0D5 = 0x00;
  TXB0D6 = 0x00;
  TXB0D7 = 0x00;
  TXB0DLC = 0b00001000; // data length (8)
  TXB0CON = 0b00001000; // mark for transmission
  while (TXB0CONbits.TXREQ) {} // Loop until TX is done
  if (mode == 1)
    {
    TXB0CON = 0;
    TXB0SIDL = 0b01000000; // Setup 0x102
    TXB0SIDH = 0b00100000; // Setup 0x102
    TXB0D0 = 0x05;
    TXB0D1 = 0x1A;
    TXB0D2 = 0x00;
    TXB0D3 = 0x00;
    TXB0D4 = (starttime >>8)&0xff;
    TXB0D5 = (starttime & 0xff);
    TXB0D6 = 0x00;
    TXB0D7 = 0x00;
    TXB0DLC = 0b00001000; // data length (8)
    TXB0CON = 0b00001000; // mark for transmission
    while (TXB0CONbits.TXREQ) {} // Loop until TX is done
    }
  }

void vehicle_teslaroadster_tx_homelink(unsigned char button)
  {
  vehicle_teslaroadster_tx_wakeup(); // Also, wakeup the car if necessary

  while (TXB0CONbits.TXREQ) {} // Loop until TX is done
  TXB0CON = 0;
  TXB0SIDL = 0b01000000; // Setup 0x102
  TXB0SIDH = 0b00100000; // Setup 0x102
  TXB0D0 = 0x09;
  TXB0D1 = 0x00;
  TXB0D2 = button;
  TXB0DLC = 0b00000011; // data length (3)
  TXB0CON = 0b00001000; // mark for transmission
  while (TXB0CONbits.TXREQ) {} // Loop until TX is done
  }

void vehicle_teslaroadster_cooldown(void)
  {
  // We have been requested to cool down the battery pack
  char *p;
  int k;

  // Save the old charge mode and limit
  car_cooldown_wascharging = (CAR_IS_CHARGING)?1:0;
  car_cooldown_chargemode = car_chargemode;
  car_cooldown_chargelimit = car_chargelimit;

  // Work out new mode and limit
  p = par_get(PARAM_COOLDOWN);
  p = strtokpgmram(p,":");
  if (p != NULL)
    {
    car_cooldown_tbattery = atoi(p);
    p = strtokpgmram(NULL,":");
    if (p != NULL)
      {
      car_cooldown_timelimit = atoi(p);
      }
    else
      {
      car_cooldown_timelimit = COOLDOWN_DEFAULT_TIMELIMIT;
      }
    }
  else
    {
    car_cooldown_tbattery = COOLDOWN_DEFAULT_TEMPLIMIT;
    car_cooldown_timelimit = COOLDOWN_DEFAULT_TIMELIMIT;
    }

  if ((car_tbattery > car_cooldown_tbattery)&& // Car is hot enough
      (car_doors1bits.CarON == 0)&&            // Car is not ON
      (car_doors1bits.ChargePort == 1))        // Charge port is open
    {
    // We need to start a cooldown
    vehicle_teslaroadster_tx_wakeup();
    delay100(10);
    for (k=0;k<2;k++) // Be persistent, and do this a few times to make sure
      {
      vehicle_teslaroadster_tx_setchargecurrent(13); // 13A charge
      delay100(1);
      vehicle_teslaroadster_tx_setchargemode(3);     // Switch to RANGE mode
      delay100(1);
      vehicle_teslaroadster_tx_startstopcharge(1);   // Force START charge
      delay100(1);
      }
    vehicle_teslaroadster_tx_wakeuphvac();         // Start HVAC data
    car_coolingdown = 0;
    tr_cooldown_recycle = -1;
    }
  }

BOOL vehicle_teslaroadster_commandhandler(BOOL msgmode, int code, char* msg)
  {
  char *p;
  BOOL sendenv = FALSE;

  switch (code)
    {
    case 10: // Set charge mode (params: 0=standard, 1=storage,3=range,4=performance)
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        STP_NOCANWRITE(net_scratchpad, code);
        }
      else
        {
        vehicle_teslaroadster_tx_setchargemode(atoi(msg));
        STP_OK(net_scratchpad, code);
        }
      break;


    case 11: // Start charge (params unused)
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        STP_NOCANWRITE(net_scratchpad, code);
        }
      else
        {
        if ((car_doors1 & 0x04)&&(car_chargesubstate != 0x07))
          {
          vehicle_teslaroadster_tx_startstopcharge(1);
          net_notify_suppresscount = 0; // Enable notifications
          STP_OK(net_scratchpad, code);
          }
        else
          {
          STP_NOCANCHARGE(net_scratchpad, code);
          }
        }
      break;

    case 12: // Stop charge (params unused)
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        STP_NOCANWRITE(net_scratchpad, code);
        }
      else
        {
        if ((car_doors1 & 0x10))
          {
          vehicle_teslaroadster_tx_startstopcharge(0);
          net_notify_suppresscount = 30; // Suppress notifications for 30 seconds
          STP_OK(net_scratchpad, code);
          }
        else
          {
          STP_NOCANSTOPCHARGE(net_scratchpad, code);
          }
        }
      break;

    case 15: // Set charge current (params: current in amps)
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        STP_NOCANWRITE(net_scratchpad, code);
        }
      else
        {
        vehicle_teslaroadster_tx_setchargecurrent(atoi(msg));
        STP_OK(net_scratchpad, code);
        }
      break;

    case 16: // Set charge mode and current (params: mode, current)
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        STP_NOCANWRITE(net_scratchpad, code);
        }
      else
        {
        for (p=msg;(*p != 0)&&(*p != ',');p++) ;
        // check if a value exists and is separated by a comma
        if (*p == ',')
          {
          *p++ = 0;
          // At this point, <msg> points to the mode, and p to the current
          vehicle_teslaroadster_tx_setchargemode(atoi(msg));
          vehicle_teslaroadster_tx_setchargecurrent(atoi(p));
          STP_OK(net_scratchpad, code);
          }
        else
          {
          STP_INVALIDSYNTAX(net_scratchpad, code);
          }
        }
      break;

    case 17: // Set charge timer mode and start time
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        STP_NOCANWRITE(net_scratchpad, code);
        }
      else
        {
        for (p=msg;(*p != 0)&&(*p != ',');p++) ;
        // check if a value exists and is separated by a comma
        if (*p == ',')
          {
          *p++ = 0;
          // At this point, <msg> points to the mode, and p to the time
          vehicle_teslaroadster_tx_timermode(atoi(msg),atoi(p));
          STP_OK(net_scratchpad, code);
          }
        else
          {
          STP_INVALIDSYNTAX(net_scratchpad, code);
          }
        }
      break;

    case 18: // Wakeup car
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        STP_NOCANWRITE(net_scratchpad, code);
        }
      else
        {
        vehicle_teslaroadster_tx_wakeup();
        vehicle_teslaroadster_tx_wakeuptemps();
        STP_OK(net_scratchpad, code);
        }
      break;

    case 19: // Wakeup temperature subsystem
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        STP_NOCANWRITE(net_scratchpad, code);
        }
      else
        {
        vehicle_teslaroadster_tx_wakeuptemps();
        STP_OK(net_scratchpad, code);
        }
      break;

    case 20: // Lock car (params pin)
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        STP_NOCANWRITE(net_scratchpad, code);
        }
      else
        {
        vehicle_teslaroadster_tx_lockunlockcar(2, msg);
        STP_OK(net_scratchpad, code);
        }
      sendenv=TRUE;
      break;

    case 21: // Activate valet mode (params pin)
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        STP_NOCANWRITE(net_scratchpad, code);
        }
      else
        {
        vehicle_teslaroadster_tx_lockunlockcar(0, msg);
        STP_OK(net_scratchpad, code);
        }
      sendenv=TRUE;
      break;

    case 22: // Unlock car (params pin)
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        STP_NOCANWRITE(net_scratchpad, code);
        }
      else
        {
        vehicle_teslaroadster_tx_lockunlockcar(3, msg);
        STP_OK(net_scratchpad, code);
        }
      sendenv=TRUE;
      break;

    case 23: // Deactivate valet mode (params pin)
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        STP_NOCANWRITE(net_scratchpad, code);
        }
      else
        {
        vehicle_teslaroadster_tx_lockunlockcar(1, msg);
        STP_OK(net_scratchpad, code);
        }
      sendenv=TRUE;
      break;

    case 24: // Home Link
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        STP_NOCANWRITE(net_scratchpad, code);
        }
      else
        {
        vehicle_teslaroadster_tx_homelink(atoi(msg));
        STP_OK(net_scratchpad, code);
        }
      break;

    case 25: // Cooldown
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        STP_NOCANWRITE(net_scratchpad, code);
        }
      else
        {
        vehicle_teslaroadster_cooldown();
        STP_OK(net_scratchpad, code);
        }
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

int vehicle_teslaroadster_minutestocharge(
      unsigned char chgmod,    // charge mode: 0 (Standard), 3 (Range) or 4 (Performance)
      int wAvail,              // watts available from the wall
      int imStart,             // ideal mi at start of charge
      int imTarget,            // ideal mi desired at end of charge (use <=0 for full charge)
      int pctTarget,           // if imTarget == -1, specified desired percent SOC
                               // use 100 for full charge
                               // pctTarget is ignored if imEnd != -1
      int cac100,              // the battery pack CAC*100 (pass 0 if unknown)
      signed char degAmbient,  // ambient temperature in degrees C
      int *pimTarget           // if not null, will be filled in with expected ideal
                               // miles at target charge level
      )
  {
  int bIntercept;
  int mx1000;
  int whPerIM;
  signed long secPerIMSteady;
  signed long seconds;
  int imCapacityNominal;
  int imCapacity;
  int imTaperBase;
  signed long numTaper;
  int denBaseTaper;

#ifdef OVMS_DIAGMODULE
  char *p;

  if (net_state == NET_STATE_DIAGMODE)
    {
    p = stp_i(net_scratchpad,"\r\n# TR minutestocharge(",chgmod);
    p = stp_i(p,", ",imStart);
    p = stp_i(p,", ",imTarget);
    p = stp_i(p,", ",pctTarget);
    p = stp_i(p,", ",cac100);
    p = stp_i(p,", ",wAvail);
    p = stp_i(p,", ",degAmbient);
    p = stp_rom(p,")\r\n");
    net_puts_ram(net_scratchpad);
    }
#endif

  if (cac100 == 0) cac100=16000; // Default to a nominal new battery pack
  if (pctTarget <= 0) pctTarget=100;   // Default to a full charge

  switch (chgmod)
    {
    case 0: // Standard
	  // standard mode IM capacity is about 1.1891 * CAC + 0.8 (10/10/2013 survey data)
	  imCapacity = (1189L * cac100 + 80000 + 50000)/ 100000;

       // algorithm parameters, determined from OVMS Log Data, 2418 data points
      imCapacityNominal = 191;
      imTaperBase = 169;
      numTaper = 197578L;
      denBaseTaper = 19777;
      break;

    case 3: // Range
      // range mode IM capacity is about 1.5463 * CAC - 3.4 (10/10/2013 survey data)
      imCapacity = (1546L * cac100 - 340000 + 50000)/ 100000;

      // algorithm parameters, determined from OVMS Log Data, 234 data points
      imCapacityNominal = 244;
      imTaperBase = 215;
      numTaper = 212156L;
      denBaseTaper = 24647;
      break;

    case 4: // Performance
      // interpolate standard and range to get performance mode
      // IM = 1.3677 * CAC - 1.3
      imCapacity = (1368L * cac100 - 130000 + 50000)/ 100000;

      // algorithm parameters, determined from OVMS Log Data, 13 data points
      imCapacityNominal = 218;
      imTaperBase = 167;
      numTaper = 429204L;
      denBaseTaper = 23122;
      break;

    default: // invalid charge mode passed in (Storage mode doesn't make sense)
      return -(int)(100+chgmod);
    }

  // if needed, calculate charge target from specified percent
  if (imTarget <= 0)
    imTarget = (imCapacity * pctTarget + 50)/100;

  // tell the caller the expected ideal miles at the requested charge level
  if (pimTarget != NULL)
    *pimTarget = imTarget;

  // check for silly cases
  if (wAvail <= 0)
    return -2;
  if (imTarget <= imStart)
    return -3;

  // I don't believe air temperatures above 60 C, and this avoids overflow issues
  if (degAmbient > 60)
    degAmbient = 60;

  // normalize the IM values to look like a nominal new pack
  imStart  += imCapacityNominal - imCapacity;
  imTarget += imCapacityNominal - imCapacity;
  if (imTarget > imCapacityNominal)
    imTarget = imCapacityNominal;

  // calculate temperature to charge rate equation
  bIntercept = (wAvail >= 2300) ? 288 : (signed long)745 - (signed long)199 * wAvail / 1000;
  mx1000 = (signed long)3588 - (signed long)250 * wAvail / 1000;

  // the data says that 70A gets slightly faster in high heat,
  // but I think that's an anomoly in the small data set,
  // so take that out of the model
  if (mx1000 < 0)
    mx1000 = 0;

  // calculate seconds per ideal mile
  whPerIM = bIntercept + (signed long)mx1000 * degAmbient / 1000;
  secPerIMSteady = whPerIM * 3600L / wAvail;

  // detect implausible low power values that can lead to overflowing the number of minutes
  if ((0x7FFFL*60+30)/secPerIMSteady < 244)
    return -4;

  // ready to calculate the charge duration
  seconds = 0;

  // calculate time spent in the steady charge region
  if (imStart < imTaperBase)
    {
    int imEndSteady = imTarget < imTaperBase ? imTarget : imTaperBase;
    seconds += secPerIMSteady * (imEndSteady - imStart);
    }

  // figure out time spent in the tapered charge region
  if (imTarget > imTaperBase)
    {
    int im = imStart > imTaperBase ? imStart : imTaperBase;
    int secPerIMMost = 1117000L/(wAvail > 2000 ? 2000 : wAvail);

    for ( ; im < imTarget; ++im)
      {
      int secPerIMTaper = numTaper/(denBaseTaper - 100*im);
      if (secPerIMTaper < 0 || secPerIMTaper > secPerIMMost)
      	secPerIMTaper = secPerIMMost;
      else if (secPerIMTaper < secPerIMSteady)
        secPerIMTaper = secPerIMSteady;
      seconds += secPerIMTaper;
      }
    }

#ifdef OVMS_DIAGMODULE
  if (net_state == NET_STATE_DIAGMODE)
    {
    p = stp_i(net_scratchpad,"\r\n# TR minutestocharge result=",(seconds + 30) / 60);
    p = stp_rom(p,"\r\n");
    net_puts_ram(net_scratchpad);
    }
#endif

  return (seconds + 30) / 60;
  }

BOOL vehicle_teslaroadster_ticker1(void)
  {
  if ((car_coolingdown>=0)&&(CAR_IS_CHARGING))
    {
    if (tr_cooldown_recycle > 0)
      {
      tr_cooldown_recycle--;
      if (tr_cooldown_recycle == 10)
        {
        // Switch to PERFORMANCE mode for ten seconds
        vehicle_teslaroadster_tx_setchargemode(4);     // Switch to PERFORMANCE mode
        }
      else if (tr_cooldown_recycle == 0)
        {
        // Switch back to RANGE mode, and reset cycle
        vehicle_teslaroadster_tx_setchargemode(3);     // Switch to RANGE mode
        tr_cooldown_recycle = 60;
        }
      }
    if (car_chargelimit != 13)
      {
      vehicle_teslaroadster_tx_setchargecurrent(13); // 13A charge when cooling down
      }
    }

  return FALSE;
  }

BOOL vehicle_teslaroadster_ticker60(void)
  {
  if (car_doors1 & 0x10)
    {
    // Vehicle is charging
    car_chargefull_minsremaining = vehicle_teslaroadster_minutestocharge(
        car_chargemode,             // charge mode, Standard, Range and Performance are supported
        car_linevoltage*car_chargelimit, // watts available from the wall
        car_idealrange,             // ideal mi at start of charge (units determined by can_mileskm)
        -1,                         // ideal mi desired at end of charge (use -1 for full charge)
        100,                        // SOC percent desired at end of charge (ignored if ideal mi specified)
        car_cac100,                 // the battery pack CAC*100 (pass 0 if unknown)
        car_ambient_temp,           // ambient temperature in degrees C
        NULL
        );
    car_chargelimit_minsremaining_range = -1;
    if (car_chargelimit_rangelimit>0)
      {
      car_chargelimit_minsremaining_range = vehicle_teslaroadster_minutestocharge(
          car_chargemode,             // charge mode, Standard, Range and Performance are supported
          car_linevoltage*car_chargelimit, // watts available from the wall
          car_idealrange,             // ideal mi at start of charge (units determined by can_mileskm)
          car_chargelimit_rangelimit, // ideal mi desired at end of charge (use -1 for full charge)
          100,                        // SOC percent desired at end of charge (ignored if ideal mi specified)
          car_cac100,                 // the battery pack CAC*100 (pass 0 if unknown)
          car_ambient_temp,           // ambient temperature in degrees C
          NULL
          );
      }
    car_chargelimit_minsremaining_soc = -1;
    if (car_chargelimit_soclimit>0)
      {
      car_chargelimit_minsremaining_soc = vehicle_teslaroadster_minutestocharge(
          car_chargemode,             // charge mode, Standard, Range and Performance are supported
          car_linevoltage*car_chargelimit, // watts available from the wall
          car_idealrange,             // ideal mi at start of charge (units determined by can_mileskm)
          -1,                         // ideal mi desired at end of charge (use -1 for full charge)
          car_chargelimit_soclimit,   // SOC percent desired at end of charge (ignored if ideal mi specified)
          car_cac100,                 // the battery pack CAC*100 (pass 0 if unknown)
          car_ambient_temp,           // ambient temperature in degrees C
          NULL
          );
      }

    if (car_coolingdown>=0)
      {
      car_cooldown_timelimit--;
      vehicle_teslaroadster_tx_wakeuphvac();         // Start HVAC data
      if ((car_cooldown_timelimit == 0)||
          (car_tbattery <= car_cooldown_tbattery))
        {
        // Stop the cooldown...
        net_notify_suppresscount = 30;
        vehicle_teslaroadster_tx_setchargecurrent(car_cooldown_chargelimit); // Restore charge limit
        vehicle_teslaroadster_tx_setchargemode(car_cooldown_chargemode);     // Restore charge mode
        if (car_cooldown_wascharging == 0)
          {
          vehicle_teslaroadster_tx_startstopcharge(0);                        // Force START charge
          }
        car_coolingdown = -1;
        tr_cooldown_recycle = -1;
        }
      }
    }
  else
    {
    // Vehicle is not charging
    if (car_coolingdown>0)
      {
      net_notify_suppresscount = 30;
      vehicle_teslaroadster_tx_setchargecurrent(car_cooldown_chargelimit); // Restore charge limit
      vehicle_teslaroadster_tx_setchargemode(car_cooldown_chargemode);     // Restore charge mode
      car_coolingdown = -1;
      tr_cooldown_recycle = -1;
      }
    }
  
  return FALSE;
  }

////////////////////////////////////////////////////////////////////////
// vehicle_teslaroadster_initialise()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to initialise itself.
//
void vehicle_teslaroadster_initialise(void)
  {
  char *p;

  car_type[0] = 'T'; // Car is type TR - Tesla Roadster
  car_type[1] = 'R';
  car_type[2] = 0;
  car_type[3] = 0;
  car_type[4] = 0;

  CANCON = 0b10010000; // Initialize CAN
  while (!CANSTATbits.OPMODE2); // Wait for Configuration mode

  // We are now in Configuration Mode
  RXB0CON = 0b00000000; // RX buffer0 uses Mask RXM0 and filters RXF0, RXF1

  RXM0SIDL = 0b10100000;
  RXM0SIDH = 0b11111111;       // Set Mask0 to 0x7fd

  RXF0SIDL = 0b00000000;       // Setup Filter0 and Mask so that only CAN ID 0x100 and 0x102 will be accepted
  RXF0SIDH = 0b00100000;       // Set Filter0 to 0x100

  RXB1CON = 0b00000000;        // RX buffer1 uses Mask RXM1 and filters RXF2, RXF3, RXF4, RXF5

  RXM1SIDL = 0b11100000;
  RXM1SIDH = 0b11111111;       // Set Mask1 to 0x7ff

  RXF2SIDL = 0b10000000;       // Setup Filter2 so that CAN ID 0x344 will be accepted
  RXF2SIDH = 0b01101000;

  RXF3SIDL = 0b01000000;       // Setup Filter3 so that CAN ID 0x402 will be accepted
  RXF3SIDH = 0b10000000;

  RXF4SIDL = 0b00000000;        // Setup Filter4 so that CAN ID 0x400 will be accepted
  RXF4SIDH = 0b10000000;

  BRGCON1 = 0; // SET BAUDRATE to 1 Mbps
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

  car_cooldown_wascharging = 0;
  can_lastspeedmsg[0] = 0;
  can_lastspeedrpt = 0;
  tr_requestcac = 0;
  tr_cooldown_recycle = -1;

  net_fnbits |= NET_FN_SOCMONITOR;    // Require SOC monitor

  // Hook in...
  can_capabilities = teslaroadster_capabilities;
  vehicle_fn_poll0 = &vehicle_teslaroadster_poll0;
  vehicle_fn_poll1 = &vehicle_teslaroadster_poll1;
  vehicle_fn_ticker10th = &vehicle_teslaroadster_ticker10th;
  vehicle_fn_ticker1 = &vehicle_teslaroadster_ticker1;
  vehicle_fn_ticker60 = &vehicle_teslaroadster_ticker60;
  vehicle_fn_idlepoll = &vehicle_teslaroadster_idlepoll;
  vehicle_fn_commandhandler = &vehicle_teslaroadster_commandhandler;
  vehicle_fn_minutestocharge = &vehicle_teslaroadster_minutestocharge;
  }
