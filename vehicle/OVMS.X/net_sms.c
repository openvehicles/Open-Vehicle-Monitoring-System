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
#include "net.h"
#include "net_sms.h"
#include "net_msg.h"

#pragma udata
char net_sms_notify = 0;

rom char NET_MSG_DENIED[] = "Permission denied";
rom char NET_MSG_REGISTERED[] = "Your phone has been registered as the owner.";
rom char NET_MSG_PASSWORD[] = "Your password has been changed.";
rom char NET_MSG_PARAMS[] = "System parameters have been set.";
rom char NET_MSG_VALETTRUNK[] = "Trunk has been opened (valet mode).";
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
  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return;

  net_send_sms_start(number);
  net_puts_rom(message);
  net_puts_rom("\x1a");
  }

void net_sms_params(char* number)
  {
  unsigned char k;
  char *p;

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return;

  net_send_sms_start(number);
  net_puts_rom("Params:");
  for (k=0;k<PARAM_MAX;k++)
    {
    p = par_get(k);
    if (*p != 0)
      {
      sprintf(net_scratchpad, (rom far char*)" %u:", k);
      net_puts_ram(net_scratchpad);
      net_puts_ram(p);
      }
    }
  net_puts_rom("\x1a");
  }

void net_sms_gps(char* number)
  {
  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return;

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

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return;

  delay100(2);
  net_send_sms_start(number);

  if (car_doors1 & 0x04)
    { // Charge port door is open, we are charging
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
      case 0x0d:
        net_puts_rom("Charging, Preparing"); // Preparing
        break;
      case 0x0f:
        net_puts_rom("Charging, Heating"); // Preparing
        break;
      default:
        net_puts_rom("Charging Stopped"); // Stopped
      }
    }
  else
    { // Charge port door is closed, we are not charging
    net_puts_rom("Not charging");
    }

  net_puts_rom(" \rIdeal Range: "); // Ideal Range
  p = par_get(PARAM_MILESKM);
  if (*p == 'M') // Kmh or Miles
    sprintf(net_scratchpad, (rom far char*)"%u mi", car_idealrange); // Miles
  else
    sprintf(net_scratchpad, (rom far char*)"%u Km", (((car_idealrange << 4)+5)/10)); // Kmh
  net_puts_ram(net_scratchpad);

  net_puts_rom(" \rSOC: ");
  sprintf(net_scratchpad, (rom far char*)"%u%%", car_SOC); // 95%
  net_puts_ram(net_scratchpad);
  net_puts_rom("\x1a");
  }

void net_sms_valettrunk(char* number)
  {
  char *p;

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return;

  delay100(2);
  net_send_sms_start(number);
  net_puts_rom(NET_MSG_VALETTRUNK);
  net_puts_rom("\x1a");
  }

void net_sms_socalert(char* number)
  {
  char *p;

  delay100(10);
  net_send_sms_start(number);

  sprintf(net_scratchpad, (rom far char*)"ALERT!!! CRITICAL SOC LEVEL APPROACHED (%u%% SOC)", car_SOC); // 95%
  net_puts_ram(net_scratchpad);
  net_puts_rom("\x1a");
  delay100(5);
  }

// SMS Command Handlers
//
// All the net_sms_handle_* functions are command handlers for SMS commands

// We start with two helper functions
unsigned char net_sms_checkcaller(char *caller)
  {
  char *p = par_get(PARAM_REGPHONE);
  if (strncmp(p,caller,strlen(p)) == 0)
    return 1;
  else
    {
    if ((sys_features[FEATURE_CARBITS]&FEATURE_CB_SAD_SMS) == 0)
      net_send_sms_rom(caller,NET_MSG_DENIED);
    return 0;
    }
  }

unsigned char net_sms_checkpassarg(char *caller, char *arguments)
  {
  char *p = par_get(PARAM_REGPASS);

  if (strncmp(p,arguments,strlen(p))==0)
    return 1;
  else
    {
    if ((sys_features[FEATURE_CARBITS]&FEATURE_CB_SAD_SMS) == 0)
      net_send_sms_rom(caller,NET_MSG_DENIED);
    return 0;
    }
  }

// Now the SMS command handlers themselves...

void net_sms_handle_register(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  if (net_sms_checkpassarg(caller, arguments))
    {
    par_set(PARAM_REGPHONE, caller);
    net_send_sms_rom(caller,NET_MSG_REGISTERED);
    }
  }

void net_sms_handle_pass(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  if (net_sms_checkcaller(caller))
    {
    par_set(PARAM_REGPASS, arguments);
    net_send_sms_rom(caller,NET_MSG_PASSWORD);
    }
  }

void net_sms_handle_gps(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  if (*arguments != 0)
    {
    if (net_sms_checkpassarg(caller, arguments))
      net_sms_gps(caller);
    }
  else if (net_sms_checkcaller(caller))
    net_sms_gps(caller);
  }

void net_sms_handle_stat(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  if (*arguments != 0)
    {
    if (net_sms_checkpassarg(caller, arguments))
      net_sms_stat(caller);
    }
  else if (net_sms_checkcaller(caller))
    net_sms_stat(caller);
  }

void net_sms_handle_paramsq(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  if (net_sms_checkcaller(caller))
    net_sms_params(caller);
  }

void net_sms_handle_params(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  char *p;

  if (net_sms_checkcaller(caller))
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
  }

void net_sms_handle_feature(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  char *p;
  
  if (net_sms_checkcaller(caller))
    {
    unsigned char y = 8;
    unsigned int f;
    while (y<=(pos+1))
      {
      if ((buf[y] == ' ')||(buf[y] == '\0'))
        {
        buf[y] = '\0';
        f = atoi(buf+8);
        if ((f>=0)&&(f<FEATURES_MAX))
          {
          sys_features[f] = atoi(buf+y+1);
          if (f>=FEATURES_MAP_PARAM) // Top N features are persistent
            par_set(PARAM_FEATURE_S+(f-FEATURES_MAP_PARAM), buf+y+1);
          }
        break; // Exit the while loop, as we are done
        }
      else
        y++;
      }
    }
  }

void net_sms_handle_reset(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  char *p;

  if (net_sms_checkcaller(caller))
    net_state_enter(NET_STATE_HARDRESET);
  }

// This is the SMS command table
//
// We're a bit limited by PIC C syntax (in particular no function pointers
// as members of structures), so keep it simple and use two tables. The first
// is a list of command strings (left match, all upper case). The second are
// the command handler function pointers. The command string table array is
// terminated by an empty "" command.

rom char sms_cmdtable[][10] =
  { "REGISTER ",
    "PASS ",
    "GPS",
    "STAT",
    "PARAMS?",
    "PARAMS ",
    "FEATURE ",
    "RESET",
    "" };

rom void (*sms_hfntable[])(char *caller, char *buf, unsigned char pos, char *command, char *arguments) =
  {
  &net_sms_handle_register,
  &net_sms_handle_pass,
  &net_sms_handle_gps,
  &net_sms_handle_stat,
  &net_sms_handle_paramsq,
  &net_sms_handle_params,
  &net_sms_handle_feature,
  &net_sms_handle_reset
  };

// net_sms_in handles reception of an SMS message
//
// It tries to find a matching command handler based on the
// command tables.

void net_sms_in(char *caller, char *buf, unsigned char pos)
  {
  // The buf contains an SMS command
  // and caller contains the caller telephone number
  char *p;
  int k;

  // Convert SMS command (first word) to upper-case
  for (p=buf; ((*p!=0)&&(*p!=' ')); p++)
  	if ((*p > 0x60) && (*p < 0x7b)) *p=*p-0x20;
  if (*p==' ') p++;

  // Command parsing...
  for (k=0; sms_cmdtable[k][0] != 0; k++)
    {
    if (memcmppgm2ram(buf, (rom char*)sms_cmdtable[k], strlenpgm((rom char*)sms_cmdtable[k])) == 0)
      {
      (*sms_hfntable[k])(caller, buf, pos, buf, p);
      return;
      }
    }

  // SMS didn't match any command pattern, forward to user via net msg
  net_msg_forward_sms(caller, buf);
  }
