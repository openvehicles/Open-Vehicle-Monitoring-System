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

#ifndef __OVMS_UTILS_H
#define __OVMS_UTILS_H

#include <GenericTypeDefs.h>

void reset_cpu(void);              // Reset the cpu
void delay100b(void);              // Delay 100ms
void delay100(unsigned char n);    // Delay in 100ms increments
void led_net(unsigned char led);   // Change NET led
void led_act(unsigned char led);   // Change ACT led
void modem_reboot(void);           // Reboot modem
void format_latlon(long latlon, char* dest);  // Format latitude/longitude string
float myatof(char *s);             // builtin atof() does not work
long gps2latlon(char *gpscoord);   // convert GPS coordinate to latlon value
WORD crc16(char *data, int length);  // Calculate a 16bit CRC and return it

#endif // #ifndef __OVMS_UTILS_H
