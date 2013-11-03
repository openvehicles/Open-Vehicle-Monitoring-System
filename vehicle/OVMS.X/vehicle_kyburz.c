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

// Kyburz state variables

#pragma udata overlay vehicle_overlay_data
unsigned char kd_candata_timer;  // A per-second timer for CAN bus data
unsigned char kd_charge_timer;   // A per-second charge timer
unsigned long kd_charge_wm;      // A per-minute watt accumulator
BOOL kd_bus_is_active;           // Indicates recent activity on the bus

#pragma udata

////////////////////////////////////////////////////////////////////////
// vehicle_kyburz_polls
// This rom table records the extended PIDs that need to be polled

rom struct
  {
  unsigned int moduleid;
  unsigned char polltime;
  unsigned int pid;
  } vehicle_kyburz_polls[]
  =
  {
    { 0x0626, 10, 0x3405 },  // SOC
    { 0x0626, 60, 0x33E7 },  // Charging current
    { 0x0000, 0,  0x0000 }
  };

////////////////////////////////////////////////////////////////////////
// vehicle_kyburz_ticker1()
// This function is an entry point from the main() program loop, and
// gives the CAN framework a ticker call approximately once per second
//
BOOL vehicle_kyburz_ticker1(void)
  {
  int k;
  BOOL doneone = FALSE;

  if (kd_candata_timer>0)
    {
    if (--kd_candata_timer == 0)
      { // Car has gone to sleep
      car_doors3 &= ~0x01;      // Car is asleep
      }
    else
      {
      car_doors3 |= 0x01;       // Car is awake
      }
    }
  car_time++;

  ////////////////////////////////////////////////////////////////////////
  // Charge state determination
  ////////////////////////////////////////////////////////////////////////

  if ((car_chargecurrent!=0)&&(car_linevoltage!=0))
    { // CAN says the car is charging
    if ((car_doors1 & 0x08)!=0)
      { // Charge is ongoing
      kd_charge_timer++;
      if (kd_charge_timer>=60)
        { // One minute has passed
        kd_charge_timer=0;

        car_chargeduration++;
        kd_charge_wm += (car_chargecurrent*car_linevoltage);
        if (kd_charge_wm >= 60000L)
          {
          // Let's move 1kWh to the virtual car
          car_chargekwh += 1;
          kd_charge_wm -= 60000L;
          }
        }
      }
    }

  ////////////////////////////////////////////////////////////////////////
  // OBDII extended PID polling
  ////////////////////////////////////////////////////////////////////////

  // bus_is_active indicates we've recently seen a message on the can bus
  // Quick exit if bus is recently not active
  if (!kd_bus_is_active) return FALSE;

  // Also, we need CAN_WRITE enabled, so return if not
  if (sys_features[FEATURE_CANWRITE]==0) return FALSE;

  // Let's run through and see if we have to poll for any data..
  for (k=0;vehicle_kyburz_polls[k].moduleid != 0; k++)
    {
    if ((can_granular_tick % vehicle_kyburz_polls[k].polltime) == 0)
      {
      // OK. Let's send it...
      if (doneone)
        delay100b(); // Delay a little... (100ms, approx)

      while (TXB0CONbits.TXREQ) {} // Loop until TX is done
      TXB0CON = 0;
      TXB0SIDL = (vehicle_kyburz_polls[k].moduleid & 0x07)<<5;
      TXB0SIDH = (vehicle_kyburz_polls[k].moduleid>>3);
      TXB0D0 = 0x42;        // Read Expedited
      TXB0D1 = vehicle_kyburz_polls[k].pid & 0xff;
      TXB0D2 = vehicle_kyburz_polls[k].pid >> 8;
      TXB0D3 = 0x00;        // Sub-index: 0
      TXB0D4 = 0x00;
      TXB0D5 = 0x00;
      TXB0D6 = 0x00;
      TXB0D7 = 0x00;
      TXB0DLC = 0b00000100; // data length (4)
      TXB0CON = 0b00001000; // mark for transmission
      doneone = TRUE;
      }
    }
  // Assume the bus is not active, so we won't poll any more until we see
  // activity on the bus
  kd_bus_is_active = FALSE;

  return FALSE;
  }

////////////////////////////////////////////////////////////////////////
// can_poll()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to poll for data.
//
BOOL vehicle_kyburz_poll0(void)
  {
  unsigned int pid;
  unsigned char k;
  unsigned int value16;
  unsigned char value8;

  kd_candata_timer = 60;   // Reset the timer

  // Quick exit if not the reply we are looking for
  if (can_databuffer[0]!=0x42)
    return TRUE;

  pid = can_databuffer[1]+((unsigned int) can_databuffer[2] << 8);
  value16 = can_databuffer[4] + ((unsigned int)can_databuffer[5] << 8);
  if (can_id == 0x5A6)
    {
    switch (pid)
      {
      case 0x3405:  // SOC
        car_SOC = value16;
        break;
      case 0x33E7:  // Charge current
        car_chargecurrent = value16;
        if (car_chargecurrent == 0)
          {
          // Charge has completed
          car_chargestate = 4;    // Charge DONE
          car_chargesubstate = 3; // Leave it as is
          car_doors1 &= ~0x0c;    // Clear charge and pilot bits
          car_chargemode = 0;     // Standard charge mode
          car_charge_b4 = 0;      // Not required
          kd_charge_timer = 0;       // Reset the per-second charge timer
          car_chargeduration = 0; // Reset charge duration
          car_chargekwh = 0;      // Reset charge kWh
          kd_charge_wm = 0;          // Reset the per-minute watt accumulator
          }
        else
          {
          car_doors1 |= 0x0c;     // Set charge and pilot bits
          car_chargemode = 0;     // Standard charge mode
          car_charge_b4 = 0;      // Not required
          car_chargestate = 1;    // Charging state
          car_chargesubstate = 3; // Charging by request
          }
        break;
      }
    }

  return TRUE;
  }

////////////////////////////////////////////////////////////////////////
// vehicle_kyburz_initialise()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to initialise itself.
//
BOOL vehicle_kyburz_initialise(void)
  {
  char *p;

  car_type[0] = 'K'; // Car is type KD - Kyburz DXP
  car_type[1] = 'D';
  car_type[2] = 0;
  car_type[3] = 0;
  car_type[4] = 0;

  // Vehicle specific data initialisation
  kd_candata_timer = 0;
  kd_charge_timer = 0;
  kd_charge_wm = 0;
  car_stale_timer = -1; // Timed charging is not supported for OVMS Kyburz
  car_chargestate = 4; // Assume charge has completed
  car_time = 0;

  CANCON = 0b10010000; // Initialize CAN
  while (!CANSTATbits.OPMODE2); // Wait for Configuration mode

  // We are now in Configuration Mode

  // Filters: low byte bits 7..5 are the low 3 bits of the filter, and bits 4..0 are all zeros
  //          high byte bits 7..0 are the high 8 bits of the filter
  //          So total is 11 bits

  // Buffer 0 (filters 0, 1) for extended PID responses
  RXB0CON = 0b00000000;

  RXM0SIDL = 0b11100000;        // Mask   11111111111
  RXM0SIDH = 0b11111111;

  RXF0SIDL = 0b11000000;        // Filter 5A6 = 10110100110
  RXF0SIDH = 0b10110100;

  // Buffer 1 (filters 2, 3, 4 and 5) for direct can bus messages
  RXB1CON  = 0b00000000;	// RX buffer1 uses Mask RXM1 and filters RXF2, RXF3, RXF4, RXF5

  // CAN bus baud rate

  BRGCON1 = 3;            // SET BAUDRATE to 125Kbps
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
  vehicle_fn_poll0 = &vehicle_kyburz_poll0;
  vehicle_fn_ticker1 = &vehicle_kyburz_ticker1;

  net_fnbits |= NET_FN_INTERNALGPS;   // Require internal GPS
  net_fnbits |= NET_FN_12VMONITOR;    // Require 12v monitor
  net_fnbits |= NET_FN_SOCMONITOR;    // Require SOC monitor

  return TRUE;
  }
