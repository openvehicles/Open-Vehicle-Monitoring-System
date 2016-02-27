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
#include <stdlib.h>
#include <string.h>
#include "ovms.h"
#include "utils.h"
#include "vehicle.h"

// replace these with globals that get set via PARAMS
#define chDecimal '.'
#define chSeparator ','

// Reset the cpu
void reset_cpu(void)
  {
  _asm reset _endasm
  }

void delay5b(void)
  {
  unsigned char count = 0;

  T2CON=0b01111100;
  TMR2=0;
  PR2=255;
  while (count<6)
    {
    while (!PIR1bits.TMR2IF);
    PIR1bits.TMR2IF=0;
    count++;
    }
  }

// Delay in 5ms increments
// N.B. Interrupts (async and can) will still be handled, and queued
void delay5(unsigned char n)
  {
  while(n>0)
    {
    delay5b();
    n--;
    }
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
//  PORTCbits.RC5 = led;
  }

// Set the status of the ACT (RED) led
void led_act(unsigned char led)
  {
//  PORTCbits.RC4 = led;
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
  delay100(20);
  PORTBbits.RB0 = 1;
}

unsigned char string_to_mode(char *mode)
  {
  // Convert a string to a mode number
  if (memcmppgm2ram(mode, (char const rom far*)"STA", 3) == 0)
    return 0;
  else if (memcmppgm2ram(mode, (char const rom far*)"STO", 3) == 0)
    return 1;
  else if (memcmppgm2ram(mode, (char const rom far*)"RAN", 3) == 0)
    return 3;
  else if (memcmppgm2ram(mode, (char const rom far*)"PER", 3) == 0)
    return 4;
  else
    return 0;
  }

int timestring_to_mins(char* arg)
  {
  // Take a time string of the format HH:MM (24 hour) and return as number of minutes.
  int min = 0;
  int hour = 0;
  int sign = 1;
  int *pval = &hour;
  char ch;

  while ((ch = *arg++) != 0)
    {
    if (ch == '-')
      sign = -sign;
    else if (ch == ':')
      pval = &min;
    else
      *pval = *pval*10 + ch - '0';
    }
  return sign*(hour*60 + min);
  }

// convert miles to kilometers by multiplying by ~1.609344
unsigned long KmFromMi(unsigned long miles)
{
 unsigned long high = miles >> 16;
 unsigned long low = miles & 0xFFFF;

 // approximate 0.609344 with 39934/2^16
 // do the multiply and divide for the high and low 16 bits to
 // preserve precision and avoid overflow
 // this yields six significant digits of accuracy and doesn't
 // overflow until the results no longer fit in 32 bits
 return miles + (high * 39934) + ((low * 39934 + 0x7FFF) >> 16);
}

// convert kilometers to miles by multiplying by ~1/1.609344
unsigned long MiFromKm(unsigned long km)
{
 unsigned long high = km >> 16;
 unsigned long low = km & 0xFFFF;

 // approximate 1/1.609344 with (40722 + 1/6)/2^16
 // do the multiply and divide for the high and low 16 bits to
 // preserve precision and avoid overflow
 // this yields six significant digits of accuracy and doesn't
 // overflow for any 32-bit input value.
 return (high * 40722) + ((low * 40722 + km/6 + 0x7FFF) >> 16);
}

// decodes Julian Date to year, month, day valid from 
// http://aa.usno.navy.mil/faq/docs/JD_Formula.php
// only valid for the years 1801–2099 (because 1800 and 2100 are not leap years)
void JdToYMD(unsigned long jd, int *pyear, int *pmonth, int *pday)
  {
  long L,N,I,J,K;

  L= jd+68569;
  N= 4*L/146097;
  L= L-(146097*N+3)/4;
  I= 4000*(L+1)/1461001;
  L= L-1461*I/4+31;
  J= 80*L/2447;
  K= L-2447*J/80;
  L= J/11;
  J= J+2-12*L;
  I= 100*(N-49)+I+L;
  
  *pyear = (int)I;
  *pmonth = (int)J;
  *pday = (int)K;
}

// computes the Julian date from year, month, day
// http://aa.usno.navy.mil/faq/docs/JD_Formula.php
// only valid for the years 1801–2099 (because 1800 and 2100 are not leap years)
unsigned long JdFromYMD(int year, int month, int day)
{
  return day-32075+1461L*(year+4800+(month-14)/12)/4+367*(month-2-(month-14)/12*12)/12-3*((year+4900+(month-14)/12)/100)/4;
}

// builtin atof() does not work... returns strange values

float myatof(char *src)
{
  long whole, frac, pot;
  char *s;

  whole = atol(src);

  if (s = strchr(src, '.'))
  {
    frac = 0;
    pot = 1;

    while (*++s)
    {
      frac = frac * 10 + (*s - 48);
      pot = pot * 10;
    }

    return (float) whole + (float) frac / pot;
  }
  else
  {
    return (float) whole;
  }
}


// hex string decode:

unsigned long axtoul(char *s)
{
  unsigned long val = 0;
  unsigned char c;
  unsigned int dig = 0;

  while (s && *s) {
    c = *s | 0x20;

    if (c == 'x') {
      val = 0; // prefix 0x => reset val
    }
    else if (c >= '0' && c <= '9') {
      dig = c - '0';
      val = (val << 4) | dig;
    }
    else if (c >= 'a' && c <= 'f') {
      dig = c - 'a' + 10;
      val = (val << 4) | dig;
    }
    else {
      break; // no hex char, stop
    }

    s++;
  }

  return val;
}


// Convert GPS coordinate form DDDMM.MMMMMM to internal latlon value

long gps2latlon(char *gpscoord)
{
  float f;
  long d;

  f = myatof(gpscoord);
  d = (long) (f / 100); // extract degrees
  f = (float) d + (f - (d * 100)) / 60; // convert to decimal format
  return (long) (f * 3600 * 2048); // convert to raw format
}


// Calculate a 16bit CRC and return it
WORD crc16(char *data, int length)
  {
  WORD crc = 0xffff;
  int k;

  while (length>0)
    {
    crc ^= (BYTE)*data++;
    length--;

    for (k = 0; k < 8; ++k)
      {
      if (crc & 1)
        crc = (crc >> 1) ^ 0xA001;
      else
        crc = (crc >> 1);
      }
    }

  return crc;
}

////////////////////////////////////////////////////////////////////////
// convert GSM clock response string to timestamp
// timezone string must be set correctly to convert local time to UTC
//
// supported string format:
// +CCLK: "yy/mm/dd,hh:mm:ss+00"
unsigned long datestring_to_timestamp(const char *arg)
  {
  char aval[6];
  int ival = -1;
  char ch;
  char *ptimezone = par_get(PARAM_TIMEZONE);

  aval[0] = 0; // the rest get handled as we go
  while ((ch = *arg++) != 0 && ival < (int)DIM(aval))
    {
    switch (ch)
      {
      case '/':
      case ':':
      case ',':
        if (ival != -1)
          aval[++ival] = 0;
        break;
      case '+':
        if (ival != -1)
          ++ival;
        break;
      case '"':
        ++ival;
        break;
      default:
        if (0 <= ival && ival < DIM(aval))
          aval[ival] = aval[ival] * 10 + ch - '0';
        break;
      }
    }

  return (JdFromYMD(2000+aval[0], aval[1], aval[2]) - JDEpoch) * (24L * 3600);
              + ((aval[3] * 60L + aval[4]) * 60) + aval[5]
              - timestring_to_mins(ptimezone) * 60L;
  }

// cr2lf: replace \r by \n in s (to convert msg text to sms)
//   and '^' by '/' to prevent '?' (SIM908 SMS bug?)

void cr2lf(char *s)
{
  while (s && *s)
  {
    if (*s == '\r') *s = '\n';
    else if (*s == '^') *s = '/';
    ++s;
  }
}


// string prefix check:

BOOL starts_with(char *s, const rom char *pfx)
{
  while ((*s == *pfx) && (*pfx != 0))
  {
    pfx++;
    s++;
  }
  return (*pfx == 0);
}


// string-print rom string:

char *stp_rom(char *dst, const rom char *val)
{
  while (*dst = *val++) dst++;
  return dst;
}

// string-print ram string:

char *stp_ram(char *dst, const char *val)
{
  while (*dst = *val++) dst++;
  return dst;
}

// string-print ram string with optional prefix:

char *stp_s(char *dst, const rom char *prefix, char *val)
{
  if (prefix)
    dst = stp_rom(dst, prefix);
  return stp_ram(dst, val);
}

// string-print rom string with optional prefix:

char *stp_rs(char *dst, const rom char *prefix, const rom char *val)
{
  if (prefix)
    dst = stp_rom(dst, prefix);
  return stp_rom(dst, val);
}

// string-print integer with optional string prefix:

char *stp_i(char *dst, const rom char *prefix, int val)
{
  if (prefix)
    dst = stp_rom(dst, prefix);
  itoa(val, dst);
  while (*dst) dst++;
  return dst;
}

// string-print long with optional string prefix:

char *stp_l(char *dst, const rom char *prefix, long val)
{
  if (prefix)
    dst = stp_rom(dst, prefix);
  ltoa(val, dst);
  while (*dst) dst++;
  return dst;
}

// string-print unsigned long with optional string prefix:

char *stp_ul(char *dst, const rom char *prefix, unsigned long val)
{
  if (prefix)
    dst = stp_rom(dst, prefix);
  ultoa(val, dst);
  while (*dst) dst++;
  return dst;
}

// ltox
//  (note: fills fixed len chars with '0' padding = sprintf %04x)

void ltox(unsigned long i, char *s, unsigned int len)
{
  unsigned char n;

  s += len;
  *s = '\0';

  for (n = len; n != 0; --n)
  {
    *--s = "0123456789ABCDEF"[i & 0x0F];
    i >>= 4;
  }
}

// string-print unsigned integer hexadecimal with optional string prefix:
//  (note: fills fixed 4 chars with '0' padding = sprintf %04x)

char *stp_x(char *dst, const rom char *prefix, unsigned int val)
{
  if (prefix)
    dst = stp_rom(dst, prefix);
  ltox(val, dst, 4);
  while (*dst) dst++;
  return dst;
}

// string-print unsigned long hexadecimal with optional string prefix:
//  (note: fills fixed 8 chars with '0' padding = sprintf %08lx)

char *stp_lx(char *dst, const rom char *prefix, unsigned long val)
{
  if (prefix)
    dst = stp_rom(dst, prefix);
  ltox(val, dst, 8);
  while (*dst) dst++;
  return dst;
}

// string-print unsigned integer hexadecimal with optional string prefix:
//  (note: fills fixed 2 chars with '0' padding = sprintf %02x)

char *stp_sx(char *dst, const rom char *prefix, unsigned char val)
{
  if (prefix)
    dst = stp_rom(dst, prefix);
  ltox(val, dst, 2);
  while (*dst) dst++;
  return dst;
}

// string-print unsigned long to fixed size with padding

char *stp_ulp(char *dst, const rom char *prefix, unsigned long val, int len, char pad)
{
  char buf[11];
  char bl;

  if (prefix)
    dst = stp_rom(dst, prefix);

  ultoa(val, buf);

  for (bl = strlen(buf); bl < len; bl++)
    *dst++ = pad;

  dst = stp_ram(dst, buf);

  return dst;
}

// string-print fixed precision long as float

char *stp_l2f(char *dst, const rom char *prefix, long val, int prec)
{
  long factor;
  char p;

  for (factor = 1, p = prec; p > 0; p--)
    factor *= 10;

  dst = stp_l(dst, prefix, val / factor);
  *dst++ = '.';
  dst = stp_ulp(dst, NULL, ABS(val) % factor, prec, '0');

  return dst;
}

// string-print unsigned long as fixed point number with optional string prefix

char *stp_l2f_h(char *dst, const rom char *prefix, unsigned long val, int cdecimal)
{
  char *start, *end;
  int cch;

  if (prefix)
    dst = stp_rom(dst, prefix);

  start = dst;
  cch = 0;
  // decompose the number, writing out the digits backwards
  while (val != 0 || cch <= cdecimal)
  {
    // write out the thousands separator when needed
    if (((cch - cdecimal) % 3) == 0 && cch > cdecimal)
		*dst++ = chSeparator;

    // peel off the next digit
    *dst++ = '0' + (val % 10);
    val /= 10;

    // write out the decimal point when needed
    if (++cch == cdecimal)
        *dst++ = chDecimal;
  }
  end = dst - 1;
  // reverse the string in place
  while (start < end)
  {
    char chT = *start;
    *start++ = *end;
    *end-- = chT;
  }
  // null terminate
  *dst = 0;
  return dst;
}

// string-print Latitude/Longitude as a string

char *stp_latlon(char *dst, const rom char *prefix, long latlon)
{
  float res;

  if (prefix)
    dst = stp_rom(dst, prefix);

  if (latlon < 0)
  {
    *dst++ = '-';
    latlon = ~latlon; // and invert value
  }
  res = (float) latlon / 2048 / 3600; // Tesla specific GPS conversion
  return stp_l2f(dst, NULL, res * 1000000, 6);
}


#ifndef OVMS_NO_SMSTIME

char *stp_time(char *dst, const rom char *prefix, unsigned long timestamp)
{
  char *start, *end;
  int k;

  if (prefix)
    dst = stp_rom(dst, prefix);

  start = dst;
  for (k=0;k<3;++k)
  {
    if (k>0)
      *dst++ = ':';
    if (k==2)
      timestamp %= 24;
    *dst++ = '0' + (timestamp % 10);
    timestamp /= 10;
    *dst++ = '0' + (timestamp % 6);
    timestamp /= 6;
  }
  end = dst - 1;
  // reverse the string in place
  while (start < end)
  {
    char chT = *start;
    *start++ = *end;
    *end-- = chT;
  }
  // null terminate
  *dst = 0;
  return dst;
}

char *stp_date(char *dst, const rom char *prefix, unsigned long timestamp)
  {
  int month, day, year;	
  unsigned long jd = JDEpoch + timestamp / (24*3600L);
  JdToYMD(jd, &year, &month, &day);

  dst = stp_i(dst, prefix, year);
  dst = stp_ulp(dst, "-", month, 2, '0');
  return stp_ulp(dst, "-", day, 2, '0');
  }

#endif //OVMS_NO_SMSTIME


char *stp_mode(char *dst, const rom char *prefix, unsigned char mode)
  {
  if (prefix)
    dst = stp_rom(dst, prefix);

  switch (mode)
    {
    case 0: dst = stp_rom(dst, "standard"); break;
    case 1: dst = stp_rom(dst, "storage"); break;
    case 3: dst = stp_rom(dst, "range"); break;
    case 4: dst = stp_rom(dst, "performance"); break;
    default: dst = stp_i(dst, "mode ",mode);
    }

  return dst;
  }


#ifdef OVMS_ACCMODULE

#define GPSFromDeg(deg) ((long)((deg)*3600L*2048L))
#define Rad14FromGPS(gps) ((int)((gps)/25783L))   // gives radians * 2^14

int IntCosine14(int rad);

int FIsLatLongClose(long lat1, long long1, long lat2, long long2, int meterClose)
{
  long dlong;
  int sCosine;
  long distLong;
  long dlat = ABS(lat2 - lat1);

  // convert to meters
  long distLat = dlat / 66;

  // is the latitude separation far enough to fail the test?
  if (distLat > meterClose)
    return 0;

  // normalize the longitude separation, a slightly tricky business
  // since we can't represent angles over 291.27 degrees, so 360 is right out
  if (long1 > long2)
  {
    // swap values so long2 >= long1
    long longT = long1;
    long1 = long2;
    long2 = longT;
  }
  if (long2 > 0 && long1 < long2 - GPSFromDeg(180))
  {
    // if long2 - long1 > 180, shuffle values to compute
    // (l1 + 180) - (l2 - 180) = l1 - l2 + 360
    // while carefully avoiding overflowing 32 bits
    long longT = long1 + GPSFromDeg(180);
    long1 = long2 - GPSFromDeg(180);
    long2 = longT;
  }
  dlong = long2 - long1;

  // cosine(77) > 1/5, so here's a quick check to see if dlong is obviously too large
  if (ABS(lat1) < GPSFromDeg(80) && dlong > 66L * 5L * (long)meterClose)
    return 0;

  // no easy out, we have to do some math; compute cosine(lat1) * 2^14
  sCosine = IntCosine14(Rad14FromGPS(lat1));

  // distLong = dlong * cosine(lat1) / 66; done carefully to preserve precision
  distLong = ((((dlong & 0x3FFF) * sCosine) >> 14) + ((dlong >> 14) * sCosine)) / 66;

  // is the longitude separation large enough to fail the test?
  if (distLong > meterClose)
    return 0;

  // we're forced to do the Euclid thing
  return distLat * distLat + distLong * distLong <= (long)meterClose * meterClose;
}

// input: radians * 2^14
// output: cosine * 2^14
int IntCosine14(int rad)
{
 long cos;
 int cDouble = 0;

 if (rad < 0)
   rad = -rad;

 cDouble = 0;
 while (rad > (1L << 12)) // get the angle under a 1/4 radian
 {
   rad = (rad + 1) / 2;
   ++cDouble;
 }

 // cosine Taylor series: 1 - x^2/2!
 cos = (1L << 14);
 cos -= ((long)rad * rad + (1L << 14)) >> 15;

 while (cDouble-- > 0)
 {
   // cos(2x) = 2 * cos(x) * cos(x) - 1;
   cos = (((long)cos * cos + (1L << 12)) >> 13) - (1L << 14);
 }

 return cos;
}

#endif
