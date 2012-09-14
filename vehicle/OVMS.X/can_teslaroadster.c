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
#include "ovms.h"
#include "params.h"

#pragma udata
unsigned char can_datalength;                // The number of valid bytes in the can_databuffer
unsigned char can_databuffer[8];             // A buffer to store the current CAN message
unsigned char can_lastspeedmsg[8];           // A buffer to store the last speed message
unsigned char can_lastspeedrpt;              // A mechanism to repeat the tx of last speed message
unsigned char k;
unsigned char can_minSOCnotified = 0;        // minSOC notified flag
unsigned int  can_granular_tick = 0;         // An internal ticker used to generate 1min, 5min, etc, calls
unsigned char can_mileskm = 'M';             // Miles of Kilometers
#pragma udata

////////////////////////////////////////////////////////////////////////
// CAN Interrupt Service Routine (High Priority)
//
// Interupts here will interrupt Uart Interrupts
//

void high_isr(void);

#pragma code can_int_service = 0x08
void can_int_service(void)
  {
  _asm goto high_isr _endasm
  }

#pragma code
#pragma	interrupt high_isr
void high_isr(void)
  {
  // High priority CAN interrupt
    if (RXB0CONbits.RXFUL) can_poll0();
    if (RXB1CONbits.RXFUL) can_poll1();
    PIR3=0;     // Clear Interrupt flags
  }

////////////////////////////////////////////////////////////////////////
// can_initialise()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to initialise itself.
//
void can_initialise(void)
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

  RCONbits.IPEN = 1; // Enable Interrupt Priority
  PIE3bits.RXB1IE = 1; // CAN Receive Buffer 1 Interrupt Enable bit
  PIE3bits.RXB0IE = 1; // CAN Receive Buffer 0 Interrupt Enable bit
  IPR3 = 0b00000011; // high priority interrupts for Buffers 0 and 1

  p = par_get(PARAM_MILESKM);
  can_mileskm = *p;
  can_lastspeedmsg[0] = 0;
  can_lastspeedrpt = 0;
  }

////////////////////////////////////////////////////////////////////////
// can_poll()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to poll for data.
//
void can_poll0(void)                // CAN ID 100 and 102
  {
  unsigned char CANsidl = RXB0SIDL & 0b11100000;

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

  if (CANsidl == 0)
    { // CAN ID 0x100
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
          net_req_notification(NET_NOTIFY_ENV);
          }
        car_chargestate = can_databuffer[1];
        car_chargesubstate = can_databuffer[2];
        if (sys_features[FEATURE_CARBITS]&FEATURE_CB_2008) // A 2010+ roadster?
          car_chargemode = (can_databuffer[4]) & 0x0F;  // for 2008 roadsters
        else
          car_chargemode = (can_databuffer[5] >> 4) & 0x0F; // for 2010 roadsters
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

        if (((car_doors2&0x40)==0)&&(can_databuffer[2]&0x40)&&(can_databuffer[2]&0x10))
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
          car_parktime = car_time-1;    // Record it as 1 second ago, so non zero report
          net_req_notification(NET_NOTIFY_ENV);
          }
        else if ((car_doors1 & 0x80)&&  // Car is ON
                 (car_parktime != 0))   // Parktime was previously set
          {
          car_parktime = 0;
          net_req_notification(NET_NOTIFY_ENV);
          }
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
  else
    { // CAN id 0x102
    switch (can_databuffer[0])
      {
      case 0x0E: // Lock/Unlock state on ID#102
        if (car_lockstate != can_databuffer[1])
          net_req_notification(NET_NOTIFY_ENV);
        car_lockstate = can_databuffer[1];
        break;
      }
    }
  }

void can_poll1(void)                // CAN ID 344 and 402
{
  unsigned char CANctrl;

  can_datalength = RXB1DLC & 0x0F; // number of received bytes
  can_databuffer[0] = RXB1D0;
  can_databuffer[1] = RXB1D1;
  can_databuffer[2] = RXB1D2;
  can_databuffer[3] = RXB1D3;
  can_databuffer[4] = RXB1D4;
  can_databuffer[5] = RXB1D5;
  can_databuffer[6] = RXB1D6;
  can_databuffer[7] = RXB1D7;

  CANctrl=RXB1CON;		// copy CAN RX1 Control register
  RXB1CONbits.RXFUL = 0; // All bytes read, Clear flag

  if ((CANctrl & 0x07) == 4)           // Acceptance Filter 4 (RXF4) = CAN ID 400
    {
#ifdef OVMS_SPEEDO_EXPERIMENT
    // Experimental speedometer feature - replace Range->Dash with speed
    if ((can_databuffer[0]==0x02)&&         // The SPEEDO AMPS message
        (sys_features[FEATURE_SPEEDO]>0)&&  // The SPEEDO feature is on
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
      can_lastspeedrpt = sys_features[FEATURE_SPEEDO]; // Force re-transmissions
      }
#endif // #ifdef OVMS_SPEEDO_EXPERIMENT
    }
  else if ((CANctrl & 0x07) == 2)    	// Acceptance Filter 2 (RXF2) = CAN ID 344
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
  else  				// It must be CAN ID 402
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
}

////////////////////////////////////////////////////////////////////////
// can_state_ticker1()
// State Model: Per-second ticker
// This function is called approximately once per second, and gives
// the state a timeslice for activity.
//
void can_state_ticker1(void)
  {
  if (car_stale_ambient>0) car_stale_ambient--;
  if (car_stale_temps>0)   car_stale_temps--;
  if (car_stale_gps>0)     car_stale_gps--;
  if (car_stale_tpms>0)    car_stale_tpms--;
  }

////////////////////////////////////////////////////////////////////////
// can_state_ticker60()
// State Model: Per-minute ticker
// This function is called approximately once per minute (since state
// was first entered), and gives the state a timeslice for activity.
//
void can_state_ticker60(void)
  {
  int minSOC;

  // check minSOC
  minSOC = sys_features[FEATURE_MINSOC];
  if ((can_minSOCnotified == 0) && (car_SOC < minSOC))
    {
    net_req_notification(NET_NOTIFY_STAT);
    can_minSOCnotified = 1;
    }
  else if ((can_minSOCnotified == 1) && (car_SOC > minSOC + 2))
    {
    // reset the alert sent flag when SOC is 2% point higher than threshold
    can_minSOCnotified = 0;
    }
  }

////////////////////////////////////////////////////////////////////////
// can_state_ticker300()
// State Model: Per-5-minute ticker
// This function is called approximately once per five minutes (since
// state was first entered), and gives the state a timeslice for activity.
//
void can_state_ticker300(void)
  {
  }

////////////////////////////////////////////////////////////////////////
// can_state_ticker600()
// State Model: Per-10-minute ticker
// This function is called approximately once per ten minutes (since
// state was first entered), and gives the state a timeslice for activity.
//
void can_state_ticker600(void)
  {
  }

////////////////////////////////////////////////////////////////////////
// can_ticker()
// This function is an entry point from the main() program loop, and
// gives the CAN framework a ticker call approximately once per second.
//
void can_ticker(void)
  {
  // This ticker is called once every second
  can_granular_tick++;
  can_state_ticker1();
  if ((can_granular_tick % 60)==0)
    can_state_ticker60();
  if ((can_granular_tick % 300)==0)
    can_state_ticker300();
  if ((can_granular_tick % 600)==0)
    {
    can_state_ticker600();
    can_granular_tick -= 600;
    }
  }

////////////////////////////////////////////////////////////////////////
// can_ticker10th()
// This function is an entry point from the main() program loop, and
// gives the CAN framework a ticker call approximately ten times per
// second.
//
void can_ticker10th(void)
  {
#ifdef OVMS_SPEEDO_EXPERIMENT
  if (can_lastspeedrpt==0) can_lastspeedrpt=sys_features[FEATURE_SPEEDO];
#endif // #ifdef OVMS_SPEEDO_EXPERIMENT
  }

void can_idlepoll(void)
  {
  if (can_lastspeedrpt == 0) return;

#ifdef OVMS_SPEEDO_EXPERIMENT
  // Experimental speedometer feature - replace Range->Dash with speed
  if ((can_lastspeedmsg[0]==0x02)&&        // It is a valid AMPS message
      (sys_features[FEATURE_SPEEDO]>0)&&   // The SPEEDO feature is enabled
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
#endif //#ifdef OVMS_SPEEDO_EXPERIMENT
  can_lastspeedrpt--;
  }

void can_tx_wakeup(void)
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

void can_tx_wakeuptemps(void)
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

void can_tx_setchargemode(unsigned char mode)
  {
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

  can_tx_wakeup(); // Also, wakeup the car if necessary
  }

void can_tx_setchargecurrent(unsigned char current)
  {
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

  can_tx_wakeup(); // Also, wakeup the car if necessary
  }

void can_tx_startstopcharge(unsigned char start)
  {
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

  can_tx_wakeup(); // Also, wakeup the car if necessary
  }

void can_tx_lockunlockcar(unsigned char mode, char *pin)
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

void can_tx_timermode(unsigned char mode, unsigned int starttime)
  {
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

  can_tx_wakeup(); // Also, wakeup the car if necessary
  }

void can_tx_homelink(unsigned char button)
  {
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

  can_tx_wakeup(); // Also, wakeup the car if necessary
  }
