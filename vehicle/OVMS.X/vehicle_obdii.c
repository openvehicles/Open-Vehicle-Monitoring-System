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

// OBDII state variables

#pragma udata overlay vehicle_overlay_data
BOOL obdii_bus_is_active;           // Indicates recent activity on the bus
unsigned char obdii_candata_timer;  // A per-second timer for CAN bus data

unsigned char obdii_expect_pid;  // OBDII expected PID
BOOL obdii_expect_waiting;      // OBDII expected waiting for response
char obdii_expect_buf[64];      // Space for a response

#pragma udata

////////////////////////////////////////////////////////////////////////
// vehicle_obdii_polls
// This rom table records the PIDs that need to be polled

rom struct
  {
  unsigned char polltime;
  unsigned char service;
  unsigned char pid;
  } vehicle_obdii_polls[]
  =
  {
    { 30, 0x01, 0x46 },
    { 10, 0x01, 0x0d },
    { 30, 0x01, 0x2f },
    { 10, 0x01, 0x0c },
    { 10, 0x01, 0x05 },
    { 10, 0x01, 0x0f },
    { 10, 0x01, 0x5c },
    { 0,  0x00, 0x00 }
  };

////////////////////////////////////////////////////////////////////////
// vehicle_obdii_ticker1()
// This function is an entry point from the main() program loop, and
// gives the CAN framework a ticker call approximately once per second
//
BOOL vehicle_obdii_ticker1(void)
  {
  int k;
  BOOL doneone = FALSE;

  ////////////////////////////////////////////////////////////////////////
  // Stale tickers
  ////////////////////////////////////////////////////////////////////////
  if (obdii_candata_timer>0)
    {
    if (--obdii_candata_timer == 0)
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
  // OBDII pid request from net_msg
  ////////////////////////////////////////////////////////////////////////
  if (obdii_expect_waiting)
    {
    delay100(1);
    net_msg_start();
    strcpy(net_scratchpad,obdii_expect_buf);
    net_msg_encode_puts();
    obdii_expect_waiting = FALSE;
    }

  ////////////////////////////////////////////////////////////////////////
  // OBDII PID polling
  ////////////////////////////////////////////////////////////////////////

  // obdii_bus_is_active indicates we've recently seen a message on the can bus
  // Quick exit if bus is recently not active
  if (!obdii_bus_is_active) return FALSE;

  // Also, we need CAN_WRITE enabled, so return if not
  if (sys_features[FEATURE_CANWRITE]==0) return FALSE;

  // Let's run through and see if we have to poll for any data..
  for (k=0;vehicle_obdii_polls[k].polltime != 0; k++)
    {
    if ((can_granular_tick % vehicle_obdii_polls[k].polltime) == 0)
      {
      // OK. Let's send it...
      if (doneone)
        delay100b(); // Delay a little... (100ms, approx)

      while (TXB0CONbits.TXREQ) {} // Loop until TX is done
      TXB0CON = 0;
      TXB0SIDL = 0b11100000;  // 0x07df
      TXB0SIDH = 0b11111011;
      TXB0D0 = 0x02;
      TXB0D1 = vehicle_obdii_polls[k].service;
      TXB0D2 = vehicle_obdii_polls[k].pid;
      TXB0D3 = 0x00;
      TXB0D4 = 0x00;
      TXB0D5 = 0x00;
      TXB0D6 = 0x00;
      TXB0D7 = 0x00;
      TXB0DLC = 0b00001000; // data length (8)
      TXB0CON = 0b00001000; // mark for transmission
      doneone = TRUE;
      }
    }
  // Assume the bus is not active, so we won't poll any more until we see
  // activity on the bus
  obdii_bus_is_active = FALSE;
  return FALSE;
  }

////////////////////////////////////////////////////////////////////////
// can_poll()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to poll for data.
//
BOOL vehicle_obdii_poll0(void)
  {
  unsigned int pid;
  unsigned char k;
  unsigned char value1;
  unsigned int value2;
  char *p;

  obdii_bus_is_active = TRUE; // Activity has been seen on the bus
  obdii_candata_timer = 60;   // Reset the timer

  pid = can_databuffer[2];
  value1 = can_databuffer[3];
  value2 = (unsigned int)can_databuffer[3]<<8 + (unsigned int)can_databuffer[4];

  // First check for net_msg 45 (OBDII pid request)
  if ((pid == obdii_expect_pid)&&(!obdii_expect_waiting))
    {
    // This is the response we were looking for
    p = stp_rom(obdii_expect_buf, "MP-0 ");
    p = stp_i(p, "c", 45);
    p = stp_i(p, ",0,",can_id);
    p = stp_i(p, ",", pid);
    for (k=0;k<8;k++)
      p = stp_i(p, ",", can_databuffer[k]);
    obdii_expect_waiting = TRUE;
    obdii_expect_pid = 0;
    }

  if ((can_databuffer[1] < 0x40)||
      (can_databuffer[1] > 0x4a)) return TRUE; // Check the return code

  if (can_databuffer[1] == 0x41)
    {
    switch (pid)
      {
      case 0x05:  // Engine coolant temperature
        car_stale_temps = 60;
        car_tbattery = (int)value1 - 0x28;
        break;
      case 0x0f:  // Engine intake air temperature
        car_stale_temps = 60;
        car_tpem = (int)value1 - 0x28;
        break;
      case 0x5c:  // Engine oil temperature
        car_stale_temps = 60;
        car_tmotor = (int)value1 - 0x28;
        break;
      case 0x46:  // Ambient temperature
        car_stale_ambient = 60;
        car_ambient_temp = (signed char)((int)value1 - 0x28);
        break;
      case 0x0d:  // Speed
        if (can_mileskm == 'K')
          car_speed = value1;
        else
          car_speed = (unsigned char) ((((unsigned long)value1 * 1000)+500)/1609);
        break;
      case 0x2f:  // Fuel Level
        car_SOC = (char)(((int)value1 * 100) >> 8);
        break;
      case 0x0c:  // Engine RPM
        if (value2 == 0)
          { // Car engine is OFF
          car_doors1 |= 0x40;     // PARK
          car_doors1 &= ~0x80;    // CAR OFF
          if (car_parktime == 0)
            {
            car_parktime = car_time-1;    // Record it as 1 second ago, so non zero report
            net_req_notification(NET_NOTIFY_ENV);
            }
          }
        else
          { // Car engine is ON
          car_doors1 &= ~0x40;    // NOT PARK
          car_doors1 |= 0x80;     // CAR ON
          if (car_parktime != 0)
            {
            car_parktime = 0; // No longer parking
            net_req_notification(NET_NOTIFY_ENV);
            }
          }
        break;
      }
    }

  return TRUE;
  }

BOOL vehicle_obdii_fn_commandhandler(BOOL msgmode, int cmd, char *msg)
  {
  unsigned int service;

  switch (cmd)
    {
    case 45:
      // OBDII PID request
      msg = strtokpgmram(msg,",");
      if (msg == NULL) return FALSE;
      service = atoi(msg);
      msg = strtokpgmram(NULL,",");
      if (msg == NULL) return FALSE;
      obdii_expect_pid = atoi(msg);

      obdii_expect_waiting = FALSE;

      delay100b(); // Delay a little... (100ms, approx)

      while (TXB0CONbits.TXREQ) {} // Loop until TX is done
      TXB0CON = 0;
      TXB0SIDL = 0b11100000;  // 0x07df
      TXB0SIDH = 0b11111011;
      TXB0D0 = 0x02;
      TXB0D1 = service;
      TXB0D2 = obdii_expect_pid;
      TXB0D3 = 0x00;
      TXB0D4 = 0x00;
      TXB0D5 = 0x00;
      TXB0D6 = 0x00;
      TXB0D7 = 0x00;
      TXB0DLC = 0b00001000; // data length (8)
      TXB0CON = 0b00001000; // mark for transmission

      return TRUE;
    }

  // not handled
  return FALSE;
  }


////////////////////////////////////////////////////////////////////////
// vehicle_obdii_initialise()
// This function is the initialisation entry point should we be the
// selected vehicle.
//
BOOL vehicle_obdii_initialise(void)
  {
  char *p;

  car_type[0] = 'O'; // Car is type OBDII
  car_type[1] = '2';
  car_type[2] = 0;
  car_type[3] = 0;
  car_type[4] = 0;

  // Vehicle specific data initialisation
  obdii_bus_is_active = FALSE;       // Indicates recent activity on the bus
  obdii_candata_timer = 0;
  obdii_expect_pid = 0;
  obdii_expect_waiting = FALSE;
  car_stale_timer = -1; // Timed charging is not supported for OVMS OBDII
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
  vehicle_fn_poll0 = &vehicle_obdii_poll0;
  vehicle_fn_ticker1 = &vehicle_obdii_ticker1;
  vehicle_fn_commandhandler = &vehicle_obdii_fn_commandhandler;

  net_fnbits |= NET_FN_INTERNALGPS;   // Require internal GPS
  net_fnbits |= NET_FN_12VMONITOR;    // Require 12v monitor

  return TRUE;
  }
