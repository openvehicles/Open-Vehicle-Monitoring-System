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

extern rom char NET_MSG_CMDRESP[];
extern rom char NET_MSG_CMDOK[];
extern rom char NET_MSG_CMDINVALIDSYNTAX[];
extern rom char NET_MSG_CMDNOCANWRITE[];
extern rom char NET_MSG_CMDINVALIDRANGE[];
extern rom char NET_MSG_CMDNOCANCHARGE[];
extern rom char NET_MSG_CMDNOCANSTOPCHARGE[];
extern rom char NET_MSG_CMDUNIMPLEMENTED[];

#define STP_OK(buf,cmd)               stp_rom(stp_i(buf, NET_MSG_CMDRESP, cmd), NET_MSG_CMDOK)
#define STP_INVALIDSYNTAX(buf,cmd)    stp_rom(stp_i(buf, NET_MSG_CMDRESP, cmd), NET_MSG_CMDINVALIDSYNTAX)
#define STP_NOCANWRITE(buf,cmd)       stp_rom(stp_i(buf, NET_MSG_CMDRESP, cmd), NET_MSG_CMDNOCANWRITE)
#define STP_INVALIDRANGE(buf,cmd)     stp_rom(stp_i(buf, NET_MSG_CMDRESP, cmd), NET_MSG_CMDINVALIDRANGE)
#define STP_NOCANCHARGE(buf,cmd)      stp_rom(stp_i(buf, NET_MSG_CMDRESP, cmd), NET_MSG_CMDNOCANCHARGE)
#define STP_NOCANSTOPCHARGE(buf,cmd)  stp_rom(stp_i(buf, NET_MSG_CMDRESP, cmd), NET_MSG_CMDNOCANSTOPCHARGE)
#define STP_UNIMPLEMENTED(buf,cmd)    stp_rom(stp_i(buf, NET_MSG_CMDRESP, cmd), NET_MSG_CMDUNIMPLEMENTED)

extern char net_msg_serverok;
extern char net_msg_sendpending;
extern int  net_msg_cmd_code;
extern char* net_msg_cmd_msg;
extern char net_msg_scratchpad[NET_BUF_MAX];

void net_msg_init(void);
void net_msg_disconnected(void);
void net_msg_start(void);
void net_msg_send(void);
void net_msg_encode_puts(void);
void net_msg_register(void);
char net_msg_encode_statputs(char stat, WORD *oldcrc);

char net_msgp_stat(char stat);
char net_msgp_gps(char stat);
char net_msgp_tpms(char stat);
char net_msgp_firmware(char stat);
char net_msgp_environment(char stat);
char net_msgp_group(char stat, char groupnumber, char *groupname);
char net_msgp_capabilities(char stat);

void net_msg_in(char* msg);
void net_msg_cmd_in(char* msg);
void net_msg_cmd_do(void);

// Standard commands:
#define CMD_QueryFeatures       1   // ()
#define CMD_SetFeature          2   // (feature number, value)
#define CMD_QueryParams         3   // ()
#define CMD_SetParam            4   // (param number, value)
#define CMD_Reboot              5   // ()
#define CMD_Alert               6   // ()

#define CMD_SendSMS             40  // (phone number, SMS message)
#define CMD_SendUSSD            41  // (USSD_CODE)
#define CMD_SendRawAT           42  // (raw AT command)

void net_msg_forward_sms(char* caller, char* SMS);

char *net_prep_stat(char *s);
void net_msg_alert(void);
void net_msg_valettrunk(void);
void net_msg_alarm(void);
void net_msg_socalert(void);

#endif // #ifndef __OVMS_MSG_H
