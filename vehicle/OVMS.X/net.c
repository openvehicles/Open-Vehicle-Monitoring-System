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
#include "ovms.h"
#include "net_sms.h"
#include "net_msg.h"

// NET data
#pragma udata
unsigned char net_state = 0;
unsigned char net_state_vchar = 0;
unsigned int  net_state_vint = 0;
unsigned char net_timeout_goto = 0;
unsigned int  net_timeout_ticks = 0;
unsigned int  net_granular_tick = 0;
unsigned int  net_watchdog = 0;
unsigned char net_buf_pos = 0;
char net_caller[NET_TEL_MAX] = {0};
char net_scratchpad[100];

#pragma udata NETBUF
char net_buf[NET_BUF_MAX];
#pragma udata

// ROM Constants
rom char NET_INIT[] = "AT+CPIN?;+CREG=1;+CLIP=1;+CMGF=1;+CNMI=2,2;+CSDH=0\r";
rom char NET_CIPSTATUS[] = "AT+CIPSTATUS\r";
rom char NET_HANGUP[] = "ATH\r";
rom char NET_CREG[] = "AT+CREG?\r";
rom char NET_COPS[] = "AT+COPS=0\r";

// Interrupt Service Routine
void low_isr(void);

// serial interrupt taken as low priority interrupt
#pragma code uart_int_service = 0x08
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

void net_reset_async(void)
  {
  vUARTIntStatus.UARTIntRxError = 0;
  }

void net_poll(void)
  {
  unsigned char x;
  while(UARTIntGetChar(&x))
    {
    if (x == 0x0d) continue; // Skip 0x0d (CR)
    net_buf[net_buf_pos++] = x;
    if (net_buf_pos == NET_BUF_MAX) net_buf_pos--;
    if (x == 0x0A) // Newline?
      {
      net_buf_pos--;
      net_buf[net_buf_pos] = 0; // mark end of string for string search functions.
      net_state_activity();
      net_buf_pos = 0;
      }
    }
  }

void net_puts_rom(static const rom char *data)
  {
  /* Send characters up to the null */
  do
    {
    while (vUARTIntStatus.UARTIntTxBufferFull);
    UARTIntPutChar(*data);
    } while (*++data);
  }

void net_puts_ram(const char *data)
  {
  /* Send characters up to the null */
  do
    {
    while (vUARTIntStatus.UARTIntTxBufferFull);
    UARTIntPutChar(*data);
    } while (*++data);
  }

void net_putc_ram(const char data)
  {
  /* Send one character */
  while (vUARTIntStatus.UARTIntTxBufferFull);
  UARTIntPutChar(data);
  }

void net_notify_status(void)
  {
  // Emit an unsolicited notification showing current status
  char *p,*q;
  p = par_get(PARAM_NOTIFIES);
  if (strstrrampgm(p,(char const rom far*)"SMS") != NULL)
    {
    q = par_get(PARAM_REGPHONE);
    net_sms_stat(q);
    }
  }

void net_state_enter(unsigned char newstate)
  {
  char *p;

  net_state = newstate;
  switch(net_state)
    {
    case NET_STATE_START:
      net_timeout_goto = NET_STATE_DOINIT;
      net_timeout_ticks = 10;
      net_state_vchar = 1;
      led_act(net_state_vchar);
      net_msg_init();
      break;
    case NET_STATE_RESET:
      net_timeout_goto = 0;
      break;
    case NET_STATE_DOINIT:
      net_timeout_goto = NET_STATE_RESET;
      net_timeout_ticks = 10;
      led_act(0);
      net_puts_rom(NET_INIT);
      break;
    case NET_STATE_DONETINIT:
      p = par_get(PARAM_GPRSAPN);
      if (p[0] != '\0')
        {
        net_timeout_goto = NET_STATE_RESET;
        net_timeout_ticks = 60;
        led_act(0);
        net_state_vchar = 0;
        delay100(2);
        net_puts_rom("AT+CIPSHUT\r");
        break;
        }
      else
        net_state = NET_STATE_READY;
      // N.B. Drop through without a break
    case NET_STATE_READY:
      net_timeout_goto = 0;
      net_state_vchar = 0;
      if ((net_reg != 0x01)&&(net_reg != 0x05))
        net_watchdog = 120; // I need a network within 2 mins, else reset
      else
        net_watchdog = 0; // Disable net watchdog
      break;
    case NET_STATE_COPS:
      delay100(2);
      net_timeout_goto = NET_STATE_RESET;
      net_timeout_ticks = 120;
      net_msg_disconnected();
      net_puts_rom(NET_COPS);
      break;
    case NET_STATE_SMSIN:
      net_timeout_goto = NET_STATE_RESET;
      net_timeout_ticks = 20;
      break;
    }
  }

void net_state_activity()
  {
  switch (net_state)
    {
    case NET_STATE_DOINIT:
      if ((net_buf_pos >= 2)&&(net_buf[0] == 'O')&&(net_buf[1] == 'K'))
        {
        net_state_enter(NET_STATE_COPS);
        }
      else // Discard the input
        net_buf_pos = 0;
      break;
    case NET_STATE_COPS:
      if ((net_buf_pos >= 2)&&(net_buf[0] == 'O')&&(net_buf[1] == 'K'))
        net_state_enter(NET_STATE_DONETINIT); // COPS reconnect was OK
      else if ((net_buf_pos >= 5)&&(net_buf[0] == 'E')&&(net_buf[1] == 'R'))
        net_state_enter(NET_STATE_RESET); // Reset the entire async
      break;
    case NET_STATE_DONETINIT:
      if ((net_buf_pos >= 2)&&
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
        else
          {
          net_watchdog = 60; // We need connectivity within 60 seconds
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
      else if (memcmppgm2ram(net_buf, (char const rom far*)"+CMT: \"", 7) == 0)
        { // Incoming SMS message
        unsigned char x = 7;
        while ((net_buf[x++] != '\"') && (x < net_buf_pos)); // Search for start of Phone number
        net_buf[x - 1] = '\0'; // mark end of string
        net_caller[0] = '\0';
        strncpy(net_caller,net_buf+7,NET_TEL_MAX);
        net_caller[NET_TEL_MAX-1] = '\0';
        if ((net_reg != 0x01)&&(net_reg != 0x05))
          { // Treat this as a network registration
          net_watchdog=0; // Disable watchdog, as we have connectivity
          net_reg = 0x05;
          led_net(1);
          }
        net_state_enter(NET_STATE_SMSIN);
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
            net_state_enter(NET_STATE_DONETINIT);
            }
          }
        }
      else if (memcmppgm2ram(net_buf, (char const rom far*)"+IPD,", 5) == 0)
        { // Incoming TCP/IP NET message
        unsigned char x = 5;
        while ((net_buf[x++] != ':') && (x < net_buf_pos)); // Search for start of data
        net_msg_in(net_buf+x);
        }
      break;
    case NET_STATE_SMSIN:
      net_sms_in(net_caller,net_buf,net_buf_pos);
      net_state_enter(NET_STATE_READY);
      break;
    }
  }

void net_state_ticker1(void)
  {
  switch (net_state)
    {
    case NET_STATE_START:
      net_state_vchar = net_state_vchar ^ 1; // Toggle LED on/off
      led_act(net_state_vchar);
      break;
    case NET_STATE_RESET:
      net_state_enter(NET_STATE_START);
      break;
    case NET_STATE_READY:
      if (net_watchdog > 0)
        {
        if (--net_watchdog == 0)
          net_state_enter(NET_STATE_COPS); // Reset network connection
        }
      break;
    }
  }

void net_state_ticker60(void)
  {
  switch (net_state)
    {
    case NET_STATE_READY:
      net_state_vchar = net_state_vchar ^ 1;
      delay100(2);
      if (net_state_vchar == 0)
        net_puts_rom(NET_CREG);
      else
        net_puts_rom(NET_CIPSTATUS);
      break;
    }
  }

void net_state_ticker300(void)
  {
  switch (net_state)
    {
    }
  }

void net_state_ticker600(void)
  {
  switch (net_state)
    {
    case NET_STATE_READY:
      if (net_link==1)
        {
//        net_msg_start();
//        net_msg_stat();
//        net_msg_gps();
//        net_msg_send();
        }
      break;
    }
  }

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

void net_initialise(void)
  {
  // I/O configuration PORT C
  TRISC = 0x80; // Port C RC0-6 output, RC7 input
  PORTC = 0x00;
  UARTIntInit();

  led_net(0);
  net_reg = 0;
  net_state_enter(NET_STATE_START);
  }