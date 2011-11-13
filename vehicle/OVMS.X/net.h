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

#ifndef __OVMS_NET_H
#define __OVMS_NET_H

#define NET_BUF_MAX 100
#define NET_TEL_MAX 20

// STATES
#define NET_STATE_START      0x01  // Initialise and get ready to go
#define NET_STATE_RESET      0x02  // Reset and re-initialise the network
#define NET_STATE_DOINIT     0x10  // Initialise the GSM network
#define NET_STATE_READY      0x20  // READY and handling calls
#define NET_STATE_COPS       0x21  // GSM COPS carrier selection
#define NET_STATE_SMSIN      0x30  // Wait for SMS message text
#define NET_STATE_DONETINIT  0x40  // Initalise the GPRS network

extern char net_scratchpad[100];

void net_puts_rom(static const rom char *data);
void net_puts_ram(const char *data);
void net_putc_ram(const char data);

void net_initialise(void);
void net_poll(void);
void net_reset_async(void);
void net_ticker(void);

void net_state_enter(unsigned char);
void net_state_activity(void);
void net_state_ticker(void);

void net_notify_status(void);

#endif // #ifndef __OVMS_NET_H