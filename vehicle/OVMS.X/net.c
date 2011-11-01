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

// STATES
#define NET_STATE_START      0x01  // Initialise and get ready to go
#define NET_STATE_RESET      0x02  // Reset and re-initialise the network
#define NET_STATE_DOINIT     0x10  // Initialise the GSM network
#define NET_STATE_READY      0x20  // READY and handling calls
#define NET_STATE_COPS       0x21  // GSM COPS carrier selection
#define NET_STATE_SMSIN      0x30  // Wait for SMS message text
#define NET_STATE_DONETINIT  0x40  // Initalise the GPRS network

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
char net_scratchpad[32];

#pragma udata NETBUF
char net_buf[NET_BUF_MAX];
#pragma udata

// ROM Constants
#define NET_INIT_MAX 7
#define NET_NETINIT_MAX 7
rom char NET_INIT[NET_INIT_MAX][13]
  = {"AT\r","AT+CPIN?\r","AT+CREG=1\r","AT+CLIP=1\r",
     "AT+CMGF=1\r","AT+CNMI=2,2\r","AT+CSDH=0\r"};
rom char NET_CIPSTATUS[] = "AT+CIPSTATUS\r";
rom char NET_HANGUP[] = "ATH\r";
rom char NET_CREG[] = "AT+CREG?\r";
rom char NET_COPS[] = "AT+COPS=0\r";
rom char NET_MSG_DENIED[] = "Permission denied";
rom char NET_MSG_REGISTERED[] = "Your phone has been registered as the owner.";
rom char NET_MSG_PASSWORD[] = "Your password has been changed.";
rom char NET_MSG_PARAMS[] = "System parameters have been set.";
rom char NET_MSG_GOOGLEMAPS[] = "Car location:\rhttp://maps.google.com/maps/api/staticmap?zoom=15&size=500x640&scale=2&sensor=false&markers=icon:http://goo.gl/pBcX7%7C";

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

void net_send_sms_start(char* number)
  {
  net_puts_rom("AT+CMGS=\"");
  net_puts_ram(number);
  net_puts_rom("\"\r\n");
  delay100(2);
  }

void net_send_sms_rom(char* number, static const rom char* message)
  {
  net_send_sms_start(number);
  net_puts_rom(message);
  net_puts_rom("\x1a");
  }

void net_sms_params(char* number)
  {
  unsigned char k;
  char *p;

  net_send_sms_start(number);
  net_puts_rom("Params:");
  for (k=0;k<PARAM_MAX;k++)
    {
    sprintf(net_scratchpad, (rom far char*)" %u:", k);
    net_puts_ram(net_scratchpad);
    net_puts_ram(par_get(k));
    }
  net_puts_rom("\x1a");
  }

void net_sms_gps(char* number)
  {
  net_send_sms_start(number);
  net_puts_rom(NET_MSG_GOOGLEMAPS);
  format_latlon(car_latitude,net_scratchpad);
  net_puts_ram(net_scratchpad);
  net_puts_rom(",");
  format_latlon(car_longitude,net_scratchpad);
  net_puts_ram(net_scratchpad);
  net_puts_rom("\x1a");
  }

void net_sms_stat(char* number)
  {
  char *p;

  net_send_sms_start(number);

  switch (car_chargemode)
    {
    case 0x00:
      net_puts_rom("Standard - "); // Charge Mode Standard
      break;
    case 0x01:
      net_puts_rom("Storage - "); // Storage
      break;
    case 0x03:
      net_puts_rom("Range - "); // Range
      break;
    case 0x04:
      net_puts_rom("Performance - "); // Performance
    }
  switch (car_chargestate)
    {
    case 0x01:
      net_puts_rom("Charging"); // Charge State Charging
      break;
    case 0x02:
      net_puts_rom("Charging, Topping off"); // Topping off
      break;
    case 0x04:
      net_puts_rom("Charging Done"); // Done
      break;
    default:
      net_puts_rom("Charging Stopped"); // Stopped
    }

  net_puts_rom("\rIdeal Range: "); // Ideal Range
  p = par_get(PARAM_MILESKM);
  if (*p == 'M') // Kmh or Miles
    sprintf(net_scratchpad, (rom far char*)"%u mi", car_idealrange); // Miles
  else
    sprintf(net_scratchpad, (rom far char*)"%u Km", (unsigned int) ((float) car_idealrange * 1.609)); // Kmh
  net_puts_ram(net_scratchpad);

  net_puts_rom("\rSOC: ");
  sprintf(net_scratchpad, (rom far char*)"%u%%", car_SOC); // 95%
  net_puts_ram(net_scratchpad);
  net_puts_rom("\x1a");
  }

// Start to send a net msg
void net_msg_start(void)
  {
  delay100(5);
  net_puts_rom("AT+CIPSEND\r");
  delay100(2);
  }

// Finish sending a net msg
void net_msg_send(void)
  {
  net_puts_rom("\x1a");
  }

// Register to the NET OVMS server
void net_msg_register(void)
  {
  net_puts_rom("MP00");
  net_puts_ram(par_get(PARAM_MYID));
  net_puts_rom(" ");
  net_puts_ram(par_get(PARAM_NETPASS1));
  net_puts_rom(" CA\r\n");
  }

void net_msg_stat(void)
  {
  char *p;

  net_puts_rom("MP00");
  net_puts_ram(par_get(PARAM_MYID));
  net_puts_rom(" ");
  net_puts_ram(par_get(PARAM_NETPASS1));
  net_puts_rom(" CH");
  p = par_get(PARAM_MILESKM);
  sprintf(net_scratchpad,(rom far char*)"%d,%s,%d,%d,",car_SOC,p,car_linevoltage,car_chargecurrent);
  net_puts_ram(net_scratchpad);
  switch (car_chargestate)
    {
    case 0x01:
      net_puts_rom("charging,"); // Charge State Charging
      break;
    case 0x02:
      net_puts_rom("topoff,"); // Topping off
      break;
    case 0x04:
      net_puts_rom("done,"); // Done
      break;
    default:
      net_puts_rom("stopped,"); // Stopped
    }
  switch (car_chargemode)
    {
    case 0x00:
      net_puts_rom("standard,"); // Charge Mode Standard
      break;
    case 0x01:
      net_puts_rom("storage,"); // Storage
      break;
    case 0x03:
      net_puts_rom("range,"); // Range
      break;
    case 0x04:
      net_puts_rom("performance,"); // Performance
    }
  if (*p == 'M') // Kmh or Miles
    sprintf(net_scratchpad, (rom far char*)"%u,", car_idealrange);
  else
    sprintf(net_scratchpad, (rom far char*)"%u,", (unsigned int) ((float) car_idealrange * 1.609));
  net_puts_ram(net_scratchpad);
  if (*p == 'M') // Kmh or Miles
    sprintf(net_scratchpad, (rom far char*)"%u", car_estrange);
  else
    sprintf(net_scratchpad, (rom far char*)"%u", (unsigned int) ((float) car_estrange * 1.609));
  net_puts_ram(net_scratchpad);
  net_puts_rom("\r\n");
  }

void net_msg_gps(void)
  {
  char *p;

  net_puts_rom("MP00");
  net_puts_ram(par_get(PARAM_MYID));
  net_puts_rom(" ");
  net_puts_ram(par_get(PARAM_REGPASS));
  net_puts_rom(" CG");
  format_latlon(car_latitude,net_scratchpad);
  net_puts_ram(net_scratchpad);
  net_puts_rom(",");
  format_latlon(car_longitude,net_scratchpad);
  net_puts_ram(net_scratchpad);
  net_puts_rom("\r\n");
  }

void net_notify_status(void)
  {
  // Emit an unsolicited notification showing current status
  }

void net_sms_in(void)
  {
  // The net_buf contains an SMS command
  // and net_caller contains the caller telephone number
  char *p;

  // Convert SMS command (first word) to upper-case
  for (p=net_buf; ((*p!=0)&&(*p!=' ')); p++)
  	if ((*p > 0x60) && (*p < 0x7b)) *p=*p-0x20;

  // Command parsing...
  if (memcmppgm2ram(net_buf, (char const rom far*)"REGISTER ", 9) == 0)
    { // Register phone
    p = par_get(PARAM_REGPASS);
    if (strncmp(p,net_buf+9,strlen(p))==0)
      {
      par_set(PARAM_REGPHONE, net_caller);
      net_send_sms_rom(net_caller,NET_MSG_REGISTERED);
      }
    else
      net_send_sms_rom(net_caller,NET_MSG_DENIED);
    }
  else if (memcmppgm2ram(net_buf, (char const rom far*)"PASS ", 5) == 0)
    {
    p = par_get(PARAM_REGPHONE);
    if (strncmp(p,net_caller,strlen(p)) == 0)
      {
      par_set(PARAM_REGPASS, net_buf+5);
      net_send_sms_rom(net_caller,NET_MSG_PASSWORD);
      }
    else
      net_send_sms_rom(net_caller,NET_MSG_DENIED);
    }
  else if (memcmppgm2ram(net_buf, (char const rom far*)"GPS ", 4) == 0)
    {
    p = par_get(PARAM_REGPASS);
    if (strncmp(p,net_buf+4,strlen(p))==0)
      net_sms_gps(net_caller);
    else
      net_send_sms_rom(net_caller,NET_MSG_DENIED);
    }
  else if (memcmppgm2ram(net_buf, (char const rom far*)"GPS", 3) == 0)
    {
    p = par_get(PARAM_REGPHONE);
    if (strncmp(p,net_caller,strlen(p)) == 0)
      net_sms_gps(net_caller);
    else
      net_send_sms_rom(net_caller,NET_MSG_DENIED);
    }
  else if (memcmppgm2ram(net_buf, (char const rom far*)"STAT ", 5) == 0)
    {
    p = par_get(PARAM_REGPASS);
    if (strncmp(p,net_buf+5,strlen(p))==0)
      net_sms_stat(net_caller);
    else
      net_send_sms_rom(net_caller,NET_MSG_DENIED);
    }
  else if (memcmppgm2ram(net_buf, (char const rom far*)"STAT", 4) == 0)
    {
    p = par_get(PARAM_REGPHONE);
    if (strncmp(p,net_caller,strlen(p)) == 0)
      net_sms_stat(net_caller);
    else
      net_send_sms_rom(net_caller,NET_MSG_DENIED);
    }
  else if (memcmppgm2ram(net_buf, (char const rom far*)"PARAMS?", 7) == 0)
    {
    p = par_get(PARAM_REGPHONE);
    if (strncmp(p,net_caller,strlen(p)) == 0)
      net_sms_params(net_caller);
    else
      net_send_sms_rom(net_caller,NET_MSG_DENIED);
    }
  else if (memcmppgm2ram(net_buf, (char const rom far*)"PARAMS ", 7) == 0)
    {
    p = par_get(PARAM_REGPHONE);
    if (strncmp(p,net_caller,strlen(p)) == 0)
      {
      unsigned char d = PARAM_MILESKM;
      unsigned char x = 7;
      unsigned char y = x;
      while ((y<=(net_buf_pos+1))&&(d < PARAM_MAX))
        {
        if ((net_buf[y] == ' ')||(net_buf[y] == '\0'))
          {
          net_buf[y] = '\0';
          if ((net_buf[x]=='-')&&(net_buf[x+1]=='\0'))
            net_buf[x] = '\0'; // Special case '-' is empty value
          par_set(d++, net_buf+x);
          x=++y;
          }
        else
          y++;
        }
      net_send_sms_rom(net_caller,NET_MSG_PARAMS);
      net_state_enter(NET_STATE_RESET);
      }
    else
      net_send_sms_rom(net_caller,NET_MSG_DENIED);
    }
  }

// Receive a NET msg from the OVMS server
void net_msg_in(char* msg)
  {
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
      break;
    case NET_STATE_RESET:
      net_timeout_goto = 0;
      break;
    case NET_STATE_DOINIT:
      net_timeout_goto = NET_STATE_RESET;
      net_timeout_ticks = 60;
      led_act(0);
      net_state_vchar = 0;
      net_puts_rom(NET_INIT[net_state_vchar]);
      break;
    case NET_STATE_DONETINIT:
      p = par_get(PARAM_GPRSAPN);
      if (p[0] != '\0')
        {
        net_timeout_goto = NET_STATE_RESET;
        net_timeout_ticks = 60;
        led_act(0);
        net_state_vchar = 0;
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
        net_buf_pos = 0;
        if (++net_state_vchar < NET_INIT_MAX)
          {
          delay100(1);
          net_puts_rom(NET_INIT[net_state_vchar]);
          net_timeout_ticks = 60;
          }
        else
          net_state_enter(NET_STATE_COPS);
        }
      else // Discard the input
        net_buf_pos = 0;
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
      else if (memcmppgm2ram(net_buf, (char const rom far*)"STATE: ", 7) == 0)
        { // Incoming CIPSTATUS
        if (memcmppgm2ram(net_buf, (char const rom far*)"STATE: CONNECT OK", 17) == 0)
          {
          if (net_link == 0)
            {
            net_msg_start();
            net_msg_register();
            net_msg_stat();
            net_msg_gps();
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
        while ((net_buf[x++] != ':') && (x < net_buf_pos)); // Search for start of Phone number
        net_msg_in(net_buf+x);
        }
      break;
    case NET_STATE_COPS:
      if ((net_buf_pos >= 2)&&(net_buf[0] == 'O')&&(net_buf[1] == 'K'))
        net_state_enter(NET_STATE_DONETINIT); // COPS reconnect was OK
      else if ((net_buf_pos >= 5)&&(net_buf[0] == 'E')&&(net_buf[1] == 'R'))
        net_state_enter(NET_STATE_RESET); // Reset the entire async
      break;
    case NET_STATE_SMSIN:
      net_sms_in();
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
        net_msg_start();
        net_msg_stat();
        net_msg_gps();
        net_msg_send();
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