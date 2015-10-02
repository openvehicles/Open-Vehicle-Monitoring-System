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

#ifndef __OVMS_PARAMS_H
#define __OVMS_PARAMS_H

// User configurable variables
#define PARAM_MAX 32
#define PARAM_MAX_LENGTH 32
#define PARAM_BANKSIZE 8

#define PARAM_REGPHONE    0x00
#define PARAM_MODULEPASS  0x01
#define PARAM_MILESKM     0x02
#define PARAM_NOTIFIES    0x03
#define PARAM_SERVERIP    0x04
#define PARAM_GPRSAPN     0x05
#define PARAM_GPRSUSER    0x06
#define PARAM_GPRSPASS    0x07
#define PARAM_VEHICLEID   0x08
#define PARAM_SERVERPASS  0x09
#define PARAM_PARANOID    0x0A
#define PARAM_S_GROUP1    0x0B
#define PARAM_S_GROUP2    0x0C
#define PARAM_GSMLOCK     0x0D
#define PARAM_VEHICLETYPE 0x0E
#define PARAM_COOLDOWN    0x0F

#define PARAM_ACC_S       0x10
#define PARAM_ACC_COUNT   4
#define PARAM_ACC_1       0x10
#define PARAM_ACC_2       0x11
#define PARAM_ACC_3       0x12
#define PARAM_ACC_4       0x13

#define PARAM_GPRSDNS     0x16
#define PARAM_TIMEZONE    0x17

#define PARAM_FEATURE_BASE 0x10
#define PARAM_FEATURE_S   0x18
#define PARAM_FEATURE8    0x18
#define PARAM_FEATURE9    0x19
#define PARAM_FEATURE10   0x1A
#define PARAM_FEATURE11   0x1B
#define PARAM_FEATURE12   0x1C
#define PARAM_FEATURE13   0x1D
#define PARAM_FEATURE14   0x1E
#define PARAM_FEATURE15   0x1F

extern char par_value[PARAM_MAX_LENGTH];

void par_initialise(void);
void par_read(unsigned char param);
void par_write(unsigned char param);
char* par_get(unsigned char param);
void par_set(unsigned char param, char* value);
void par_getbase64(unsigned char param, void* dest, size_t length);
void par_setbase64(unsigned char param, void* source, size_t length);
void par_getbin(unsigned char param, void* dest, size_t length);
void par_setbin(unsigned char param, void* source, size_t length);

#endif // #ifndef __OVMS_PARAMS_H
