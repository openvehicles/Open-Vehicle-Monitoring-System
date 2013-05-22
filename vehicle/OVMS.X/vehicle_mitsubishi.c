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
#include "net_msg.h"

// Mitsubishi state variables

#pragma udata overlay vehicle_overlay_data

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

  return TRUE;
  }

BOOL vehicle_mitsubishi_poll1(void)
  {
  unsigned int id = ((unsigned int)RXB1SIDL >>5)
                  + ((unsigned int)RXB1SIDH <<3);
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

  CANctrl=RXB1CON;              // copy CAN RX1 Control register
  RXB1CONbits.RXFUL = 0;        // All bytes read, Clear flag


  if ((CANctrl & 0x07) == 2)             // Acceptance Filter 2 (RXF2) = CAN ID 373
    {
    // BatCurr & BatVolt
    }
  else if ((CANctrl & 0x07) == 3)        // Acceptance Filter 3 (RXF3) = CAN ID 374
    {
    // SOC
      car_SOC = (char)(((int)can_databuffer[1] - 10) / 2);
    }
  else if ((CANctrl & 0x07) == 4)        // Acceptance Filter 3 (RXF3) = CAN ID 412
    {
    // Speed & Odo
      if (can_mileskm == 'K')
        car_speed = can_databuffer[1];
      else
        car_speed = (unsigned char) ((((unsigned long)can_databuffer[1] * 1000)+500)/1609);
    }
  else if ((CANctrl & 0x07) == 5)        // Acceptance Filter 3 (RXF3) = CAN ID 346
    {
    // Range
      car_estrange = (unsigned int)can_databuffer[7];
      car_idealrange = car_estrange;
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

  car_type[0] = 'M'; // Car is type MI - Mitsubishi iMiev
  car_type[1] = 'I';
  car_type[2] = 0;
  car_type[3] = 0;
  car_type[4] = 0;

  // Vehicle specific data initialisation
  car_stale_timer = -1; // Timed charging is not supported for OVMS MI
  car_time = 0;

  CANCON = 0b10010000; // Initialize CAN
  while (!CANSTATbits.OPMODE2); // Wait for Configuration mode

  // We are now in Configuration Mode

  // Filters: low byte bits 7..5 are the low 3 bits of the filter, and bits 4..0 are all zeros
  //          high byte bits 7..0 are the high 8 bits of the filter
  //          So total is 11 bits

  // Buffer 0 (filters 0, 1) for extended PID responses
  RXB0CON = 0b00000000;

  RXM0SIDL = 0b00000000;        // Mask   11111111000
  RXM0SIDH = 0b11111111;

  RXF0SIDL = 0b00000000;        // Filter 11111101000 (0x7e8 .. 0x7ef)
  RXF0SIDH = 0b11111101;


  // Buffer 1 (filters 2, 3, 4 and 5) for direct can bus messages
  RXB1CON  = 0b00000000;	// RX buffer1 uses Mask RXM1 and filters RXF2, RXF3, RXF4, RXF5

  RXM1SIDL = 0b11100000;
  RXM1SIDH = 0b11111111;	// Set Mask1 to 0x7ff

  RXF2SIDL = 0b01100000;	// Setup Filter2 so that CAN ID 0x373 will be accepted
  RXF2SIDH = 0b01101110;

  RXF3SIDL = 0b10000000;	// Setup Filter3 so that CAN ID 0x374 will be accepted
  RXF3SIDH = 0b01101110;

  RXF4SIDL = 0b01000000;        // Setup Filter4 so that CAN ID 0x412 will be accepted
  RXF4SIDH = 0b10000010;

  RXF5SIDL = 0b11000000;        // Setup Filter5 so that CAN ID 0x346 will be accepted
  RXF5SIDH = 0b01101000;

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
