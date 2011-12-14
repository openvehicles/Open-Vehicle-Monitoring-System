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
#include "net.h"
#include "net_sms.h"
#include "net_msg.h"

#pragma udata
char net_sms_notify = 0;

rom char NET_MSG_DENIED[] = "Permission denied";
rom char NET_MSG_REGISTERED[] = "Your phone has been registered as the owner.";
rom char NET_MSG_PASSWORD[] = "Your password has been changed.";
rom char NET_MSG_PARAMS[] = "System parameters have been set.";
rom char NET_MSG_GOOGLEMAPS[] = "Car location:\rhttp://maps.google.com/maps/api/staticmap?zoom=15&size=500x640&scale=2&sensor=false&markers=icon:http://goo.gl/pBcX7%7C";

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
  delay100(2);
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

  delay100(2);
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

void net_sms_in(char *caller, char *buf, unsigned char pos)
  {
  // The buf contains an SMS command
  // and caller contains the caller telephone number
  char *p;

  // Convert SMS command (first word) to upper-case
  for (p=buf; ((*p!=0)&&(*p!=' ')); p++)
  	if ((*p > 0x60) && (*p < 0x7b)) *p=*p-0x20;

  // Command parsing...
  if (memcmppgm2ram(buf, (char const rom far*)"REGISTER ", 9) == 0)
    { // Register phone
    p = par_get(PARAM_REGPASS);
    if (strncmp(p,buf+9,strlen(p))==0)
      {
      par_set(PARAM_REGPHONE, caller);
      net_send_sms_rom(caller,NET_MSG_REGISTERED);
      }
    else
      net_send_sms_rom(caller,NET_MSG_DENIED);
    }
  else if (memcmppgm2ram(buf, (char const rom far*)"PASS ", 5) == 0)
    {
    p = par_get(PARAM_REGPHONE);
    if (strncmp(p,caller,strlen(p)) == 0)
      {
      par_set(PARAM_REGPASS, buf+5);
      net_send_sms_rom(caller,NET_MSG_PASSWORD);
      }
    else
      net_send_sms_rom(caller,NET_MSG_DENIED);
    }
  else if (memcmppgm2ram(buf, (char const rom far*)"GPS ", 4) == 0)
    {
    p = par_get(PARAM_REGPASS);
    if (strncmp(p,buf+4,strlen(p))==0)
      net_sms_gps(caller);
    else
      net_send_sms_rom(caller,NET_MSG_DENIED);
    }
  else if (memcmppgm2ram(buf, (char const rom far*)"GPS", 3) == 0)
    {
    p = par_get(PARAM_REGPHONE);
    if (strncmp(p,caller,strlen(p)) == 0)
      net_sms_gps(caller);
    else
      net_send_sms_rom(caller,NET_MSG_DENIED);
    }
  else if (memcmppgm2ram(buf, (char const rom far*)"STAT ", 5) == 0)
    {
    p = par_get(PARAM_REGPASS);
    if (strncmp(p,buf+5,strlen(p))==0)
      net_sms_stat(caller);
    else
      net_send_sms_rom(caller,NET_MSG_DENIED);
    }
  else if (memcmppgm2ram(buf, (char const rom far*)"STAT", 4) == 0)
    {
    p = par_get(PARAM_REGPHONE);
    if (strncmp(p,caller,strlen(p)) == 0)
      net_sms_stat(caller);
    else
      net_send_sms_rom(caller,NET_MSG_DENIED);
    }
  else if (memcmppgm2ram(buf, (char const rom far*)"PARAMS?", 7) == 0)
    {
    p = par_get(PARAM_REGPHONE);
    if (strncmp(p,caller,strlen(p)) == 0)
      net_sms_params(caller);
    else
      net_send_sms_rom(caller,NET_MSG_DENIED);
    }
  else if (memcmppgm2ram(buf, (char const rom far*)"PARAMS ", 7) == 0)
    {
    p = par_get(PARAM_REGPHONE);
    if (strncmp(p,caller,strlen(p)) == 0)
      {
      unsigned char d = PARAM_MILESKM;
      unsigned char x = 7;
      unsigned char y = x;
      while ((y<=(pos+1))&&(d < PARAM_MAX))
        {
        if ((buf[y] == ' ')||(buf[y] == '\0'))
          {
          buf[y] = '\0';
          if ((buf[x]=='-')&&(buf[x+1]=='\0'))
            buf[x] = '\0'; // Special case '-' is empty value
          par_set(d++, buf+x);
          x=++y;
          }
        else
          y++;
        }
      net_send_sms_rom(caller,NET_MSG_PARAMS);
      net_state_enter(NET_STATE_SOFTRESET);
      }
    else
      net_send_sms_rom(caller,NET_MSG_DENIED);
    }
  else // SMS didn't match any command pattern, forward to user via net msg
    {
    net_msg_forward_sms(caller, buf);
    }
  }
