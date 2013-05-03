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

// Volt/Ampera state variables

#pragma udata overlay vehicle_overlay_data
unsigned int va_soc_largest;     // Largest SOC value seen
unsigned int va_soc_smallest;    // Smallest SOC value seen
BOOL va_bus_is_active;           // Indicates recent activity on the bus
unsigned char va_charge_timer;   // A per-second charge timer
unsigned long va_charge_wm;      // A per-minute watt accumulator
unsigned char va_candata_timer;  // A per-second timer for CAN bus data

unsigned int va_obd_expect_id;   // ODBII expected ID
unsigned int va_obd_expect_pid;  // OBDII expected PID
BOOL va_obd_expect_waiting;      // OBDII expected waiting for response
char va_obd_expect_buf[64];      // Space for a response

unsigned int va_drive_distance_bat_max; // maximum distance drive on battery

#pragma udata

////////////////////////////////////////////////////////////////////////
// vehicle_voltampera_polls
// This rom table records the extended PIDs that need to be polled

rom struct
  {
  unsigned int moduleid;
  unsigned char polltime;
  unsigned int pid;
  } vehicle_voltampera_polls[]
  =
  {
    { 0x07E0, 10, 0x000D },
    { 0x07E4, 10, 0x4369 },
    { 0x07E4, 10, 0x4368 },
    { 0x07E4, 10, 0x801f },
    { 0x07E4, 10, 0x801e },
    { 0x07E4, 10, 0x434f },
    { 0x07E4, 10, 0x1c43 },
    { 0x07E4, 10, 0x8334 },
    { 0x07E1, 100, 0x2487 },
    { 0x0000, 0,  0x0000 }
  };

////////////////////////////////////////////////////////////////////////
// vehicle_voltampera_ticker1()
// This function is an entry point from the main() program loop, and
// gives the CAN framework a ticker call approximately once per second
//
BOOL vehicle_voltampera_ticker1(void)
  {
  int k;
  BOOL doneone = FALSE;

  ////////////////////////////////////////////////////////////////////////
  // Stale tickers
  ////////////////////////////////////////////////////////////////////////
  if (car_stale_ambient>0) car_stale_ambient--;
  if (car_stale_temps>0) car_stale_temps--;
  if (va_candata_timer>0)
    {
    if (--va_candata_timer == 0)
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
      car_doors5bits.Charging12V = 1;  //MJ
    if ((car_doors1 & 0x08)==0)
      { // Charge has started
      car_doors1 |= 0x0c;     // Set charge and pilot bits
      car_chargemode = 0;     // Standard charge mode
      car_charge_b4 = 0;      // Not required
      car_chargestate = 1;    // Charging state
      car_chargesubstate = 3; // Charging by request
      car_chargelimit = 16;   // Hard-code 16A charge limit
      car_chargeduration = 0; // Reset charge duration
      car_chargekwh = 0;      // Reset charge kWh
      va_charge_timer = 0;       // Reset the per-second charge timer
      va_charge_wm = 0;          // Reset the per-minute watt accumulator
      net_req_notification(NET_NOTIFY_STAT);
      }
    else
      { // Charge is ongoing
        car_doors1bits.ChargePort = 1;  //MJ
      va_charge_timer++;
      if (va_charge_timer>=60)
        { // One minute has passed
        va_charge_timer=0;

        car_chargeduration++;
        va_charge_wm += (car_chargecurrent*car_linevoltage);
        if (va_charge_wm >= 60000L)
          {
          // Let's move 1kWh to the virtual car
          car_chargekwh += 1;
          va_charge_wm -= 60000L;
          }
        }
      }
    }
  else if ((car_chargecurrent==0)&&(car_linevoltage==0))
    { // CAN says the car is not charging
    if (car_doors1 & 0x08)
      { // Charge has completed / stopped
      car_doors1 &= ~0x0c;    // Clear charge and pilot bits
      car_doors1bits.ChargePort = 1;  //MJ
      car_chargemode = 0;     // Standard charge mode
      car_charge_b4 = 0;      // Not required
      if (car_SOC < 95)
        { // Assume charge was interrupted
        car_chargestate = 21;    // Charge STOPPED
        car_chargesubstate = 14; // Charge INTERRUPTED
        net_req_notification(NET_NOTIFY_CHARGE);
        }
      else
        { // Assume charge completed normally
        car_chargestate = 4;    // Charge DONE
        car_chargesubstate = 3; // Leave it as is
        net_req_notification(NET_NOTIFY_CHARGE);  // Charge done Message MJ
        }
      va_charge_timer = 0;       // Reset the per-second charge timer
      va_charge_wm = 0;          // Reset the per-minute watt accumulator
      net_req_notification(NET_NOTIFY_STAT);
      }
    car_doors5bits.Charging12V = 0;  // MJ
    }

  ////////////////////////////////////////////////////////////////////////
  // OBDII extended pid request from net_msg
  ////////////////////////////////////////////////////////////////////////
  if (va_obd_expect_waiting)
    {
    delay100(1);
    net_msg_start();
    strcpy(net_scratchpad,va_obd_expect_buf);
    net_msg_encode_puts();
    va_obd_expect_waiting = FALSE;
    }

  ////////////////////////////////////////////////////////////////////////
  // OBDII extended PID polling
  ////////////////////////////////////////////////////////////////////////

  // bus_is_active indicates we've recently seen a message on the can bus
  // Quick exit if bus is recently not active
  if ((!va_bus_is_active) && (car_chargecurrent==0) && (car_linevoltage==0)) return FALSE;

  // Also, we need CAN_WRITE enabled, so return if not
  if (sys_features[FEATURE_CANWRITE]==0) return FALSE;

  // Let's run through and see if we have to poll for any data..
  for (k=0;vehicle_voltampera_polls[k].moduleid != 0; k++)
    {
    if ((can_granular_tick % vehicle_voltampera_polls[k].polltime) == 0)
      {
      // OK. Let's send it...
      if (doneone)
        delay100b(); // Delay a little... (100ms, approx)

      while (TXB0CONbits.TXREQ) {} // Loop until TX is done
      TXB0CON = 0;
      TXB0SIDL = (vehicle_voltampera_polls[k].moduleid & 0x07)<<5;
      TXB0SIDH = (vehicle_voltampera_polls[k].moduleid>>3);
      TXB0D0 = 0x03;
      TXB0D1 = 0x22;        // Get extended PID
      TXB0D2 = vehicle_voltampera_polls[k].pid >> 8;
      TXB0D3 = vehicle_voltampera_polls[k].pid & 0xff;
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
  va_bus_is_active = FALSE;
  return FALSE;
  }

////////////////////////////////////////////////////////////////////////
// can_poll()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to poll for data.
//
BOOL vehicle_voltampera_poll0(void)
  {
  unsigned int pid;
  unsigned char value;
  unsigned char k;
  char *p;
  unsigned int id = ((unsigned int)RXB0SIDL >>5)
                  + ((unsigned int)RXB0SIDH <<3);

   unsigned int edrive_distance = 0;

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

  va_candata_timer = 60;   // Reset the timer

  pid = can_databuffer[3]+((unsigned int) can_databuffer[2] << 8);
  value = can_databuffer[4];

  // First check for net_msg 46 (OBDII extended pid request)
  if ((pid == va_obd_expect_pid)&&(id == va_obd_expect_id)&&(!va_obd_expect_waiting))
    {
    // This is the response we were looking for
    p = stp_rom(va_obd_expect_buf, "MP-0 ");
    p = stp_i(p, "c", 46);
    p = stp_i(p, ",0,",id);
    p = stp_i(p, ",", pid);
    for (k=0;k<8;k++)
      p = stp_i(p, ",", can_databuffer[k]);
    va_obd_expect_waiting = TRUE;
    va_obd_expect_id = 0;
    va_obd_expect_pid = 0;
    }

  if (can_databuffer[1] != 0x62) return TRUE; // Check the return code

  if (id == 0x7ec)
    {
    switch (pid)
      {
      case 0x4369:  // On-board charger current
        car_chargecurrent = (unsigned int)value / 5;
        break;
      case 0x4368:  // On-board charger voltage
        car_linevoltage = (unsigned int)value << 1;
        break;
      case 0x801f:  // Outside temperature (filtered) (aka ambient temperature)
        car_stale_ambient = 60;
        car_ambient_temp = (signed char)((int)value/2 - 0x28);
        break;
      case 0x801e:  // Outside temperature (raw)
        car_stale_temps = 60;
        break;
      case 0x434f:  // High-voltage Battery temperature
        car_stale_temps = 60;
        car_tbattery = (int)value - 0x28;
        break;
      case 0x1c43:  // PEM temperature
        car_stale_temps = 60;
        car_tpem = (int)value - 0x28;
        break;
      case 0x8334:  // SOC
        car_stale_temps = 60;
        car_SOC = (char)(((int)value * 39) / 99);
        //car_idealrange = ((unsigned int)car_SOC * (unsigned int)37)/100;  // Kludgy, but ok for the moment
        car_idealrange = ((unsigned int)car_SOC * (unsigned int)va_drive_distance_bat_max) / 100;
        car_estrange = car_idealrange;                              // Very kludgy, but ok ...
        break;
      }
    }
  else if (id == 0x7e8)
    {
    switch (pid)
      {
      case 0x000d:  // Vehicle speed
        if (can_mileskm == 'K')
          car_speed = value;
        else
          car_speed = (unsigned char) ((((unsigned long)value * 1000)+500)/1609);
        break;
      }
    }
  else if (id == 0x7e9)
    {
    switch (pid)
      {
      case 0x2487:  //Distance Traveled on Battery Energy This Drive Cycle
          edrive_distance = KM2MI((can_databuffer[5] + ((unsigned int)can_databuffer[4] << 8)) / 100); // German Volt Report im KM
          if ((edrive_distance > va_drive_distance_bat_max) && (car_chargestate == 4)) va_drive_distance_bat_max = edrive_distance;
        break;
      }
    }


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

  va_bus_is_active = TRUE; // Activity has been seen on the bus
  va_candata_timer = 60;   // Reset the timer

  if ((CANctrl & 0x07) == 2)             // Acceptance Filter 2 (RXF2) = CAN ID 0x206
    {
    // SOC
    // For the SOC, each 4,000 is 1kWh. Assuming a 16.1kWh battery, 1% SOC is 644 decimal bytes
    // The SOC itself is d1<<8 + d2
    //unsigned int v = (can_databuffer[1]+((unsigned int) can_databuffer[0] << 8));
    //if ((v<va_soc_smallest)&&(v>0)) v=va_soc_smallest;
    //if (v>va_soc_largest) v=va_soc_largest;
    //car_SOC = (char)((v-va_soc_smallest)/((va_soc_largest-va_soc_smallest)/100));
    //car_idealrange = ((unsigned int)car_SOC * (unsigned int)37)/100;  // Kludgy, but ok for the moment
    //car_estrange = car_idealrange;                              // Very kludgy, but ok ...
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
  else if ((CANctrl & 0x07) == 5)        // Acceptance Filter 5 (RXF5) = CAN ID 135
    {
    if (can_databuffer[0] == 0)
      { // Car is in PARK
      car_doors1 |= 0x40;     //  PARK (Handbrake activate) MJ
      car_doors1 &= ~0x80;    // CAR OFF
      if (car_parktime == 0)
        {
        car_parktime = car_time-1;    // Record it as 1 second ago, so non zero report
        net_req_notification(NET_NOTIFY_ENV);
        }
      }
    else
      { // Car is not in PARK
      car_doors1 &= ~0x40;     // NOT PARK (Handbrake release) MJ
      car_doors1 |= 0x80;     // CAR ON
      if (car_parktime != 0)
        {
        car_parktime = 0; // No longer parking
        net_req_notification(NET_NOTIFY_ENV);
        }
      }
    }

  return TRUE;
  }

BOOL vehicle_voltampera_fn_commandhandler(BOOL msgmode, int cmd, char *msg)
  {
  switch (cmd)
    {
    case 46:
      // OBDII extended PID request
      msg = strtokpgmram(msg,",");
      if (msg == NULL) return FALSE;
      va_obd_expect_id = atoi(msg);
      msg = strtokpgmram(NULL,",");
      if (msg == NULL) return FALSE;
      va_obd_expect_pid = atoi(msg);

      va_obd_expect_waiting = FALSE;

      delay100b(); // Delay a little... (100ms, approx)

      while (TXB0CONbits.TXREQ) {} // Loop until TX is done
      TXB0CON = 0;
      TXB0SIDL = (va_obd_expect_id & 0x07)<<5;
      TXB0SIDH = (va_obd_expect_id >>3);
      va_obd_expect_id += 8;   // Get ready for reply
      TXB0D0 = 0x03;
      TXB0D1 = 0x22;        // Get extended PID
      TXB0D2 = va_obd_expect_pid >> 8;
      TXB0D3 = va_obd_expect_pid & 0xff;
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

  // Vehicle specific data initialisation
  va_soc_largest  = 56028;
  va_soc_smallest = 13524;
  va_bus_is_active = FALSE;       // Indicates recent activity on the bus
  va_charge_timer = 0;
  va_charge_wm = 0;
  va_candata_timer = 0;
  va_obd_expect_id = 0;
  va_obd_expect_pid = 0;
  va_obd_expect_waiting = FALSE;
  car_stale_timer = -1; // Timed charging is not supported for OVMS VA
  car_time = 0;

  va_drive_distance_bat_max = KM2MI(35);    // initial Battery distance in km

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

  RXF2SIDL = 0b11000000;	// Setup Filter2 so that CAN ID 0x206 will be accepted
  RXF2SIDH = 0b01000000;

  RXF3SIDL = 0b00100000;	// Setup Filter3 so that CAN ID 0x4E1 will be accepted
  RXF3SIDH = 0b10011100;

  RXF4SIDL = 0b10000000;  // Setup Filter4 so that CAN ID 0x514 will be accepted
  RXF4SIDH = 0b10100010;

  RXF5SIDL = 0b10100000;  // Setup Filter5 so that CAN ID 0x135 will be accepted
  RXF5SIDH = 0b00100110;

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
  vehicle_fn_poll0 = &vehicle_voltampera_poll0;
  vehicle_fn_poll1 = &vehicle_voltampera_poll1;
  vehicle_fn_ticker1 = &vehicle_voltampera_ticker1;
  vehicle_fn_commandhandler = &vehicle_voltampera_fn_commandhandler;
  
  net_fnbits |= NET_FN_INTERNALGPS;   // Require internal GPS
  net_fnbits |= NET_FN_12VMONITOR;    // Require 12v monitor

  return TRUE;
  }
