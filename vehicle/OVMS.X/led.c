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
unsigned int  led_carticker = 0;

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
  TRISC &= 0b11001111; // Set both LEDs to outputs

  led_carticker = 0;

  // Timer 1 enabled, Fosc/4, 16 bit mode, prescaler 1:8
  // Crystal specified as 20 MHz. In HS mode, the main oscillator runs at FOSC = 20 MHz.
  // (#pragma config OSC = HS)
  // Starting with 20 MHz, FOSC/4 = 5 MHz
  // The 1:8 prescaler takes that down to 625 kHz
  // TMR1H:TMR1L of 0x0000 is effectively 0x10000, or 65,536, bringing the frequency down to 9.5367431640625 Hz
  // The reciprocal is exactly 104.8576 ms
  // Conclusion: This is going to give us one interrupt every 104.8576ms
  T1CON = 0b10110101;
  IPR1bits.TMR1IP = 0; // Low priority interrupt
  PIE1bits.TMR1IE = 1; // Enable interrupt
  TMR1L = 0;
  TMR1H = 0;
  }


// ISR optimization, see http://www.xargs.com/pic/c18-isr-optim.pdf
#pragma tmpdata low_isr_tmpdata

void led_isr(void)
  {
  if(PIR1bits.TMR1IF)
    {
    if ((net_fnbits & NET_FN_CARTIME)>0)
      {
      // 104.8576ms ticks per interrupt
      // Using a marksec of 1/45681 means there would be 4790.000026 marksecs per
      // interrupt. Rounding that to 4790 for the counter increment value
      // introduces an error of -0.00000000056 seconds per interrupt, which works
      // out to losing -0.00046 seconds per day.
      // So, each interrupt, increment the counter by 4790. When the total is equal
      // to or greater than 45681, increment car_time and subtract 45681 from the
      // counter.
      // (45681/4790)*104.8576 = 1000.0000053411
      led_carticker += 4790;
      if (led_carticker >= 45681)
        {
        led_carticker -= 45681;
        car_time++;
        }
      }
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

#pragma tmpdata

