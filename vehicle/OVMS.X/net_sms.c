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
rom char NET_MSG_ALARM[] = "Vehicle alarm is sounding!";
rom char NET_MSG_VALETTRUNK[] = "Trunk has been opened (valet mode).";
//rom char NET_MSG_GOOGLEMAPS[] = "Car location:\r\nhttp://maps.google.com/maps/api/staticmap?zoom=15&size=500x640&scale=2&sensor=false&markers=icon:http://goo.gl/pBcX7%7C";
rom char NET_MSG_GOOGLEMAPS[] = "Car location:\r\nhttps://maps.google.com/maps?q=";



void net_send_sms_start(char* number)
  {
  if (net_state == NET_STATE_DIAGMODE)
    {
    net_puts_rom("# ");
    }
  else
    {
    net_puts_rom("AT+CMGS=\"");
    net_puts_ram(number);
    net_puts_rom("\"\r\n");
    delay100(2);
    }
  }

void net_send_sms_finish(void)
  {
  if (net_state == NET_STATE_DIAGMODE)
    {
    net_puts_rom("\r\n");
    }
  else
    {
    net_puts_rom("\x1a");
    }
  }

void net_send_sms_rom(char* number, static const rom char* message)
  {
  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return;

  net_send_sms_start(number);
  net_puts_rom(message);
  net_send_sms_finish();
  }

void net_send_sms_ram(char* number, static const char* message)
  {
  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return;

  net_send_sms_start(number);
  net_puts_ram(message);
  net_send_sms_finish();
  }

BOOL net_sms_stat(char* number)
  {
  char *p;

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

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

  net_puts_rom(" \r\nRange: "); // Estimated + Ideal Range
  p = par_get(PARAM_MILESKM);
  if (*p == 'M') // Kmh or Miles
    sprintf(net_scratchpad, (rom far char*)"%u - %u mi"
            , car_estrange
            , car_idealrange ); // Miles
  else
    sprintf(net_scratchpad, (rom far char*)"%u - %u Km"
            , (((car_estrange << 4)+5)/10)
            , (((car_idealrange << 4)+5)/10) ); // Km
  net_puts_ram(net_scratchpad);

  net_puts_rom(" \r\nSOC: ");
  sprintf(net_scratchpad, (rom far char*)"%u%%", car_SOC); // 95%
  net_puts_ram(net_scratchpad);

  net_puts_rom(" \r\nODO: ");
  if (*p == 'M') // Km or Miles
    sprintf(net_scratchpad, (rom far char*)"%lu mi", car_odometer / 10); // Miles
  else
    sprintf(net_scratchpad, (rom far char*)"%lu Km", (((car_odometer << 4)+5)/100)); // Km
  net_puts_ram(net_scratchpad);

  return TRUE;
  }

void net_sms_alarm(char* number)
  {
  char *p;

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return;

  delay100(2);
  net_send_sms_start(number);
  net_puts_rom(NET_MSG_ALARM);
  net_send_sms_finish();
  }

void net_sms_valettrunk(char* number)
  {
  char *p;

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return;

  delay100(2);
  net_send_sms_start(number);
  net_puts_rom(NET_MSG_VALETTRUNK);
  net_send_sms_finish();
  }

void net_sms_socalert(char* number)
  {
  char *p;

  delay100(10);
  net_send_sms_start(number);

  sprintf(net_scratchpad, (rom far char*)"ALERT!!! CRITICAL SOC LEVEL APPROACHED (%u%% SOC)", car_SOC); // 95%
  net_puts_ram(net_scratchpad);
  net_send_sms_finish();
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

  if ((arguments != NULL)&&(strncmp(p,arguments,strlen(p))==0))
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

BOOL net_sms_handle_registerq(char *caller, char *command, char *arguments)
  {
  char *p = par_get(PARAM_REGPHONE);

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

  net_send_sms_start(caller);
  sprintf(net_scratchpad, (rom far char*)"Registered number: %s", p);
  net_puts_ram(net_scratchpad);
  return TRUE;
  }

BOOL net_sms_handle_register(char *caller, char *command, char *arguments)
  {
  par_set(PARAM_REGPHONE, caller);

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

  net_send_sms_start(caller);
  net_puts_rom(NET_MSG_REGISTERED);
  return TRUE;
  }

BOOL net_sms_handle_passq(char *caller, char *command, char *arguments)
  {
  char *p = par_get(PARAM_MODULEPASS);

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

  net_send_sms_start(caller);
  sprintf(net_scratchpad, (rom far char*)"Module password: %s", p);
  net_puts_ram(net_scratchpad);
  return TRUE;
  }

BOOL net_sms_handle_pass(char *caller, char *command, char *arguments)
  {
  par_set(PARAM_MODULEPASS, arguments);

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

  net_send_sms_start(caller);
  net_puts_rom(NET_MSG_PASSWORD);
  return TRUE;
  }

BOOL net_sms_handle_gps(char *caller, char *command, char *arguments)
  {
  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

  delay100(2);
  net_send_sms_start(caller);
  net_puts_rom(NET_MSG_GOOGLEMAPS);
  format_latlon(car_latitude,net_scratchpad);
  net_puts_ram(net_scratchpad);
  net_puts_rom(",");
  format_latlon(car_longitude,net_scratchpad);
  net_puts_ram(net_scratchpad);
  return TRUE;
  }

BOOL net_sms_handle_stat(char *caller, char *command, char *arguments)
  {
  return net_sms_stat(caller);
  }

BOOL net_sms_handle_paramsq(char *caller, char *command, char *arguments)
  {
  unsigned char k;
  char *p;

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

  net_send_sms_start(caller);
  net_puts_rom("Params:");
  for (k=0;k<PARAM_MAX;k++)
    {
    p = par_get(k);
    if (*p != 0)
      {
      sprintf(net_scratchpad, (rom far char*)"\r\n %u:", k);
      net_puts_ram(net_scratchpad);
      net_puts_ram(p);
      }
    }
  return TRUE;
  }

BOOL net_sms_handle_params(char *caller, char *command, char *arguments)
  {
  unsigned char d = PARAM_MILESKM;

  while ((d < PARAM_MAX)&&(arguments != NULL))
    {
    if ((arguments[0]=='-')&&(arguments[1]==0))
      par_set(d++, arguments+1);
    else
      par_set(d++, arguments);
    arguments = net_sms_nextarg(arguments);
    }

  net_send_sms_start(caller);
  net_puts_rom(NET_MSG_PARAMS);
  net_send_sms_finish();

  if (net_state != NET_STATE_DIAGMODE)
    net_state_enter(NET_STATE_DONETINIT);

  return FALSE;
  }

BOOL net_sms_handle_ap(char *caller, char *command, char *arguments)
  {
  unsigned char d = 0;
  char *p = par_get(PARAM_MODULEPASS);

  while ((d < PARAM_MAX)&&(arguments != NULL))
    {
    if ((arguments[0]=='-')&&(arguments[1]==0))
      par_set(d++, arguments+1);
    else
      par_set(d++, arguments);
    arguments = net_sms_nextarg(arguments);
    }

  if (net_state != NET_STATE_DIAGMODE)
    net_state_enter(NET_STATE_FIRSTRUN);

  return FALSE;
  }

BOOL net_sms_handle_moduleq(char *caller, char *command, char *arguments)
  {
  char *p;

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

  net_send_sms_start(caller);
  net_puts_rom("Module:");

  p = par_get(PARAM_VEHICLEID);
  sprintf(net_scratchpad, (rom far char*)"\r\n VehicleID:%s", p);
  net_puts_ram(net_scratchpad);

  p = par_get(PARAM_VEHICLETYPE);
  sprintf(net_scratchpad, (rom far char*)"\r\n VehicleType:%s", p);
  net_puts_ram(net_scratchpad);

  p = par_get(PARAM_MILESKM);
  sprintf(net_scratchpad, (rom far char*)"\r\n Units:%s", p);
  net_puts_ram(net_scratchpad);

  p = par_get(PARAM_NOTIFIES);
  sprintf(net_scratchpad, (rom far char*)"\r\n Notifications:%s", p);
  net_puts_ram(net_scratchpad);

  return TRUE;
  }

// MODULE <vehicleid> <units> <notifications>
BOOL net_sms_handle_module(char *caller, char *command, char *arguments)
  {
  BOOL moduleq_result;

  par_set(PARAM_VEHICLEID, arguments);
  arguments = net_sms_nextarg(arguments);
  if (arguments != NULL)
    {
    par_set(PARAM_MILESKM, arguments);
    arguments = net_sms_nextarg(arguments);
    if (arguments != NULL)
      {
      par_set(PARAM_NOTIFIES, arguments);
      arguments = net_sms_nextarg(arguments);
      if (arguments != NULL)
        {
        par_set(PARAM_VEHICLETYPE, arguments);
        }
      }
    }
  moduleq_result = net_sms_handle_moduleq(caller, command, arguments);
  vehicle_initialise();

  if (net_state != NET_STATE_DIAGMODE)
    net_state_enter(NET_STATE_DONETINIT);

  return moduleq_result;
  }

BOOL net_sms_handle_gprsq(char *caller, char *command, char *arguments)
  {
  char *p;

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

  net_send_sms_start(caller);
  net_puts_rom("GPRS:");

  p = par_get(PARAM_GPRSAPN);
  sprintf(net_scratchpad, (rom far char*)"\r\n APN:%s", p);
  net_puts_ram(net_scratchpad);

  p = par_get(PARAM_GPRSUSER);
  sprintf(net_scratchpad, (rom far char*)"\r\n User:%s", p);
  net_puts_ram(net_scratchpad);

  p = par_get(PARAM_GPRSPASS);
  sprintf(net_scratchpad, (rom far char*)"\r\n Password:%s", p);
  net_puts_ram(net_scratchpad);

  sprintf(net_scratchpad, (rom far char*)"\r\n GSM:%s", car_gsmcops);
  net_puts_ram(net_scratchpad);

  if (net_msg_serverok)
    net_puts_rom("\r\n GPRS: OK\r\n Server: Connected OK");
  else if (net_state == NET_STATE_READY)
    net_puts_rom("\r\n GSM: OK\r\n Server: Not connected");
  else
    {
    sprintf(net_scratchpad, (rom far char*)"\r\n GSM/GPRS: Not connected (0x%02x)", net_state);
    net_puts_ram(net_scratchpad);
    }

  return TRUE;
  }

// GPRS <APN> <username> <password>
BOOL net_sms_handle_gprs(char *caller, char *command, char *arguments)
  {
  BOOL gprsq_result;

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
  gprsq_result = net_sms_handle_gprsq(caller, command, arguments);

  if (net_state != NET_STATE_DIAGMODE)
    net_state_enter(NET_STATE_DONETINIT);

  return gprsq_result;
  }

BOOL net_sms_handle_gsmlockq(char *caller, char *command, char *arguments)
  {
  char *p;

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

  net_send_sms_start(caller);
  net_puts_rom("GSMLOCK: ");

  p = par_get(PARAM_GSMLOCK);
  if (*p == 0)
    {
    net_puts_rom("(none)\r\n");
    }
  else
    {
    net_puts_ram(p);
    net_puts_rom("\r\n");
    }

  sprintf(net_scratchpad, (rom far char*)"Current: %s", car_gsmcops);
  net_puts_ram(net_scratchpad);

  return TRUE;
  }

// GSMLOCK <provider>
BOOL net_sms_handle_gsmlock(char *caller, char *command, char *arguments)
  {
  BOOL gsmlockq_result;

  if (arguments == NULL)
    {
    char e[1] = "";
    par_set(PARAM_GSMLOCK, e);
    }
  else
    {
    par_set(PARAM_GSMLOCK, arguments);
    }

  gsmlockq_result = net_sms_handle_gsmlockq(caller, command, arguments);

  if (net_state != NET_STATE_DIAGMODE)
    net_state_enter(NET_STATE_DONETINIT);

  return gsmlockq_result;
  }

BOOL net_sms_handle_serverq(char *caller, char *command, char *arguments)
  {
  char *p;

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

  net_send_sms_start(caller);
  net_puts_rom("Server:");

  p = par_get(PARAM_SERVERIP);
  sprintf(net_scratchpad, (rom far char*)"\r\n IP:%s", p);
  net_puts_ram(net_scratchpad);

  p = par_get(PARAM_SERVERPASS);
  sprintf(net_scratchpad, (rom far char*)"\r\n Password:%s", p);
  net_puts_ram(net_scratchpad);

  p = par_get(PARAM_PARANOID);
  sprintf(net_scratchpad, (rom far char*)"\r\n Paranoid:%s", p);
  net_puts_ram(net_scratchpad);

  return TRUE;
  }

// SERVER <serverip> <serverpassword> <paranoidmode>
BOOL net_sms_handle_server(char *caller, char *command, char *arguments)
  {
  BOOL serverq_result;

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
  serverq_result = net_sms_handle_serverq(caller, command, arguments);

  if (net_state != NET_STATE_DIAGMODE)
    net_state_enter(NET_STATE_DONETINIT);

  return serverq_result;
  }

BOOL net_sms_handle_diag(char *caller, char *command, char *arguments)
  {
  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

  net_send_sms_start(caller);
  net_puts_rom("DIAG:");

  sprintf(net_scratchpad, (rom far char*)"\r\n RED Led:%d", led_code[OVMS_LED_RED]);
  net_puts_ram(net_scratchpad);

  sprintf(net_scratchpad, (rom far char*)"\r\n GRN Led:%d", led_code[OVMS_LED_GRN]);
  net_puts_ram(net_scratchpad);

  sprintf(net_scratchpad, (rom far char*)"\r\n NET State:0x%02x", net_state);
  net_puts_ram(net_scratchpad);

  if (car_12vline>0)
    {
    sprintf(net_scratchpad, (rom far char*)"\r\n 12V Line:%d.%d", car_12vline/10,car_12vline%10);
    net_puts_ram(net_scratchpad);
    }

  return TRUE;
  }

BOOL net_sms_handle_featuresq(char *caller, char *command, char *arguments)
  {
  unsigned char k;

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

  net_send_sms_start(caller);
  net_puts_rom("Features:");
  for (k=0;k<FEATURES_MAX;k++)
    {
    if (sys_features[k] != 0)
      {
      sprintf(net_scratchpad, (rom far char*)"\r\n %u:%d", k,sys_features[k]);
      net_puts_ram(net_scratchpad);
      }
    }
  return TRUE;
  }

BOOL net_sms_handle_feature(char *caller, char *command, char *arguments)
  {
  unsigned char f;

  if (arguments == NULL) return FALSE;

  f = atoi(arguments);
  arguments = net_sms_nextarg(arguments);
  if ((f>=0)&&(f<FEATURES_MAX)&&(arguments != NULL))
    {
    sys_features[f] = atoi(arguments);
    if (f>=FEATURES_MAP_PARAM) // Top N features are persistent
      par_set(PARAM_FEATURE_S+(f-FEATURES_MAP_PARAM), arguments);
    if (f == FEATURE_CANWRITE) vehicle_initialise();
    net_send_sms_start(caller);
    net_puts_rom(NET_MSG_FEATURE);
    return TRUE;
    }
  return FALSE;
  }

BOOL net_sms_handle_homelink(char *caller, char *command, char *arguments)
  {
  if (vehicle_fn_commandhandler != NULL)
    vehicle_fn_commandhandler(FALSE, 24, arguments);
  net_send_sms_start(caller);
  net_puts_rom(NET_MSG_HOMELINK);
  return TRUE;
  }

BOOL net_sms_handle_lock(char *caller, char *command, char *arguments)
  {
  if (vehicle_fn_commandhandler != NULL)
    vehicle_fn_commandhandler(FALSE, 20, arguments);
  net_send_sms_start(caller);
  net_puts_rom(NET_MSG_LOCK);
  return TRUE;
  }

BOOL net_sms_handle_unlock(char *caller, char *command, char *arguments)
  {
  if (vehicle_fn_commandhandler != NULL)
    vehicle_fn_commandhandler(FALSE, 22, arguments);
  net_send_sms_start(caller);
  net_puts_rom(NET_MSG_UNLOCK);
  return TRUE;
  }

BOOL net_sms_handle_valet(char *caller, char *command, char *arguments)
  {
  if (vehicle_fn_commandhandler != NULL)
    vehicle_fn_commandhandler(FALSE, 21, arguments);
  net_send_sms_start(caller);
  net_puts_rom(NET_MSG_VALET);
  return TRUE;
  }

BOOL net_sms_handle_unvalet(char *caller, char *command, char *arguments)
  {
  if (vehicle_fn_commandhandler != NULL)
    vehicle_fn_commandhandler(FALSE, 23, arguments);
  net_send_sms_start(caller);
  net_puts_rom(NET_MSG_UNVALET);
  return TRUE;
  }

// Set charge mode (params: 0=standard, 1=storage,3=range,4=performance) and optional current limit
BOOL net_sms_handle_chargemode(char *caller, char *command, char *arguments)
  {
  if (arguments == NULL) return FALSE;

  strupr(arguments);
  if (vehicle_fn_commandhandler != NULL)
    {
    if (memcmppgm2ram(arguments, (char const rom far*)"STA", 3) == 0)
      vehicle_fn_commandhandler(FALSE, 10, (char*)"0");
    else if (memcmppgm2ram(arguments, (char const rom far*)"STO", 3) == 0)
      vehicle_fn_commandhandler(FALSE, 10, (char*)"1");
    else if (memcmppgm2ram(arguments, (char const rom far*)"RAN", 3) == 0)
      vehicle_fn_commandhandler(FALSE, 10, (char*)"3");
    else if (memcmppgm2ram(arguments, (char const rom far*)"PER", 3) == 0)
      vehicle_fn_commandhandler(FALSE, 10, (char*)"4");
    else
      return FALSE;

    arguments = net_sms_nextarg(arguments);
    if (arguments != NULL)
      vehicle_fn_commandhandler(FALSE, 15, arguments);
    }

  net_send_sms_start(caller);
  net_puts_rom(NET_MSG_CHARGEMODE);
  return TRUE;
  }

BOOL net_sms_handle_chargestart(char *caller, char *command, char *arguments)
  {
  if (vehicle_fn_commandhandler != NULL)
    vehicle_fn_commandhandler(FALSE, 11, NULL);
  net_notify_suppresscount = 0; // Enable notifications
  net_send_sms_start(caller);
  net_puts_rom(NET_MSG_CHARGESTART);
  return TRUE;
  }

BOOL net_sms_handle_chargestop(char *caller, char *command, char *arguments)
  {
  if (vehicle_fn_commandhandler != NULL)
    vehicle_fn_commandhandler(FALSE, 12, NULL);
  net_notify_suppresscount = 30; // Suppress notifications for 30 seconds
  net_send_sms_start(caller);
  net_puts_rom(NET_MSG_CHARGESTOP);
  return TRUE;
  }

BOOL net_sms_handle_version(char *caller, char *command, char *arguments)
  {
  unsigned char hwv = 1;
  char *p;
  #ifdef OVMS_HW_V2
  hwv = 2;
  #endif

  p = par_get(PARAM_VEHICLETYPE);
  sprintf(net_scratchpad, (rom far char*)"OVMS Firmware version: %d.%d.%d/%s/V%d",
          ovms_firmware[0],ovms_firmware[1],ovms_firmware[2],p,hwv);
  net_send_sms_start(caller);
  net_puts_ram(net_scratchpad);
  return TRUE;
  }

BOOL net_sms_handle_reset(char *caller, char *command, char *arguments)
  {
  char *p;

  net_state_enter(NET_STATE_HARDRESET);
  return FALSE;
  }

BOOL net_sms_handle_help(char *caller, char *command, char *arguments);

// This is the SMS command table
//
// We're a bit limited by PIC C syntax (in particular no function pointers
// as members of structures), so keep it simple and use two tables. The first
// is a list of command strings (left match, all upper case). The second are
// the command handler function pointers. The command string table array is
// terminated by an empty "" command.
// The function pointes are BOOL return. A true result requests the framework
// to issue the net_send_sms_finish() to complete a transmitted SMS.
// The command strings are prefixed with a security control flag:
//   space: no security control
//   1:     the first argument must be the module password
//   2:     the caller must be the registered telephone
//   3:     the caller must be the registered telephone, or first argument the module password

rom char sms_cmdtable[][NET_SMS_CMDWIDTH] =
  { "3REGISTER?",
    "1REGISTER",
    "3PASS?",
    "2PASS ",
    "3GPS",
    "3STAT",
    "3PARAMS?",
    "2PARAMS ",
    "1AP ",
    "3MODULE?",
    "2MODULE ",
    "3GPRS?",
    "2GPRS ",
    "3GSMLOCK?",
    "2GSMLOCK",
    "3SERVER?",
    "2SERVER ",
    "3DIAG",
    "3FEATURES?",
    "2FEATURE ",
    "2HOMELINK ",
    "2LOCK ",
    "2UNLOCK ",
    "2VALET ",
    "2UNVALET ",
    "2CHARGEMODE ",
    "2CHARGESTART",
    "2CHARGESTOP",
    "3VERSION",
    "3RESET",
    "3HELP",
    "" };

rom BOOL (*sms_hfntable[])(char *caller, char *command, char *arguments) =
  {
  &net_sms_handle_registerq,
  &net_sms_handle_register,
  &net_sms_handle_passq,
  &net_sms_handle_pass,
  &net_sms_handle_gps,
  &net_sms_handle_stat,
  &net_sms_handle_paramsq,
  &net_sms_handle_params,
  &net_sms_handle_ap,
  &net_sms_handle_moduleq,
  &net_sms_handle_module,
  &net_sms_handle_gprsq,
  &net_sms_handle_gprs,
  &net_sms_handle_gsmlockq,
  &net_sms_handle_gsmlock,
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
  &net_sms_handle_reset,
  &net_sms_handle_help
  };


// net_sms_checkauth: check SMS caller & first argument
//   according to auth mode
BOOL net_sms_checkauth(char authmode, char *caller, char **arguments)
{
  switch (authmode)
    {
    case '1':
      //   1:     the first argument must be the module password
      if (net_sms_checkpassarg(caller, *arguments) == 0)
        return FALSE;
      *arguments = net_sms_nextarg(*arguments); // Skip over the password
      break;
    case '2':
      //   2:     the caller must be the registered telephone
      if (net_sms_checkcaller(caller) == 0)
        return FALSE;
      break;
    case '3':
      //   3:     the caller must be the registered telephone, or first argument the module password
      if (net_sms_checkcaller(caller) == 0)
        {
        if (net_sms_checkpassarg(caller, *arguments)==0)
          return FALSE;
        *arguments = net_sms_nextarg(*arguments);
        }
      break;
    }

  return TRUE;
}


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
    if (memcmppgm2ram(buf, (char const rom far*)sms_cmdtable[k]+1, strlenpgm((char const rom far*)sms_cmdtable[k])-1) == 0)
      {
      BOOL result = FALSE;
      char *arguments = net_sms_initargs(p);

      if (!net_sms_checkauth(sms_cmdtable[k][0], caller, &arguments))
          return;

      if (vehicle_fn_smshandler != NULL)
        {
        if (vehicle_fn_smshandler(TRUE, caller, buf, arguments))
          return;
        }
      result = (*sms_hfntable[k])(caller, buf, arguments);
      if (result)
        {
        if (vehicle_fn_smshandler != NULL)
          vehicle_fn_smshandler(FALSE, caller, buf, arguments);
        net_send_sms_finish();
        }
      return;
      }
    }

  if (vehicle_fn_smsextensions != NULL)
    {
    // Try passing the command to the vehicle module for handling...
    if (vehicle_fn_smsextensions(caller, buf, p))
      return;
    }

  // SMS didn't match any command pattern, forward to user via net msg
  net_msg_forward_sms(caller, buf);
  }

BOOL net_sms_handle_help(char *caller, char *command, char *arguments)
  {
  int k;

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

  net_send_sms_start(caller);
  net_puts_rom("Commands:");
  for (k=0; sms_cmdtable[k][0] != 0; k++)
    {
    net_puts_rom(" ");
    net_puts_rom(sms_cmdtable[k]+1);
    }
  return TRUE;
  }