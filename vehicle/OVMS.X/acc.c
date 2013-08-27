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

#include <stdlib.h>
#include <string.h>
#include "ovms.h"
#include "acc.h"
#include "net_sms.h"
#include "utils.h"
#include "vehicle.h"

// ACC data
#pragma udata ACC
unsigned char acc_state = 0;                // The current state
unsigned char acc_state_vchar = 0;          //   A per-state CHAR variable
unsigned int  acc_state_vint = 0;           //   A per-state INT variable
unsigned char acc_current_loc = 0;          // Current ACC location
struct acc_record acc_current_rec;          // Current ACC record
unsigned char acc_timeout_goto = 0;         // State to auto-transition to, after timeout
unsigned int  acc_timeout_ticks = 0;        // Number of seconds before timeout auto-transition
unsigned int  acc_granular_tick = 0;        // An internal ticker used to generate 1min, 5min, etc, calls

rom char ACC_NOTHERE[] = "ACC not at this location";

signed char acc_find(struct acc_record* ar, int range)
  {
  int k;

  for (k=0;k<PARAM_ACC_COUNT;k++)
    {
    par_getbase64(k+PARAM_ACC_S, ar, sizeof(ar));
    if ((ar->acc_latitude != 0)||(ar->acc_longitude != 0))
      {
      // We have a used location
      if (FIsLatLongClose(ar->acc_latitude, ar->acc_longitude, car_latitude, car_longitude, range)>0)
        {
        // This location matches...
        return k+1;
        }
      }
    }

  return 0;
  }

void acc_state_enter(unsigned char newstate)
  {
  char *p;
  char m[2];

  acc_state = newstate;

  switch (acc_state)
    {
    case ACC_STATE_FIRSTRUN:
      // First time run
      break;
    case ACC_STATE_FREE:
      // Outside a charge store area
      break;
    case ACC_STATE_DRIVINGIN:
      // Driving in a charge store area
      if (acc_current_rec.acc_flags.Homelink)
        {
        // We must activate the requested homelink
        m[0] = acc_current_rec.acc_homelink;
        m[1] = 0;
        vehicle_fn_commandhandler(FALSE, 24, m);
        }
      break;
    case ACC_STATE_PARKEDIN:
      // Parked in a charge store area
      break;
    case ACC_STATE_CPWAIT:
      // Charge port has been opened, wait 3 seconds
      // Logic is to wait 3 seconds, then see what we have to do
      acc_timeout_ticks = 3;
      acc_timeout_goto = ACC_STATE_CPDECIDE;
      break;
    case ACC_STATE_CPDECIDE:
      // Charge port has been opened, so decide what to do
      break;
    case ACC_STATE_COOLDOWN:
      // Cooldown in a charge store area
      if (vehicle_fn_commandhandler != NULL)
        {
        vehicle_fn_commandhandler(FALSE, 25, NULL); // Cooldown
        }
      break;
    case ACC_STATE_WAITCHARGE:
      // Waiting for charge time in a charge store area
      break;
    case ACC_STATE_CHARGINGIN:
      // Charging in a charge store area
      car_chargelimit_rangelimit = acc_current_rec.acc_stoprange;
      car_chargelimit_soclimit = acc_current_rec.acc_stopsoc;
      if (vehicle_fn_commandhandler != NULL)
        {
        stp_i(net_scratchpad, "", acc_current_rec.acc_chargemode);
        vehicle_fn_commandhandler(FALSE, 10, net_scratchpad);
        stp_i(net_scratchpad, "", acc_current_rec.acc_chargelimit);
        vehicle_fn_commandhandler(FALSE, 15, net_scratchpad);
        vehicle_fn_commandhandler(FALSE, 11, NULL); // Start charge
        }
      break;
    case ACC_STATE_CHARGEDONE:
      // Completed charging in a charge store area
      car_chargelimit_rangelimit = 0;
      car_chargelimit_soclimit = 0;
      break;
    }
  }

void acc_state_ticker1(void)
  {
  char *s;

  switch (acc_state)
    {
    case ACC_STATE_FIRSTRUN:
      // First time run
      acc_current_loc = acc_find(&acc_current_rec,ACC_RANGE2);
      if (acc_current_loc == 0)
        { acc_state_enter(ACC_STATE_FREE); }
      else if (CAR_IS_ON)
        { acc_state_enter(ACC_STATE_DRIVINGIN); }
      else if (CAR_IS_CHARGING)
        { acc_state_enter(ACC_STATE_CHARGINGIN); }
      else
        { acc_state_enter(ACC_STATE_PARKEDIN); }
      break;
    case ACC_STATE_DRIVINGIN:
      // Driving in a charge store area
      if (! CAR_IS_ON)
        {
        acc_state_enter(ACC_STATE_PARKEDIN);
        }
      break;
    case ACC_STATE_PARKEDIN:
      // Parked in a charge store area
      if (car_doors1bits.ChargePort)
        {
        // The charge port has been opened.
        acc_state_enter(ACC_STATE_CPWAIT);
        }
      else if (CAR_IS_ON)
        {
        // We've started to drive
        // Kind of kludgy, but set state directly without 'entering' the state
        // so without activating homelink
        acc_state = ACC_STATE_DRIVINGIN;
        }
      break;
    case ACC_STATE_CPWAIT:
      // Charge port has been opened, wait 4 seconds
      if (! car_doors1bits.ChargePort)
        {
        // The charge port has been closed.
        acc_timeout_ticks = 0;
        acc_state_enter(ACC_STATE_PARKEDIN);
        }
      break;
    case ACC_STATE_CPDECIDE:
      // Charge port has been opened, so decide what to do
      if (acc_current_rec.acc_flags.Cooldown)
        {
        acc_state_enter(ACC_STATE_COOLDOWN);
        }
      else if (acc_current_rec.acc_flags.ChargeAtPlugin)
        {
        acc_state_enter(ACC_STATE_CHARGINGIN);
        }
      break;
    case ACC_STATE_COOLDOWN:
      // Cooldown in a charge store area
      if (car_coolingdown < 0)
        {
        // Cooldown has completed
        if ((car_doors1bits.ChargePort)&&
            (acc_current_rec.acc_flags.ChargeAtPlugin))
          {
          // Let's now do a normal charge...
          acc_state_enter(ACC_STATE_CHARGINGIN);
          }
        }
      else if (!CAR_IS_CHARGING)
        {
        net_state_enter(ACC_STATE_CHARGEDONE);
        }
      break;
    case ACC_STATE_WAITCHARGE:
      // Waiting for charge time in a charge store area
      break;
    case ACC_STATE_CHARGINGIN:
      // Charging in a charge store area
      if (CAR_IS_CHARGING)
        {
        if (((car_chargelimit_rangelimit>0)&&(car_idealrange>car_chargelimit_rangelimit))||
            ((car_chargelimit_soclimit>0)&&(car_SOC>car_chargelimit_soclimit)))
          {
          // Charge has hit the limit, so can be stopped
          vehicle_fn_commandhandler(FALSE, 12, NULL); // Stop charge
          }
        }
      else
        {
        net_state_enter(ACC_STATE_CHARGEDONE);
        }
      break;
    case ACC_STATE_CHARGEDONE:
      // Completed charging in a charge store area
      if (CAR_IS_CHARGING)
        {
        net_state_enter(ACC_STATE_CHARGINGIN);
        }
      else if (CAR_IS_ON)
        {
        net_state_enter(ACC_STATE_DRIVINGIN);
        }
      break;
    }
  }

void acc_state_ticker10(void)
  {
  switch (acc_state)
    {
    case ACC_STATE_FIRSTRUN:
      // First time run
      break;
    case ACC_STATE_FREE:
      // Outside a charge store area
      if (CAR_IS_ON)
        {
        // Poll to see if we're entering an ACC location
        acc_current_loc = acc_find(&acc_current_rec,ACC_RANGE2);
        if (acc_current_loc > 0)
          {
          acc_state_enter(ACC_STATE_DRIVINGIN);
          }
        }
      break;
    case ACC_STATE_DRIVINGIN:
      // Driving in a charge store area
      // Poll to see if we're leaving an ACC location
      acc_current_loc = acc_find(&acc_current_rec,ACC_RANGE2);
      if (acc_current_loc == 0)
        {
        acc_state_enter(ACC_STATE_FREE);
        }
      break;
    case ACC_STATE_PARKEDIN:
      // Parked in a charge store area
      break;
    case ACC_STATE_COOLDOWN:
      // Cooldown in a charge store area
      break;
    case ACC_STATE_WAITCHARGE:
      // Waiting for charge time in a charge store area
      break;
    case ACC_STATE_CHARGINGIN:
      // Charging in a charge store area
      break;
    case ACC_STATE_CHARGEDONE:
      // Completed charging in a charge store area
      break;
    }
  }

void acc_state_ticker60(void)
  {
  switch (acc_state)
    {
    case ACC_STATE_FIRSTRUN:
      // First time run
      break;
    case ACC_STATE_FREE:
      // Outside a charge store area
      break;
    case ACC_STATE_DRIVINGIN:
      // Driving in a charge store area
      break;
    case ACC_STATE_PARKEDIN:
      // Parked in a charge store area
      break;
    case ACC_STATE_COOLDOWN:
      // Cooldown in a charge store area
      break;
    case ACC_STATE_WAITCHARGE:
      // Waiting for charge time in a charge store area
      break;
    case ACC_STATE_CHARGINGIN:
      // Charging in a charge store area
      break;
    case ACC_STATE_CHARGEDONE:
      // Completed charging in a charge store area
      break;
    }
  }

void acc_initialise(void)        // ACC Initialisation
  {
  net_state_enter(ACC_STATE_FIRSTRUN);
  }

BOOL acc_cmd_here(BOOL sms, char* caller)
  {
  // Turn on ACC here
  unsigned char k;
  char *s;
  struct acc_record ar;

  net_send_sms_start(caller);

  k = acc_find(&ar,ACC_RANGE1);
  if (k > 0)
    {
    // This location matches...
    s = stp_i(net_scratchpad, "ACC #", k);
    s = stp_rom(s," already here");
    net_puts_ram(net_scratchpad);
    return TRUE;
    }

  for (k=0;k<PARAM_ACC_COUNT;k++)
    {
    par_getbase64(k+PARAM_ACC_S, &ar, sizeof(ar));
    if ((ar.acc_latitude == 0)&&(ar.acc_longitude == 0))
      {
      // We have a free location
      ar.acc_latitude = car_latitude;
      ar.acc_longitude = car_longitude;
      ar.acc_recversion = ACC_RECVERSION;
      par_setbase64(k+PARAM_ACC_S,&ar,sizeof(ar));
      s = stp_i(net_scratchpad, "ACC #", k+1);
      s = stp_rom(s," set");
      net_puts_ram(net_scratchpad);
      return TRUE;
      }
    }

  net_puts_rom("ACC could not be set - no free space");
  return TRUE;
  }

BOOL acc_cmd_nothere(BOOL sms, char* caller)
  {
  // Turn off ACC here
  unsigned char k;
  char *s;
  unsigned char found = 0;
  struct acc_record ar;

  s = stp_rom(net_scratchpad, "ACC cleared");

  while ((k=acc_find(&ar,ACC_RANGE1))>0)
    {
    // Existing location matches...
    par_set((k+PARAM_ACC_S)-1,NULL);
    s = stp_i(s," #",k);
    found++;
    }

  net_send_sms_start(caller);
  if (found>0)
    net_puts_ram(net_scratchpad);
  else
    net_puts_rom(ACC_NOTHERE);
  return TRUE;
  }

BOOL acc_cmd_clear(BOOL sms, char* caller)
  {
  // Clear all ACC data
  unsigned char k;
  struct acc_record ar;
  memset(&ar,0,sizeof(ar));

  for (k=0;k<PARAM_ACC_COUNT;k++)
    {
    par_set(k+PARAM_ACC_S,NULL);
    }

  net_send_sms_start(caller);
  net_puts_rom("ACC configuration cleared");
  return TRUE;
  }

BOOL acc_cmd_stat(BOOL sms, char* caller)
  {
  // Return ACC status
  struct acc_record ar;
  int k;
  char *s,*p;
  unsigned long r;

  k = acc_find(&ar,ACC_RANGE1);
  net_send_sms_start(caller);
  s = stp_i(net_scratchpad,"ACC Status #",k);
  if (k>0)
    {
    s = stp_latlon(s, "\r\n  ", ar.acc_latitude);
    s = stp_latlon(s, ", ", ar.acc_longitude);
    s = stp_rom(s, (ar.acc_flags.AccEnabled)?"\r\n  Enabled":"\r\n  Disabled");
    if (ar.acc_flags.Cooldown)
      s = stp_rom(s, "\r\n  Cooldown");
    if (ar.acc_flags.Homelink)
      s = stp_i(s, "\r\h  Homelink #",ar.acc_homelink);
    if (ar.acc_flags.ChargeAtPlugin)
      s = stp_rom(s, "\r\n Charge at plugin");
    if (ar.acc_flags.ChargeAtTime)
      s = stp_time(s, "\r\n Charge at time ",(unsigned long)ar.acc_chargetime * 60);
    if (ar.acc_flags.ChargeByTime)
      s = stp_time(s, "\r\n Charge by time ",(unsigned long)ar.acc_chargetime * 60);
    s = stp_mode(s, "\r\n  Mode: ",ar.acc_chargemode);
    s = stp_i(s, " (",ar.acc_chargelimit);
    s = stp_rom(s, "A)");
    if (ar.acc_stopsoc>0)
      {
      s = stp_i(s, "\r\n Stop at SOC ",ar.acc_stopsoc);
      s = stp_rom(s, "%");
      }
    if (ar.acc_stoprange>0)
      {
      r = (unsigned long)ar.acc_stoprange;
      if (can_mileskm=='M')
        {
        s = stp_l(s, "\r\n Stop at range ",r);
        s = stp_rom(s, "miles");
        }
      else
        {
        s = stp_l(s, "\r\n Stop at range ",KmFromMi(r));
        s = stp_rom(s, "km");
        }
      }
    }
  else
    {
    s = stp_rom(s," - No ACC defined here");
    }

  net_puts_ram(net_scratchpad);
  return TRUE;
  }

BOOL acc_cmd_enable(BOOL sms, char* caller, unsigned char enabled)
  {
  // Enable/Disable ACC
  struct acc_record ar;
  int k;
  char *s;

  k = acc_find(&ar,ACC_RANGE1);
  net_send_sms_start(caller);
  if (k<0)
    {
    s = stp_rom(net_scratchpad,ACC_NOTHERE);
    }
  else
    {
    ar.acc_flags.AccEnabled = enabled;
    par_setbase64(k+PARAM_ACC_S,&ar,sizeof(ar));
    s = stp_rom(net_scratchpad,(enabled)?"ACC enabled":"ACC disabled");
    }

  net_puts_ram(net_scratchpad);
  return TRUE;
  }

BOOL acc_cmd_params(BOOL sms, char* caller, char *arguments)
  {
  // Set ACC params
  struct acc_record ar;
  int k;
  char p;
  char *s;

  k = acc_find(&ar,ACC_RANGE1);
  net_send_sms_start(caller);
  if (k<0)
    {
    s = stp_rom(net_scratchpad,ACC_NOTHERE);
    }
  else
    {
    while ((arguments != NULL)&&(arguments = net_sms_nextarg(arguments)))
      {
      strupr(arguments);
      if (strcmppgm2ram(arguments,"COOLDOWN")==0)
        { ar.acc_flags.Cooldown = 1; }
      else if (strcmppgm2ram(arguments,"NOCOOLDOWN")==0)
        { ar.acc_flags.Cooldown = 0; }
      else if (strcmppgm2ram(arguments,"HOMELINK")==0)
        {
        ar.acc_flags.Homelink = 1;
        arguments = net_sms_nextarg(arguments);
        if (arguments != NULL)
          { ar.acc_homelink = atoi(arguments); }
        }
      else if (strcmppgm2ram(arguments,"NOHOMELINK")==0)
        { ar.acc_flags.Homelink = 0; }
      else if (strcmppgm2ram(arguments,"CHARGEPLUGIN")==0)
        {
        ar.acc_flags.ChargeAtPlugin = 1;
        ar.acc_flags.ChargeAtTime = 0;
        ar.acc_flags.ChargeByTime = 0;
        }
      else if (strcmppgm2ram(arguments,"CHARGEAT")==0)
        {
        ar.acc_flags.ChargeAtPlugin = 0;
        ar.acc_flags.ChargeAtTime = 1;
        ar.acc_flags.ChargeByTime = 0;
        arguments = net_sms_nextarg(arguments);
        if (arguments != NULL)
          { ar.acc_chargetime = atoi(arguments); }
        }
      else if (strcmppgm2ram(arguments,"CHARGEBY")==0)
        {
        ar.acc_flags.ChargeAtPlugin = 0;
        ar.acc_flags.ChargeAtTime = 0;
        ar.acc_flags.ChargeByTime = 1;
        arguments = net_sms_nextarg(arguments);
        if (arguments != NULL)
          { ar.acc_chargetime = atoi(arguments); }
        }
      else if (strcmppgm2ram(arguments,"NOCHARGE")==0)
        {
        ar.acc_flags.ChargeAtPlugin = 0;
        ar.acc_flags.ChargeAtTime = 0;
        ar.acc_flags.ChargeByTime = 0;
        }
      else if (strcmppgm2ram(arguments,"LIMIT")==0)
        {
        arguments = net_sms_nextarg(arguments);
        if (arguments != NULL)
          { ar.acc_chargelimit = atoi(arguments); }
        }
      else if (strcmppgm2ram(arguments,"MODE")==0)
        {
        arguments = net_sms_nextarg(arguments);
        if (arguments != NULL)
          { ar.acc_chargemode = string_to_mode(arguments); }
        }
      else if (strcmppgm2ram(arguments,"STOPRANGE")==0)
        {
        arguments = net_sms_nextarg(arguments);
        if (arguments != NULL)
          { ar.acc_stoprange = (can_mileskm=='M')?atoi(arguments):MiFromKm(atoi(arguments)); }
        }
      else if (strcmppgm2ram(arguments,"STOPSOC")==0)
        {
        arguments = net_sms_nextarg(arguments);
        if (arguments != NULL)
          { ar.acc_stopsoc = atoi(arguments); }
        }
      }
    }

  net_puts_ram(net_scratchpad);
  return TRUE;
  }

BOOL acc_cmd(char *caller, char *command, char *arguments, BOOL sms)
  {
  strupr(arguments); // Convert command to upper case
  
  if (strcmppgm2ram(arguments,"HERE")==0)
    {
    return acc_cmd_here(sms, caller);
    }
  else if (strcmppgm2ram(arguments,"NOTHERE")==0)
    {
    return acc_cmd_nothere(sms, caller);
    }
  else if (strcmppgm2ram(arguments,"CLEAR")==0)
    {
    return acc_cmd_clear(sms, caller);
    }
  else if (strcmppgm2ram(arguments,"STAT")==0)
    {
    return acc_cmd_stat(sms, caller);
    }
  else if (strcmppgm2ram(arguments,"ENABLE")==0)
    {
    return acc_cmd_enable(sms, caller, 1);
    }
  else if (strcmppgm2ram(arguments,"DISABLE")==0)
    {
    return acc_cmd_enable(sms, caller, 0);
    }
  else if (strcmppgm2ram(arguments,"PARAMS")==0)
    {
    return acc_cmd_params(sms, caller, arguments);
    }
  else
    {
    net_send_sms_start(caller);
    net_puts_rom("ACC command not recognised");
    return TRUE;
    }
  }

BOOL acc_handle_sms(char *caller, char *command, char *arguments)
  {
  return acc_cmd(caller,command,arguments,TRUE);
  }

void acc_ticker(void)            // ACC Ticker
  {
  // This ticker is called once every second
  acc_granular_tick++;
  if ((acc_timeout_goto > 0)&&(acc_timeout_ticks-- == 0))
    {
    acc_state_enter(acc_timeout_goto);
    }
  else
    {
    acc_state_ticker1();
    }
  if ((acc_granular_tick % 10)==0) acc_state_ticker10();
  if ((acc_granular_tick % 60)==0)
    {
    acc_state_ticker60();
    acc_granular_tick -= 60;
    }
  }
