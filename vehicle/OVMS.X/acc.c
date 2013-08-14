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

#include <string.h>
#include "ovms.h"
#include "acc.h"
#include "net_sms.h"
#include "utils.h"

// ACC data
#pragma udata ACC
unsigned char acc_state = 0;                // The current state
unsigned char acc_state_vchar = 0;          //   A per-state CHAR variable
unsigned int  acc_state_vint = 0;           //   A per-state INT variable
unsigned char acc_timeout_goto = 0;         // State to auto-transition to, after timeout
unsigned int  acc_timeout_ticks = 0;        // Number of seconds before timeout auto-transition
unsigned int  acc_granular_tick = 0;        // An internal ticker used to generate 1min, 5min, etc, calls

void acc_state_enter(unsigned char newstate)
  {
  char *p;

  acc_state = newstate;
  }

void acc_state_ticker1(void)
  {
  }

void acc_state_ticker60(void)
  {
  }

void acc_state_ticker3600(void)
  {
  }

void acc_initialise(void)        // ACC Initialisation
  {
  }

signed char acc_find(struct acc_record* ar)
  {
  int k;

  for (k=0;k<PARAM_ACC_COUNT;k++)
    {
    par_getbase64(k+PARAM_ACC_S, ar, sizeof(ar));
    if ((ar->acc_latitude != 0)||(ar->acc_longitude != 0))
      {
      // We have a used location
      if (FIsLatLongClose(ar->acc_latitude, ar->acc_longitude, car_latitude, car_longitude, ACC_RANGE1)>0)
        {
        // This location matches...
        return k+1;
        }
      }
    }

  return 0;
  }

BOOL acc_cmd_here(BOOL sms, char* caller)
  {
  // Turn on ACC here
  unsigned char k;
  char *s;
  struct acc_record ar;

  net_send_sms_start(caller);

  k = acc_find(&ar);
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

  while ((k=acc_find(&ar))>0)
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
    net_puts_rom("ACC not at this location");
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

  k = acc_find(&ar);
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
      p = par_get(PARAM_MILESKM);
      r = (unsigned long)ar.acc_stoprange;
      if (p[0]=='K')
        {
        s = stp_l(s, "\r\n Stop at range ",KmFromMi(r));
        s = stp_rom(s, "km");
        }
      else
        {
        s = stp_l(s, "\r\n Stop at range ",r);
        s = stp_rom(s, "miles");
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
  if ((acc_granular_tick % 60)==0)    acc_state_ticker60();
  if ((acc_granular_tick % 3600)==0)
    {
    acc_state_ticker3600();
    acc_granular_tick -= 3600;
    }
  }
