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
; Thanks to TMC, Scott451 for figuring out many of the Roadster CAN bus
; messages used by the RCM, and Fuzzzylogic for the RCM project as a
; reference base.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ovms.h"
#include "utils.h"
#include "led.h"
#include "inputs.h"
#ifdef OVMS_LOGGINGMODULE
#include "logging.h"
#endif
#ifdef OVMS_ACCMODULE
#include "acc.h"
#endif

// Configuration settings
#pragma	config FCMEN = OFF,      IESO = OFF
#pragma	config PWRT = ON,        BOREN = OFF,      BORV = 0
#pragma	config WDTPS = 4096     // WDT timeout set to 16 secs
#if defined(__DEBUG)
  #pragma config MCLRE  = ON
  #pragma config DEBUG = ON
  #pragma config WDT = OFF
#else
  #pragma config MCLRE  = OFF
  #pragma config DEBUG = OFF
  #pragma config WDT = ON
#endif
#pragma config OSC = HS
#pragma	config LPT1OSC = OFF,    PBADEN = OFF
#pragma	config XINST = ON,       BBSIZ = 1024,     LVP = OFF,        STVREN = ON
#pragma	config CP0 = OFF,        CP1 = OFF,        CP2 = OFF,        CP3 = OFF
#pragma	config CPB = OFF,        CPD = OFF
#pragma	config WRT0 = OFF,       WRT1 = OFF,       WRT2 = OFF,       WRT3 = OFF
#pragma	config WRTB = OFF,       WRTC = OFF,       WRTD = OFF
#pragma	config EBTR0 = OFF,      EBTR1 = OFF,      EBTR2 = OFF,      EBTR3 = OFF
#pragma	config EBTRB = OFF

// Global data
#pragma udata
rom unsigned char ovms_firmware[3] = {OVMS_FIRMWARE_VERSION}; // Firmware version
unsigned int car_linevoltage = 0; // Line Voltage
unsigned char car_chargecurrent = 0; // Charge Current
unsigned char car_chargelimit = 0; // Charge Limit (amps)
unsigned int car_chargeduration = 0; // Charge Duration (minutes)
unsigned char car_chargestate = 4; // 1=charging, 2=top off, 4=done, 13=preparing to charge, 21-25=stopped charging
unsigned char car_chargesubstate = 0;
unsigned char car_chargemode = 0; // 0=standard, 1=storage, 3=range, 4=performance
unsigned char car_charge_b4 = 0; // B4 byte of charge state
unsigned char car_chargekwh = 0; // KWh of charge
unsigned char car_doors1 = 0; //
unsigned char car_doors2 = 0; //
unsigned char car_doors3 = 0; //
unsigned char car_doors4 = 0; //
unsigned char car_doors5 = 0; //
unsigned char car_lockstate = 0; // Lock State
unsigned char car_speed = 0; // speed in defined units (mph or kph)
unsigned char car_SOC = 0; // State of Charge in %
unsigned int car_idealrange = 0; // Ideal Range in miles
unsigned int car_estrange = 0; // Estimated Range
unsigned long car_time = 1; // UTC Time
unsigned long car_parktime = 0; // UTC time car was parked (or 0 if not)
signed int car_ambient_temp = -127; // Ambient Temperature (celcius)
unsigned char car_vin[18] = "-----------------"; // VIN
unsigned char car_type[5]; // Car Type, intentionally uninitialised for vehicle init
signed int car_tpem = 0; // Tpem (inverter/controller)
signed int car_tmotor = 0; // Tmotor
signed int car_tbattery = 0; // Tbattery
signed int car_tcharger = 0; // Tcharger
signed int car_tpms_t[4] = {0,0,0,0}; // TPMS temperature
unsigned char car_tpms_p[4] = {0,0,0,0}; // TPMS pressure
unsigned int car_trip = 0; // ODO trip in miles /10
unsigned long car_odometer = 0; //Odometer in miles /10
signed long car_latitude = 0x16DEC6D9; // Raw GPS Latitude  (52.04246 zero in converted result)
signed long car_longitude = 0xFE444A36; // Raw GPS Longitude ( -3.94409, not verified if this is correct)
unsigned int car_direction = 0; // GPS direction of the car
signed int car_altitude = 0; // GPS altitude of the car
signed char car_timermode = 0; // Timer mode (0=onplugin, 1=timer)
unsigned int car_timerstart = 0; // Timer start
unsigned char car_gpslock = 0; // GPS lock status
signed char car_stale_ambient = -1; // 0 = Ambient temperature is stale
signed char car_stale_temps = -1; // 0 = Powertrain temperatures are stale
signed char car_stale_gps = -1; // 0 = gps is stale
signed char car_stale_tpms = -1; // 0 = tpms is stale
signed char car_stale_timer = -1; // 0 = timer is stale
unsigned char net_reg = 0; // Network registration
unsigned char net_link = 0; // Network link status
unsigned char net_iccid[MAX_ICCID]; // ICCID
char net_apps_connected = 0; // Network apps connected
char sys_features[FEATURES_MAX]; // System features
unsigned char net_sq = 0; // GSM Network Signal Quality
unsigned char car_12vline = 0; // 12V line level
unsigned char car_12vline_ref = 0; // 12V line level reference
unsigned char car_gsmcops[9] = ""; // GSM provider

unsigned int car_cac100 = 0; // CAC = Calculated Amphour Capacity (Ah x 100)

signed int car_chargefull_minsremaining = -1;  // ETR for 100%
signed int car_chargelimit_minsremaining_range = -1; // ETR for range limit
signed int car_chargelimit_minsremaining_soc = -1; // ETR for SOC limit
unsigned int car_chargelimit_rangelimit = 0;   // Range limit (in vehicle units)
unsigned char car_chargelimit_soclimit = 0;    // SOC% limit

unsigned int car_max_idealrange = 0; // Maximum ideal range in miles

signed char car_coolingdown = -1;              // >=0 if car is cooling down
unsigned char car_cooldown_chargemode = 0;     // 0=standard, 1=storage, 3=range, 4=performance
unsigned char car_cooldown_chargelimit = 0;    // Charge Limit (amps)
signed int car_cooldown_tbattery = 0;          // Cooldown temperature limit
unsigned int car_cooldown_timelimit = 0;       // Cooldown time limit (minutes) remaining
unsigned char car_cooldown_wascharging = 0;    // TRUE if car was charging when cooldown started

int car_chargeestimate = -1;                   // ACC: charge time estimation for current charger capabilities (min.)

unsigned char car_SOCalertlimit = 5;           // Low limit of SOC at which alert should be raised

#ifndef OVMS_NO_CRASHDEBUG
UINT8 debug_crashcnt;           // crash counter, cleared on normal power up
UINT8 debug_crashreason;        // last saved reset reason (bit set)
UINT8 debug_checkpoint;         // number of last checkpoint before crash
#endif // OVMS_NO_CRASHDEBUG


void main(void)
{
  unsigned char x, y;

  // DEBUG / QA stats: get last reset reason:
  x = (~RCON) & 0x1f;
  if (STKPTRbits.STKFUL) x += 32;
  if (STKPTRbits.STKUNF) x += 64;

  // ...clear RCON:
  RCONbits.NOT_BOR = 1; // b0 = 1  = Brown Out Reset
  RCONbits.NOT_POR = 1; // b1 = 2  = Power On Reset
  //RCONbits.NOT_PD = 1;    // b2 = 4  = Power Down detection
  //RCONbits.NOT_TO = 1;    // b3 = 8  = watchdog TimeOut occured
  RCONbits.NOT_RI = 1; // b4 = 16 = Reset Instruction
  
  // ...clear stack overflow & underflow flags:
  STKPTRbits.STKFUL = 0;
  STKPTRbits.STKUNF = 0;
  
  if (x == 3) // 3 = normal Power On
  {
#ifndef OVMS_NO_CRASHDEBUG
    debug_crashreason = 0;
    debug_crashcnt = 0;
#endif // OVMS_NO_CRASHDEBUG
#ifdef OVMS_LOGGINGMODULE
    logging_initialise();
#endif
  }
  else
  {
#ifndef OVMS_NO_CRASHDEBUG
    debug_crashreason = x | 0x80; // 0x80 = keep checkpoint until sent to server
    debug_crashcnt++;
#endif // OVMS_NO_CRASHDEBUG
  }

  CHECKPOINT(0x20)

  for (x = 0; x < FEATURES_MAP_PARAM; x++)
    sys_features[x] = 0; // Turn off the features

  // The top N features are persistent
  for (x = FEATURES_MAP_PARAM; x < FEATURES_MAX; x++)
  {
    sys_features[x] = atoi(par_get(PARAM_FEATURE_S + (x - FEATURES_MAP_PARAM)));
  }

  // Make sure cooldown is off
  car_coolingdown = -1;

  // Port configuration
  inputs_initialise();
  TRISB = 0xFE;

  // Timer 0 enabled, Fosc/4, 16 bit mode, prescaler 1:256
  // This gives us one tick every 51.2uS before prescale (13.1ms after)
  T0CON = 0b10000111; // @ 5Mhz => 51.2uS

  // Initialisation...
  led_initialise();
  par_initialise();
  vehicle_initialise();
  net_initialise();

  CHECKPOINT(0x21)

  // Startup sequence...
  // Holding the RED led on, pulse out the firmware version on the GREEN led
  delay100(10); // Delay 1 second
  led_set(OVMS_LED_RED, OVMS_LED_ON);
  led_set(OVMS_LED_GRN, OVMS_LED_OFF);
  led_start();
  delay100(10); // Delay 1.0 seconds
  led_set(OVMS_LED_GRN, ovms_firmware[0]);
  led_start();
  delay100(35); // Delay 3.5 seconds
  ClrWdt(); // Clear Watchdog Timer
  led_set(OVMS_LED_GRN, ovms_firmware[1]);
  led_start();
  delay100(35); // Delay 3.5 seconds
  ClrWdt(); // Clear Watchdog Timer
  led_set(OVMS_LED_GRN, ovms_firmware[2]);
  led_start();
  delay100(35); // Delay 3.5 seconds
  ClrWdt(); // Clear Watchdog Timer
  led_set(OVMS_LED_GRN, OVMS_LED_OFF);
  led_set(OVMS_LED_RED, OVMS_LED_OFF);
  led_start();
  delay100(10); // Delay 1 second
  ClrWdt(); // Clear Watchdog Timer

  // Setup ready for the main loop
  led_set(OVMS_LED_GRN, OVMS_LED_OFF);
  led_start();

#ifdef OVMS_HW_V2
  car_12vline = inputs_voltage()*10;
  car_12vline_ref = 0;
#endif

#ifdef OVMS_ACCMODULE
  acc_initialise();
#endif

  // Proceed to main loop
  y = 0; // Last TMR0H
  while (1) // Main Loop
  {
    CHECKPOINT(0x22)
    if ((vUARTIntStatus.UARTIntRxError) ||
            (vUARTIntStatus.UARTIntRxOverFlow))
      net_reset_async();

    while (!vUARTIntStatus.UARTIntRxBufferEmpty)
    {
      CHECKPOINT(0x23)
      net_poll();
    }

    CHECKPOINT(0x24)
    vehicle_idlepoll();

    ClrWdt(); // Clear Watchdog Timer

    x = TMR0L;
    if (TMR0H >= 0x4c) // Timout ~1sec (actually 996ms)
    {
      TMR0H = 0;
      TMR0L = 0; // Reset timer
      CHECKPOINT(0x25)
      net_ticker();
      CHECKPOINT(0x26)
      vehicle_ticker();
#ifdef OVMS_LOGGINGMODULE
      CHECKPOINT(0x27)
      logging_ticker();
#endif
#ifdef OVMS_ACCMODULE
      CHECKPOINT(0x28)
      acc_ticker();
#endif
    }
    else if (TMR0H != y)
    {
      if ((TMR0H % 0x04) == 0)
      {
        CHECKPOINT(0x29)
        net_ticker10th();
        CHECKPOINT(0x2A)
        vehicle_ticker10th();
        CHECKPOINT(0x2B)
      }
      y = TMR0H;
    }
  }
}
