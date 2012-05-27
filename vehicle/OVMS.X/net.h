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

// NET data
extern unsigned char net_state;                // The current state

#define NET_BUF_MAX 200
#define NET_TEL_MAX 20

// NET_BUF_MODES
#define NET_BUF_IPD          0xfd  // net_buf is waiting on IPD data
#define NET_BUF_SMS          0xfe  // net_buf is waiting for 2nd line of SMS
#define NET_BUF_CRLF         0xff  // net_buf is waiting for CRLF line
// otherwise the number of bytes outstanding for IP data

// STATES
#define NET_STATE_START      0x01  // Initialise and get ready to go
#define NET_STATE_SOFTRESET  0x02  // Reset and re-initialise the network
#define NET_STATE_HARDRESET  0x03  // Hard Reset the modem, then start again
#define NET_STATE_HARDSTOP   0x04  // Hard stop the modem, then reset modem, then processor
#define NET_STATE_HARDSTOP2  0x05  // Reset modem, then processor
#define NET_STATE_STOP       0x06  // Stop and reset the processor
#define NET_STATE_DOINIT     0x10  // Initialise the GSM network - SIM card check
#define NET_STATE_DOINIT2    0x11  // Initialise the GSM network - SIM PIN check
#define NET_STATE_DOINIT3    0x12  // Initialise the GSM network - Full Init
#define NET_STATE_READY      0x20  // READY and handling calls
#define NET_STATE_COPS       0x21  // GSM COPS carrier selection
#define NET_STATE_DONETINIT  0x40  // Initalise the GPRS network
#define NET_STATE_NETINITP   0x41  // Short pause during GPRS initialisation

// LED MODES
#define NET_LED_WAKEUP       10    // Attempting to wake up the modem
#define NET_LED_INITSIM1     9     // Checking SIM card insertion status
#define NET_LED_INITSIM2     8     // Checking SIM card PIN status
#define NET_LED_INITSIM3     7     // Initialising modem
#define NET_LED_COPS         6     // COPS initialisation
#define NET_LED_NETINIT      5     // GPRS NET initialisation
#define NET_LED_NETAPNOK     4     // GPRS APN is OK, final init
#define NET_LED_NETCALL      3     // GPRS Network call
#define NET_LED_READY        2     // READY state
#define NET_LED_READYGPRS    1     // READY GPRS state

// LED ERRORS
#define NET_LED_ERRLOSTSIG   1     // Lost signal
#define NET_LED_ERRMODEM     2     // Problem communicating with modem
#define NET_LED_ERRSIM1      3     // SIM is not inserted/detected
#define NET_LED_ERRSIM2      4     // PIN lock on the SIM
#define NET_LED_ERRCOPS      6     // COPS GSM lock could not be obtained
#define NET_LED_ERRGPRSRETRY 7     // Error (maybe temp) during GPRS init
#define NET_LED_ERRGPRSFAIL  8     // GPRS NET INIT failed

extern char net_scratchpad[NET_BUF_MAX];

void net_puts_rom(static const rom char *data);
void net_puts_ram(const char *data);
void net_putc_ram(const char data);

void net_initialise(void);
void net_poll(void);
void net_reset_async(void);
void net_ticker(void);
void net_ticker10th(void);

void net_state_enter(unsigned char);
void net_state_activity(void);
void net_state_ticker(void);

void net_notify_status(unsigned char notify);
void net_notify_environment(void);

#endif // #ifndef __OVMS_NET_H