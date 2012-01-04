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

  CANCON = 0b10010000; // Initialize CAN
  while (!CANSTATbits.OPMODE2); // Wait for Configuration mode

  // We are now in Configuration Mode
  RXB0CON = 0b00000000; // RX buffer0 uses Mask RXM0 and filters RXF0, RXF1

  RXM0SIDL = 0b10100000;
  RXM0SIDH = 0b11111111;	// Set Mask0 to 0x7fd

  RXF0SIDL = 0b00000000;	// Setup Filter0 and Mask so that only CAN ID 0x100 and 0x102 will be accepted
  RXF0SIDH = 0b00100000;	// Set Filter0 to 0x100

  RXB1CON = 0b00000000;	// RX buffer1 uses Mask RXM1 and filters RXF2, RXF3, RXF4, RXF5

  RXM1SIDL = 0b11100000;
  RXM1SIDH = 0b11111111;	// Set Mask1 to 0x7ff

  RXF2SIDL = 0b10000000;	// Setup Filter2 so that CAN ID 0x344 will be accepted
  RXF2SIDH = 0b01101000;

  RXF3SIDL = 0b01000000;	// Setup Filter3 so that CAN ID 0x402 will be accepted
  RXF3SIDH = 0b10000000;

  RXF4SIDL = 0b00000000;        // Setup Filter4 so that CAN ID 0x400 will be accepted
  RXF4SIDH = 0b10000000;

  BRGCON1 = 0; // SET BAUDRATE to 1 Mbps
  BRGCON2 = 0xD2;
  BRGCON3 = 0x02;

  CIOCON = 0b00100000; // CANTX pin will drive VDD when recessive
#ifdef OVMS_CAN_WRITE
  CANCON = 0b00000000;  // Normal mode
#else
  CANCON = 0b01100000; // Listen only mode, Receive bufer 0
#endif // #ifdef OVMS_CAN_WRITE

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

  switch (can_databuffer[0])
    {
    case 0x0E: // Lock/Unlock state on ID#102
      if (car_lockstate != can_databuffer[1])
        net_notify_environment();
      car_lockstate = can_databuffer[1];
      break;
    case 0x80: // Range / State of Charge
      car_SOC = can_databuffer[1];
      car_idealrange = can_databuffer[2]+((unsigned int) can_databuffer[3] << 8);
      car_estrange = can_databuffer[6]+((unsigned int) can_databuffer[7] << 8);
      break;
    case 0x81: // Time/ Date UTC
      car_time = can_databuffer[4]
                 + ((unsigned long) can_databuffer[5] << 8)
                 + ((unsigned long) can_databuffer[6] << 16)
                 + ((unsigned long) can_databuffer[7] << 24);
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
    case 0x88: // Charging Current / Duration
      car_chargecurrent = can_databuffer[1];
      break;
    case 0x89: // Charging Voltage / Iavailable
      if (can_mileskm=='M')
        car_speed = can_databuffer[1];     // speed in miles/hour
      else
        car_speed = (unsigned char)(((float)can_databuffer[1]*1.609)+0.5);     // speed in km/hour
      car_linevoltage = can_databuffer[2]
                        + ((unsigned int) can_databuffer[3] << 8);
      break;
    case 0x95: // Charging mode
      car_chargestate = can_databuffer[1];
#ifdef OVMS_CAR_2010
      car_chargemode = (can_databuffer[5] >> 4) & 0x0F; // for 2010 roadsters
#else
      car_chargemode = (can_databuffer[4]) & 0x0F;  // for 2008 roadsters
#endif // #ifdef OVMS_CAR_2010
      if (car_stopped) // Stopped charging?
        {
        car_stopped = 0;
        net_notify_status();
        }
      break;
    case 0x96: // Doors / Charging yes/no
      if ((car_charging) && !(can_databuffer[1] & 0x10)) // Already Charging? Stopped?
        {
        car_stopped = 1; // Yes Roadster stopped charging, set flag.
        }
      car_charging = (can_databuffer[1] >> 4) & 0x01; //Charging status
      if ((car_doors1 != can_databuffer[1])||
          (car_doors2 != can_databuffer[2]))
        net_notify_environment();
      car_doors1 = can_databuffer[1]; // Doors #1
      car_doors2 = can_databuffer[2]; // Doors #2
      if (((car_doors1 & 0x80)==0)&&  // Car is not ON
          (car_parktime == 0)&&       // Parktime was not previously set
          (car_time != 0))            // We know the car time
        {
        car_parktime = car_time-1;    // Record it as 1 second ago, so non zero report
        net_notify_environment();
        }
      else if ((car_doors1 & 0x80)&&  // Car is ON
               (car_parktime != 0))   // Parktime was previously set
        {
        car_parktime = 0;
        net_notify_environment();
        }
      break;
    case 0xA3: // Temperatures
      car_tpem = can_databuffer[1]; // Tpem
      car_tmotor = can_databuffer[2]; // Tmotor
      car_tbattery = can_databuffer[6]; // Tbattery
      break;
    case 0xA4: // 7 VIN bytes i.e. "SFZRE2B"
      for (k=0;k<7;k++)
        car_vin[k] = can_databuffer[k+1];
      break;
    case 0xA5: // 7 VIN bytes i.e. "39A3000"
      for (k=0;k<7;k++)
        car_vin[k+7] = can_databuffer[k+1];
      break;
    case 0xA6: // 3 VIN bytes i.e. "359"
      car_vin[14] = can_databuffer[1];
      car_vin[15] = can_databuffer[2];
      car_vin[16] = can_databuffer[3];
      break;
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
    // Experimental speedometer feature - replace Range->Dash with speed
    if ((can_databuffer[0]==0x02)&&         // The SPEEDO AMPS message
        (sys_features[FEATURE_SPEEDO]>0)&&  // The SPEEDO feature is on
        (car_doors1 & 0x80)&&               // The car is on
        (car_speed != can_databuffer[2]))   // The speed != Amps
      {
#ifdef OVMS_CAN_WRITE
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
#endif // #ifdef OVMS_CAN_WRITE
      }
    }
  else if ((CANctrl & 0x07) == 2)    	// Acceptance Filter 2 (RXF2) = CAN ID 344
    {
    // TPMS code here
    if (can_databuffer[3]>0) // front-right
      {
      car_tpms_p[0] = can_databuffer[2];
      car_tpms_t[0] = can_databuffer[3];
      }
    if (can_databuffer[7]>0) // rear-right
      {
      car_tpms_p[1] = can_databuffer[6];
      car_tpms_t[1] = can_databuffer[7];
      }
    if (can_databuffer[1]>0) // front-left
      {
      car_tpms_p[2] = can_databuffer[0];
      car_tpms_t[2] = can_databuffer[1];
      }
    if (can_databuffer[5]>0)
      {
      car_tpms_p[3] = can_databuffer[4];
      car_tpms_t[3] = can_databuffer[5];
      }
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
  minSOC = atoi(par_get(PARAM_MINSOC));
  if ((can_minSOCnotified == 0) && (car_SOC < minSOC))
    {
    net_notify_status();
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
  if (can_lastspeedrpt==0) can_lastspeedrpt=sys_features[FEATURE_SPEEDO];
  }

void can_idlepoll(void)
  {
  if (can_lastspeedrpt == 0) return;

#ifdef OVMS_CAN_WRITE
  // Experimental speedometer feature - replace Range->Dash with speed
  if ((can_lastspeedmsg[0]==0x02)&&        // It is a valid AMPS message
      (sys_features[FEATURE_SPEEDO]>0)&&   // The SPEEDO feature is enabled
      (car_doors1 & 0x80))                 // The car is on
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
#endif // #ifdef OVMS_CAN_WRITE
  can_lastspeedrpt--;
  }
