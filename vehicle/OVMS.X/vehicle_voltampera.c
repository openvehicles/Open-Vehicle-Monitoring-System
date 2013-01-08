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

// Volt/Ampera state variables

#pragma udata overlay vehicle_overlay_data
unsigned int soc_largest  = 56028;
unsigned int soc_smallest = 13524;
BOOL bus_is_active = FALSE;       // Indicates recent activity on the bus

#pragma udata

////////////////////////////////////////////////////////////////////////
// vehicle_voltampera_polls
// This rom table records the extended PIDs that need to be polled

rom struct
  {
  unsigned int moduleid;
  unsigned char requestid;
  unsigned char polltime;
  unsigned int pid1;
  unsigned int pid2;
  unsigned int pid3;
  } vehicle_voltampera_polls[]
  =
  {
    { 0x07E0, 0xA0, 10, 0x0046, 0x0000, 0x0000 },
    { 0x0000, 0x00, 0,  0x0000, 0x0000, 0x0000 }
  };

////////////////////////////////////////////////////////////////////////
// vehicle_voltampera_ticker1()
// This function is an entry point from the main() program loop, and
// gives the CAN framework a ticker call approximately once per second
//
BOOL vehicle_voltampera_ticker1(void)
  {
  int k;

  // bus_is_active indicates we've recently seen a message on the can bus
  // Quick exit if bus is recently not active
  if (!bus_is_active) return FALSE;

  // Also, we need CAN_WRITE enabled, so return if not
  if (sys_features[FEATURE_CANWRITE]==0) return FALSE;

  // Let's run through and see if we have to poll for any data..
  for (k=0;vehicle_voltampera_polls[k].moduleid != 0; k++)
    {
    if ((can_granular_tick % vehicle_voltampera_polls[k].polltime) == 0)
      {
      // OK. Let's send it...
      while (TXB0CONbits.TXREQ) {} // Loop until TX is done
      if ((vehicle_voltampera_polls[k].pid3==0)&&
          (vehicle_voltampera_polls[k].pid2==0))
        { // Single PID request...
        TXB0CON = 0;
        TXB0SIDL = (vehicle_voltampera_polls[k].moduleid & 0x07)<<5;
        TXB0SIDH = (vehicle_voltampera_polls[k].moduleid>>3);
        TXB0D0 = 0x04;
        TXB0D1 = 0x2C;        // Get extended PID
        TXB0D2 = vehicle_voltampera_polls[k].requestid;
        TXB0D3 = vehicle_voltampera_polls[k].pid1 >> 8;
        TXB0D4 = vehicle_voltampera_polls[k].pid1 & 0xff;
        TXB0D5 = 0x00;
        TXB0D6 = 0x00;
        TXB0D7 = 0x00;
        TXB0DLC = 0b00001000; // data length (8)
        TXB0CON = 0b00001000; // mark for transmission
        }
      else if (vehicle_voltampera_polls[k].pid3==0)
        { // Dual PID request...
        TXB0CON = 0;
        TXB0SIDL = (vehicle_voltampera_polls[k].moduleid & 0x07)<<5;
        TXB0SIDH = (vehicle_voltampera_polls[k].moduleid>>3);
        TXB0D0 = 0x06;
        TXB0D1 = 0x2C;        // Get extended PID
        TXB0D2 = vehicle_voltampera_polls[k].requestid;
        TXB0D3 = vehicle_voltampera_polls[k].pid1 >> 8;
        TXB0D4 = vehicle_voltampera_polls[k].pid1 & 0xff;
        TXB0D5 = vehicle_voltampera_polls[k].pid2 >> 8;
        TXB0D6 = vehicle_voltampera_polls[k].pid2 & 0xff;
        TXB0D7 = 0x00;
        TXB0DLC = 0b00001000; // data length (8)
        TXB0CON = 0b00001000; // mark for transmission
        }
      else
        { // Triple PID request...
        TXB0CON = 0;
        TXB0SIDL = (vehicle_voltampera_polls[k].moduleid & 0x07)<<5;
        TXB0SIDH = (vehicle_voltampera_polls[k].moduleid>>3);
        TXB0D0 = 0x10;
        TXB0D1 = 0x08;
        TXB0D2 = 0x2C;        // Get extended PID
        TXB0D3 = vehicle_voltampera_polls[k].requestid;
        TXB0D4 = vehicle_voltampera_polls[k].pid1 >> 8;
        TXB0D5 = vehicle_voltampera_polls[k].pid1 & 0xff;
        TXB0D6 = vehicle_voltampera_polls[k].pid2 >> 8;
        TXB0D7 = vehicle_voltampera_polls[k].pid2 & 0xff;
        TXB0DLC = 0b00001000; // data length (8)
        TXB0CON = 0b00001000; // mark for transmission
        while (TXB0CONbits.TXREQ) {} // Loop until TX is done
        TXB0CON = 0;
        TXB0SIDL = (vehicle_voltampera_polls[k].moduleid & 0x07)<<5;
        TXB0SIDH = (vehicle_voltampera_polls[k].moduleid>>3);
        TXB0D0 = 0x21;
        TXB0D1 = vehicle_voltampera_polls[k].pid3 >> 8;
        TXB0D2 = vehicle_voltampera_polls[k].pid3 & 0xff;
        TXB0D3 = 0x00;
        TXB0D4 = 0x00;
        TXB0D5 = 0x00;
        TXB0D6 = 0x00;
        TXB0D7 = 0x00;
        TXB0DLC = 0b00001000; // data length (8)
        TXB0CON = 0b00001000; // mark for transmission
        while (TXB0CONbits.TXREQ) {} // Loop until TX is done
        }
      while (TXB0CONbits.TXREQ) {} // Loop until TX is done
      TXB0CON = 0;
      TXB0SIDL = (vehicle_voltampera_polls[k].moduleid & 0x07)<<5;
      TXB0SIDH = (vehicle_voltampera_polls[k].moduleid>>3);
      TXB0D0 = 0x03;
      TXB0D1 = 0xAA;
      TXB0D2 = 0x01; // One shot
      TXB0D3 = vehicle_voltampera_polls[k].requestid;
      TXB0D4 = 0x00;
      TXB0D5 = 0x00;
      TXB0D6 = 0x00;
      TXB0D7 = 0x00;
      TXB0DLC = 0b00001000; // data length (8)
      TXB0CON = 0b00001000; // mark for transmission
      }
    }
  // Assume the bus is not active, so we won't poll any more until we see
  // activity on the bus
  bus_is_active = FALSE;
  return FALSE;
  }

////////////////////////////////////////////////////////////////////////
// can_poll()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to poll for data.
//
BOOL vehicle_voltampera_poll0(void)
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

  return TRUE;
  }

BOOL vehicle_voltampera_poll1(void)
  {
  unsigned char CANctrl;
  unsigned char k;

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

  bus_is_active = TRUE; // Activity has been seen on the bus

  if ((CANctrl & 0x07) == 2)             // Acceptance Filter 2 (RXF2) = CAN ID 0x206
    {
    // SOC
    // For the SOC, each 4,000 is 1kWh. Assuming a 16.1kWh battery, 1% SOC is 644 decimal bytes
    // The SOC itself is d1<<8 + d2
    unsigned int v = (can_databuffer[1]+((unsigned int) can_databuffer[0] << 8));
    if ((v<soc_smallest)&&(v>0)) v=soc_smallest;
    if (v>soc_largest) v=soc_largest;
    car_SOC = (char)((v-soc_smallest)/((soc_largest-soc_smallest)/100));
    }
  else if ((CANctrl & 0x07) == 3)        // Acceptance Filter 3 (RXF3) = CAN ID 4E1
    {
    // The VIN can be constructed by taking the number "1" and converting the CAN IDs 4E1 and 514 to ASCII.
    // So with "4E1 4255313032363839" and "514 4731524436453436",
    // you would end up with 42 55 31 30 32 36 38 39 47 31 52 44 36 45 34 36,
    // where 42 is ASCII for B, 55 is U, etc.
    for (k=0;k<8;k++)
      car_vin[k+9] = can_databuffer[k];
    car_vin[17] = 0;
    }
  else if ((CANctrl & 0x07) == 4)        // Acceptance Filter 4 (RXF4) = CAN ID 514
    {
    car_vin[0] = '1';
    for (k=0;k<8;k++)
      car_vin[k+1] = can_databuffer[k];
    }
  else if ((CANctrl & 0x07) == 5)       // Acceptance Filter 5 (RXF5) = CAN ID 5E8
    {
    if (can_databuffer[0] == 0xA0)
      {
      car_ambient_temp = (can_databuffer[1]>>1)-40;
      }
    }
  return TRUE;
  }

////////////////////////////////////////////////////////////////////////
// vehicle_voltampera_initialise()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to initialise itself.
//
BOOL vehicle_voltampera_initialise(void)
  {
  char *p;

  car_type[0] = 'V'; // Car is type VA - Volt/Ampera
  car_type[1] = 'A';
  car_type[2] = 0;
  car_type[3] = 0;
  car_type[4] = 0;

  CANCON = 0b10010000; // Initialize CAN
  while (!CANSTATbits.OPMODE2); // Wait for Configuration mode

  // We are now in Configuration Mode
  RXB0CON = 0b00000000; // RX buffer0 uses Mask RXM0 and filters RXF0, RXF1

  RXM0SIDL = 0b10100000;
  RXM0SIDH = 0b11111111;	// Set Mask0 to 0x7fd

  // Not yet used for VA
  RXF0SIDL = 0b00000000;	// Setup Filter0 and Mask so that only CAN ID 0x100 and 0x102 will be accepted
  RXF0SIDH = 0b00100000;	// Set Filter0 to 0x100

  RXB1CON = 0b00000000;	// RX buffer1 uses Mask RXM1 and filters RXF2, RXF3, RXF4, RXF5

  RXM1SIDL = 0b11100000;
  RXM1SIDH = 0b11111111;	// Set Mask1 to 0x7ff

  // Filters: low byte bits 7..5 are the low 3 bits of the filter, and bits 4..0 are all zeros
  //          high byte bits 7..0 are the high 8 bits of the filter
  //          So total is 11 bits

  // N.B. This is a very wasteful use of filters, but good enough for proof-of-concept
  //      Final implementation will most likely have to use masks to better affect

  // Filter 2: Used for ID#0x206 (SOC)
  RXF2SIDL = 0b11000000;	// Setup Filter2 so that CAN ID 0x206 will be accepted
  RXF2SIDH = 0b01000000;

  // Filter 3: Used for ID#0x4E1
  RXF3SIDL = 0b00100000;	// Setup Filter3 so that CAN ID 0x4E1 will be accepted
  RXF3SIDH = 0b10011100;

  // Filter 4: Used for ID#0x514
  RXF4SIDL = 0b10000000;        // Setup Filter4 so that CAN ID 0x514 will be accepted
  RXF4SIDH = 0b10100010;

  // Filter 5: Used for ID#0x5E8
  RXF5SIDL = 0b00000000;        // Setup Filter5 so that CAN ID 0x5E8 will be accepted
  RXF5SIDH = 0b10111101;

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
  vehicle_fn_poll0 = &vehicle_voltampera_poll0;
  vehicle_fn_poll1 = &vehicle_voltampera_poll1;
  vehicle_fn_ticker1 = &vehicle_voltampera_ticker1;
  
  net_fnbits |= NET_FN_INTERNALGPS;   // Require internal GPS

  return TRUE;
  }
