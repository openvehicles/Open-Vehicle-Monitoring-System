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
#include "ovms.h"

// Configuration settings
#pragma	config FCMEN = OFF,      IESO = OFF
#pragma	config PWRT = ON,        BOREN = OFF,      BORV = 0
#pragma	config WDT = OFF,        WDTPS = 32768
#if defined(__DEBUG)
  #pragma config MCLRE  = ON
  #pragma config DEBUG = ON
#else
  #pragma config MCLRE  = OFF
  #pragma config DEBUG = OFF
#endif
#pragma config OSC = HS
#pragma	config LPT1OSC = OFF,    PBADEN = OFF
#pragma	config XINST = OFF,      BBSIZ = 1024,     LVP = OFF,        STVREN = ON
#pragma	config CP0 = OFF,        CP1 = OFF,        CP2 = OFF,        CP3 = OFF
#pragma	config CPB = OFF,        CPD = OFF
#pragma	config WRT0 = OFF,       WRT1 = OFF,       WRT2 = OFF,       WRT3 = OFF
#pragma	config WRTB = OFF,       WRTC = OFF,       WRTD = OFF
#pragma	config EBTR0 = OFF,      EBTR1 = OFF,      EBTR2 = OFF,      EBTR3 = OFF
#pragma	config EBTRB = OFF

// Global data
#pragma udata
unsigned int car_linevoltage = 0; // Line Voltage
unsigned char car_chargecurrent = 0; // Charge Current
unsigned char car_chargestate = 4; // 1=charging, 2=top off, 4=done, 13=preparing to charge, 21-25=stopped charging
unsigned char car_chargemode = 0; // 0=standard, 1=storage, 3=range, 4=performance
unsigned char car_charging = 0; // 1=yes/0=no
unsigned char car_stopped = 0; // 1=yes,0=no
unsigned char car_doors = 0; //
unsigned char car_speed = 0; // speed in miles/hour
unsigned char car_SOC = 0; // State of Charge in %
unsigned int car_idealrange = 0; // Ideal Range in miles
unsigned int car_estrange = 0; // Estimated Range
unsigned long car_time; // UTC Time
signed long car_latitude = 0x16DEC6D9; // Raw GPS Latitude  (52.04246 zero in converted result)
signed long car_longitude = 0xFE444A36; // Raw GPS Longitude ( -3.94409, not verified if this is correct)
unsigned char net_reg = 0; // Network registration
unsigned char net_link = 0; // Network link status
char net_apps_connected = 0; // Network apps connected

void main(void)
  {
  unsigned char x;

  PORTA = 0x00; // Initialise port A
  ADCON1 = 0x0F; // Switch off A/D converter
  TRISB = 0xFF;

  // Timer 0 enabled, Fosc/4, 16 bit mode, prescaler 1:256
  // This gives us one tick every 51.2uS before prescale (13.1ms after)
  T0CON = 0b10000111; // @ 5Mhz => 51.2uS

  // Initialisation...
  par_initialise();
  can_initialise();
  net_initialise();

  // Proceed to main loop
  while (1) // Main Loop
    {
    if (RXB0CONbits.RXFUL) can_poll();
    if((vUARTIntStatus.UARTIntRxError) ||
       (vUARTIntStatus.UARTIntRxOverFlow))
      net_reset_async();
    if (! vUARTIntStatus.UARTIntRxBufferEmpty)
      net_poll();

    x = TMR0L;
    if (TMR0H >= 0x4c) // Timout ~1sec (actually 996ms)
      {
      TMR0H = 0; TMR0L = 0; // Reset timer
      net_ticker();
      can_ticker();
      }
    }
  }