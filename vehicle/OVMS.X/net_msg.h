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

#ifndef __OVMS_MSG_H
#define __OVMS_MSG_H

#include "net.h"

extern char net_msg_notify;
extern char net_msg_notifyenvironment;
extern char net_msg_serverok;
extern char net_msg_sendpending;
extern int  net_msg_cmd_code;
extern char net_msg_cmd_msg[100];

void net_msg_init(void);
void net_msg_disconnected(void);
void net_msg_start(void);
void net_msg_send(void);
void net_msg_register(void);
void net_msg_stat(void);
void net_msg_gps(void);
void net_msg_tpms(void);
void net_msg_firmware(void);
void net_msg_environment(void);
void net_msg_group(char *groupname);
void net_msg_in(char* msg);
void net_msg_cmd_in(char* msg);
void net_msg_cmd_do(void);
void net_msg_forward_sms(char* caller, char* SMS);
void net_msg_alert(void);

#endif // #ifndef __OVMS_MSG_H
