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
#include <usart.h>
#include <string.h>
#include <stdlib.h>
#include "ovms.h"
#include "net_sms.h"
#include "net_msg.h"
#include "utils.h"
#include "led.h"
#include "inputs.h"
#ifdef OVMS_DIAGMODULE
#include "diag.h"
#endif // #ifdef OVMS_DIAGMODULE
#ifdef OVMS_LOGGINGMODULE
#include "logging.h"
#endif // #ifdef OVMS_LOGGINGMODULE

// NET data
#pragma udata
unsigned char net_state = 0;                // The current state
unsigned char net_state_vchar = 0;          //   A per-state CHAR variable
unsigned int  net_state_vint = NET_GPRS_RETRIES; //   A per-state INT variable
unsigned char net_cops_tries = 0;           // A counter for COPS attempts
unsigned char net_timeout_goto = 0;         // State to auto-transition to, after timeout
unsigned int  net_timeout_ticks = 0;        // Number of seconds before timeout auto-transition
unsigned int  net_granular_tick = 0;        // An internal ticker used to generate 1min, 5min, etc, calls
unsigned int  net_watchdog = 0;             // Second count-down for network connectivity
unsigned int  net_timeout_rxdata = NET_RXDATA_TIMEOUT; // Second count-down for RX data timeout
char net_caller[NET_TEL_MAX] = {0};         // The telephone number of the caller

unsigned char net_buf_pos = 0;              // Current position (aka length) in the network buffer
unsigned char net_buf_mode = NET_BUF_CRLF;  // Mode of the buffer (CRLF, SMS or MSG)
unsigned char net_buf_todo = 0;             // Bytes outstanding on a reception
unsigned char net_buf_todotimeout = 0;      // Timeout for bytes outstanding

unsigned char net_fnbits = 0;               // Net functionality bits

#ifdef OVMS_SOCALERT
unsigned char net_socalert_sms = 0;         // SOC Alert (msg) 10min ticks remaining
unsigned char net_socalert_msg = 0;         // SOC Alert (sms) 10min ticks remaining
#endif //#ifdef OVMS_SOCALERT

unsigned int  net_notify_errorcode = 0;     // An error code to be notified
unsigned long net_notify_errordata = 0;     // Ancilliary data
unsigned int  net_notify_lasterrorcode = 0; // Last error code to be notified
unsigned char net_notify_lastcount = 0;     // A counter used to clear error codes
unsigned int  net_notify = 0;               // Bitmap of notifications outstanding
unsigned char net_notify_suppresscount = 0; // To suppress STAT notifications (seconds)

#pragma udata NETBUF_SP
char net_scratchpad[NET_BUF_MAX];           // A general-purpose scratchpad
#pragma udata
#pragma udata NETBUF
char net_buf[NET_BUF_MAX];                  // The network buffer itself
#pragma udata

// ROM Constants

#ifdef OVMS_INTERNALGPS
// Using internal SIM908 GPS:
rom char NET_WAKEUP_GPSON[] = "AT+CGPSPWR=1\r";
rom char NET_WAKEUP_GPSOFF[] = "AT+CGPSPWR=0\r";
rom char NET_REQGPS[] = "AT+CGPSINF=2;+CGPSINF=64;+CGPSPWR=1\r";
rom char NET_INIT1_GPSON[] = "AT+CGPSRST=0;+CSMINS?\r";
#else
// Using external GPS from car:
rom char NET_WAKEUP[] = "AT\r";
#endif

rom char NET_INIT1[] = "AT+CSMINS?\r";
rom char NET_INIT2[] = "AT+CCID;+CPBF=\"O-\";+CPIN?\r";
rom char NET_INIT3[] = "AT+IPR?;+CREG=1;+CLIP=1;+CMGF=1;+CNMI=2,2;+CSDH=1;+CIPSPRT=0;+CIPQSEND=1;+CLTS=1;E0\r";
//rom char NET_INIT3[] = "AT+IPR?;+CREG=1;+CLIP=1;+CMGF=1;+CNMI=2,2;+CSDH=1;+CIPSPRT=0;+CIPQSEND=1;+CLTS=1;E1\r";
// NOTE: changing IP mode to QSEND=0 needs handling change for "SEND OK"/"DATA ACCEPT" in net_state_activity()
rom char NET_COPS[] = "AT+COPS=0,1;+COPS?\r";

rom char NET_HANGUP[] = "ATH\r";
rom char NET_CREG_CIPSTATUS[] = "AT+CREG?;+CIPSTATUS;+CCLK?;+CSQ\r";
rom char NET_CREG_STATUS[] = "AT+CREG?\r";
rom char NET_IPR_SET[] = "AT+IPR=9600\r"; // sets fixed baud rate for the modem

////////////////////////////////////////////////////////////////////////
// The Interrupt Service Routine is standard PIC code
//
// It takes the incoming async data and puts it in an internal buffer,
// and also outputs data queued for transmission.
//
void low_isr(void);

// serial interrupt taken as low priority interrupt
#pragma code uart_int_service = 0x18
void uart_int_service(void)
  {
  _asm goto low_isr _endasm
  }
#pragma code

// ISR optimization, see http://www.xargs.com/pic/c18-isr-optim.pdf
#pragma tmpdata low_isr_tmpdata
#pragma	interruptlow low_isr nosave=section(".tmpdata")
void low_isr(void)
  {
  // call of library module function, MUST
  UARTIntISR();
  led_isr();
  }
#pragma tmpdata


////////////////////////////////////////////////////////////////////////
// net_reset_async()
// Reset the async status following an overun or other such error
//
void net_reset_async(void)
  {
  vUARTIntStatus.UARTIntRxError = 0;
  }


////////////////////////////////////////////////////////////////////////
// net_assert_caller():
// check for valid (non empty) caller, fallback to PARAM_REGPHONE
//
char *net_assert_caller(char *caller)
  {
  if (!caller || !*caller)
    {
    strcpy(net_caller, par_get(PARAM_REGPHONE));
    return(net_caller);
    }
  return caller;
  }


////////////////////////////////////////////////////////////////////////
// net_poll()
// This function is an entry point from the main() program loop, and
// gives the NET framework an opportunity to poll for data.
//
// It polls the interrupt-handler async buffer and move characters into
// net_buf, depending on net_buf_mode. This internally handles normal
// line-by-line (CRLF) data, as well as SMS (+CMT) and MSG (+IPD) modes.
//
// This function is also the dispatcher for net_state_activity(),
// to pass the incoming data to the current state for handling.
//
void net_poll(void)
  {
  unsigned char x;

  CHECKPOINT(0x30)

  while(UARTIntGetChar(&x))
    {
    if (net_buf_mode==NET_BUF_CRLF)
      { // CRLF (either normal or SMS second line) mode
      if ((net_state==NET_STATE_FIRSTRUN)||(net_state==NET_STATE_DIAGMODE))
        {
        // Special handling in FIRSTRUN and DIAGMODE
        if (x == 0x0a) // Skip 0x0a (LF)
          continue;
        else if (x == 0x0d) // Treat CR as LF
          x = 0x0a;
        else if (x == 0x08 && net_buf_pos > 0) // Backspace
          {
          net_buf[--net_buf_pos] = 0;
          continue;
          }
        else if (x == 0x01 || x == 0x03) // Ctrl-A / Ctrl-C
          {
          net_buf_pos = 0;
          net_buf[0] = 0;
          net_puts_rom("\r\n");
          continue;
          }
        }
      if (x == 0x0d) continue; // Skip 0x0d (CR)
      net_buf[net_buf_pos++] = x;
      if (net_buf_pos == NET_BUF_MAX) net_buf_pos--;
      if ((x == ':')&&(net_buf_pos>=6)&&
          (net_buf[0]=='+')&&(net_buf[1]=='I')&&
          (net_buf[2]=='P')&&(net_buf[3]=='D'))
        {
        CHECKPOINT(0x31)
        net_buf[net_buf_pos-1] = 0; // Change the ':' to an end
        net_buf_todo = atoi(net_buf+5);
        net_buf_todotimeout = 60; // 60 seconds to receive the rest
        net_buf_mode = NET_BUF_IPD;
        net_buf_pos = 0;
        continue; // We have switched to IPD mode
        }
      if (x == 0x0A) // Newline?
        {
        CHECKPOINT(0x32)
        net_buf_pos--;
        net_buf[net_buf_pos] = 0; // mark end of string for string search functions.
        if ((net_buf_pos>=4)&&
            (net_buf[0]=='+')&&(net_buf[1]=='C')&&
            (net_buf[2]=='M')&&(net_buf[3]=='T'))
          {
          x = 7;
          while ((net_buf[x++] != '\"') && (x < net_buf_pos)); // Search for start of Phone number
          net_buf[x - 1] = '\0'; // mark end of string
          net_caller[0] = '\0';
          strncpy(net_caller,net_buf+7,NET_TEL_MAX);
          net_caller[NET_TEL_MAX-1] = '\0';
          x = net_buf_pos;
          while ((net_buf[x--] != ',')&&(x>0)); // Search for last comma seperator
          net_buf_todo = atoi(net_buf+x+2); // Length of SMS message
          net_buf_todotimeout = 60; // 60 seconds to receive the rest
          net_buf_pos = 0;
          net_buf_mode = NET_BUF_SMS;
          continue;
          }
        net_timeout_rxdata = NET_RXDATA_TIMEOUT;
        net_state_activity();
        net_buf_pos = 0;
        net_buf[0] = 0;
        net_buf_mode = NET_BUF_CRLF;
        }
      }
    else if (net_buf_mode==NET_BUF_SMS)
      { // SMS data mode
      CHECKPOINT(0x33)
      if ((x==0x0d)||(x==0x0a))
        net_buf[net_buf_pos++] = ' '; // \d, \r => space
      else
        net_buf[net_buf_pos++] = x;
      if (net_buf_pos == NET_BUF_MAX) net_buf_pos--;
      net_buf_todo--;
      if (net_buf_todo==0)
        {
        net_buf[net_buf_pos] = 0; // Zero-terminate
        net_timeout_rxdata = NET_RXDATA_TIMEOUT;
        net_state_activity();
        net_buf_pos = 0;
        net_buf_mode = NET_BUF_CRLF;
        }
      }
    else
      { // IP data modea
      CHECKPOINT(0x34)
      if (x != 0x0d)
        {
        net_buf[net_buf_pos++] = x; // Swallow CR
        if (net_buf_pos == NET_BUF_MAX) net_buf_pos--;
        }
      net_buf_todo--;
      if (x == 0x0A) // Newline?
        {
        net_buf_pos--;
        net_buf[net_buf_pos] = 0; // mark end of string for string search functions.
        net_timeout_rxdata = NET_RXDATA_TIMEOUT;
        net_state_activity();
        net_buf_pos = 0;
        }
      if (net_buf_todo==0)
        {
        net_buf_pos = 0;
        net_buf_mode = NET_BUF_CRLF;
        }
      }
    }
  }

////////////////////////////////////////////////////////////////////////
// net_puts_rom()
// Transmit zero-terminated character data from ROM to the async port.
// N.B. This may block if the transmit buffer is full.
//

// Macro to wait for TxBuffer before call to PutChar():
#define UART_WAIT_PUTC(c) \
  { \
  while (vUARTIntStatus.UARTIntTxBufferFull) ; \
  while (UARTIntPutChar(c)==0) ; \
  }

void net_puts_rom(const rom char *data)
  {
  
  if (net_msg_bufpos)
    {
    // NET SMS wrapper mode: (189 byte = max MP payload)
    for (;*data && (net_msg_bufpos < (net_buf+189));data++)
      *net_msg_bufpos++ = *data;
    }

#ifdef OVMS_DIAGMODULE
  // Help diag terminals with line breaks
  else if ( net_state == NET_STATE_DIAGMODE )
    {
    char lastdata = 0;

    while(1)
      {
      if ( *data == '\n' && lastdata != '\r' )
        UART_WAIT_PUTC('\r') // insert \r before \n if missing:
      else if( lastdata == '\r' && *data != '\n' )
         UART_WAIT_PUTC('\n') // insert \n after \r if missing

      if ( !*data )
        break;

      // output char
      UART_WAIT_PUTC(*data)

      lastdata = *data++;
      }
    }
#endif // OVMS_DIAGMODULE

  else
    {
    // Send characters up to the null
    for (;*data;data++)
      UART_WAIT_PUTC(*data)
    }
  }

////////////////////////////////////////////////////////////////////////
// net_puts_ram()
// Transmit zero-terminated character data from RAM to the async port.
// N.B. This may block if the transmit buffer is full.
//
void net_puts_ram(const char *data)
  {

  if (net_msg_bufpos)
    {
    // NET SMS wrapper mode: (189 byte = max MP payload)
    for (;*data && (net_msg_bufpos < (net_buf+189));data++)
      *net_msg_bufpos++ = *data;
    }

#ifdef OVMS_DIAGMODULE
  // Help diag terminals with line breaks
  else if( net_state == NET_STATE_DIAGMODE )
    {
    char lastdata = 0;

    while(1)
      {
      if ( *data == '\n' && lastdata != '\r' )
        UART_WAIT_PUTC('\r') // insert \r before \n if missing
      else if( lastdata == '\r' && *data != '\n' )
        UART_WAIT_PUTC('\n') // insert \n after \r if missing

      if ( !*data )
        break;

      // output char
      UART_WAIT_PUTC(*data)

      lastdata = *data++;
      }
    }
#endif // OVMS_DIAGMODULE

  else
    {
    // Send characters up to the null
    for (;*data;data++)
      UART_WAIT_PUTC(*data)
    }
  }

////////////////////////////////////////////////////////////////////////
// net_putc_ram()
// Transmit a single character from RAM to the async port.
// N.B. This may block if the transmit buffer is full.
void net_putc_ram(const char data)
  {
  if (net_msg_bufpos)
    {
    // NET SMS wrapper mode: (189 byte = max MP payload)
    if (net_msg_bufpos < (net_buf+189))
      *net_msg_bufpos++ = data;
    }
  else
    {
    // Send one character
    UART_WAIT_PUTC(data)
    }
  }

////////////////////////////////////////////////////////////////////////
// net_req_notification_error()
// Request notification of an error
void net_req_notification_error(unsigned int errorcode, unsigned long errordata)
  {
  if (errorcode != 0)
    {
    // We have an error being set
    if ((errorcode != net_notify_lasterrorcode)&&
        ((sys_features[FEATURE_CARBITS]&FEATURE_CB_SVALERTS)==0))
      {
      // This is a new error, so set it and time it out after 60 seconds
      net_notify_errorcode = errorcode;
      net_notify_errordata = errordata;
      net_notify_lasterrorcode = errorcode;
      net_notify_lastcount = 60;
      }
    else
      {
      // Reset the timer for another 60 seconds
      net_notify_lastcount = 60;
      }
    }
  else
    {
    // Clear the error
    net_notify_errorcode = 0;
    net_notify_errordata = 0;
    net_notify_lasterrorcode = 0;
    net_notify_lastcount = 0;
    }
  }

////////////////////////////////////////////////////////////////////////
// net_req_notification()
// Request notification of one or more of the types specified
// in net.h NET_NOTIFY_*
//
void net_req_notification(unsigned int notify)
  {
  char *p;
  p = par_get(PARAM_NOTIFIES);
  if (strstrrampgm(p,(char const rom far*)"SMS") != NULL)
    {
    net_notify |= (notify<<8); // SMS notification flags are top 8 bits
    }
  if (strstrrampgm(p,(char const rom far*)"IP") != NULL)
    {
    net_notify |= notify;      // NET notification flags are bottom 8 bits
    }
  }

////////////////////////////////////////////////////////////////////////
// net_phonebook: SIM phonebook auto provisioning system
//
// Usage:
// Create phonebook entries like "O-8-MYVEHICLEID", "O-5-APN", etc.
// The O- prefix tells us this is OVMS, the next number is the parameter
// number to set, and the remainder is the value.
// This is very flexible, but the textual fields are limited in length
// (typically 16 characters or so).
// 
void net_phonebook(char *pb)
  {
  // We have a phonebook entry line from the SIM card / modem
  // Looking like: +CPBF: 1,"0",129,"O-X-Y"
  char *n,*p, *ep;

  n = strtokpgmram(pb,"\"");
  if (n==NULL) return;
  n = strtokpgmram(NULL,"\"");
  if (n==NULL) return;
  p = strtokpgmram(NULL,"\"");
  if (p==NULL) return;
  p = strtokpgmram(NULL,"\"");
  if (p==NULL) return;

  // At this point, *n is the second token (the number), and *p is
  // the fourth token (the parameter). Both are defined.
  if ((p[0]=='O')&&(p[1]=='-'))
    {
    n = strtokpgmram(p+2,"-");
    if (n==NULL) return;
    p = strtokpgmram(NULL,"-");
    ep = par_get(atoi(n));
    if (p==NULL)
      {
      if (ep[0] != 0)
        par_set((unsigned char)atoi(n), (char*)"");
      }
    else
      {
      if (strcmp(ep,p) != 0)
        par_set((unsigned char)atoi(n), p);
      }
    }
  }

////////////////////////////////////////////////////////////////////////
// net_state_enter(newstate)
// State Model: A new state has been entered.
// This should do any initialisation and other actions required on
// a per-state basis. It is called when the state is entered.
//
void net_state_enter(unsigned char newstate)
  {
  char *p;

// Enable for debugging (via serial port) of state transitions
//  p = stp_x(net_scratchpad, "\r\n# ST-ENTER: ", newstate);
//  p = stp_rom(p, "\r\n");
//  net_puts_ram(net_scratchpad);
//  delay100(1);
  
  net_state = newstate;
  switch(net_state)
    {
    case NET_STATE_FIRSTRUN:
      net_msg_bufpos = NULL;
      net_timeout_rxdata = NET_RXDATA_TIMEOUT;
      led_set(OVMS_LED_GRN,OVMS_LED_ON);
      led_set(OVMS_LED_RED,OVMS_LED_ON);
      led_start();
      net_timeout_goto = NET_STATE_START;
      net_timeout_ticks = 10; // Give everything time to start slowly
      break;
#ifdef OVMS_DIAGMODULE
    case NET_STATE_DIAGMODE:
      diag_initialise();
      break;
#endif // #ifdef OVMS_DIAGMODULE
    case NET_STATE_START:
      led_set(OVMS_LED_GRN,NET_LED_WAKEUP);
      led_set(OVMS_LED_RED,OVMS_LED_OFF);
      net_timeout_goto = NET_STATE_HARDRESET;
      net_timeout_ticks = 20; // modem cold start takes 5 secs, warm restart takes 2 secs, 3 secs required for autobuad sync, 20 secs should be sufficient for everything
      net_apps_connected = 0;
      net_msg_init();
      break;
    case NET_STATE_SOFTRESET:
      net_timeout_goto = 0;
      break;
    case NET_STATE_HARDRESET:
      led_set(OVMS_LED_GRN,OVMS_LED_ON);
      led_set(OVMS_LED_RED,OVMS_LED_ON);
      led_start();
      modem_reboot();
      net_timeout_goto = NET_STATE_SOFTRESET;
      net_timeout_ticks = 2;
      net_state_vchar = 0;
      net_apps_connected = 0;
      net_msg_disconnected();
      net_cops_tries = 0; // Reset the COPS counter
      break;
    case NET_STATE_HARDSTOP:
      net_timeout_goto = NET_STATE_HARDSTOP2;
      net_timeout_ticks = 10;
      net_state_vchar = NETINIT_START;
      net_apps_connected = 0;
      net_msg_disconnected();
      delay100(10);
      net_puts_rom("AT+CIPSHUT\r");
      break;
    case NET_STATE_HARDSTOP2:
      net_timeout_goto = NET_STATE_STOP;
      net_timeout_ticks = 2;
      net_state_vchar = 0;
      modem_reboot();
      break;
    case NET_STATE_STOP:
      led_set(OVMS_LED_GRN,OVMS_LED_OFF);
      led_set(OVMS_LED_RED,OVMS_LED_OFF);
      CHECKPOINT(0xF0)
      reset_cpu();
      break;
    case NET_STATE_DOINIT:
      led_set(OVMS_LED_GRN,NET_LED_INITSIM1);
      led_set(OVMS_LED_RED,OVMS_LED_OFF);
      net_timeout_goto = NET_STATE_HARDRESET;
      net_timeout_ticks = 95;
      net_state_vchar = 0;
#ifdef OVMS_INTERNALGPS
      // Using internal SIMx08 GPS:
      if ((net_fnbits & NET_FN_INTERNALGPS) != 0)
        {
        net_puts_rom(NET_INIT1_GPSON);
        }
      else
        {
        net_puts_rom(NET_INIT1);
        }
#else
      // Using external GPS from car:
      net_puts_rom(NET_INIT1);
#endif
      break;
    case NET_STATE_DOINIT2:
      led_set(OVMS_LED_GRN,NET_LED_INITSIM2);
      led_set(OVMS_LED_RED,OVMS_LED_OFF);
      net_timeout_goto = NET_STATE_HARDRESET;
      net_timeout_ticks = 95;
      net_state_vchar = 0;
      net_puts_rom(NET_INIT2);
      break;
    case NET_STATE_DOINIT3:
      led_set(OVMS_LED_GRN,NET_LED_INITSIM3);
      led_set(OVMS_LED_RED,OVMS_LED_OFF);
      net_timeout_goto = NET_STATE_HARDRESET;
      net_timeout_ticks = 35;
      net_state_vchar = 0;
      net_puts_rom(NET_INIT3);
      break;
    case NET_STATE_NETINITP:
      led_set(OVMS_LED_GRN,NET_LED_NETINIT);
      if (--net_state_vint > 0)
        {
        led_set(OVMS_LED_RED,NET_LED_ERRGPRSRETRY);
        net_timeout_goto = NET_STATE_DONETINIT;
        }
      else
        {
        led_set(OVMS_LED_RED,NET_LED_ERRGPRSFAIL);
        net_timeout_goto = NET_STATE_SOFTRESET;
        }
      net_timeout_ticks = (NET_GPRS_RETRIES-net_state_vint)*15;
      break;
    case NET_STATE_NETINITCP:
      led_set(OVMS_LED_GRN,NET_LED_NETINIT);
      net_puts_rom("AT+CIPCLOSE\r");
      led_set(OVMS_LED_RED,NET_LED_ERRCONNFAIL);
      if (--net_state_vint > 0)
        {
        net_timeout_goto = NET_STATE_DONETINITC;
        }
      else
        {
        net_timeout_goto = NET_STATE_SOFTRESET;
        }
      net_timeout_ticks = (NET_GPRS_RETRIES-net_state_vint)*15;
      break;
    case NET_STATE_DONETINITC:
      led_set(OVMS_LED_GRN,NET_LED_NETINIT);
      led_set(OVMS_LED_RED,OVMS_LED_OFF);
      net_watchdog=0; // Disable watchdog, as we have connectivity
      net_reg = 0x05; // Assume connectivity (as COPS worked)
      net_timeout_goto = NET_STATE_SOFTRESET;
      net_timeout_ticks = 60;
      net_state_vchar = NETINIT_CLPORT;
      net_apps_connected = 0;
      net_msg_disconnected();
      delay100(2);
      net_puts_rom("AT+CLPORT=\"TCP\",\"6867\"\r");
      net_state = NET_STATE_DONETINIT;
      break;
    case NET_STATE_DONETINIT:
      led_set(OVMS_LED_GRN,NET_LED_NETINIT);
      led_set(OVMS_LED_RED,OVMS_LED_OFF);
      net_watchdog=0; // Disable watchdog, as we have connectivity
      net_reg = 0x05; // Assume connectivity (as COPS worked)
      p = par_get(PARAM_GPRSAPN);
      if ((p[0] != '\0') && inputs_gsmgprs()) // APN defined AND switch is set to GPRS mode
        {
        net_timeout_goto = NET_STATE_SOFTRESET;
        net_timeout_ticks = 60;
        net_state_vchar = NETINIT_START;
        net_apps_connected = 0;
        net_msg_disconnected();
        delay100(2);
        net_puts_rom("AT+CIPSHUT\r");
        break;
        }
      else
        net_state = NET_STATE_READY;
      // N.B. Drop through without a break
    case NET_STATE_READY:
      led_set(OVMS_LED_GRN,NET_LED_READY);
      led_set(OVMS_LED_RED,OVMS_LED_OFF);
      net_msg_sendpending = 0;
      net_timeout_goto = 0;
      net_state_vchar = 0;
      if ((net_reg != 0x01)&&(net_reg != 0x05))
        net_watchdog = 120; // I need a network within 2 mins, else reset
      else
        net_watchdog = 0; // Disable net watchdog
      break;
    case NET_STATE_COPS:
      led_set(OVMS_LED_GRN,NET_LED_COPS);
      led_set(OVMS_LED_RED,OVMS_LED_OFF);
      delay100(2);
      net_timeout_goto = NET_STATE_HARDRESET;
      net_timeout_ticks = 120;
      net_msg_disconnected();
      p = par_get(PARAM_GSMLOCK);
      if (*p==0)
        {
        net_puts_rom(NET_COPS);
        }
      else
        {
        net_puts_rom("AT+COPS=1,1,\"");
        net_puts_ram(p);
        net_puts_rom("\";+COPS?\r");
        }
      break;
    case NET_STATE_COPSSETTLE:
      net_timeout_ticks = 10;
      net_timeout_goto = NET_STATE_DONETINIT;
      break;
    case NET_STATE_COPSWAIT:
      led_set(OVMS_LED_GRN,NET_LED_COPS);
      led_set(OVMS_LED_RED,OVMS_LED_OFF);
      net_timeout_goto = NET_STATE_COPSWDONE;
      net_timeout_ticks = 300;
      break;
    case NET_STATE_COPSWDONE:
      if (net_cops_tries++ < 20)
        {
        net_state_enter(NET_STATE_SOFTRESET); // Reset the entire async
        }
      else
        {
        net_state_enter(NET_STATE_HARDRESET); // Reset the entire async
        }
      break;
    }
  }

////////////////////////////////////////////////////////////////////////
// net_state_activity()
// State Model: Some async data has been received
// This is called to indicate to the state that a complete piece of async
// data has been received in net_buf (with length net_buf_pos, and mode
// net_buf_mode), and the data should be completely handled before
// returning.
//
void net_state_activity()
  {
  char *b;

  CHECKPOINT(0x35)

  if (net_buf_mode == NET_BUF_SMS)
    {
    // An SMS has arrived, and net_caller has been primed
    if ((net_reg != 0x01)&&(net_reg != 0x05))
      { // Treat this as a network registration
      net_watchdog=0; // Disable watchdog, as we have connectivity
      net_reg = 0x05;
      led_set(OVMS_LED_RED,OVMS_LED_OFF);
    }
    CHECKPOINT(0x36)
    net_sms_in(net_caller,net_buf);
    CHECKPOINT(0x35)
    return;
    }
  else if (net_buf_mode != NET_BUF_CRLF)
    {
    // An IP data message has arrived
    CHECKPOINT(0x37)
    net_msg_in(net_buf);
    CHECKPOINT(0x35)
    // Getting GPRS data from the server means our connection was good
    net_state_vint = NET_GPRS_RETRIES; // Count-down for DONETINIT attempts
    return;
    }

  if ((net_buf_pos >= 8)&&
      (memcmppgm2ram(net_buf, (char const rom far*)"*PSUTTZ:", 8) == 0))
    {
    // We have a time source from the GSM provider
    // e.g.; *PSUTTZ: 2013, 12, 16, 15, 45, 17, "+32", 1
    return;
    }

  switch (net_state)
    {
#ifdef OVMS_DIAGMODULE
    case NET_STATE_FIRSTRUN:
      if ((net_buf_pos >= 5)&&
          (memcmppgm2ram(net_buf, (char const rom far*)"SETUP", 5) == 0))
        {
        net_state_enter(NET_STATE_DIAGMODE);
        }
      break;
#endif // #ifdef OVMS_DIAGMODULE
    case NET_STATE_START:
      if ((net_buf_pos >= 2)&&(net_buf[0] == 'O')&&(net_buf[1] == 'K'))
        {
        // OK response from the modem
        led_set(OVMS_LED_RED,OVMS_LED_OFF);
        net_state_enter(NET_STATE_DOINIT);
        }
      break;
    case NET_STATE_DOINIT:
      if ((net_buf_pos >= 4)&&(net_buf[0]=='+')&&(net_buf[1]=='C')&&(net_buf[2]=='S')&&(net_buf[3]=='M'))
        {
        if (net_buf[strlen(net_buf)-1] != '1')
          {
          // The SIM card is not inserted
          led_set(OVMS_LED_RED,NET_LED_ERRSIM1);
          net_state_vchar = 1;
          }
        }
      else if ((net_state_vchar==0)&&(net_buf_pos >= 2)&&(net_buf[0] == 'O')&&(net_buf[1] == 'K'))
        {
        // The SIM card is inserted
        led_set(OVMS_LED_RED,OVMS_LED_OFF);
        net_state_enter(NET_STATE_DOINIT2);
        }
      break;
    case NET_STATE_DOINIT2:
      if ((net_buf_pos >= 16)&&(net_buf[0]=='8')&&(net_buf[1]=='9'))
        {
        // Looks like the ICCID
        strncpy(net_iccid,net_buf,MAX_ICCID);
        net_iccid[MAX_ICCID-1] = 0;
        }
      else if ((net_buf_pos >= 8)&&(net_buf[0]=='+')&&(net_buf[1]=='C')&&(net_buf[2]=='P')&&
               (net_buf[3]=='B')&&(net_buf[4]=='F')&&(net_buf[5]==':'))
        {
        net_phonebook(net_buf);
        }
      else if ((net_buf_pos >= 8)&&(net_buf[0]=='+')&&(net_buf[1]=='C')&&(net_buf[2]=='P')&&(net_buf[3]=='I'))
        {
        if (net_buf[7] != 'R')
          {
          // The SIM card has some form of pin lock
          led_set(OVMS_LED_RED,NET_LED_ERRSIM2);
          net_state_vchar = 1;
          }
        }
      else if ((net_state_vchar==0)&&(net_buf_pos >= 2)&&(net_buf[0] == 'O')&&(net_buf[1] == 'K'))
        {
        // The SIM card has no pin lock
        led_set(OVMS_LED_RED,OVMS_LED_OFF);
        net_state_enter(NET_STATE_DOINIT3);
        }
      break;
    case NET_STATE_DOINIT3:
      if ((net_buf_pos >= 6)&&(net_buf[0] == '+')&&(net_buf[1] == 'I')&&(net_buf[2] == 'P')&&(net_buf[3] == 'R')&&(net_buf[6] != '9'))
        {
        // +IPR != 9600
        // SET IPR (baudrate)
        net_puts_rom(NET_IPR_SET);
        }
      else if ((net_buf_pos >= 2)&&(net_buf[0] == 'O')&&(net_buf[1] == 'K'))
        {
        led_set(OVMS_LED_RED,OVMS_LED_OFF);
        net_state_enter(NET_STATE_COPS);
        }
      break;
    case NET_STATE_COPS:
      if ((net_buf_pos >= 2)&&(net_buf[0] == 'O')&&(net_buf[1] == 'K'))
        {
        net_state_vint = NET_GPRS_RETRIES; // Count-down for DONETINIT attempts
        net_cops_tries = 0; // Successfully out of COPS
        net_state_enter(NET_STATE_COPSSETTLE); // COPS reconnect was OK
        }
      else if ( ((net_buf_pos >= 5)&&(net_buf[0] == 'E')&&(net_buf[1] == 'R')) ||
              (memcmppgm2ram(net_buf, (char const rom far*)"+CME ERROR", 10) == 0) )
        {
        net_state_enter(NET_STATE_COPSWAIT); // Try to wait a bit to see if we get a CREG
        }
      else if (memcmppgm2ram(net_buf, (char const rom far*)"+COPS:", 6) == 0)
        {
        // COPS network registration
        b = strtokpgmram(net_buf,"\"");
        if (b != NULL)
          {
          b = strtokpgmram(NULL,"\"");
          if (b != NULL)
            {
            strncpy(car_gsmcops,b,8);
            car_gsmcops[8] = 0;
            }
          }
        }
      break;
    case NET_STATE_COPSWAIT:
      if (memcmppgm2ram(net_buf, (char const rom far*)"+CREG", 5) == 0)
        { // "+CREG" Network registration
        if (net_buf[8]==',')
          net_reg = net_buf[9]&0x07; // +CREG: 1,x
        else
          net_reg = net_buf[7]&0x07; // +CREG: x
        if ((net_reg == 0x01)||(net_reg == 0x05)) // Registered to network?
          {
          net_state_vint = NET_GPRS_RETRIES; // Count-down for DONETINIT attempts
          net_cops_tries = 0; // Successfully out of COPS
          net_state_enter(NET_STATE_DONETINIT); // COPS reconnect was OK
          }
        }
      break;
    case NET_STATE_DONETINIT:
      if ((net_buf_pos >= 2)&&
               (net_buf[0] == 'E')&&
               (net_buf[1] == 'R'))
        {
        if ((net_state_vchar == NETINIT_CSTT)||
                (net_state_vchar == NETINIT_CIICR))// ERROR response to AT+CSTT OR AT+CIICR
          {
          // try setting up GPRS again, after short pause
          net_state_enter(NET_STATE_NETINITP);
          }
        else if (net_state_vchar == NETINIT_CIFSR) // ERROR response to AT+CIFSR
          {
          // This is a nasty case. The GPRS has locked up.
          // The only solution I can find is a hard reset of the modem
          net_state_enter(NET_STATE_HARDRESET);
          }
        }
      else if ((net_buf_pos >= 2)&&
          (((net_buf[0] == 'O')&&(net_buf[1] == 'K')))|| // OK
          (((net_buf[0] == 'S')&&(net_buf[1] == 'H')))||  // SHUT OK
          (net_state_vchar == NETINIT_CIFSR)) // Local IP address
        {
        net_buf_pos = 0;
        net_timeout_ticks = 30;
        net_link = 0;
        delay100(2);
        switch (++net_state_vchar)
          {
          case NETINIT_CGDCONT:
            net_puts_rom("AT+CGDCONT=1,\"IP\",\"");
            net_puts_ram(par_get(PARAM_GPRSAPN));
            net_puts_rom("\"\r");
            break;
          case NETINIT_CSTT:
            net_puts_rom("AT+CSTT=\"");
            net_puts_ram(par_get(PARAM_GPRSAPN));
            net_puts_rom("\",\"");
            net_puts_ram(par_get(PARAM_GPRSUSER));
            net_puts_rom("\",\"");
            net_puts_ram(par_get(PARAM_GPRSPASS));
            net_puts_rom("\"\r");
            break;
          case NETINIT_CIICR:
            led_set(OVMS_LED_GRN,NET_LED_NETAPNOK);
            net_puts_rom("AT+CIICR\r");
            break;
          case NETINIT_CIPHEAD:
            net_puts_rom("AT+CIPHEAD=1\r");
            break;
          case NETINIT_CIFSR:
            net_puts_rom("AT+CIFSR\r");
            break;
          case NETINIT_CDNSCFG:
            b = par_get(PARAM_GPRSDNS);
            if (*b == 0)
              net_puts_rom("AT\r");
            else
              {
              net_puts_rom("AT+CDNSCFG=\"");
              net_puts_ram(b);
              net_puts_rom("\"\r");
              }
            break;
          case NETINIT_CLPORT:
            net_puts_rom("AT+CLPORT=\"TCP\",\"6867\"\r");
            break;
          case NETINIT_CIPSTART:
            led_set(OVMS_LED_GRN,NET_LED_NETCALL);
            net_puts_rom("AT+CIPSTART=\"TCP\",\"");
            net_puts_ram(par_get(PARAM_SERVERIP));
            net_puts_rom("\",\"6867\"\r");
            break;
          case NETINIT_CONNECTING:
            net_state_enter(NET_STATE_READY);
            break;
          }
        }
      else if ((net_buf_pos>=7)&&
               (memcmppgm2ram(net_buf, (char const rom far*)"+CREG: 0", 8) == 0))
        { // Lost network connectivity during NETINIT
        net_state_enter(NET_STATE_SOFTRESET);
        }
      else if (memcmppgm2ram(net_buf, (char const rom far*)"+PDP: DEACT", 11) == 0)
        { // PDP couldn't be activated - try again...
        net_state_enter(NET_STATE_SOFTRESET);
        }
      break;
    case NET_STATE_READY:
      if (memcmppgm2ram(net_buf, (char const rom far*)"+CREG", 5) == 0)
        { // "+CREG" Network registration
        if (net_buf[8]==',')
          net_reg = net_buf[9]&0x07; // +CREG: 1,x
        else
          net_reg = net_buf[7]&0x07; // +CREG: x
        if ((net_reg == 0x01)||(net_reg == 0x05)) // Registered to network?
          {
          net_watchdog=0; // Disable watchdog, as we have connectivity
          led_set(OVMS_LED_RED,OVMS_LED_OFF);
          }
        else if (net_watchdog == 0)
          {
          net_watchdog = 120; // We need connectivity within 120 seconds
          led_set(OVMS_LED_RED,NET_LED_ERRLOSTSIG);
          }
        }
      else if (memcmppgm2ram(net_buf, (char const rom far*)"+CLIP", 5) == 0)
        { // Incoming CALL
        if ((net_reg != 0x01)&&(net_reg != 0x05))
          { // Treat this as a network registration
          net_watchdog=0; // Disable watchdog, as we have connectivity
          net_reg = 0x05;
          led_set(OVMS_LED_RED,OVMS_LED_OFF);
          }
        delay100(1);
        net_puts_rom(NET_HANGUP);
        }
      else if (memcmppgm2ram(net_buf, (char const rom far*)"+CCLK", 5) == 0)
        {
        // local clock update
        // e.g.; +CCLK: "13/12/16,22:01:39+32"
        if ((net_fnbits & NET_FN_CARTIME)>0)
          {
          unsigned long newtime = datestring_to_timestamp(net_buf+7);
          if (newtime != car_time)
            {
            // Need to adjust the car_time
            if (newtime > car_time)
              {
              unsigned long diff = newtime - car_time;
              if (car_parktime!=0) { car_parktime += diff; }
              car_time += diff;
              }
            else
              {
              unsigned long diff = car_time - newtime;
              if (car_parktime!=0) { car_parktime -= diff; }
              car_time -= diff;
              }
            }
          }
        }
#ifdef OVMS_INTERNALGPS
      else if ((memcmppgm2ram(net_buf, (char const rom far*)"2,", 2) == 0)&&
               ((net_fnbits & NET_FN_INTERNALGPS)>0))
        {
        // Incoming GPS coordinates
        // NMEA format $GPGGA: Global Positioning System Fixed Data
        // 2,<Time>,<Lat>,<NS>,<Lon>,<EW>,<Fix>,<SatCnt>,<HDOP>,<Alt>,<Unit>,...

        long lat, lon;
        char ns, ew;
        char fix;
        int alt;

        // Parse string:
        if( b = strtokpgmram( net_buf+2, "," ) )
            ;                                     // Time
        if( b = strtokpgmram( NULL, "," ) )
            lat = gps2latlon( b );                // Latitude
        if( b = strtokpgmram( NULL, "," ) )
            ns = *b;                              // North / South
        if( b = strtokpgmram( NULL, "," ) )
            lon = gps2latlon( b );                // Longitude
        if( b = strtokpgmram( NULL, "," ) )
            ew = *b;                              // East / West
        if( b = strtokpgmram( NULL, "," ) )
            fix = *b;                             // Fix (0/1)
        if( b = strtokpgmram( NULL, "," ) )
            ;                                     // Satellite count
        if( b = strtokpgmram( NULL, "," ) )
            ;                                     // HDOP
        if( b = strtokpgmram( NULL, "," ) )
            alt = atoi( b );                      // Altitude

        if( b )
          {
          // data set complete, store:

          car_gpslock = fix & 0x01;

          if( car_gpslock )
            {
            if( ns == 'S' ) lat = ~lat;
            if( ew == 'W' ) lon = ~lon;

            car_latitude = lat;
            car_longitude = lon;
            car_altitude = alt;

            car_stale_gps = 120; // Reset stale indicator
            }
          else
            {
            car_stale_gps = 0;
            }
         }

      }
    else if ((memcmppgm2ram(net_buf, (char const rom far*)"64,", 3) == 0)&&
             ((net_fnbits & NET_FN_INTERNALGPS)>0))
      {
      // Incoming GPS coordinates
      // NMEA format $GPVTG: Course over ground
      // 64,<Course>,<Ref>,...

      int dir;

      // Parse string:
      if( b = strtokpgmram( net_buf+3, "," ) )
        dir = atoi( b );                      // Course

      if( b )
        {
        // data set complete, store:
        if( car_gpslock )
          {
          car_direction = dir;
          }
        }

      }
#endif
      else if (memcmppgm2ram(net_buf, (char const rom far*)"CONNECT OK", 10) == 0)
        {
        if (net_link == 0)
          {
          led_set(OVMS_LED_GRN,NET_LED_READYGPRS);
          net_msg_start();
          net_msg_register();
          net_msg_send();
          }
        net_link = 1;
        }
      else if (memcmppgm2ram(net_buf, (char const rom far*)"STATE: ", 7) == 0)
        { // Incoming CIPSTATUS
        if (memcmppgm2ram(net_buf, (char const rom far*)"STATE: CONNECT OK", 17) == 0)
          {
          if (net_link == 0)
            {
            led_set(OVMS_LED_GRN,NET_LED_READYGPRS);
            net_msg_start();
            net_msg_register();
            net_msg_send();
            }
          net_link = 1;
          }
        else if (memcmppgm2ram(net_buf, (char const rom far*)"STATE: TCP CONNECTING", 21) == 0)
          {
          // Connection in progress, ignore it...
          }
        else if (memcmppgm2ram(net_buf, (char const rom far*)"STATE: TCP CLOSED", 17) == 0)
          {
          // Re-initialize TCP socket, after short pause
          net_msg_disconnected();
          net_state_enter(NET_STATE_NETINITCP);
          }
        else
          {
          net_link = 0;
          led_set(OVMS_LED_GRN,NET_LED_READY);
          if ((net_reg == 0x01)||(net_reg == 0x05))
            {
            // We have a GSM network, but CIPSTATUS is not up
            net_msg_disconnected();
            // try setting up GPRS again, after short pause
            net_state_enter(NET_STATE_NETINITP);
            }
          }
        }
      else if (memcmppgm2ram(net_buf, (char const rom far*)"+CSQ:", 5) == 0)
        {
        // Signal Quality
          if (net_buf[8]==',')  // two digits
             net_sq = (net_buf[6]&0x07)*10 + (net_buf[7]&0x07);
          else net_sq = net_buf[6]&0x07;
        }
      else if (memcmppgm2ram(net_buf, (char const rom far*)"SEND OK", 7) == 0)
        {
        // CIPSEND success response in QSEND=0 mode
        //net_msg_sendpending = 0;
        // as we operate in QSEND=1 mode this means we somehow missed
        // a modem crash/reset: do full re-init
        net_msg_disconnected();
        net_state_enter(NET_STATE_START);
        }
      else if (memcmppgm2ram(net_buf, (char const rom far*)"DATA ACCEPT", 11) == 0)
        {
        // CIPSEND success response in QSEND=1 mode
        net_msg_sendpending = 0;
        }
      else if ( (memcmppgm2ram(net_buf, (char const rom far*)"CLOSED", 6) == 0) ||
              (memcmppgm2ram(net_buf, (char const rom far*)"CONNECT FAIL", 12) == 0) )
        {
        // Re-initialize TCP socket, after short pause
        net_msg_disconnected();
        net_state_enter(NET_STATE_NETINITCP);
        }
      else if ( (memcmppgm2ram(net_buf, (char const rom far*)"SEND FAIL", 9) == 0)||
                (memcmppgm2ram(net_buf, (char const rom far*)"+CME ERROR", 10) == 0)||
                (memcmppgm2ram(net_buf, (char const rom far*)"+PDP: DEACT", 11) == 0) )
        { // Various GPRS error results
        // Re-initialize GPRS network and TCP socket, after short pause
        net_msg_disconnected();
        net_state_enter(NET_STATE_NETINITP);
        }
      else if ( (memcmppgm2ram(net_buf, (char const rom far*)"RDY", 4) == 0)||
                (memcmppgm2ram(net_buf, (char const rom far*)"+CFUN:", 6) == 0) )
        {
        // Modem crash/reset: do full re-init
        net_msg_disconnected();
        net_state_enter(NET_STATE_START);
        }
      else if (memcmppgm2ram(net_buf, (char const rom far*)"+CUSD:", 6) == 0)
      {
        // reply MMI/USSD command result:
        net_msg_reply_ussd(net_buf, net_buf_pos);
      }
      break;
#ifdef OVMS_DIAGMODULE
    case NET_STATE_DIAGMODE:
      diag_activity(net_buf,net_buf_pos);
      break;
#endif // #ifdef OVMS_DIAGMODULE
    }
  }

////////////////////////////////////////////////////////////////////////
// net_state_ticker1()
// State Model: Per-second ticker
// This function is called approximately once per second, and gives
// the state a timeslice for activity.
//
void net_state_ticker1(void)
  {
  char stat;
  char cmd[5];

  CHECKPOINT(0x38)

  // Time out error codes
  if (net_notify_lastcount>0)
    {
    net_notify_lastcount--;
    if (net_notify_lastcount == 0)
      {
      net_notify_lasterrorcode = 0;
      }
    }

  switch (net_state)
    {
    case NET_STATE_START:
      if (net_timeout_ticks < 5)
        {
        // We are about to timeout, so let's set the error code...
        led_set(OVMS_LED_RED,NET_LED_ERRMODEM);
        }
#ifdef OVMS_INTERNALGPS
      // Using internal SIMx08 GPS:
      if ((net_fnbits & NET_FN_INTERNALGPS) != 0)
        {
        net_puts_rom(NET_WAKEUP_GPSON);
        }
      else
        {
        net_puts_rom(NET_WAKEUP_GPSOFF);
        }
#else
      // Using external GPS from car:
      net_puts_rom(NET_WAKEUP);
#endif
      break;
    case NET_STATE_DOINIT:
      if ((net_timeout_ticks % 3)==0)
        net_puts_rom(NET_INIT1);
      break;
    case NET_STATE_DOINIT2:
      if ((net_timeout_ticks % 3)==0)
        net_puts_rom(NET_INIT2);
      break;
    case NET_STATE_DOINIT3:
      if ((net_timeout_ticks % 3)==0)
        net_puts_rom(NET_INIT3);
      break;
    case NET_STATE_COPS:
      if (net_timeout_ticks < 20)
        {
        // We are about to timeout, so let's set the error code...
        led_set(OVMS_LED_RED,NET_LED_ERRCOPS);
        }
      break;
    case NET_STATE_COPSWAIT:
      if ((net_timeout_ticks % 10)==0)
        {
        delay100(2);
        net_puts_rom(NET_CREG_STATUS);
        while(vUARTIntTxBufDataCnt>0) { delay100(1); } // Wait for TX flush
        delay100(2); // Wait for stable result
        }
      break;
    case NET_STATE_SOFTRESET:
      net_state_enter(NET_STATE_FIRSTRUN);
      break;

    case NET_STATE_READY:

      if (net_buf_mode != NET_BUF_CRLF)
        {
        if (net_buf_todotimeout == 1)
          {
          // Timeout waiting for arrival of SMS/IP data
          net_buf_todotimeout = 0;
          net_buf_mode = NET_BUF_CRLF;
          net_state_enter(NET_STATE_COPS); // Reset network connection
          return;
          }
        else if (net_buf_todotimeout > 1)
          net_buf_todotimeout--;
        }

      if (net_watchdog > 0)
        {
        if (--net_watchdog == 0)
          {
          net_state_enter(NET_STATE_COPS); // Reset network connection
          return;
          }
        }

      if (net_msg_sendpending>0)
        {
        net_msg_sendpending++;
        if (net_msg_sendpending>60)
          {
          // Pending for more than 60 seconds..
          net_state_enter(NET_STATE_DONETINIT); // Reset GPRS link
          return;
          }
        }

    // ...case NET_STATE_READY + registered...
      if ((net_reg == 0x01)||(net_reg == 0x05))
        {

        if ((net_msg_cmd_code!=0)
                && (net_msg_serverok==1) && (net_msg_sendpending==0))
          {
          net_msg_cmd_do();
          return;
          }

        if ((net_notify_errorcode>0)
                && (net_msg_serverok==1) && (net_msg_sendpending==0))
          {
          delay100(10);
          if (net_notify_errorcode > 0)
            {
            net_msg_erroralert(net_notify_errorcode, net_notify_errordata);
            }
          net_notify_errorcode = 0;
          net_notify_errordata = 0;
          return;
          }

        if (((net_notify & NET_NOTIFY_NETPART)>0)
                && (net_msg_serverok==1) && (net_msg_sendpending==0))
          {
          delay100(10);
#ifndef OVMS_NO_VEHICLE_ALERTS
          if ((net_notify & NET_NOTIFY_NET_ALARM)>0)
            {
            net_notify &= ~(NET_NOTIFY_NET_ALARM); // Clear notification flag
            net_msg_alarm();
            return;
            }
          else
#endif //OVMS_NO_VEHICLE_ALERTS
          if ((net_notify & NET_NOTIFY_NET_CHARGE)>0)
            {
            net_notify &= ~(NET_NOTIFY_NET_CHARGE); // Clear notification flag
            if (net_notify_suppresscount==0)
              {
              // execute CHARGE ALERT command:
              net_msg_cmd_code = 6;
              net_msg_cmd_msg = cmd;
              net_msg_cmd_msg[0] = 0;
              net_msg_cmd_do();
              }
            return;
            }
          else if ((net_notify & NET_NOTIFY_NET_12VLOW)>0)
            {
            net_notify &= ~(NET_NOTIFY_NET_12VLOW); // Clear notification flag
            if (net_fnbits & NET_FN_12VMONITOR) net_msg_12v_alert();
            return;
            }
#ifndef OVMS_NO_VEHICLE_ALERTS
          else if ((net_notify & NET_NOTIFY_NET_TRUNK)>0)
            {
            net_notify &= ~(NET_NOTIFY_NET_TRUNK); // Clear notification flag
            net_msg_valettrunk();
            return;
            }
#endif //OVMS_NO_VEHICLE_ALERTS
          else if ((net_notify & NET_NOTIFY_NET_STAT)>0)
            {
            net_notify &= ~(NET_NOTIFY_NET_STAT); // Clear notification flag
            delay100(10);
            if (net_msgp_stat(2) != 2);
              net_msg_send();
            return;
            }
          else if ((net_notify & NET_NOTIFY_NET_ENV)>0)
            {
            net_notify &= ~(NET_NOTIFY_NET_ENV); // Clear notification flag
            // A bit of a kludge, but only notify environment if an app connected
            if ((net_apps_connected>0))
              {
              stat = 2;
              delay100(10);
              stat = net_msgp_environment(stat);
              stat = net_msgp_stat(stat);
              if (stat != 2)
                net_msg_send();
              return;
              }
            }
          } // if NET_NOTIFY_NETPART

        if ((net_notify & NET_NOTIFY_SMSPART)>0)
          {
          delay100(10);
          net_assert_caller(NULL); // set net_caller to PARAM_REGPHONE
#ifndef OVMS_NO_VEHICLE_ALERTS
          if ((net_notify & NET_NOTIFY_SMS_ALARM)>0)
            {
            net_notify &= ~(NET_NOTIFY_SMS_ALARM); // Clear notification flag
            net_sms_alarm(net_caller);
            return;
            }
          else
#endif //OVMS_NO_VEHICLE_ALERTS
          if ((net_notify & NET_NOTIFY_SMS_CHARGE)>0)
            {
            net_notify &= ~(NET_NOTIFY_SMS_CHARGE); // Clear notification flag
            if (net_notify_suppresscount==0)
              {
                stp_rom(cmd, "STAT");
                net_sms_in(net_caller, cmd);
              }
            return;
            }
          else if ((net_notify & NET_NOTIFY_SMS_12VLOW)>0)
            {
            net_notify &= ~(NET_NOTIFY_SMS_12VLOW); // Clear notification flag
            if (net_fnbits & NET_FN_12VMONITOR) net_sms_12v_alert(net_caller);
            return;
            }
#ifndef OVMS_NO_VEHICLE_ALERTS
          else if ((net_notify & NET_NOTIFY_SMS_TRUNK)>0)
            {
            net_notify &= ~(NET_NOTIFY_SMS_TRUNK); // Clear notification flag
            net_sms_valettrunk(net_caller);
            return;
            }
#endif //OVMS_NO_VEHICLE_ALERTS
          else if ((net_notify & NET_NOTIFY_SMS_STAT)>0)
            {
            net_notify &= ~(NET_NOTIFY_SMS_STAT); // Clear notification flag
            }
          else if ((net_notify & NET_NOTIFY_SMS_ENV)>0)
            {
            net_notify &= ~(NET_NOTIFY_SMS_ENV); // Clear notification flag
            return;
            }
          } // if NET_NOTIFY_SMSPART

        // GPS location streaming:
        if ((car_speed>0) &&
            (sys_features[FEATURE_STREAM]&1) &&
            (net_msg_sendpending==0) &&
            (net_apps_connected>0) &&
            ( ((net_fnbits & NET_FN_INTERNALGPS) == 0)
              || ((net_granular_tick % 2) == 0) ))
          {
          // Car moving, and streaming on, apps connected, and not sending
          //delay100(2); // necessary? net_msg_start() delays too
          if (net_msgp_gps(2) != 2)
            net_msg_send();
          }

        } // if ((net_reg == 0x01)||(net_reg == 0x05))

#ifdef OVMS_INTERNALGPS
        // Request internal SIM908 GPS coordinates
        // once per second while car is on,
        // else once every minute (to trace theft / transportation)
        if ((car_doors1bits.CarON || ((net_granular_tick % 60) == 0))
                && (net_msg_sendpending==0)
                && ((net_fnbits & NET_FN_INTERNALGPS) > 0))
        {
          net_puts_rom(NET_REQGPS);
          delay100(1); // to get modem reply on next net_poll()
        }
#endif

      break;

#ifdef OVMS_DIAGMODULE
    case NET_STATE_DIAGMODE:
      diag_ticker();
      break;
#endif // #ifdef OVMS_DIAGMODULE
    }
  }

////////////////////////////////////////////////////////////////////////
// net_state_ticker30()
// State Model: 30-second ticker
// This function is called approximately once per 30 seconds (since state
// was first entered), and gives the state a timeslice for activity.
//
void net_state_ticker30(void)
  {
  CHECKPOINT(0x39)

  switch (net_state)
    {
    case NET_STATE_READY:
      if (net_msg_sendpending>0)
        {
        net_granular_tick -= 5; // Try again in 5 seconds...
        return;
        }
      if ((net_granular_tick % 60) != 0)
        {
        delay100(2);
        net_puts_rom(NET_CREG_CIPSTATUS);
        while(vUARTIntTxBufDataCnt>0) { delay100(1); } // Wait for TX flush
        delay100(2); // Wait for stable result
        }
      break;
    }
  }

////////////////////////////////////////////////////////////////////////
// net_state_ticker60()
// State Model: Per-minute ticker
// This function is called approximately once per minute (since state
// was first entered), and gives the state a timeslice for activity.
//
void net_state_ticker60(void)
  {
  char stat;
  char *p;

  CHECKPOINT(0x3A)

#ifdef OVMS_HW_V2

#define BATT_12V_CALMDOWN_TIME 15  // calm down time in minutes after charge end

  // Take 12v reading:
  if (car_12vline == 0)
  {
    // first reading:
    car_12vline = inputs_voltage()*10;
    car_12vline_ref = 0;
  }
  else
  {
    // filter peaks/misreadings:
    car_12vline = ((int)car_12vline + (int)(inputs_voltage()*10) + 1) / 2;

    // OR direct reading to test A/D converter fix: (failed...)
    //car_12vline = inputs_voltage()*10;
  }

  // Calibration: take reference voltage after charging
  //    Note: ref value 0 is "charging"
  //          ref value 1..CALMDOWN_TIME is calmdown counter
  if (car_doors5bits.Charging12V)
  {
    // charging now, reset ref:
    car_12vline_ref = 0;
  }
  else if (car_12vline_ref < BATT_12V_CALMDOWN_TIME)
  {
    // calmdown phase:
    if (car_doors1bits.CarON)
    {
      // car has been turned ON during calmdown; reset timer:
      car_12vline_ref = 0;
    }
    else
    {
      // wait CALMDOWN_TIME minutes after end of charge:
      car_12vline_ref++;
    }
  }
  else if ((car_12vline_ref == BATT_12V_CALMDOWN_TIME) && !car_doors1bits.CarON)
  {
    // calmdown done & car off: take new ref voltage & reset alert:
    car_12vline_ref = car_12vline;
    can_minSOCnotified &= ~CAN_MINSOC_ALERT_12V;
  }

  // Check voltage if ref is valid:
  if (car_12vline_ref > BATT_12V_CALMDOWN_TIME)
  {
    if (car_doors1bits.CarON)
    {
      // Reset 12V alert if car is on
      // because DC/DC system charges 12V from main battery
      can_minSOCnotified &= ~CAN_MINSOC_ALERT_12V;
    }
    else
    {
      // Car is off, trigger alert if necessary

      // Info: healthy lead/acid discharge depth is ~ nom - 1.0 V
      //        ref is ~ nom + 0.5 V

      // Trigger 12V alert if voltage <= ref - 1.6 V:
      if (!(can_minSOCnotified & CAN_MINSOC_ALERT_12V)
              && (car_12vline <= (car_12vline_ref - 16)))
      {
        can_minSOCnotified |= CAN_MINSOC_ALERT_12V;
        net_req_notification(NET_NOTIFY_12VLOW);
      }

      // Reset 12V alert if voltage >= ref - 1.0 V:
      else if ((can_minSOCnotified & CAN_MINSOC_ALERT_12V)
              && (car_12vline >= (car_12vline_ref - 10)))
      {
        can_minSOCnotified &= ~CAN_MINSOC_ALERT_12V;
        net_req_notification(NET_NOTIFY_12VLOW);
      }
    }
  }

#endif

  switch (net_state)
    {
    case NET_STATE_READY:
      if (net_msg_sendpending>0)
        {
        net_granular_tick -= 5; // Try again in 5 seconds...
        return;
        }
#ifdef OVMS_LOGGINGMODULE
      if ((net_link==1)&&(logging_haspending() > 0))
        {
        delay100(10);
        net_msg_start();
        logging_sendpending();
        net_msg_send();
        return;
        }
#endif // #ifdef OVMS_LOGGINGMODULE
      if ((net_link==1)&&(net_apps_connected>0))
        {
        delay100(10);
        stat = 2;
        p = par_get(PARAM_S_GROUP1);
        if (*p != 0) stat = net_msgp_group(stat,1,p);
        p = par_get(PARAM_S_GROUP2);
        if (*p != 0) stat = net_msgp_group(stat,2,p);
        stat = net_msgp_stat(stat);
        stat = net_msgp_gps(stat);
        stat = net_msgp_tpms(stat);
        stat = net_msgp_environment(stat);
        stat = net_msgp_capabilities(stat);
        if (stat != 2)
          net_msg_send();
        }
      net_state_vchar = net_state_vchar ^ 1;
      break;
    }
  }

////////////////////////////////////////////////////////////////////////
// net_state_ticker300()
// State Model: Per-5-minute ticker
// This function is called approximately once per five minutes (since
// state was first entered), and gives the state a timeslice for activity.
//
void net_state_ticker300(void)
  {
  CHECKPOINT(0x3B)

  switch (net_state)
    {
    }
  }

////////////////////////////////////////////////////////////////////////
// net_state_ticker600()
// State Model: Per-10-minute ticker
// This function is called approximately once per ten minutes (since
// state was first entered), and gives the state a timeslice for activity.
//
void net_state_ticker600(void)
  {
  char stat;
  char *p;
  BOOL carbusy = ((car_chargestate==1)||    // Charging
                  (car_chargestate==2)||    // Topping off
                  (car_chargestate==15)||   // Heating
                  ((car_doors1&0x80)>0));   // Car On

  CHECKPOINT(0x3C)

  switch (net_state)
    {
    case NET_STATE_READY:
#ifdef OVMS_SOCALERT
      if ((car_SOC<car_SOCalertlimit)&&((car_doors1 & 0x80)==0)) // Car is OFF, and SOC<car_SOCalertlimit
        {
        if (net_socalert_msg==0)
          {
          if ((net_link==1)&&(net_msg_sendpending==0))
            {
            if (net_fnbits & NET_FN_SOCMONITOR) net_msg_socalert();
            net_socalert_msg = 72; // 72x10mins = 12hours
            }
          }
        else
          net_socalert_msg--;
        if (net_socalert_sms==0)
          {
          p = par_get(PARAM_REGPHONE);
          if (net_fnbits & NET_FN_SOCMONITOR) net_sms_socalert(p);
          net_socalert_sms = 72; // 72x10mins = 12hours
          }
        else
          net_socalert_sms--;
        }
      else
        {
        net_socalert_sms = 6;         // Check in 1 hour
        net_socalert_msg = 6;         // Check in 1 hour
        }
#endif //#ifdef OVMS_SOCALERT
      if ((net_link==1)&&(net_apps_connected==0)&&carbusy)
        {
        if (net_msg_sendpending>0)
          {
          net_granular_tick -= 5; // Try again in 5 seconds...
          }
        else
          {
          stat = 2;
          p = par_get(PARAM_S_GROUP1);
          if (*p != 0) stat = net_msgp_group(stat,1,p);
          p = par_get(PARAM_S_GROUP2);
          if (*p != 0) stat = net_msgp_group(stat,2,p);
          stat = net_msgp_stat(stat);
          stat = net_msgp_gps(stat);
          stat = net_msgp_tpms(stat);
          stat = net_msgp_environment(stat);
          stat = net_msgp_capabilities(stat);
          if (stat != 2)
            net_msg_send();
          }
        }
      break;
    }
  }

////////////////////////////////////////////////////////////////////////
// net_state_ticker3600()
// State Model: Per-hour ticker
// This function is called approximately once per hour (since
// state was first entered), and gives the state a timeslice for activity.
//
void net_state_ticker3600(void)
  {
  char stat;
  char *p;
  BOOL carbusy = ((car_chargestate==1)||    // Charging
                  (car_chargestate==2)||    // Topping off
                  (car_chargestate==15)||   // Heating
                  ((car_doors1&0x80)>0));   // Car On

  CHECKPOINT(0x3D)

  switch (net_state)
    {
    case NET_STATE_READY:
      if ((net_link==1)&&(net_apps_connected==0)&&(!carbusy))
        {
        if (net_msg_sendpending>0)
          {
          net_granular_tick -= 5; // Try again in 5 seconds...
          }
        else
          {
          stat = 2;
          p = par_get(PARAM_S_GROUP1);
          if (*p != 0) stat = net_msgp_group(stat,1,p);
          p = par_get(PARAM_S_GROUP2);
          if (*p != 0) stat = net_msgp_group(stat,2,p);
          stat = net_msgp_stat(stat);
          stat = net_msgp_gps(stat);
          stat = net_msgp_tpms(stat);
          stat = net_msgp_environment(stat);
          stat = net_msgp_capabilities(stat);
          if (stat != 2)
            net_msg_send();
          }
        }
      break;
    }
  }

////////////////////////////////////////////////////////////////////////
// net_ticker()
// This function is an entry point from the main() program loop, and
// gives the NET framework a ticker call approximately once per second.
// It is used to internally generate the other net_state_ticker*() calls.
//
void net_ticker(void)
  {
  // This ticker is called once every second

  CHECKPOINT(0x3E)

  if (net_notify_suppresscount>0) net_notify_suppresscount--;
  net_granular_tick++;
  if ((net_timeout_goto > 0)&&(net_timeout_ticks-- == 0))
    {
    net_state_enter(net_timeout_goto);
    }
  else
    {
    net_state_ticker1();
    }
  if ((net_granular_tick % 30)==0)    net_state_ticker30();
  if ((net_granular_tick % 60)==0)    net_state_ticker60();
  if ((net_granular_tick % 300)==0)   net_state_ticker300();
  if ((net_granular_tick % 600)==0)   net_state_ticker600();
  if ((net_granular_tick % 3600)==0)
    {
    net_state_ticker3600();
    net_granular_tick -= 3600;
    }

  if (--net_timeout_rxdata == 0)
    {
    // A major problem - we've lost connectivity to the modem
    // Best solution is to reset everything
    led_set(OVMS_LED_GRN,OVMS_LED_OFF);
    led_set(OVMS_LED_RED,OVMS_LED_OFF);
    net_timeout_rxdata = NET_RXDATA_TIMEOUT;
    // Temporary kludge to record in feature #10 the number of times this happened
    sys_features[10] += 1;
    stp_i(net_scratchpad, NULL, sys_features[10]);
    par_set(PARAM_FEATURE10,net_scratchpad);
    CHECKPOINT(0xF1)
    reset_cpu();
    }
  }

////////////////////////////////////////////////////////////////////////
// net_ticker10th()
// This function is an entry point from the main() program loop, and
// gives the NET framework a ticker call approximately ten times per
// second. It is used to flash the RED LED when the link is up
//
void net_ticker10th(void)
  {
  }

////////////////////////////////////////////////////////////////////////
// net_initialise()
// This function is an entry point from the main() program loop, and
// gives the NET framework an opportunity to initialise itself.
//
void net_initialise(void)
  {
  // I/O configuration PORT C
  TRISC = 0x80; // Port C RC0-6 output, RC7 input
  UARTIntInit();

  net_reg = 0;
  net_state_enter(NET_STATE_FIRSTRUN);
  }
