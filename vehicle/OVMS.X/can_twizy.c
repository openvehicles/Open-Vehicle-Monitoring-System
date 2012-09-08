/*
;    Project:       Open Vehicle Monitor System
;    Date:          6 May 2012
;
;    Changes:
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

  car_type[0] = 'R'; // Car is type RT - Renault Twizy
  car_type[1] = 'T';
  car_type[2] = 0;

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
  }

void can_idlepoll(void)
  {
  }

void can_tx_wakeup(void)
  {
  }

void can_tx_wakeuptemps(void)
  {
  }

void can_tx_setchargemode(unsigned char mode)
  {
  }

void can_tx_setchargecurrent(unsigned char current)
  {
  }

void can_tx_startstopcharge(unsigned char start)
  {
  }

void can_tx_lockunlockcar(unsigned char mode, char *pin)
  {
  }

void can_tx_timermode(unsigned char mode, unsigned int starttime)
  {
  }

void can_tx_homelink(unsigned char button)
  {
  }
