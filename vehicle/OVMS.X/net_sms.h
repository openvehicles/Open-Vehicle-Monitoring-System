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
; Permission isa hereby granted, free of charge, to any person obtaining a copy
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

#ifndef __OVMS_SMS_H
#define __OVMS_SMS_H

#include "net.h"

#define NET_SMS_CMDWIDTH    16

void net_send_sms_start(char* number);
void net_send_sms_finish(void);
void net_send_sms_rom(char* number, const rom char* message);
char* net_sms_initargs(char* arguments);
char* net_sms_nextarg(char *lastarg);
void net_sms_params(char* number);
void net_sms_gps(char* number);
BOOL net_sms_stat(char* number);
void net_sms_alarm(char* number);
void net_sms_valettrunk(char* number);
BOOL net_sms_checkauth(char authmode, char *caller, char **arguments);
BOOL net_sms_in(char *caller, char *buf);
void net_sms_socalert(char* number);
void net_sms_12v_alert(char* number);

#endif // #ifndef __OVMS_SMS_H
