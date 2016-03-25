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
#include "inputs.h"
#include "net.h"
#include "net_sms.h"
#include "net_msg.h"
#ifdef OVMS_ACCMODULE
#include "acc.h"
#endif

#pragma udata
char *net_sms_argend;
char *net_msg_bufpos; // buffer write position for net_put*

rom char NET_MSG_DENIED[] = "Permission denied";
rom char NET_MSG_INVALID[] = "Invalid command";
rom char NET_MSG_REGISTERED[] = "Your phone has been registered as the owner.";
rom char NET_MSG_PASSWORD[] = "Module password has been changed.";
rom char NET_MSG_PARAMS[] = "Parameters have been set.";
rom char NET_MSG_FEATURE[] = "Feature has been set.";
rom char NET_MSG_HOMELINK[] = "Homelink activated";
rom char NET_MSG_LOCK[] = "Vehicle lock requested";
rom char NET_MSG_UNLOCK[] = "Vehicle unlock requested";
rom char NET_MSG_VALET[] = "Valet mode requested";
rom char NET_MSG_UNVALET[] = "Valet mode cancel requested";

#ifndef OVMS_NO_CHARGECONTROL
rom char NET_MSG_CHARGEMODE[] = "Charge mode change requested";
rom char NET_MSG_CHARGESTART[] = "Charge start requested";
rom char NET_MSG_CHARGESTOP[] = "Charge stop requested";
rom char NET_MSG_COOLDOWN[] = "Cooldown requested";
#endif //OVMS_NO_CHARGECONTROL

#ifndef OVMS_NO_VEHICLE_ALERTS
rom char NET_MSG_ALARM[] = "Vehicle alarm is sounding!";
rom char NET_MSG_VALETTRUNK[] = "Trunk has been opened (valet mode).";
#endif //OVMS_NO_VEHICLE_ALERTS

//rom char NET_MSG_GOOGLEMAPS[] = "Car location:\r\nhttp://maps.google.com/maps/api/staticmap?zoom=15&size=500x640&scale=2&sensor=false&markers=icon:http://goo.gl/pBcX7%7C";
rom char NET_MSG_GOOGLEMAPS[] = "Car location:\r\nhttps://maps.google.com/maps?q=";



void net_send_sms_start(char* number)
  {
  if (net_msg_bufpos)
    {
    // NET SMS wrapper mode: nothing to do here
    // net_put* will write to net_msg_bufpos
    }
#ifdef OVMS_DIAGMODULE
  else if (net_state == NET_STATE_DIAGMODE)
    {
    // DIAG mode: screen output
    net_puts_rom("# ");
    }
#endif // OVMS_DIAGMODULE
  else
    {
    // MODEM mode:
    net_puts_rom("AT+CMGS=\"");
    net_puts_ram(number);
    net_puts_rom("\"\r\n");
    delay100(2);
    }

  // ATT: the following code tries to prepend the current time to ALL
  //    outbound SMS. It relies on a) all SMS leaving enough space
  //    to add "HH:MM:SS\r " = 10 chars and b) ALL SMS senders to
  //    call net_send_sms_start() BEFORE preparing the message in
  //    net_scratchpad -- otherwise the prepd message is lost.
#ifndef OVMS_NO_SMSTIME
  if ((car_time > 315360000)&&
      ((sys_features[FEATURE_CARBITS]&FEATURE_CB_SSMSTIME)==0))
    {
    // Car time is valid, and sms time is not disabled
    char *p = par_get(PARAM_TIMEZONE);
    char *s = stp_time(net_scratchpad, NULL, car_time + timestring_to_mins(p)*60L);
    s = stp_rom(s, "\r ");
    net_puts_ram(net_scratchpad);
    }
#endif //OVMS_NO_SMSTIME
  
  }

void net_send_sms_finish(void)
  {
  if (net_msg_bufpos)
    {
    // NET SMS wrapper mode: nothing to do here
    // net_msg_cmd_exec() will terminate the string
    }
#ifdef OVMS_DIAGMODULE
  else if (net_state == NET_STATE_DIAGMODE)
    {
    // DIAG mode:
    net_puts_rom("\r\n");
    }
#endif // OVMS_DIAGMODULE
  else
    {
    // MODEM mode:
    net_puts_rom("\x1a");
    }
  }

void net_send_sms_rom(char* number, const rom char* message)
  {
  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return;

  net_send_sms_start(number);
  net_puts_rom(message);
  net_send_sms_finish();
  }

#if 0 // unused code
void net_send_sms_ram(char* number, const char* message)
  {
  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return;

  net_send_sms_start(number);
  net_puts_ram(message);
  net_send_sms_finish();
  }
#endif

BOOL net_sms_stat(char* number)
  {
  char *p;

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

  delay100(2);
  net_send_sms_start(number);
  
  net_prep_stat(net_scratchpad);
  cr2lf(net_scratchpad);
  net_puts_ram(net_scratchpad);

  return TRUE;
  }

#ifndef OVMS_NO_CTP
BOOL net_sms_ctp(char* number, char *arguments)
  {
  char *p;

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

  delay100(2);
  net_send_sms_start(number);
  
  net_prep_ctp(net_scratchpad, arguments);
  cr2lf(net_scratchpad);
  net_puts_ram(net_scratchpad);

  return TRUE;
  }
#endif //OVMS_NO_CTP

#ifndef OVMS_NO_VEHICLE_ALERTS
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
#endif //OVMS_NO_VEHICLE_ALERTS

void net_sms_socalert(char* number)
  {
  char *s;

  delay100(10);
  net_send_sms_start(number);

  s = stp_i(net_scratchpad, "ALERT!!! CRITICAL SOC LEVEL APPROACHED (", car_SOC); // 95%
  s = stp_rom(s, "% SOC)");
  net_puts_ram(net_scratchpad);

  net_send_sms_finish();
  delay100(5);
  }

void net_sms_12v_alert(char* number)
  {
  char *s;

  delay100(10);
  net_send_sms_start(number);

  if (can_minSOCnotified & CAN_MINSOC_ALERT_12V)
    s = stp_l2f(net_scratchpad, "MP-0 PAALERT!!! 12V BATTERY CRITICAL (", car_12vline, 1);
  else
    s = stp_l2f(net_scratchpad, "MP-0 PA12V BATTERY OK (", car_12vline, 1);
  s = stp_l2f(s, "V, ref=", car_12vline_ref, 1);
  s = stp_rom(s, "V)");
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
  if (*p && strncmp(p,caller,strlen(p)) == 0)
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

  if ((arguments != NULL)&&(*p && strncmp(p,arguments,strlen(p))==0))
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

  if (arguments == NULL) return NULL;

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

  if (lastarg == NULL) return NULL;

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
  stp_s(net_scratchpad, "Registered number: ", p);
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
  stp_s(net_scratchpad, "Module password: ", p);
  net_puts_ram(net_scratchpad);
  return TRUE;
  }

BOOL net_sms_handle_pass(char *caller, char *command, char *arguments)
  {
  // Password may not be empty:
  if (*arguments == 0)
  {
    net_send_sms_start(caller);
    net_puts_rom(NET_MSG_INVALID);
    return TRUE;
  }
  
  par_set(PARAM_MODULEPASS, arguments);

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

  net_send_sms_start(caller);
  net_puts_rom(NET_MSG_PASSWORD);
  return TRUE;
  }

BOOL net_sms_handle_gps(char *caller, char *command, char *arguments)
  {
  char *s;

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

  delay100(2);
  net_send_sms_start(caller);
  
  s = stp_latlon(net_scratchpad, NET_MSG_GOOGLEMAPS, car_latitude);
  s = stp_latlon(s, ",", car_longitude);

  net_puts_ram(net_scratchpad);

  return TRUE;
  }

BOOL net_sms_handle_stat(char *caller, char *command, char *arguments)
  {
  return net_sms_stat(caller);
  }

BOOL net_sms_handle_paramsq(char *caller, char *command, char *arguments)
  {
  unsigned char k, splen, msglen;
  char *p, *s;

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

  net_send_sms_start(caller);
  net_puts_rom("Params:");
  msglen=7;
  for (k=0;k<PARAM_MAX;k++)
    {
    p = par_get(k);
    if (*p != 0)
      {
      s = stp_i(net_scratchpad, "\n", k);
      s = stp_s(s, ":", p);
      splen = s - net_scratchpad;
      if((msglen+splen) > 160)
        {
          // SMS becomes too long, finish & start next:
          net_send_sms_finish();
          delay100(20);
          net_send_sms_start(caller);
          net_puts_rom("Params:");
          msglen=7+splen;
        }
      net_puts_ram(net_scratchpad);
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

  vehicle_initialise();

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
  char *s;

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

  net_send_sms_start(caller);

  s = stp_rom(net_scratchpad, "Module:");
  s = stp_s(s, "\r\n VehicleID:", par_get(PARAM_VEHICLEID));
  s = stp_s(s, "\r\n Units:", par_get(PARAM_MILESKM));
  s = stp_s(s, "\r\n Notifications:", par_get(PARAM_NOTIFIES));
  s = stp_s(s, "\r\n VehicleType:", par_get(PARAM_VEHICLETYPE));

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
    {
    net_state_vint = NET_GPRS_RETRIES;
    net_state_enter(NET_STATE_NETINITP);
    }

  return moduleq_result;
  }

BOOL net_sms_handle_vehicleq(char *caller, char *command, char *arguments)
  {
  char *s;

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

  net_send_sms_start(caller);

  s = stp_rom(net_scratchpad, "Vehicle:");
  s = stp_s(s, "\r\n VehicleType: ", par_get(PARAM_VEHICLETYPE));

  net_puts_ram(net_scratchpad);

  return TRUE;
  }

// VEHICLE <vehicletype>
BOOL net_sms_handle_vehicle(char *caller, char *command, char *arguments)
  {
  BOOL vehicleq_result;

  if (arguments[0]=='-') arguments[0]=0;
  par_set(PARAM_VEHICLETYPE, arguments);

  vehicleq_result = net_sms_handle_vehicleq(caller, command, arguments);
  vehicle_initialise();

  if (net_state != NET_STATE_DIAGMODE)
    {
    net_state_vint = NET_GPRS_RETRIES;
    net_state_enter(NET_STATE_NETINITP);
    }

  return vehicleq_result;
  }

BOOL net_sms_handle_gprsq(char *caller, char *command, char *arguments)
  {
  char *s;

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

  net_send_sms_start(caller);

  s = stp_rom(net_scratchpad, "GPRS:");
  s = stp_s(s, "\r\n APN:", par_get(PARAM_GPRSAPN));
  s = stp_s(s, "\r\n User:", par_get(PARAM_GPRSUSER));
  s = stp_s(s, "\r\n Password:", par_get(PARAM_GPRSPASS));
  s = stp_s(s, "\r\n DNS:", par_get(PARAM_GPRSDNS));
  s = stp_s(s, "\r\n GSM:", car_gsmcops);

  if (!inputs_gsmgprs())
    s = stp_rom(s, "\r\n GPRS: DISABLED");
  else if (net_msg_serverok)
    s = stp_rom(s, "\r\n GPRS: OK\r\n Server: Connected OK");
  else if (net_state == NET_STATE_READY)
    s = stp_rom(s, "\r\n GSM: OK\r\n Server: Not connected");
  else
    {
    s = stp_x(s, "\r\n GSM/GPRS: Not connected (0x", net_state);
    s = stp_rom(s, ")");
    }

  net_puts_ram(net_scratchpad);

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
    arguments = net_sms_nextarg(arguments);
    if (arguments != NULL)
      {
      if ((arguments[0]=='-')&&(arguments[1]==0))
        par_set(PARAM_GPRSDNS, arguments+1);
      else
        par_set(PARAM_GPRSDNS, arguments);
      }
    }
  gprsq_result = net_sms_handle_gprsq(caller, command, arguments);

  if (net_state != NET_STATE_DIAGMODE)
    {
    net_state_vint = NET_GPRS_RETRIES;
    net_state_enter(NET_STATE_NETINITP);
    }

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

  stp_s(net_scratchpad, "Current: ", car_gsmcops);
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
    {
    net_state_vint = NET_GPRS_RETRIES;
    net_state_enter(NET_STATE_NETINITP);
    }

  return gsmlockq_result;
  }

BOOL net_sms_handle_serverq(char *caller, char *command, char *arguments)
  {
  char *s;

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

  net_send_sms_start(caller);

  s = stp_rom(net_scratchpad, "Server:");
  s = stp_s(s, "\r\n IP:", par_get(PARAM_SERVERIP));
  s = stp_s(s, "\r\n Password:", par_get(PARAM_SERVERPASS));
  s = stp_s(s, "\r\n Paranoid:", par_get(PARAM_PARANOID));

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
    {
    net_state_vint = NET_GPRS_RETRIES;
    net_state_enter(NET_STATE_NETINITP);
    }

  return serverq_result;
}

BOOL net_sms_handle_diag(char *caller, char *command, char *arguments)
{
  char *s;

  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS) return FALSE;

  net_send_sms_start(caller);

  s = stp_rom(net_scratchpad, "DIAG:");
  s = stp_i(s, "\n RED Led:", led_code[OVMS_LED_RED]);
  s = stp_i(s, "\n GRN Led:", led_code[OVMS_LED_GRN]);
  s = stp_x(s, "\n NET State:0x", net_state);

  if (car_12vline > 0)
  {
    s = stp_l2f(s, "\n 12V Line:", car_12vline, 1);
    s = stp_l2f(s, " ref=", car_12vline_ref, 1);
  }

#ifndef OVMS_NO_CRASHDEBUG
  /* DEBUG / QA stats: output crash counter and decode last reason:
   */
  s = stp_i(s, "\n Crashes:", debug_crashcnt);
  if (debug_crashreason)
  {
    s = stp_rom(s, "\n ..last:");
    if (debug_crashreason & 0x01)
      s = stp_rom(s, " BOR"); // Brown Out Reset
    if (debug_crashreason & 0x02)
      s = stp_rom(s, " POR"); // Power On Reset
    if (debug_crashreason & 0x04)
      s = stp_rom(s, " PD"); // Power-Down Detection
    if (debug_crashreason & 0x08)
      s = stp_rom(s, " TO"); // Watchdog Timeout
    if (debug_crashreason & 0x10)
      s = stp_rom(s, " RI"); // Reset Instruction
    if (debug_crashreason & 0x20)
      s = stp_rom(s, " STKFUL"); // Stack overflow
    if (debug_crashreason & 0x40)
      s = stp_rom(s, " STKUNF"); // Stack underflow
    s = stp_i(s, " - ", debug_checkpoint);
  }
#endif // OVMS_NO_CRASHDEBUG

  net_puts_ram(net_scratchpad);

  return TRUE;
}

BOOL net_sms_handle_featuresq(char *caller, char *command, char *arguments)
  {
  unsigned char k;
  char *s;

  if (sys_features[FEATURE_CARBITS]&FEATURE_CB_SOUT_SMS) return FALSE;

  net_send_sms_start(caller);
  net_puts_rom("Features:");
  for (k=0;k<FEATURES_MAX;k++)
    {
    if (sys_features[k] != 0)
      {
      s = stp_i(net_scratchpad, "\r\n ", k);
      s = stp_i(s, ":", sys_features[k]);
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

#ifndef OVMS_NO_CHARGECONTROL

// Set charge mode (params: 0=standard, 1=storage,3=range,4=performance) and optional current limit
BOOL net_sms_handle_chargemode(char *caller, char *command, char *arguments)
  {
  char mode[2];
  if (arguments == NULL) return FALSE;

  strupr(arguments);
  if (vehicle_fn_commandhandler != NULL)
    {
    mode[0] = '0' + string_to_mode(arguments);
    mode[1] = 0;
    vehicle_fn_commandhandler(FALSE, 10, mode);

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
#ifdef OVMS_ACCMODULE
  acc_handle_msg(FALSE, 11, NULL);
#endif
  return TRUE;
  }

BOOL net_sms_handle_chargestop(char *caller, char *command, char *arguments)
  {
  if (vehicle_fn_commandhandler != NULL)
    vehicle_fn_commandhandler(FALSE, 12, NULL);
  net_notify_suppresscount = 30; // Suppress notifications for 30 seconds
  net_send_sms_start(caller);
  net_puts_rom(NET_MSG_CHARGESTOP);
#ifdef OVMS_ACCMODULE
  acc_handle_msg(FALSE, 12, NULL);
#endif
  return TRUE;
  }

BOOL net_sms_handle_cooldown(char *caller, char *command, char *arguments)
  {
  if (vehicle_fn_commandhandler != NULL)
    vehicle_fn_commandhandler(FALSE, 25, NULL);
  net_send_sms_start(caller);
  net_puts_rom(NET_MSG_COOLDOWN);
#ifdef OVMS_ACCMODULE
  acc_handle_msg(FALSE, 25, NULL);
#endif
  return TRUE;
  }

#endif // OVMS_NO_CHARGECONTROL


BOOL net_sms_handle_version(char *caller, char *command, char *arguments)
  {
  unsigned char hwv = 1;
  char *s;
  #ifdef OVMS_HW_V2
  hwv = 2;
  #endif

  net_send_sms_start(caller);

  s = stp_i(net_scratchpad, "OVMS Firmware version: ", ovms_firmware[0]);
  s = stp_i(s, ".", ovms_firmware[1]);
  s = stp_i(s, ".", ovms_firmware[2]);
  s = stp_s(s, "/", par_get(PARAM_VEHICLETYPE));
  if (vehicle_version)
    s = stp_rom(s, vehicle_version);
  s = stp_i(s, "/V", hwv);
  s = stp_rs(s, "/", OVMS_BUILDCONFIG);
  net_puts_ram(net_scratchpad);

  return TRUE;
  }

BOOL net_sms_handle_reset(char *caller, char *command, char *arguments)
  {
  char *p;

  net_state_enter(NET_STATE_HARDSTOP);
  return FALSE;
  }

#ifndef OVMS_NO_CTP
BOOL net_sms_handle_ctp(char *caller, char *command, char *arguments)
  {
  return net_sms_ctp(caller, arguments);
  }
#endif //OVMS_NO_CTP

BOOL net_sms_handle_temps(char *caller, char *command, char *arguments)
  {
  char *s;

  s = stp_i(net_scratchpad, "Temperatures:\r\n  Ambient: ", car_ambient_temp);
  s = stp_i(s, "C\r\n  PEM: ", car_tpem);
  s = stp_i(s, "C\r\n  Motor: ", car_tmotor);
  s = stp_i(s, "C\r\n  Battery: ", car_tbattery);
  s = stp_i(s, "C\r\n  Charger: ", car_tcharger);
  s = stp_rom(s, "C");
  if ((car_stale_ambient==0)||(car_stale_temps==0))
    s = stp_rom(s, "\r\n  (stale)");

  net_send_sms_start(caller);
  net_puts_ram(net_scratchpad);
  return TRUE;
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
    "3VEHICLE?",
    "2VEHICLE ",
    "3GPRS?",
    "2GPRS ",
    "3GSMLOCK?",
    "2GSMLOCK",
    "3SERVER?",
    "2SERVER ",
    "3DIAG",
    "3FEATURES?",
    "2FEATURE ",
    "2HOMELINK",
    "2LOCK",
    "2UNLOCK",
    "2VALET",
    "2UNVALET",
#ifndef OVMS_NO_CHARGECONTROL
    "2CHARGEMODE ",
    "2CHARGESTART",
    "2CHARGESTOP",
    "2COOLDOWN",
#endif // OVMS_NO_CHARGECONTROL
    "3VERSION",
    "3RESET",
#ifndef OVMS_NO_CTP
    "3CTP",
#endif //OVMS_NO_CTP
    "3TEMPS",
#ifdef OVMS_ACCMODULE
    "2ACC ",
#endif
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
  &net_sms_handle_vehicleq,
  &net_sms_handle_vehicle,
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
#ifndef OVMS_NO_CHARGECONTROL
  &net_sms_handle_chargemode,
  &net_sms_handle_chargestart,
  &net_sms_handle_chargestop,
  &net_sms_handle_cooldown,
#endif // OVMS_NO_CHARGECONTROL
  &net_sms_handle_version,
  &net_sms_handle_reset,
#ifndef OVMS_NO_CTP
  &net_sms_handle_ctp,
#endif //OVMS_NO_CTP
  &net_sms_handle_temps,
#ifdef OVMS_ACCMODULE
  &acc_handle_sms,
#endif
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

BOOL net_sms_in(char *caller, char *buf)
  {
  // The buf contains an SMS command
  // and caller contains the caller telephone number
  char *p;
  UINT8 k;

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
          return FALSE; // auth error

      if (vehicle_fn_smshandler != NULL)
        {
        if (vehicle_fn_smshandler(TRUE, caller, buf, arguments))
          return TRUE; // handled
        }

      result = (*sms_hfntable[k])(caller, buf, arguments);
      if (result)
        {
        if (vehicle_fn_smshandler != NULL)
          vehicle_fn_smshandler(FALSE, caller, buf, arguments);
        net_send_sms_finish();
        }
      return result;
      }
    }

  if (vehicle_fn_smsextensions != NULL)
    {
    // Try passing the command to the vehicle module for handling...
    if (vehicle_fn_smsextensions(caller, buf, p))
      return TRUE; // handled
    }

  // SMS didn't match any command pattern, forward to user via net msg
  net_msg_forward_sms(caller, buf);
  return FALSE; // unknown command
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
