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

// NET data
#pragma udata
unsigned char net_state = 0;                // The current state
unsigned char net_state_vchar = 0;          //   A per-state CHAR variable
unsigned int  net_state_vint = 0;           //   A per-state INT variable
unsigned char net_timeout_goto = 0;         // State to auto-transition to, after timeout
unsigned int  net_timeout_ticks = 0;        // Number of seconds before timeout auto-transition
unsigned int  net_granular_tick = 0;        // An internal ticker used to generate 1min, 5min, etc, calls
unsigned int  net_watchdog = 0;             // Second count-down for network connectivity
char net_caller[NET_TEL_MAX] = {0};         // The telephone number of the caller
char net_scratchpad[100];                   // A general-purpose scratchpad

unsigned char net_buf_pos = 0;              // Current position (aka length) in the network buffer
unsigned char net_buf_mode = NET_BUF_CRLF;  // Mode of the buffer (CRLF, SMS or MSG)
#pragma udata NETBUF
char net_buf[NET_BUF_MAX];                  // The network buffer itself
#pragma udata

// ROM Constants
rom char NET_WAKEUP[] = "AT\r";
rom char NET_INIT[] = "AT+CPIN?;+CREG=1;+CLIP=1;+CMGF=1;+CNMI=2,2;+CSDH=0;+CIPSPRT=0;+CIPQSEND=1;E0\r";
rom char NET_HANGUP[] = "ATH\r";
rom char NET_COPS[] = "AT+COPS=0\r";
rom char NET_CREG_CIPSTATUS[] = "AT+CREG?;+CIPSTATUS;+CSQ\r";
rom char NET_CLOSETCP[] = "AT+CIPCLOSE\r";

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
    if ((net_buf_mode==NET_BUF_SMS)||(net_buf_mode==NET_BUF_CRLF))
      { // CRLF (either normal or SMS second line) mode
      if (x == 0x0d) continue; // Skip 0x0d (CR)
      net_buf[net_buf_pos++] = x;
      if (net_buf_pos == NET_BUF_MAX) net_buf_pos--;
      if ((x == ':')&&(net_buf_pos>=6)&&
          (net_buf[0]=='+')&&(net_buf[1]=='I')&&
          (net_buf[2]=='P')&&(net_buf[3]=='D'))
        {
        net_buf[net_buf_pos-1] = 0; // Change the ':' to an end
        net_buf_mode = atoi(net_buf+5);
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
          net_buf_pos = 0;
          net_buf_mode = NET_BUF_SMS;
          continue;
          }
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
      net_buf_mode--;
      if (x == 0x0A) // Newline?
        {
        net_buf_pos--;
        net_buf[net_buf_pos] = 0; // mark end of string for string search functions.
        net_state_activity();
        net_buf_pos = 0;
        }
      if (net_buf_mode==0)
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
  do
    {
    while (vUARTIntStatus.UARTIntTxBufferFull);
    UARTIntPutChar(*data);
    } while (*++data);
  }

////////////////////////////////////////////////////////////////////////
// net_puts_ram()
// Transmit zero-terminated character data from RAM to the async port.
// N.B. This may block if the transmit buffer is full.
//
void net_puts_ram(const char *data)
  {
  /* Send characters up to the null */
  do
    {
    while (vUARTIntStatus.UARTIntTxBufferFull);
    UARTIntPutChar(*data);
    } while (*++data);
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
void net_notify_status(void)
  {
  // Emit an unsolicited notification showing current status
  char *p,*q;
  p = par_get(PARAM_NOTIFIES);
  if (strstrrampgm(p,(char const rom far*)"SMS") != NULL)
    {
    net_sms_notify = 1;
    }
  if (strstrrampgm(p,(char const rom far*)"IP") != NULL)
    {
    net_msg_notify = 1;
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
      PORTBbits.RB0 = 1;
      net_timeout_goto = NET_STATE_DOINIT;
      net_timeout_ticks = 30;
      net_state_vchar = 1;
      net_apps_connected = 0;
      led_act(net_state_vchar);
      net_msg_init();
      break;
    case NET_STATE_SOFTRESET:
      PORTBbits.RB0 = 1;
      led_act(0);
      led_net(0);
      net_timeout_goto = 0;
      break;
    case NET_STATE_HARDRESET:
      net_timeout_goto = NET_STATE_SOFTRESET;
      net_timeout_ticks = 2;
      led_net(0);
      led_act(1);
      net_state_vchar = 0;
      net_apps_connected = 0;
      net_msg_disconnected();
      PORTBbits.RB0 = 0;
      break;
    case NET_STATE_DOINIT:
      net_timeout_goto = NET_STATE_HARDRESET;
      net_timeout_ticks = 35;
      led_act(0);
      led_net(0);
      net_puts_rom(NET_INIT);
      break;
    case NET_STATE_DONETINIT:
      net_watchdog=0; // Disable watchdog, as we have connectivity
      net_reg = 0x05; // Assume connectivity (as COPS worked)
      led_net(1);
      led_act(0);
      p = par_get(PARAM_GPRSAPN);
      if (p[0] != '\0')
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
      net_msg_sendpending = 0;
      net_timeout_goto = 0;
      net_state_vchar = 0;
      if ((net_reg != 0x01)&&(net_reg != 0x05))
        net_watchdog = 120; // I need a network within 2 mins, else reset
      else
        net_watchdog = 0; // Disable net watchdog
      break;
    case NET_STATE_COPS:
      delay100(2);
      net_timeout_goto = NET_STATE_HARDRESET;
      net_state_vchar = 0;
      led_net(net_state_vchar);
      net_timeout_ticks = 240;
      net_msg_disconnected();
      net_puts_rom(NET_COPS);
      break;
    case NET_STATE_GRACERESET:
      net_timeout_goto = NET_STATE_HARDRESET;
      net_timeout_ticks = 5; // allow 5 seconds for TCP shutdown
      led_act(0);
      led_net(0);
      net_puts_rom(NET_CLOSETCP);
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
      led_net(1);
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
        net_state_enter(NET_STATE_DOINIT);
        }
      break;
    case NET_STATE_DOINIT:
      if ((net_buf_pos >= 2)&&(net_buf[0] == 'O')&&(net_buf[1] == 'K'))
        {
        net_state_enter(NET_STATE_COPS);
        }
      break;
    case NET_STATE_COPS:
      if ((net_buf_pos >= 2)&&(net_buf[0] == 'O')&&(net_buf[1] == 'K'))
        net_state_enter(NET_STATE_DONETINIT); // COPS reconnect was OK
      else if ((net_buf_pos >= 5)&&(net_buf[0] == 'E')&&(net_buf[1] == 'R'))
        net_state_enter(NET_STATE_SOFTRESET); // Reset the entire async
      break;
    case NET_STATE_DONETINIT:
      if ((net_buf_pos >= 2)&&
               (net_buf[0] == 'E')&&
               (net_buf[1] == 'R')&&
               (net_state_vchar == 5)) // ERROR response to AT+CIFSR
        {
        // This is a nasty case. The GPRS has locked up.
        // The only solution I can find is a hard reset of the modem
        led_act(0);
        net_state_enter(NET_STATE_HARDRESET);
        }
      else if ((net_buf_pos >= 2)&&
          (((net_buf[0] == 'O')&&(net_buf[1] == 'K')))|| // OK
          (((net_buf[0] == 'S')&&(net_buf[1] == 'H')))||  // SHUT OK
          (net_state_vchar == 5)) // Local IP address
        {
        net_buf_pos = 0;
        net_timeout_ticks = 60;
        net_link = 0;
        delay100(1);
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
          led_net(1);
          }
        else if (net_watchdog == 0)
          {
          net_watchdog = 120; // We need connectivity within 120 seconds
          led_net(0);
          }
        }
      else if (memcmppgm2ram(net_buf, (char const rom far*)"+CLIP", 5) == 0)
        { // Incoming CALL
        if ((net_reg != 0x01)&&(net_reg != 0x05))
          { // Treat this as a network registration
          net_watchdog=0; // Disable watchdog, as we have connectivity
          net_reg = 0x05;
          led_net(1);
          }
        delay100(1);
        net_puts_rom(NET_HANGUP);
        net_notify_status();
        }
      else if (memcmppgm2ram(net_buf, (char const rom far*)"CONNECT OK", 10) == 0)
        {
        if (net_link == 0)
          {
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
            net_msg_start();
            net_msg_register();
            net_msg_send();
            }
          net_link = 1;
          }
        else
          {
          net_link = 0;
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
      net_state_vchar = net_state_vchar ^ 1; // Toggle LED on/off
      led_act(net_state_vchar);
      if (net_timeout_ticks%4 == 0) // Every four seconds, try to wake up
        net_puts_rom(NET_WAKEUP);
      break;

     // Timeout to short, if not connected within 10 sec
//    case NET_STATE_DOINIT:
//      if ((net_timeout_ticks==10)||(net_timeout_ticks==20))
//        net_puts_rom(NET_INIT); // Try again...
//      break;
    case NET_STATE_COPS:
      net_state_vchar = net_state_vchar ^ 1; // Toggle LED on/off
      led_net(net_state_vchar);
      led_act(net_state_vchar^1);
      break;
    case NET_STATE_SOFTRESET:
      net_state_enter(NET_STATE_START);
      break;
    case NET_STATE_READY:
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
        if ((net_msg_notify==1)&&(net_msg_serverok==1))
          {
          net_msg_notify = 0;
          delay100(10);
          net_msg_alert();
          return;
          }
        if (net_sms_notify==1)
          {
          net_sms_notify = 0;
          delay100(10);
          p = par_get(PARAM_REGPHONE);
          net_sms_stat(p);
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
// net_state_ticker60()
// State Model: Per-minute ticker
// This function is called approximately once per minute (since state
// was first entered), and gives the state a timeslice for activity.
//
void net_state_ticker60(void)
  {
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
        net_msg_stat();
        net_msg_gps();
        net_msg_tpms();
        net_msg_environment();
        net_msg_send();
        }
      net_state_vchar = net_state_vchar ^ 1;
      delay100(2);
      net_puts_rom(NET_CREG_CIPSTATUS);
      while(vUARTIntTxBufDataCnt>0) { delay100(1); } // Wait for TX flush
      delay100(2); // Wait for stable result
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
  switch (net_state)
    {
    case NET_STATE_READY:
      if ((net_link==1)&&(net_apps_connected==0))
        {
        if (net_msg_sendpending>0)
          {
          net_granular_tick -= 5; // Try again in 5 seconds...
          }
        else
          {
          net_msg_start();
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
  if ((net_state==NET_STATE_READY)&&(net_link==1))
    {
    if (TMR0H < 1)
      led_act(1);
    else
      led_act(0);
    }
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
  PORTBbits.RB0 = 1;
  UARTIntInit();

  led_net(0);
  net_reg = 0;
  net_state_enter(NET_STATE_START);
  }
