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

// Integer Miles <-> Kilometer conversions
// 1 mile = 1.609344 kilometers
// smalles error approximation: mi = (km * 10 - 5) / 16 + 1
#define KM2MI(KM)       (((KM) <= 0) ? 0 : ((((KM) * 10 - 5) >> 4) + 1))
#define MI2KM(MI)       ( ( ( (MI) << 4 ) + 5 ) / 10 )

// Math utils:
#define SQR(n) ((n)*(n))
#define ABS(n) (((n) < 0) ? -(n) : (n))

void reset_cpu(void);              // Reset the cpu
void delay5b(void);                // Delay 5ms
void delay100b(void);              // Delay 100ms
void delay100(unsigned char n);    // Delay in 100ms increments
void led_net(unsigned char led);   // Change NET led
void led_act(unsigned char led);   // Change ACT led
void modem_reboot(void);           // Reboot modem
//void format_latlon(long latlon, char* dest);  // Format latitude/longitude string
#define format_latlon(latlon,dest) stp_latlon(dest,NULL,latlon)
float myatof(char *s);             // builtin atof() does not work
long gps2latlon(char *gpscoord);   // convert GPS coordinate to latlon value
WORD crc16(char *data, int length);  // Calculate a 16bit CRC and return it
void cr2lf(char *s);                // replace \r by \n in s (to convert msg text to sms)

// sprintf replacement utils: stp string print
char *stp_rom(char *dst, const rom char *val);
char *stp_ram(char *dst, const char *val);
char *stp_s(char *dst, const rom char *prefix, char *val);
char *stp_i(char *dst, const rom char *prefix, int val);
char *stp_l(char *dst, const rom char *prefix, long val);
char *stp_ul(char *dst, const rom char *prefix, unsigned long val);
void itox(unsigned int i, char *s);
char *stp_x(char *dst, const rom char *prefix, unsigned int val);
void ltox(unsigned long i, char *s);
char *stp_lx(char *dst, const rom char *prefix, unsigned long val);
char *stp_ulp(char *dst, const rom char *prefix, unsigned long val, int len, char pad);
char *stp_l2f(char *dst, const rom char *prefix, long val, int prec);
char *stp_l2f_h(char *dst, const rom char *prefix, unsigned long val, int cdecimal);
char *stp_latlon(char *dst, const rom char *prefix, long latlon);

#endif // #ifndef __OVMS_UTILS_H
