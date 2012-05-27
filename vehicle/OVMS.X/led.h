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

#ifndef __OVMS_LED_H
#define __OVMS_LED_H

#define OVMS_LED_N 2               // Number of LEDs
#define OVMS_LED_O 4               // Bit offset of first LED
#define OVMS_LED_S 3               // LED blink speed

#define OVMS_LED_OFF 0             // LED fully off
#define OVMS_LED_ON  -1            // LED fully on

#define OVMS_LED_RED 0             // Red LED
#define OVMS_LED_GRN 1             // Green LED

extern unsigned char led_code[OVMS_LED_N];

void led_set(unsigned char, signed char); // Set LED
void led_start(void);              // Restart LED sequence immediately
void led_initialise(void);        // LED Initialisation
void led_isr(void);                // LED ISR

#endif // #ifndef __OVMS_LED_H
