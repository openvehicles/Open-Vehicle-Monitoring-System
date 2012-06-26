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

// NET data
#pragma udata
unsigned char net_state = 0;                // The current state
unsigned char net_state_vchar = 0;          //   A per-state CHAR variable
unsigned int  net_state_vint = 0;           //   A per-state INT variable
unsigned char net_cops_tries = 0;           // A counter for COPS attempts
unsigned char net_timeout_goto = 0;         // State to auto-transition to, after timeout
unsigned int  net_timeout_ticks = 0;        // Number of seconds before timeout auto-transition
unsigned int  net_granular_tick = 0;        // An internal ticker used to generate 1min, 5min, etc, calls
unsigned int  net_watchdog = 0;             // Second count-down for network connectivity
char net_caller[NET_TEL_MAX] = {0};         // The telephone number of the caller

unsigned char net_buf_pos = 0;              // Current position (aka length) in the network buffer
unsigned char net_buf_mode = NET_BUF_CRLF;  // Mode of the buffer (CRLF, SMS or MSG)
unsigned char net_buf_todo = 0;             // Bytes outstanding on a reception
unsigned char net_buf_todotimeout = 0;      // Timeout for bytes outstanding

#ifdef OVMS_SOCALERT
unsigned char net_socalert_sms = 0;         // SOC Alert (msg) 10min ticks remaining
unsigned char net_socalert_msg = 0;         // SOC Alert (sms) 10min ticks remaining
#endif //#ifdef OVMS_SOCALERT

#pragma udata NETBUF_SP
char net_scratchpad[NET_BUF_MAX];           // A general-purpose scratchpad
#pragma udata
#pragma udata NETBUF
char net_buf[NET_BUF_MAX];                  // The network buffer itself
#pragma udata

// ROM Constants
rom char NET_INIT1[] = "AT+CSMINS?\r";
rom char NET_INIT2[] = "AT+CPIN?\r";
rom char NET_INIT3[] = "AT+IPR?;+CREG=1;+CLIP=1;+CMGF=1;+CNMI=2,2;+CSDH=1;+CIPSPRT=0;+CIPQSEND=1;E0\r";
rom char NET_COPS[] = "AT+COPS=0\r";
//rom char NET_INIT3[] = "AT+IPR?;+CREG=1;+CLIP=1;+CMGF=1;+CNMI=2,2;+CSDH=1;+CIPSPRT=0;+CIPQSEND=1;E1\r";
//rom char NET_COPS[] = "AT+COPS=4,0,\"3(2G)\"\r";

rom char NET_WAKEUP[] = "AT\r";
rom char NET_HANGUP[] = "ATH\r";
rom char NET_CREG_CIPSTATUS[] = "AT+CREG?;+CIPSTATUS;+CSQ\r";
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

//#pragma	interruptlow low_isr save=section(".tmpdata")
#pragma	interruptlow low_isr
void low_isr(void)
  {
  // call of library module function, MUST
  UARTIntISR();
  led_isr();
  }

////////////////////////////////////////////////////////////////////////
// net_reset_async()
// Reset the async status following an overun or other such error
//
void net_reset_async(void)
  {
  vUARTIntStatus.UARTIntRxError = 0;
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

  while(UARTIntGetChar(&x))
    {
    if (net_buf_mode==NET_BUF_CRLF)
      { // CRLF (either normal or SMS second line) mode
      if (x == 0x0d) continue; // Skip 0x0d (CR)
      net_buf[net_buf_pos++] = x;
      if (net_buf_pos == NET_BUF_MAX) net_buf_pos--;
      if ((x == ':')&&(net_buf_pos>=6)&&
          (net_buf[0]=='+')&&(net_buf[1]=='I')&&
          (net_buf[2]=='P')&&(net_buf[3]=='D'))
        {
        net_buf[net_buf_pos-1] = 0; // Change the ':' to an end
        net_buf_todo = atoi(net_buf+5);
        net_buf_todotimeout = 60; // 60 seconds to receive the rest
        net_buf_mode = NET_BUF_IPD;
        net_buf_pos = 0;
        continue; // We have switched to IPD mode
        }
      if (x == 0x0A) // Newline?
        {
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
        net_state_activity();
        net_buf_pos = 0;
        net_buf_mode = NET_BUF_CRLF;
        }
      }
    else if (net_buf_mode==NET_BUF_SMS)
      { // SMS data mode
      if ((x==0x0d)||(x==0x0a))
        net_buf[net_buf_pos++] = ' '; // \d, \r => space
      else
        net_buf[net_buf_pos++] = x;
      if (net_buf_pos == NET_BUF_MAX) net_buf_pos--;
      net_buf_todo--;
      if (net_buf_todo==0)
        {
        net_buf[net_buf_pos] = 0; // Zero-terminate
        net_state_activity();
        net_buf_pos = 0;
        net_buf_mode = NET_BUF_CRLF;
        }
      }
    else
      { // IP data mode
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
void net_puts_rom(static const rom char *data)
  {
  /* Send characters up to the null */
  for (;*data;data++)
    {
    while (vUARTIntStatus.UARTIntTxBufferFull);
    UARTIntPutChar(*data);
    }
  }

////////////////////////////////////////////////////////////////////////
// net_puts_ram()
// Transmit zero-terminated character data from RAM to the async port.
// N.B. This may block if the transmit buffer is full.
//
void net_puts_ram(const char *data)
  {
  /* Send characters up to the null */
  for (;*data;data++)
    {
    while (vUARTIntStatus.UARTIntTxBufferFull);
    UARTIntPutChar(*data);
    }
  }

////////////////////////////////////////////////////////////////////////
// net_putc_ram()
// Transmit a single character from RAM to the async port.
// N.B. This may block if the transmit buffer is full.
void net_putc_ram(const char data)
  {
  /* Send one character */
  while (vUARTIntStatus.UARTIntTxBufferFull);
  UARTIntPutChar(data);
  }

////////////////////////////////////////////////////////////////////////
// net_notify_status()
// Emits a status notification error to the user (by SMS)
// upon request (e.g. by an incoming call or sms STAT command).
//
void net_notify_status(unsigned char notify)
  {
  // Emit an unsolicited notification showing current status
  char *p,*q;
  p = par_get(PARAM_NOTIFIES);
  if (strstrrampgm(p,(char const rom far*)"SMS") != NULL)
    {
    net_sms_notify = notify;
    }
  if (strstrrampgm(p,(char const rom far*)"IP") != NULL)
    {
    net_msg_notify = notify;
    }
  }

////////////////////////////////////////////////////////////////////////
// net_notify_environment()
// Emits an environment notification
//
void net_notify_environment(void)
  {
  net_msg_notifyenvironment = 1;
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

  net_state = newstate;
  switch(net_state)
    {
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
      net_state_vchar = 0;
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
      reset_cpu();
      break;
    case NET_STATE_DOINIT:
      led_set(OVMS_LED_GRN,NET_LED_INITSIM1);
      led_set(OVMS_LED_RED,OVMS_LED_OFF);
      net_timeout_goto = NET_STATE_HARDRESET;
      net_timeout_ticks = 95;
      net_state_vchar = 0;
      net_puts_rom(NET_INIT1);
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
      net_timeout_ticks = 6;
      break;
    case NET_STATE_DONETINIT:
      led_set(OVMS_LED_GRN,NET_LED_NETINIT);
      led_set(OVMS_LED_RED,OVMS_LED_OFF);
      net_watchdog=0; // Disable watchdog, as we have connectivity
      net_reg = 0x05; // Assume connectivity (as COPS worked)
      p = par_get(PARAM_GPRSAPN);
      if ((p[0] != '\0')&&(PORTAbits.RA0==0)) // APN defined AND switch is set to GPRS mode
        {
        net_timeout_goto = NET_STATE_SOFTRESET;
        net_timeout_ticks = 60;
        net_state_vchar = 0;
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
      net_timeout_ticks = 240;
      net_msg_disconnected();
      net_puts_rom(NET_COPS);
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
  if (net_buf_mode == NET_BUF_SMS)
    {
    // An SMS has arrived, and net_caller has been primed
    if ((net_reg != 0x01)&&(net_reg != 0x05))
      { // Treat this as a network registration
      net_watchdog=0; // Disable watchdog, as we have connectivity
      net_reg = 0x05;
      led_set(OVMS_LED_RED,OVMS_LED_OFF);
      }
    net_sms_in(net_caller,net_buf,net_buf_pos);
    return;
    }
  else if (net_buf_mode != NET_BUF_CRLF)
    {
    // An IP data message has arrived
    net_msg_in(net_buf);
    return;
    }

  switch (net_state)
    {
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
      if ((net_buf_pos >= 8)&&(net_buf[0]=='+')&&(net_buf[1]=='C')&&(net_buf[2]=='P')&&(net_buf[3]=='I'))
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
        net_state_vint = 10; // Count-down for DONETINIT attempts
        net_cops_tries = 0; // Successfully out of COPS
        net_state_enter(NET_STATE_DONETINIT); // COPS reconnect was OK
        }
      else if ( ((net_buf_pos >= 5)&&(net_buf[0] == 'E')&&(net_buf[1] == 'R')) ||
              (memcmppgm2ram(net_buf, (char const rom far*)"+CME ERROR", 10) == 0) )
        if (net_cops_tries++ < 20)
          {
          net_state_enter(NET_STATE_SOFTRESET); // Reset the entire async
          }
        else
          {
          net_state_enter(NET_STATE_HARDRESET); // Reset the entire async
          }
      break;
    case NET_STATE_DONETINIT:
      if ((net_buf_pos >= 2)&&
               (net_buf[0] == 'E')&&
               (net_buf[1] == 'R'))
        {
        if ((net_state_vchar == 2)||
                (net_state_vchar == 3))// ERROR response to AT+CSTT OR AT+CIICR
          {
          // try setting up GPRS again, after short pause
          net_state_enter(NET_STATE_NETINITP);
          }
        else if (net_state_vchar == 5) // ERROR response to AT+CIFSR
          {
          // This is a nasty case. The GPRS has locked up.
          // The only solution I can find is a hard reset of the modem
          net_state_enter(NET_STATE_HARDRESET);
          }
        }
      else if ((net_buf_pos >= 2)&&
          (((net_buf[0] == 'O')&&(net_buf[1] == 'K')))|| // OK
          (((net_buf[0] == 'S')&&(net_buf[1] == 'H')))||  // SHUT OK
          (net_state_vchar == 5)) // Local IP address
        {
        net_buf_pos = 0;
        net_timeout_ticks = 30;
        net_link = 0;
        delay100(2);
        switch (++net_state_vchar)
          {
          case 1:
            net_puts_rom("AT+CGDCONT=1,\"IP\",\"");
            net_puts_ram(par_get(PARAM_GPRSAPN));
            net_puts_rom("\"\r");
            break;
          case 2:
            net_puts_rom("AT+CSTT=\"");
            net_puts_ram(par_get(PARAM_GPRSAPN));
            net_puts_rom("\",\"");
            net_puts_ram(par_get(PARAM_GPRSUSER));
            net_puts_rom("\",\"");
            net_puts_ram(par_get(PARAM_GPRSPASS));
            net_puts_rom("\"\r");
            break;
          case 3:
            led_set(OVMS_LED_GRN,NET_LED_NETAPNOK);
            net_puts_rom("AT+CIICR\r");
            break;
          case 4:
            net_puts_rom("AT+CIPHEAD=1\r");
            break;
          case 5:
            net_puts_rom("AT+CIFSR\r");
            break;
          case 6:
            net_puts_rom("AT+CLPORT=\"TCP\",\"6867\"\r");
            break;
          case 7:
            led_set(OVMS_LED_GRN,NET_LED_NETCALL);
            net_puts_rom("AT+CIPSTART=\"TCP\",\"");
            net_puts_ram(par_get(PARAM_SERVERIP));
            net_puts_rom("\",\"6867\"\r");
            break;
          case 8:
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
        //net_notify_status(2); // Disable this feature, as not really used and can lead to SMS charges
        }
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
        else
          {
          net_link = 0;
          led_set(OVMS_LED_GRN,NET_LED_READY);
          if ((net_reg == 0x01)||(net_reg == 0x05))
            {
            // We have a GSM network, but CIPSTATUS is not up
            net_msg_disconnected();
            net_state_enter(NET_STATE_DONETINIT);
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
        net_msg_sendpending = 0;
        }
      else if (memcmppgm2ram(net_buf, (char const rom far*)"DATA ACCEPT", 11) == 0)
        {
        net_msg_sendpending = 0;
        }
      else if ( (memcmppgm2ram(net_buf, (char const rom far*)"SEND FAIL", 9) == 0)||
                (memcmppgm2ram(net_buf, (char const rom far*)"CLOSED", 6) == 0)||
                (memcmppgm2ram(net_buf, (char const rom far*)"+CME ERROR", 10) == 0)||
                (memcmppgm2ram(net_buf, (char const rom far*)"+PDP: DEACT", 11) == 0) )
        { // Various GPRS error results
        // Re-initialize GPRS network and TCP socket
        net_msg_disconnected();
        net_state_enter(NET_STATE_DONETINIT);
        }
      break;
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
  char *p;

  switch (net_state)
    {
    case NET_STATE_START:
      if (net_timeout_ticks < 5)
        {
        // We are about to timeout, so let's set the error code...
        led_set(OVMS_LED_RED,NET_LED_ERRMODEM);
        }
      net_puts_rom(NET_WAKEUP);
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
    case NET_STATE_SOFTRESET:
      net_state_enter(NET_STATE_START);
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
      if ((net_reg == 0x01)||(net_reg == 0x05))
        {
        if ((net_msg_cmd_code!=0)&&(net_msg_serverok==1)&&(net_msg_sendpending==0))
          {
          net_msg_cmd_do();
          return;
          }
        if ((net_msg_notify>0)&&(net_msg_serverok==1))
          {
          delay100(10);
          if ((net_msg_notify==1)&&(car_chargestate!=0x04)&&(car_chargesubstate!=0x03))
            net_msg_alert();
          else if (net_msg_notify==2)
            net_msg_alert();
          else if (net_msg_notify==3)
            net_msg_valettrunk();
          net_msg_notify = 0;
          return;
          }
        if (net_sms_notify>0)
          {
          delay100(10);
          p = par_get(PARAM_REGPHONE);
          if ((net_sms_notify==1)&&(car_chargestate!=0x04)&&(car_chargesubstate!=0x03))
            net_sms_stat(p);
          else if (net_sms_notify==2)
            net_sms_stat(p);
          else if (net_sms_notify==3)
            net_sms_valettrunk(p);
          net_sms_notify = 0;
          return;
          }
        if ((net_msg_notifyenvironment==1)&&
            (net_msg_serverok==1)&&
            (net_apps_connected>0)&&
            (net_msg_sendpending==0))
          {
          net_msg_notifyenvironment = 0;
          delay100(10);
          net_msg_start();
          net_msg_environment();
          net_msg_stat();
          net_msg_send();
          return;
          }
        if ((car_speed>0)&&
            (sys_features[FEATURE_STREAM]>0)&&
            (net_msg_sendpending==0)&&
            (net_apps_connected>0))
          { // Car moving, and streaming on, apps connected, and not sending
          delay100(2);
          net_msg_start();
          net_msg_gps();
          net_msg_send();
          }
        }
      break;
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
  char *p;

  switch (net_state)
    {
    case NET_STATE_READY:
      if (net_msg_sendpending>0)
        {
        net_granular_tick -= 5; // Try again in 5 seconds...
        return;
        }
      if ((net_link==1)&&(net_apps_connected>0))
        {
        net_msg_start();
        p = par_get(PARAM_S_GROUP1);
        if (*p != 0) net_msg_group(p);
        p = par_get(PARAM_S_GROUP2);
        if (*p != 0) net_msg_group(p);
        net_msg_stat();
        net_msg_gps();
        net_msg_tpms();
        net_msg_environment();
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
  char *p;

  switch (net_state)
    {
    case NET_STATE_READY:
#ifdef OVMS_SOCALERT
      if ((car_SOC<5)&&((car_doors1 & 0x80)==0)) // Car is OFF, and SOC<5%
        {
        if (net_socalert_msg==0)
          {
          if ((net_link==1)&&(net_msg_sendpending==0))
            {
            net_msg_start();
            net_msg_socalert();
            net_msg_send();
            net_socalert_msg = 72; // 72x10mins = 12hours
            }
          }
        else
          net_socalert_msg--;
        if (net_socalert_sms==0)
          {
          p = par_get(PARAM_REGPHONE);
          net_sms_socalert(p);
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
      if ((net_link==1)&&(net_apps_connected==0))
        {
        if (net_msg_sendpending>0)
          {
          net_granular_tick -= 5; // Try again in 5 seconds...
          }
        else
          {
          net_msg_start();
          p = par_get(PARAM_S_GROUP1);
          if (*p != 0) net_msg_group(p);
          p = par_get(PARAM_S_GROUP2);
          if (*p != 0) net_msg_group(p);
          net_msg_stat();
          net_msg_gps();
          net_msg_tpms();
          net_msg_environment();
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
  net_granular_tick++;
  if ((net_timeout_goto > 0)&&(net_timeout_ticks-- == 0))
    {
    net_state_enter(net_timeout_goto);
    }
  else
    {
    net_state_ticker1();
    }
  if ((net_granular_tick % 30)==0)
    net_state_ticker30();
  if ((net_granular_tick % 60)==0)
    net_state_ticker60();
  if ((net_granular_tick % 300)==0)
    net_state_ticker300();
  if ((net_granular_tick % 600)==0)
    {
    net_state_ticker600();
    net_granular_tick -= 600;
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
  net_state_enter(NET_STATE_START);
  }
