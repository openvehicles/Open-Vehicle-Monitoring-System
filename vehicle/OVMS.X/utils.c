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

#include <stdio.h>
#include "ovms.h"

// Reset the cpu
void reset_cpu(void)
  {
  _asm reset _endasm
  }

void delay100b(void)
  {
  unsigned char count = 0;

  T2CON=0b01111100;
  TMR2=0;
  PR2=255;
  while (count<122)
    {
    while (!PIR1bits.TMR2IF);
    PIR1bits.TMR2IF=0;
    count++;
    }
  }

// Delay in 100ms increments
// N.B. Interrupts (async and can) will still be handled, and queued
void delay100(unsigned char n)
  {
  while(n>0)
    {
    delay100b();
    n--;
    }
  }

// Set the status of the NET (GREEN) led
void led_net(unsigned char led)
  {
  PORTCbits.RC5 = led;
  }

// Set the status of the ACT (RED) led
void led_act(unsigned char led)
  {
  PORTCbits.RC4 = led;
  }

// Cold restart the SIM900 modem
void modem_reboot(void)
  {
  // pull PWRKEY up if it's not already pulled up
  if (PORTBbits.RB0 == 0)
  {
    PORTBbits.RB0 = 1;
    delay100(5);
  }

  // send the reset signal by pulling down PWRKEY >1s
  PORTBbits.RB0 = 0;
  delay100(12);
  PORTBbits.RB0 = 1;
  }

// Format a Latitude/Longitude as a string
// <dest> must have room to receive the output
void format_latlon(long latlon, char* dest)
  {
  float res;
  long lWhole; // Stores digits left of decimal
  unsigned long ulPart; // Stores digits right of decimal

  if (latlon < 0)
    {
    *dest++ = '-';
    latlon = ~latlon; // and invert value
    }
  res = (float) latlon / 2048 / 3600; // Tesla specific GPS conversion
  lWhole = (long) ((float) res); // As the PIC has no floating point support in printf,
  // we do it manually
  ulPart = (unsigned long) ((float) res * 1000000) - lWhole * 1000000;
  sprintf(dest, (rom far char*)"%li.%06li", lWhole, ulPart); // make sure we print leading zero's after the decimal point
  }