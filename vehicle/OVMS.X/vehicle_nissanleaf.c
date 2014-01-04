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

// Nissan Leaf state variables

#pragma udata overlay vehicle_overlay_data

#pragma udata

////////////////////////////////////////////////////////////////////////
// can_poll()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to poll for data.
//
BOOL vehicle_nissanleaf_poll0(void)
  {
  return TRUE;
  }

BOOL vehicle_nissanleaf_poll1(void)
  {
  switch (can_id)
    {
    case 0x280:
      if (can_mileskm=='M')
        car_speed = (((int)can_databuffer[4]<<8)+((int)can_databuffer[5]))/160;
      else
        car_speed = (((int)can_databuffer[4]<<8)+((int)can_databuffer[5]))/100;
      break;
    case 0x358:
      car_doors2bits.Headlights = (can_databuffer[0] & 0x80);
      break;
    case 0x58a:
      if ((can_databuffer[0]&0x10) != 0)
        {
        // Car is parked
        vehicle_poll_setstate(0);
        car_doors1 |= 0x40;     // PARK
        car_doors1 &= ~0x80;    // CAR OFF
        car_speed = 0;
        if (car_parktime == 0)
          {
          car_parktime = car_time-1;    // Record it as 1 second ago, so non zero report
          net_req_notification(NET_NOTIFY_ENV);
          }
        }
      else
        {
        // Car is not parked
        vehicle_poll_setstate(1);
        car_doors1 &= ~0x40;    // NOT PARK
        car_doors1 |= 0x80;     // CAR ON
        if (car_parktime != 0)
          {
          car_parktime = 0; // No longer parking
          net_req_notification(NET_NOTIFY_ENV);
          }
        }
    case 0x5b3:
      // Rough and kludgy
      car_SOC = ((((int)can_databuffer[5]&0x01)<<1) + ((int)can_databuffer[6])) / 3;
      break;
    }
  return TRUE;
  }

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_initialise()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to initialise itself.
//
BOOL vehicle_nissanleaf_initialise(void)
  {
  char *p;

  car_type[0] = 'N'; // Car is type NL - Nissan Leaf
  car_type[1] = 'L';
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
  RXB0CON = 0b00000000;

  RXM0SIDL = 0b00000000;        // Mask   11111111000
  RXM0SIDH = 0b11111111;

  RXF0SIDL = 0b00000000;        // Filter 11111101000 (0x7e8 .. 0x7ef)
  RXF0SIDH = 0b11111101;

  // Buffer 1 (filters 2, 3, 4 and 5) for direct can bus messages
  RXB1CON  = 0b00000000;	// RX buffer1 uses Mask RXM1 and filters RXF2, RXF3, RXF4, RXF5

  RXM1SIDL = 0b00000000;
  RXM1SIDH = 0b11100000;	// Set Mask1 to 11100000000

  RXF2SIDL = 0b00000000;	// Setup Filter2 so that CAN ID 0x500 - 0x5FF will be accepted
  RXF2SIDH = 0b10100000;

  RXF3SIDL = 0b00000000;	// Setup Filter3 so that CAN ID 0x200 - 0x2FF will be accepted
  RXF3SIDH = 0b01000000;

  RXF4SIDL = 0b00000000;  // Setup Filter4 so that CAN ID 0x300 - 0x3FF will be accepted
  RXF4SIDH = 0b01100000;

  RXF5SIDL = 0b00000000;  // Setup Filter5 so that CAN ID 0x000 - 0x0FF will be accepted
  RXF5SIDH = 0b00000000;

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
  vehicle_fn_poll0 = &vehicle_nissanleaf_poll0;
  vehicle_fn_poll1 = &vehicle_nissanleaf_poll1;

  net_fnbits |= NET_FN_INTERNALGPS;   // Require internal GPS
  net_fnbits |= NET_FN_12VMONITOR;    // Require 12v monitor
  net_fnbits |= NET_FN_SOCMONITOR;    // Require SOC monitor

  return TRUE;
  }

