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
signed char obdii_cooldown_recycle;             // Ticker counter for cooldown recycle
#pragma udata

////////////////////////////////////////////////////////////////////////
// vehicle_obdii_polls
// This rom table records the PIDs that need to be polled

rom  vehicle_pid_t vehicle_obdii_polls[]
  =
  {
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x05, {  0, 30, 30 } }, // Engine coolant temp
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x0c, { 10, 10, 10 } }, // Engine RPM
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x0d, {  0, 10, 10 } }, // Speed
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x0f, {  0, 30, 30 } }, // Engine air intake temp
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x2f, {  0, 30, 30 } }, // Fuel level
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x46, {  0, 30, 30 } }, // Ambiant temp
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x5c, {  0, 30, 30 } }, // Engine oil temp
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIIVEHICLE, 0x02, {999,999,999 } }, // VIN
    { 0, 0, 0x00, 0x00, { 0, 0, 0 } }
  };

////////////////////////////////////////////////////////////////////////
// vehicle_obdii_ticker1()
// This function is an entry point from the main() program loop, and
// gives the CAN framework a ticker call approximately once per second
//
BOOL vehicle_obdii_ticker1(void)
  {
  if (vehicle_poll_state==0)
    {
    car_doors3 &= ~0x01;      // Car is asleep
    }
  else
    {
    car_doors3 |= 0x01;       // Car is awake
    }

  vehicle_poll_busactive = 60; // Force bus active

  return FALSE;
  }

////////////////////////////////////////////////////////////////////////
// can_poll()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to poll for data.
//

// ISR optimization, see http://www.xargs.com/pic/c18-isr-optim.pdf
#pragma tmpdata high_isr_tmpdata

BOOL vehicle_obdii_poll0(void)
  {
  unsigned char value1;
  unsigned int value2;

  value1 = can_databuffer[3];
  value2 = ((unsigned int)can_databuffer[3]<<8) + (unsigned int)can_databuffer[4];
  
  switch (vehicle_poll_pid)
    {
    case 0x02:  // VIN (multi-line response)
      for (value1=0;value1<can_datalength;value1++)
        {
        car_vin[value1+(vehicle_poll_ml_offset-can_datalength)] = can_databuffer[value1];
        }
      if (vehicle_poll_ml_remain==0)
        car_vin[value1+vehicle_poll_ml_offset] = 0;
      break;
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
        { // Car engine is ON
        vehicle_poll_setstate(1);
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

  return TRUE;
  }

#pragma tmpdata


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
  car_stale_timer = -1; // Timed charging is not supported for OVMS OBDII

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
  vehicle_fn_poll0 = &vehicle_obdii_poll0;
  vehicle_fn_ticker1 = &vehicle_obdii_ticker1;
  if (sys_features[FEATURE_CANWRITE]>0)
    {
    CANCON = 0b00000000;  // Normal mode
    vehicle_poll_setpidlist(vehicle_obdii_polls);
    vehicle_poll_setstate(0);
    }
  else
    {
    CANCON = 0b01100000; // Listen only mode, Receive bufer 0
    }

  net_fnbits |= NET_FN_INTERNALGPS;   // Require internal GPS
  net_fnbits |= NET_FN_12VMONITOR;    // Require 12v monitor
  net_fnbits |= NET_FN_CARTIME;       // Require car_time support

  return TRUE;
  }
