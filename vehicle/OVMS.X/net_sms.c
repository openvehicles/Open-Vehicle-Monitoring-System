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
#include "led.h"
#include "net.h"
#include "net_sms.h"
#include "net_msg.h"

#pragma udata
char net_sms_notify = 0;
char *net_sms_argend;

rom char NET_MSG_DENIED[] = "Permission denied";
rom char NET_MSG_REGISTERED[] = "Your phone has been registered as the owner.";
rom char NET_MSG_PASSWORD[] = "Module password has been changed.";
rom char NET_MSG_PARAMS[] = "Parameters have been set.";
rom char NET_MSG_FEATURE[] = "Feature has been set.";
rom char NET_MSG_HOMELINK[] = "Homelink activated";
rom char NET_MSG_LOCK[] = "Vehicle lock requested";
rom char NET_MSG_UNLOCK[] = "Vehicle unlock requested";
rom char NET_MSG_VALET[] = "Valet mode requested";
rom char NET_MSG_UNVALET[] = "Valet mode cancel requested";
rom char NET_MSG_CHARGEMODE[] = "Charge mode change requested";
rom char NET_MSG_CHARGESTART[] = "Charge start requested";
rom char NET_MSG_CHARGESTOP[] = "Charge stop requested";
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

void net_send_sms_ram(char* number, static const char* message)
  {
  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return;

  net_send_sms_start(number);
  net_puts_ram(message);
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
  char *p = par_get(PARAM_MODULEPASS);

  if (strncmp(p,arguments,strlen(p))==0)
    return 1;
  else
    {
    if ((sys_features[FEATURE_CARBITS]&FEATURE_CB_SAD_SMS) == 0)
      net_send_sms_rom(caller,NET_MSG_DENIED);
    return 0;
    }
  }

char* net_sms_initargs(char* arguments)
  {
  char *p;

  net_sms_argend = arguments + strlen(arguments);
  if (net_sms_argend == arguments) return NULL;

  // Zero-terminate the first argument
  for (p=arguments;(*p!=' ')&&(*p!=0);p++) {}
  if (*p==' ') *p=0;

  return arguments;
  }

char* net_sms_nextarg(char *lastarg)
  {
  char *p;

  for (p=lastarg;(*p!=0);p++) {}
  if (p == net_sms_argend) return NULL;

  // Zero-terminate the next argument
  p++;
  lastarg=p;
  for (;(*p!=' ')&&(*p!=0);p++) {}
  if (*p==' ') *p=0;

  return lastarg;
  }

// Now the SMS command handlers themselves...

void net_sms_handle_registerq(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  if (((*arguments != 0)&&(net_sms_checkpassarg(caller, arguments)))||
      (net_sms_checkcaller(caller)))
    {
    char *p = par_get(PARAM_REGPHONE);
    sprintf(net_scratchpad, (rom far char*)"Registered number: %s", p);
    net_send_sms_ram(caller,net_scratchpad);
    }
  }

void net_sms_handle_register(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  if (net_sms_checkpassarg(caller, arguments))
    {
    par_set(PARAM_REGPHONE, caller);
    net_send_sms_rom(caller,NET_MSG_REGISTERED);
    }
  }

void net_sms_handle_passq(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  if (((*arguments != 0)&&(net_sms_checkpassarg(caller, arguments)))||
      (net_sms_checkcaller(caller)))
    {
    char *p = par_get(PARAM_MODULEPASS);
    sprintf(net_scratchpad, (rom far char*)"Module password: %s", p);
    net_send_sms_ram(caller,net_scratchpad);
    }
  }

void net_sms_handle_pass(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  if (net_sms_checkcaller(caller))
    {
    par_set(PARAM_MODULEPASS, arguments);
    net_send_sms_rom(caller,NET_MSG_PASSWORD);
    }
  }

void net_sms_handle_gps(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  if (((*arguments != 0)&&(net_sms_checkpassarg(caller, arguments)))||
      (net_sms_checkcaller(caller)))
    {
    if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return;

    delay100(2);
    net_send_sms_start(caller);
    net_puts_rom(NET_MSG_GOOGLEMAPS);
    format_latlon(car_latitude,net_scratchpad);
    net_puts_ram(net_scratchpad);
    net_puts_rom(",");
    format_latlon(car_longitude,net_scratchpad);
    net_puts_ram(net_scratchpad);
    net_puts_rom("\x1a");
    }
  }

void net_sms_handle_stat(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  if (((*arguments != 0)&&(net_sms_checkpassarg(caller, arguments)))||
      (net_sms_checkcaller(caller)))
    net_sms_stat(caller);
  }

void net_sms_handle_paramsq(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  unsigned char k;
  char *p;

  if (((*arguments != 0)&&(net_sms_checkpassarg(caller, arguments)))||
      (net_sms_checkcaller(caller)))
    {
    if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return;

    net_send_sms_start(caller);
    net_puts_rom("Params:");
    for (k=0;k<PARAM_MAX;k++)
      {
      p = par_get(k);
      if (*p != 0)
        {
        sprintf(net_scratchpad, (rom far char*)"\r %u:", k);
        net_puts_ram(net_scratchpad);
        net_puts_ram(p);
        }
      }
    net_puts_rom("\x1a");
    }
  }

void net_sms_handle_params(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  unsigned char d = PARAM_MILESKM;

  if (!net_sms_checkcaller(caller)) return;

  if (net_sms_initargs(arguments) == NULL) return;
  while ((d < PARAM_MAX)&&(arguments != NULL))
    {
    if ((arguments[0]=='-')&&(arguments[1]==0))
      par_set(d++, arguments+1);
    else
      par_set(d++, arguments);
    arguments = net_sms_nextarg(arguments);
    }
  net_send_sms_rom(caller,NET_MSG_PARAMS);
  net_state_enter(NET_STATE_DONETINIT);
  }

void net_sms_handle_moduleq(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  char *p;

  if (((*arguments != 0)&&(net_sms_checkpassarg(caller, arguments)))||
      (net_sms_checkcaller(caller)))
    {
    if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return;

    net_send_sms_start(caller);
    net_puts_rom("Module:");

    p = par_get(PARAM_VEHICLEID);
    sprintf(net_scratchpad, (rom far char*)"\r VehicleID:%s", p);
    net_puts_ram(net_scratchpad);

    p = par_get(PARAM_MILESKM);
    sprintf(net_scratchpad, (rom far char*)"\r Units:%s", p);
    net_puts_ram(net_scratchpad);

    p = par_get(PARAM_NOTIFIES);
    sprintf(net_scratchpad, (rom far char*)"\r Notifications:%s", p);
    net_puts_ram(net_scratchpad);

    net_puts_rom("\x1a");
    }
  }

// MODULE <vehicleid> <units> <notifications>
void net_sms_handle_module(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  if (!net_sms_checkcaller(caller)) return;

  if (net_sms_initargs(arguments) == NULL) return;

  par_set(PARAM_VEHICLEID, arguments);
  arguments = net_sms_nextarg(arguments);
  if (arguments != NULL)
    {
    par_set(PARAM_MILESKM, arguments);
    arguments = net_sms_nextarg(arguments);
    if (arguments != NULL)
      {
      par_set(PARAM_NOTIFIES, arguments);
      }
    }
  net_send_sms_rom(caller,NET_MSG_PARAMS);
  can_initialise();
  net_state_enter(NET_STATE_DONETINIT);
  }

void net_sms_handle_gprsq(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  char *p;

  if (((*arguments != 0)&&(net_sms_checkpassarg(caller, arguments)))||
      (net_sms_checkcaller(caller)))
    {
    if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return;

    net_send_sms_start(caller);
    net_puts_rom("GPRS:");

    p = par_get(PARAM_GPRSAPN);
    sprintf(net_scratchpad, (rom far char*)"\r APN:%s", p);
    net_puts_ram(net_scratchpad);

    p = par_get(PARAM_GPRSUSER);
    sprintf(net_scratchpad, (rom far char*)"\r User:%s", p);
    net_puts_ram(net_scratchpad);

    p = par_get(PARAM_GPRSPASS);
    sprintf(net_scratchpad, (rom far char*)"\r Password:%s", p);
    net_puts_ram(net_scratchpad);

    if (net_msg_serverok)
      net_puts_rom("\r GPRS: OK\r Server: Connected OK");
    else if (net_state == NET_STATE_READY)
      net_puts_rom("\r GSM: OK\r Server: Not connected");
    else
      {
      sprintf(net_scratchpad, (rom far char*)"\r GSM/GPRS: Not connected (0x%02x)", net_state);
      net_puts_ram(net_scratchpad);
      }

    net_puts_rom("\x1a");
    }
  }

// GPRS <APN> <username> <password>
void net_sms_handle_gprs(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  if (!net_sms_checkcaller(caller)) return;

  if (net_sms_initargs(arguments) == NULL) return;

  par_set(PARAM_GPRSAPN, arguments);
  arguments = net_sms_nextarg(arguments);
  if (arguments != NULL)
    {
    if ((arguments[0]=='-')&&(arguments[1]==0))
      par_set(PARAM_GPRSUSER, arguments+1);
    else
      par_set(PARAM_GPRSUSER, arguments);
    arguments = net_sms_nextarg(arguments);
    if (arguments != NULL)
      {
      if ((arguments[0]=='-')&&(arguments[1]==0))
        par_set(PARAM_GPRSPASS, arguments+1);
      else
        par_set(PARAM_GPRSPASS, arguments);
      }
    }
  net_send_sms_rom(caller,NET_MSG_PARAMS);
  net_state_enter(NET_STATE_DONETINIT);
  }

void net_sms_handle_serverq(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  char *p;

  if (((*arguments != 0)&&(net_sms_checkpassarg(caller, arguments)))||
      (net_sms_checkcaller(caller)))
    {
    if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return;

    net_send_sms_start(caller);
    net_puts_rom("Server:");

    p = par_get(PARAM_SERVERIP);
    sprintf(net_scratchpad, (rom far char*)"\r IP:%s", p);
    net_puts_ram(net_scratchpad);

    p = par_get(PARAM_SERVERPASS);
    sprintf(net_scratchpad, (rom far char*)"\r Password:%s", p);
    net_puts_ram(net_scratchpad);

    p = par_get(PARAM_PARANOID);
    sprintf(net_scratchpad, (rom far char*)"\r Paranoid:%s", p);
    net_puts_ram(net_scratchpad);

    net_puts_rom("\x1a");
    }
  }

// SERVER <serverip> <serverpassword> <paranoidmode>
void net_sms_handle_server(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  if (!net_sms_checkcaller(caller)) return;

  if (net_sms_initargs(arguments) == NULL) return;

  par_set(PARAM_SERVERIP, arguments);
  arguments = net_sms_nextarg(arguments);
  if (arguments != NULL)
    {
    par_set(PARAM_SERVERPASS, arguments);
    arguments = net_sms_nextarg(arguments);
    if (arguments != NULL)
      {
      par_set(PARAM_PARANOID, arguments);
      }
    }
  net_send_sms_rom(caller,NET_MSG_PARAMS);
  net_state_enter(NET_STATE_DONETINIT);
  }

void net_sms_handle_diag(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  if (((*arguments != 0)&&(net_sms_checkpassarg(caller, arguments)))||
      (net_sms_checkcaller(caller)))
    {
    if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return;

    net_send_sms_start(caller);
    net_puts_rom("DIAG:");

    sprintf(net_scratchpad, (rom far char*)"\r RED Led:%d", led_code[OVMS_LED_RED]);
    net_puts_ram(net_scratchpad);

    sprintf(net_scratchpad, (rom far char*)"\r GRN Led:%d", led_code[OVMS_LED_GRN]);
    net_puts_ram(net_scratchpad);

    sprintf(net_scratchpad, (rom far char*)"\r NET State:0x%02x", net_state);
    net_puts_ram(net_scratchpad);

    net_puts_rom("\x1a");
    }
  }

void net_sms_handle_featuresq(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  unsigned char k;

  if (((*arguments != 0)&&(net_sms_checkpassarg(caller, arguments)))||
      (net_sms_checkcaller(caller)))
    {
    if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return;

    net_send_sms_start(caller);
    net_puts_rom("Features:");
    for (k=0;k<FEATURES_MAX;k++)
      {
      if (sys_features[k] != 0)
        {
        sprintf(net_scratchpad, (rom far char*)"\r %u:%d", k,sys_features[k]);
        net_puts_ram(net_scratchpad);
        }
      }
    net_puts_rom("\x1a");
    }
  }

void net_sms_handle_feature(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  unsigned char f;

  if (!net_sms_checkcaller(caller)) return;

  if (net_sms_initargs(arguments) == NULL) return;

  f = atoi(arguments);
  arguments = net_sms_nextarg(arguments);
  if ((f>=0)&&(f<FEATURES_MAX)&&(arguments != NULL))
    {
    sys_features[f] = atoi(arguments);
    if (f>=FEATURES_MAP_PARAM) // Top N features are persistent
      par_set(PARAM_FEATURE_S+(f-FEATURES_MAP_PARAM), arguments);
    if (f == FEATURE_CANWRITE) can_initialise();
    net_send_sms_rom(caller,NET_MSG_FEATURE);
    }
  }

void net_sms_handle_homelink(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  if (net_sms_checkcaller(caller))
    {
    can_tx_homelink(atoi(arguments));
    net_send_sms_rom(caller,NET_MSG_HOMELINK);
    }
  }

void net_sms_handle_lock(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  if (net_sms_checkcaller(caller))
    {
    can_tx_lockunlockcar(2, arguments);
    net_send_sms_rom(caller,NET_MSG_LOCK);
    }
  }

void net_sms_handle_unlock(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  if (net_sms_checkcaller(caller))
    {
    can_tx_lockunlockcar(3, arguments);
    net_send_sms_rom(caller,NET_MSG_UNLOCK);
    }
  }

void net_sms_handle_valet(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  if (net_sms_checkcaller(caller))
    {
    can_tx_lockunlockcar(0, arguments);
    net_send_sms_rom(caller,NET_MSG_VALET);
    }
  }

void net_sms_handle_unvalet(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  if (net_sms_checkcaller(caller))
    {
    can_tx_lockunlockcar(1, arguments);
    net_send_sms_rom(caller,NET_MSG_UNVALET);
    }
  }

// Set charge mode (params: 0=standard, 1=storage,3=range,4=performance) and optional current limit
void net_sms_handle_chargemode(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  if (!net_sms_checkcaller(caller)) return;

  if (net_sms_initargs(arguments) == NULL) return;

  strupr(arguments);
  if (memcmppgm2ram(arguments, (char const rom far*)"STA", 3) == 0)
    can_tx_setchargemode(0);
  else if (memcmppgm2ram(arguments, (char const rom far*)"STO", 3) == 0)
    can_tx_setchargemode(1);
  else if (memcmppgm2ram(arguments, (char const rom far*)"RAN", 3) == 0)
    can_tx_setchargemode(3);
  else if (memcmppgm2ram(arguments, (char const rom far*)"PER", 3) == 0)
    can_tx_setchargemode(4);
  else
    return;

  arguments = net_sms_nextarg(arguments);
  if (arguments != NULL)
    {
    can_tx_setchargecurrent(atoi(arguments));
    }

  net_send_sms_rom(caller,NET_MSG_CHARGEMODE);
  }

void net_sms_handle_chargestart(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  if (((*arguments != 0)&&(net_sms_checkpassarg(caller, arguments)))||
      (net_sms_checkcaller(caller)))
    {
    can_tx_startstopcharge(1);
    net_send_sms_rom(caller,NET_MSG_CHARGESTART);
    }
  }

void net_sms_handle_chargestop(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  if (((*arguments != 0)&&(net_sms_checkpassarg(caller, arguments)))||
      (net_sms_checkcaller(caller)))
    {
    can_tx_startstopcharge(0);
    net_send_sms_rom(caller,NET_MSG_CHARGESTOP);
    }
  }

void net_sms_handle_version(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  sprintf(net_scratchpad, (rom far char*)"OVMS Firmware version: %d.%d.%d",
          ovms_firmware[0],ovms_firmware[1],ovms_firmware[2]);
  net_send_sms_ram(caller,net_scratchpad);
  }

void net_sms_handle_reset(char *caller, char *buf, unsigned char pos, char *command, char *arguments)
  {
  char *p;

  if (((*arguments != 0)&&(net_sms_checkpassarg(caller, arguments)))||
      (net_sms_checkcaller(caller)))
    net_state_enter(NET_STATE_HARDRESET);
  }

// This is the SMS command table
//
// We're a bit limited by PIC C syntax (in particular no function pointers
// as members of structures), so keep it simple and use two tables. The first
// is a list of command strings (left match, all upper case). The second are
// the command handler function pointers. The command string table array is
// terminated by an empty "" command.

rom char sms_cmdtable[][27] =
  { "REGISTER?",
    "REGISTER",
    "PASS?",
    "PASS ",
    "GPS",
    "STAT",
    "PARAMS?",
    "PARAMS ",
    "MODULE?",
    "MODULE ",
    "GPRS?",
    "GPRS ",
    "SERVER?",
    "SERVER ",
    "DIAG",
    "FEATURES?",
    "FEATURE ",
    "HOMELINK ",
    "LOCK ",
    "UNLOCK ",
    "VALET ",
    "UNVALET ",
    "CHARGEMODE ",
    "CHARGESTART",
    "CHARGESTOP",
    "VERSION",
    "RESET",
    "" };

rom void (*sms_hfntable[])(char *caller, char *buf, unsigned char pos, char *command, char *arguments) =
  {
  &net_sms_handle_registerq,
  &net_sms_handle_register,
  &net_sms_handle_passq,
  &net_sms_handle_pass,
  &net_sms_handle_gps,
  &net_sms_handle_stat,
  &net_sms_handle_paramsq,
  &net_sms_handle_params,
  &net_sms_handle_moduleq,
  &net_sms_handle_module,
  &net_sms_handle_gprsq,
  &net_sms_handle_gprs,
  &net_sms_handle_serverq,
  &net_sms_handle_server,
  &net_sms_handle_diag,
  &net_sms_handle_featuresq,
  &net_sms_handle_feature,
  &net_sms_handle_homelink,
  &net_sms_handle_lock,
  &net_sms_handle_unlock,
  &net_sms_handle_valet,
  &net_sms_handle_unvalet,
  &net_sms_handle_chargemode,
  &net_sms_handle_chargestart,
  &net_sms_handle_chargestop,
  &net_sms_handle_version,
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
    if (memcmppgm2ram(buf, (char const rom far*)sms_cmdtable[k], strlenpgm((char const rom far*)sms_cmdtable[k])) == 0)
      {
      (*sms_hfntable[k])(caller, buf, pos, buf, p);
      return;
      }
    }

  // SMS didn't match any command pattern, forward to user via net msg
  net_msg_forward_sms(caller, buf);
  }
