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

#include "ovms.h"
#include "led.h"

// LED data
#pragma udata
unsigned char led_code[OVMS_LED_N] = {0,0};
signed char   led_stat[OVMS_LED_N] = {0,0};
signed char   led_tick = -10;
unsigned char led_k;

// Set LED
void led_set(unsigned char led, signed char digit)
  {
  led_code[led] = digit;
  if (digit == OVMS_LED_ON)
    {
    // Turn ON the LED, immediately
    PORTC |= (1<<(OVMS_LED_O+digit));
    led_stat[led] = 0;
    }
  // N.B. We don't otherwise alter it for this run, as that would confuse the user
  }

// Restart LED sequence immediately
void led_start(void)
  {
  led_tick = 0;
  }

// Initialise the LED system
void led_initialise(void)
  {
  PORTC &= 0b11001111; // Turn off both LEDs

  // Timer 1 enabled, Fosc/4, 16 bit mode, prescaler 1:8
  T1CON = 0b10110101; // @ 5Mhz => 51.2uS / 256
  IPR1bits.TMR1IP = 0; // Low priority interrupt
  PIE1bits.TMR1IE = 1; // Enable interrupt
  TMR1L = 0;
  TMR1H = 0;
  }

void led_isr(void)
  {
  if(PIR1bits.TMR1IF)
    {
    if (led_tick < 0)
      {
      // Idle count down, with leds off as necessary
      led_tick++;
      }
    else if (led_tick == 0)
      {
      // Time to show the LED value
      for (led_k=0;led_k<OVMS_LED_N;led_k++)
        {
        led_stat[led_k] = led_code[led_k];
        if (led_code[led_k] == OVMS_LED_OFF)
          {
          PORTC &= (~(1<<(OVMS_LED_O+led_k))); // LED off
          }
        else if (led_code[led_k] == OVMS_LED_ON)
          {
          PORTC |= (1<<(OVMS_LED_O+led_k)); // LED on
          }
        else
          {
          PORTC &= (~(1<<(OVMS_LED_O+led_k))); // LED off
          }
        }
      led_tick = (OVMS_LED_S*10)+1;
      }
    else if (led_tick == 1)
      {
      // The tick is about to hit zero - end of sequence...
      led_tick = -10;
      }
    else
      {
      // Flash the LED value, if necessary
      led_tick--;
      for (led_k=0;led_k<OVMS_LED_N;led_k++)
        {
        if (led_stat[led_k] > 0)
          {
          if ((led_tick % OVMS_LED_S)==0)
            {
            led_stat[led_k]--;
            PORTC |= (1<<(OVMS_LED_O+led_k)); // LED on
            }
          else
            PORTC &= (~(1<<(OVMS_LED_O+led_k))); // LED off
          }
        else if (led_stat[led_k] == 0)
          {
          PORTC &= (~(1<<(OVMS_LED_O+led_k))); // LED off
          }
        }
      }
    PIR1bits.TMR1IF = 0;
    }
  }
