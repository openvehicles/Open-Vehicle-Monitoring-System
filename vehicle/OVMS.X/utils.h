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

// Math utils:
#define SQR(n) ((n)*(n))
#define ABS(n) (((n) < 0) ? -(n) : (n))
#define MIN(n,m) ((n) < (m) ? (n) : (m))
#define MAX(n,m) ((n) > (m) ? (n) : (m))
#define LIMIT_MIN(n,lim) ((n) < (lim) ? (lim) : (n))
#define LIMIT_MAX(n,lim) ((n) > (lim) ? (lim) : (n))

// let the compiler compute the size of statically allocated arrays
#define DIM(a) (sizeof(a)/sizeof(*(a)))

void reset_cpu(void);              // Reset the cpu
void delay5b(void);                // Delay 5ms
void delay5(unsigned char n);      // Delay in 5ms increments
void delay100b(void);              // Delay 100ms
void delay100(unsigned char n);    // Delay in 100ms increments
void led_net(unsigned char led);   // Change NET led
void led_act(unsigned char led);   // Change ACT led
void modem_reboot(void);           // Reboot modem
unsigned char string_to_mode(char *mode); // Convert a string to a mode number
int timestring_to_mins(char* arg); // Convert a time string to minutes

//void format_latlon(long latlon, char* dest);  // Format latitude/longitude string
#define format_latlon(latlon,dest) stp_latlon(dest,NULL,latlon)
float myatof(char *s);             // builtin atof() does not work
unsigned long axtoul(char *s);     // hex string decode
long gps2latlon(char *gpscoord);   // convert GPS coordinate to latlon value
WORD crc16(char *data, int length);  // Calculate a 16bit CRC and return it
unsigned long datestring_to_timestamp(const char *arg); // convert GSM clock response string to timestamp
void cr2lf(char *s);                // replace \r by \n in s (to convert msg text to sms)
void ltox(unsigned long i, char *s, unsigned int len); // format hexadecimal numbers

// convert miles to kilometers and vice-versa, using factor 1.609344
unsigned long KmFromMi(unsigned long miles);
unsigned long MiFromKm(unsigned long km);

// functions to convert between YMD and Julian Date
#define JDEpoch 2440588 // Julian date of the Unix epoch
void JdToYMD(unsigned long jd, int *pyear, int *pmonth, int *pday);
unsigned long JdFromYMD(int year, int month, int day);

// string prefix check (replacement for memcmppgm2ram/strncmppgm2ram)
BOOL starts_with(char *s, const rom char *pfx);

// sprintf replacement utils: stp string print
char *stp_rom(char *dst, const rom char *val);
char *stp_ram(char *dst, const char *val);
char *stp_s(char *dst, const rom char *prefix, char *val);
char *stp_rs(char *dst, const rom char *prefix, const rom char *val);
char *stp_i(char *dst, const rom char *prefix, int val);
char *stp_l(char *dst, const rom char *prefix, long val);
char *stp_ul(char *dst, const rom char *prefix, unsigned long val);
char *stp_x(char *dst, const rom char *prefix, unsigned int val);
char *stp_lx(char *dst, const rom char *prefix, unsigned long val);
char *stp_sx(char *dst, const rom char *prefix, unsigned char val);
char *stp_ulp(char *dst, const rom char *prefix, unsigned long val, int len, char pad);
char *stp_l2f(char *dst, const rom char *prefix, long val, int prec);
char *stp_l2f_h(char *dst, const rom char *prefix, unsigned long val, int cdecimal);
char *stp_latlon(char *dst, const rom char *prefix, long latlon);
char *stp_time(char *dst, const rom char *prefix, unsigned long timestamp);
char *stp_date(char *dst, const rom char *prefix, unsigned long timestamp);
char *stp_mode(char *dst, const rom char *prefix, unsigned char mode);

// longitude/latitude math
int FIsLatLongClose(long lat1, long long1, long lat2, long long2, int meterClose);

#endif // #ifndef __OVMS_UTILS_H
