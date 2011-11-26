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

#include "ovms.h"

unsigned char can_datalength;                // The number of valid bytes in the can_databuffer
unsigned char can_databuffer[8];             // A buffer to store the current CAN message

////////////////////////////////////////////////////////////////////////
// can_initialise()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to initialise itself.
//
void can_initialise(void)
  {
  CANCON = 0b10010000; // Initialize CAN
  while (!CANSTATbits.OPMODE2); // Wait for Configuration mode

  // We are now in Configuration Mode
  RXB0CON = 0b00000000; // Receive all valid messages

  RXF0SIDL = 0b00000000; // Setup Filter and Mask so that only CAN ID 0x100 will be accepted
  RXF0SIDH = 0b00100000; // Set Filter to 0x100

  RXM0SIDL = 0b11100000;
  RXM0SIDH = 0b11111111; // Set Mask to 0x7ff

  BRGCON1 = 0; // SET BAUDRATE to 1 Mbps
  BRGCON2 = 0xD2;
  BRGCON3 = 0x02;

  CIOCON = 0b00100000; // CANTX pin will drive VDD when recessive
#ifdef OVMS_CAN_WRITE
  CANCON = 0b00000000;  // Normal mode
#else
  CANCON = 0b01100000; // Listen only mode, Receive bufer 0
#endif // #ifdef OVMS_CAN_WRITE
  }

////////////////////////////////////////////////////////////////////////
// can_poll()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to poll for data.
//
void can_poll(void)
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
    case 0x80: // Range / State of Charge
      car_SOC = can_databuffer[1];
      car_idealrange = can_databuffer[2]+(can_databuffer[3]*256);
      car_estrange = can_databuffer[6]+(can_databuffer[7]*256);
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
      car_speed = can_databuffer[1];     // speed in miles/hour
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
      car_doors = can_databuffer[1]; // Doors
                                     // bit0 left door opened
                                     // bit1 right door opened
                                     // bit2 charge door opened
                                     // bit4 charging 
      break;
    case 0xA4: // 7 VIN bytes i.e. "SFZRE2B"
    case 0xA5: // 7 VIN bytes i.e. "39A3000"
    case 0xA6: // 3 VIN bytes i.e. "359"
      break;
    }
  }

////////////////////////////////////////////////////////////////////////
// can_ticker()
// This function is an entry point from the main() program loop, and
// gives the CAN framework a ticker call approximately once per second.
//
void can_ticker(void)
  {
  // This ticker is called once every second
  }
