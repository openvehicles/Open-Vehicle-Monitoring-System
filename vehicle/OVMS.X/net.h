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

#define NET_BUF_MAX 200
#define NET_TEL_MAX 20
#define NET_GPRS_RETRIES 10
#define NET_RXDATA_TIMEOUT 1800

// NET_BUF_MODES
#define NET_BUF_IPD          0xfd  // net_buf is waiting on IPD data
#define NET_BUF_SMS          0xfe  // net_buf is waiting for 2nd line of SMS
#define NET_BUF_CRLF         0xff  // net_buf is waiting for CRLF line
// otherwise the number of bytes outstanding for IP data

// STATES
#define NET_STATE_FIRSTRUN   0x00  // First time run
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
#define NET_STATE_COPSSETTLE 0x22  // GSM COPS wait for settle after lock
#define NET_STATE_COPSWAIT   0x23  // GSM COPS wait for stable after failed lock
#define NET_STATE_COPSWDONE  0x24  // GSM COPS wait complete
#define NET_STATE_DONETINIT  0x40  // Initalise the GPRS network
#define NET_STATE_NETINITP   0x41  // Short pause during GPRS initialisation
#define NET_STATE_NETINITCP  0x42  // Short pause during GPRS TCP reconnect
#define NET_STATE_DONETINITC 0x43  // Initalise the GPRS TCP reconnect
#define NET_STATE_DIAGMODE   0x7F  // Diagnostic mode

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
#define NET_LED_ERRCONNFAIL  9     // GPRS TCP CONNECT failed
#define NET_LED_ERRDIAGMODE  10    // DIAGNOSTIC mode

// NETINIT sub-states
#define NETINIT_START        0
#define NETINIT_CGDCONT      1
#define NETINIT_CSTT         2
#define NETINIT_CIICR        3
#define NETINIT_CIPHEAD      4
#define NETINIT_CIFSR        5
#define NETINIT_CDNSCFG      6
#define NETINIT_CLPORT       7
#define NETINIT_CIPSTART     8
#define NETINIT_CONNECTING   9

// NET data
extern unsigned char net_state;                // The current state
extern unsigned char net_state_vchar;          //   A per-state CHAR variable
extern unsigned int  net_state_vint;           //   A per-state INT variable
extern unsigned char net_cops_tries;           // A counter for COPS attempts
extern unsigned char net_timeout_goto;         // State to auto-transition to, after timeout
extern unsigned int  net_timeout_ticks;        // Number of seconds before timeout auto-transition
extern unsigned int  net_granular_tick;        // An internal ticker used to generate 1min, 5min, etc, calls
extern unsigned int  net_watchdog;             // Second count-down for network connectivity
extern unsigned int  net_timeout_rxdata;       // Second count-down for RX data timeout
extern char net_caller[NET_TEL_MAX];           // The telephone number of the caller

extern unsigned char net_buf_pos;              // Current position (aka length) in the network buffer
extern unsigned char net_buf_mode;             // Mode of the buffer (CRLF, SMS or MSG)
extern unsigned char net_buf_todo;             // Bytes outstanding on a reception
extern unsigned char net_buf_todotimeout;      // Timeout for bytes outstanding

// Generic functionality bits
extern unsigned char net_fnbits;               // Net functionality bits

#define NET_FN_INTERNALGPS    0x01             // Internal GPS Required
#define NET_FN_12VMONITOR     0x02             // Monitoring of 12V line Required
#define NET_FN_SOCMONITOR     0x04             // Monitoring of SOC Required
#define NET_FN_CARTIME        0x08             // Control of car_time required

// The NET/SMS notification system
// We have a bitmap net_notify with bits set to request a particular notification
// type, and cleared when the notification is issued.
// We also have a countdown timer net_notify_suppresscount which, if >0, signifies
// that the charge event notification should not be sent. This is used because
// we want to suppress notification of charge events in some circumstances
// (such as if the user explicitely requests a charge to be stopped).
// The idea is that requesters set these bits, and notifiers clear them.
extern unsigned int  net_notify_errorcode;     // An error code to be notified
extern unsigned long net_notify_errordata;     // Ancilliary data
extern unsigned int  net_notify_lasterrorcode; // Last error code to be notified
extern unsigned char net_notify_lastcount;     // A counter used to clear error codes
extern unsigned int  net_notify;               // Bitmap of notifications outstanding
extern unsigned char net_notify_suppresscount; // To suppress STAT notifications (seconds)

#define NET_NOTIFY_NETPART    0x00ff   // Mask for the NET part
#define NET_NOTIFY_SMSPART    0xff00   // Mask for the SMS part

#define NET_NOTIFY_NET_ENV    0x0001   // Set to 1 to NET notify environment
#define NET_NOTIFY_NET_STAT   0x0002   // Set to 1 to NET notify status
#define NET_NOTIFY_NET_CHARGE 0x0004   // Set to 1 to NET notify charge event
#define NET_NOTIFY_NET_12VLOW 0x0008   // Set to 1 to NET notify 12V alert event
#define NET_NOTIFY_NET_TRUNK  0x0010   // Set to 1 to NET notify trunk open
#define NET_NOTIFY_NET_ALARM  0x0020   // Set to 1 to NET notify alarm sounding

#define NET_NOTIFY_SMS_ENV    0x0100   // NOT IMPLEMENTED / REQUIRED
#define NET_NOTIFY_SMS_STAT   0x0200   // NOT IMPLEMENTED / REQUIRED
#define NET_NOTIFY_SMS_CHARGE 0x0400   // Set to 1 to SMS notify charge event
#define NET_NOTIFY_SMS_12VLOW 0x0800   // Set to 1 to SMS notify 12V alert event
#define NET_NOTIFY_SMS_TRUNK  0x1000   // Set to 1 to SMS notify trunk open
#define NET_NOTIFY_SMS_ALARM  0x2000   // Set to 1 to SMS notify alarm sounding

// Convenience constants for net_notify() call
#define NET_NOTIFY_ENV        NET_NOTIFY_NET_ENV
#define NET_NOTIFY_STAT       NET_NOTIFY_NET_STAT
#define NET_NOTIFY_CHARGE     NET_NOTIFY_NET_CHARGE
#define NET_NOTIFY_12VLOW     NET_NOTIFY_NET_12VLOW
#define NET_NOTIFY_TRUNK      NET_NOTIFY_NET_TRUNK
#define NET_NOTIFY_ALARM      NET_NOTIFY_NET_ALARM

extern char net_scratchpad[NET_BUF_MAX];
extern char net_buf[NET_BUF_MAX];

void net_puts_rom(const rom char *data);
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

void net_req_notification_error(unsigned int errorcode, unsigned long errordata);
void net_req_notification(unsigned int notify);

char *net_assert_caller(char *caller);

#endif // #ifndef __OVMS_NET_H