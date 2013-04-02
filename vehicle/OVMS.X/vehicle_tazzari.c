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

// Tazzari state variables

#pragma udata overlay vehicle_overlay_data
unsigned char tz_candata_timer;  // A per-second timer for CAN bus data
unsigned char tz_charge_timer;   // A per-second charge timer
unsigned long tz_charge_wm;      // A per-minute watt accumulator
BOOL tz_bus_is_active;           // Indicates recent activity on the bus

#pragma udata

////////////////////////////////////////////////////////////////////////
// vehicle_tazzari_polls
// This rom table records the extended PIDs that need to be polled

rom struct
  {
  unsigned int moduleid;
  unsigned char polltime;
  unsigned int pid;
  } vehicle_tazzari_polls[]
  =
  {
    { 0x078A, 10, 0xF9EF },  // SOC
    { 0x078A, 60, 0xF29F },  // Battery temperature
    { 0x078A, 60, 0xF9A7 },  // Battery capacity
    { 0x078A, 10, 0xF6E0 },  // Charge status
    { 0x078A, 10, 0xF6D8 },  // Drive mode
    { 0x0000, 0,  0x0000 }
  };

////////////////////////////////////////////////////////////////////////
// vehicle_tazzari_ticker1()
// This function is an entry point from the main() program loop, and
// gives the CAN framework a ticker call approximately once per second
//
BOOL vehicle_tazzari_ticker1(void)
  {
  int k;
  BOOL doneone = FALSE;

  if (car_stale_ambient>0) car_stale_ambient--;
  if (car_stale_temps>0) car_stale_temps--;
  if (tz_candata_timer>0)
    {
    if (--tz_candata_timer == 0)
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
      tz_charge_timer++;
      if (tz_charge_timer>=60)
        { // One minute has passed
        tz_charge_timer=0;

        car_chargeduration++;
        tz_charge_wm += (car_chargecurrent*car_linevoltage);
        if (tz_charge_wm >= 60000L)
          {
          // Let's move 1kWh to the virtual car
          car_chargekwh += 1;
          tz_charge_wm -= 60000L;
          }
        }
      }
    }

  ////////////////////////////////////////////////////////////////////////
  // OBDII extended PID polling
  ////////////////////////////////////////////////////////////////////////

  // bus_is_active indicates we've recently seen a message on the can bus
  // Quick exit if bus is recently not active
  if (!tz_bus_is_active) return FALSE;

  // Also, we need CAN_WRITE enabled, so return if not
  if (sys_features[FEATURE_CANWRITE]==0) return FALSE;

  // Let's run through and see if we have to poll for any data..
  for (k=0;vehicle_tazzari_polls[k].moduleid != 0; k++)
    {
    if ((can_granular_tick % vehicle_tazzari_polls[k].polltime) == 0)
      {
      // OK. Let's send it...
      if (doneone)
        delay100b(); // Delay a little... (100ms, approx)

      while (TXB0CONbits.TXREQ) {} // Loop until TX is done
      TXB0CON = 0;
      TXB0SIDL = (vehicle_tazzari_polls[k].moduleid & 0x07)<<5;
      TXB0SIDH = (vehicle_tazzari_polls[k].moduleid>>3);
      TXB0D0 = 0x75;
      TXB0D1 = 0x21;        // Get extended PID
      TXB0D2 = vehicle_tazzari_polls[k].pid >> 8;
      TXB0D3 = vehicle_tazzari_polls[k].pid & 0xff;
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
  tz_bus_is_active = FALSE;

  return FALSE;
  }

////////////////////////////////////////////////////////////////////////
// can_poll()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to poll for data.
//
BOOL vehicle_tazzari_poll0(void)
  {
  unsigned int pid;
  unsigned char k;
  unsigned int value16;
  unsigned char value8;
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

  tz_candata_timer = 60;   // Reset the timer

  // 0A A1 F9 EF 00 00 03 E8
  // Quick exit if not the reply we are looking for
  if ((can_databuffer[0]!=0x0a)||(can_databuffer[1]!=0xa1))
    return TRUE;

  pid = can_databuffer[3]+((unsigned int) can_databuffer[2] << 8);
  value16 = can_databuffer[7] + ((unsigned int)can_databuffer[6] << 8);
  value8 = can_databuffer[7];
  if (id == 0x775)
    {
    switch (pid)
      {
      case 0xF9EF:  // SOC
        car_SOC = value16 / 10;
        break;
      case 0xF29F:  // Battery temperature
        break;
      case 0xF9A7:  // Battery capacity
          car_cac100 = value16 * 10;
        break;
      case 0xF6E0:  // Charge status
        switch (value8)
          {
          case 0x04: // Charge has completed
            car_chargestate = 4;    // Charge DONE
            car_chargesubstate = 3; // Leave it as is
            car_doors1 &= ~0x0c;    // Clear charge and pilot bits
            car_chargemode = 0;     // Standard charge mode
            car_charge_b4 = 0;      // Not required
            break;
          case 0x63: // Charge done/failed (depending where we came from)
            car_doors1 &= ~0x0c;    // Clear charge and pilot bits
            car_chargemode = 0;     // Standard charge mode
            car_charge_b4 = 0;      // Not required
            if (car_chargestate == 4)
              { // Charge done
              }
            else if (car_chargestate != 21)
              { // Charge interrupted
              car_chargestate = 21;    // Charge STOPPED
              car_chargesubstate = 14; // Charge INTERRUPTED
              net_req_notification(NET_NOTIFY_CHARGE);
              }
            break;
          case 0x01:
          case 0x02:
          case 0x03:
            if (car_chargestate != 1)
              {
              car_doors1 |= 0x0c;     // Set charge and pilot bits
              car_chargemode = 0;     // Standard charge mode
              car_charge_b4 = 0;      // Not required
              car_chargestate = 1;    // Charging state
              car_chargesubstate = 3; // Charging by request
              car_chargelimit = 16;   // Hard-code 16A charge limit
              car_chargeduration = 0; // Reset charge duration
              car_chargekwh = 0;      // Reset charge kWh
              tz_charge_timer = 0;       // Reset the per-second charge timer
              tz_charge_wm = 0;          // Reset the per-minute watt accumulator
              net_req_notification(NET_NOTIFY_STAT);
              }
            break;
          }
        break;
      case 0xF6D8:  // Drive mode
        break;
      }
    }

  return TRUE;
  }

BOOL vehicle_tazzari_poll1(void)
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

  tz_bus_is_active = TRUE; // Activity has been seen on the bus
  tz_candata_timer = 60;   // Reset the timer

  if ((CANctrl & 0x07) == 2)             // Acceptance Filter 2 (RXF2) = CAN ID 0x267
    {
    if (can_databuffer[4] == 0)
      { // Driving
      car_doors1 &= ~0x40;    // PARK
      car_doors1 |= 0x80;     // CAR ON
      if (car_parktime != 0)
        {
        car_parktime = 0; // No longer parking
        net_req_notification(NET_NOTIFY_ENV);
        }
      }
    else
      { // Not Driving
      car_doors1 |= 0x40;     // NOT PARK
      car_doors1 &= ~0x80;    // CAR OFF
      if (car_parktime == 0)
        {
        car_parktime = car_time-1;    // Record it as 1 second ago, so non zero report
        net_req_notification(NET_NOTIFY_ENV);
        }
      }
    }

  return TRUE;
  }

////////////////////////////////////////////////////////////////////////
// vehicle_tazzari_initialise()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to initialise itself.
//
BOOL vehicle_tazzari_initialise(void)
  {
  char *p;

  car_type[0] = 'T'; // Car is type TZ - Tazzari Zero
  car_type[1] = 'Z';
  car_type[2] = 'Z';
  car_type[3] = 'O';
  car_type[4] = 0;

  // Vehicle specific data initialisation
  tz_candata_timer = 0;
  tz_charge_timer = 0;
  tz_charge_wm = 0;
  car_stale_timer = -1; // Timed charging is not supported for OVMS Tazzari
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

  RXF0SIDL = 0b10100000;        // Filter 11101110101 (0x775)
  RXF0SIDH = 0b11101110;

  // Buffer 1 (filters 2, 3, 4 and 5) for direct can bus messages
  RXB1CON  = 0b00000000;	// RX buffer1 uses Mask RXM1 and filters RXF2, RXF3, RXF4, RXF5

  RXM1SIDL = 0b11100000;
  RXM1SIDH = 0b11111111;	// Set Mask1 to 0x7ff

  RXF2SIDL = 0b11100000;	// Setup Filter2 01001100111 (0x267)
  RXF2SIDH = 0b01001100;

  // CAN bus baud rate

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

  // Hook in...
  vehicle_fn_poll0 = &vehicle_tazzari_poll0;
  vehicle_fn_poll1 = &vehicle_tazzari_poll1;
  vehicle_fn_ticker1 = &vehicle_tazzari_ticker1;

  net_fnbits |= NET_FN_INTERNALGPS;   // Require internal GPS
  net_fnbits |= NET_FN_12VMONITOR;    // Require 12v monitor
  net_fnbits |= NET_FN_SOCMONITOR;    // Require SOC monitor

  return TRUE;
  }
