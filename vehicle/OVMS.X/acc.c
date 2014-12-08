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
#include "net_msg.h"
#include "net_sms.h"
#include "utils.h"
#include "vehicle.h"

// ACC data
#pragma udata ACC
unsigned char acc_state = 0;                // The current state
unsigned char acc_current_loc = 0;          // Current ACC location
struct acc_record acc_current_rec;          // Current ACC record
unsigned int acc_chargeminute = 0;          // Charge minute to awake and start the charge
unsigned char acc_timeout_goto = 0;         // State to auto-transition to, after timeout
unsigned int  acc_timeout_ticks = 0;        // Number of seconds before timeout auto-transition
unsigned int  acc_granular_tick = 0;        // An internal ticker used to generate 1min, 5min, etc, calls

unsigned int acc_last_chgmod = 0;
int acc_last_wAvail = 0;
int acc_last_ixEnd = 0;
int acc_last_pctEnd = 0;
int acc_last_car_idealrange = 0;
int acc_last_cac = 0;
unsigned char acc_last_loc = 0;
int acc_last_estimate = 0;

#define VOLTS_ACC_ASSUMED 220

rom char ACC_NOTHERE[] = "ACC not at this location";

signed char acc_find(struct acc_record* ar, BOOL enabledonly)
  {
  int k;
  int radius;

  for (k=0;k<PARAM_ACC_COUNT;k++)
    {
    par_getbase64(k+PARAM_ACC_S, ar, sizeof(ar));
    if ((ar->acc_latitude != 0)||(ar->acc_longitude != 0))
      {
      // We have a used location
      if (ar->acc_radius != 0)
        radius = ar->acc_radius;
      else
        radius = ACC_RANGE_DEFAULT;
      if (FIsLatLongClose(ar->acc_latitude, ar->acc_longitude, car_latitude, car_longitude, radius)>0)
        {
        // This location matches...
        if (enabledonly && (!ar->acc_flags.AccEnabled)) return 0;
        return k+1;
        }
      }
    }

  return 0;
  }

signed char acc_get(struct acc_record* ar, char *location)
  {
  int k;

  if (location == NULL) return 0;
 
  k = atoi(location);
  if ((k>=1)&&(k<=PARAM_ACC_COUNT))
    {
    par_getbase64(k+PARAM_ACC_S-1, ar, sizeof(ar));
    return k;
    }

  return 0;
  }

void acc_state_enter(unsigned char newstate)
  {
  char *p;
  char m[2];
  int k;

  CHECKPOINT(0x60)

  acc_state = newstate;

  // New state, so cancel any pending timeout
  acc_timeout_ticks = 0;
  acc_timeout_goto = 0;

  if (net_state == NET_STATE_DIAGMODE)
    {
    p = stp_x(net_scratchpad,"\r\n# ACC state enter ",acc_state);
    p = stp_rom(p,"\r\n");
    net_puts_ram(net_scratchpad);
    }

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
        CHECKPOINT(0x61)
        m[0] = acc_current_rec.acc_homelink + '0' - 1;
        m[1] = 0;
        net_msg_cmd_msg = m;
        vehicle_fn_commandhandler(FALSE, 24, net_msg_cmd_msg);
        }
      break;
    case ACC_STATE_PARKEDIN:
      // Parked in a charge store area
      break;
    case ACC_STATE_CPDECIDE:
      // Charge port has been opened, so decide what to do (in the ticker1)
      break;
    case ACC_STATE_COOLDOWN:
      // Cooldown in a charge store area
      CHECKPOINT(0x67)
      vehicle_fn_commandhandler(FALSE, 25, NULL); // Cooldown
      if (car_coolingdown<0)
        {
        // Cooldown could not be started...
        // Just ignore it, as we'll pick it up as an end of cooldown
        }
      else if (acc_current_rec.acc_flags.ChargeAtPlugin)
        {
        // Try to trick the car into thinking the charge was ongoing when cooldown
        // started, so that it will continue the charge.
        car_cooldown_wascharging = 1;
        }
      // A bit of a kludge, but we're going to set a state timeout transition
      // so we can only monitor charge after a minute or so. We will never
      // let this timeout actually happen...
      acc_timeout_goto = ACC_STATE_CHARGEDONE;
      acc_timeout_ticks = 120;
      break;
    case ACC_STATE_WAITCHARGE:
      // Waiting for charge time in a charge store area
      // Make sure car doesn't charge now...
      CHECKPOINT(0x68)
      vehicle_fn_commandhandler(FALSE, 12, NULL); // Stop Charge
      if (acc_current_rec.acc_flags.ChargeAtTime)
        {
        acc_chargeminute = acc_current_rec.acc_chargetime;
        }
      else if (acc_current_rec.acc_flags.ChargeByTime)
        {
        // Work out when to start the charge
        if (vehicle_fn_minutestocharge != NULL)
          {
          if (net_state == NET_STATE_DIAGMODE)
            {
            p = stp_i(net_scratchpad,"\r\n# ACC vehicle_fn_minutestocharge(",acc_current_rec.acc_chargemode);
            p = stp_i(p,", ",(int)acc_current_rec.acc_chargelimit * VOLTS_ACC_ASSUMED);
            p = stp_i(p,", ",car_idealrange);
            p = stp_i(p,", ",acc_current_rec.acc_stoprange);
            p = stp_i(p,", ",acc_current_rec.acc_stopsoc);
            p = stp_i(p,", ",car_cac100);
            p = stp_i(p,", ",car_ambient_temp);
            p = stp_rom(p,", NULL)\r\n");
            net_puts_ram(net_scratchpad);
            }

          acc_last_chgmod = acc_current_rec.acc_chargemode;
          acc_last_wAvail = (int)acc_current_rec.acc_chargelimit * VOLTS_ACC_ASSUMED;
          acc_last_ixEnd = acc_current_rec.acc_stoprange;
          acc_last_pctEnd = acc_current_rec.acc_stopsoc;
          acc_last_car_idealrange = car_idealrange;
          acc_last_cac = car_cac100;
          acc_last_loc = acc_current_loc;

          car_chargeestimate = vehicle_fn_minutestocharge(acc_current_rec.acc_chargemode,
                                                          (int)acc_current_rec.acc_chargelimit * VOLTS_ACC_ASSUMED,
                                                          car_idealrange,
                                                          acc_current_rec.acc_stoprange,
                                                          acc_current_rec.acc_stopsoc,
                                                          car_cac100,
                                                          car_ambient_temp,
                                                          NULL);
          acc_last_estimate = car_chargeestimate;

          if (net_state == NET_STATE_DIAGMODE)
            {
            p = stp_i(net_scratchpad,"\r\n# ACC vehicle_fn_minutestocharge result=",car_chargeestimate);
            p = stp_rom(p,"\r\n");
            net_puts_ram(net_scratchpad);
            }
          if (car_chargeestimate<=0)
            {
            // Not achievable - start immediately
            net_req_notification(NET_NOTIFY_CHARGE); // And notify the user as best we can
            acc_state_enter(ACC_STATE_CHARGINGIN);
            }
          else
            {
            // Schedule the charge
            if ((car_chargeestimate) <= acc_current_rec.acc_chargetime)
              acc_chargeminute = acc_current_rec.acc_chargetime - car_chargeestimate; // Schedule charge today
            else
              acc_chargeminute = (acc_current_rec.acc_chargetime + 1440) - car_chargeestimate; // Wrap to previous day
            }
          }
        }
      break;
    case ACC_STATE_CHARGINGIN:
      // Charging in a charge store area
      CHECKPOINT(0x62)
      vehicle_fn_commandhandler(FALSE, 18, NULL); // Wake up car
      delay100(10); // A short delay, to allow the car to wakeup
      car_chargelimit_rangelimit = acc_current_rec.acc_stoprange;
      car_chargelimit_soclimit = acc_current_rec.acc_stopsoc;
      for (k=0;k<2;k++) // Be persistent, do this a few times...
        {
        stp_i(net_scratchpad, "", acc_current_rec.acc_chargemode);
        net_msg_cmd_msg = net_scratchpad;
        vehicle_fn_commandhandler(FALSE, 10, net_msg_cmd_msg);
        delay100(1);
        stp_i(net_scratchpad, "", acc_current_rec.acc_chargelimit);
        net_msg_cmd_msg = net_scratchpad;
        vehicle_fn_commandhandler(FALSE, 15, net_msg_cmd_msg);
        delay100(1);
        vehicle_fn_commandhandler(FALSE, 11, NULL); // Start charge
        delay100(1);
        }
      // A bit of a kludge, but we're going to set a state timeout transition
      // so we can only monitor charge after a minute or so. We will never
      // let this timeout actually happen...
      acc_timeout_goto = ACC_STATE_CHARGEDONE;
      acc_timeout_ticks = 120;
      break;
    case ACC_STATE_WAKEUPCIN:
      // Wake up car, delay a bit, then goto ACC_STATE_CHARGINGIN
      acc_timeout_goto = ACC_STATE_CHARGINGIN;
      acc_timeout_ticks = 10;
      vehicle_fn_commandhandler(FALSE, 18, NULL); // Wake up car
      break;
    case ACC_STATE_WAKEUPWC:
      acc_timeout_goto = ACC_STATE_WAITCHARGE;
      acc_timeout_ticks = 10;
      vehicle_fn_commandhandler(FALSE, 18, NULL); // Wake up car
      for (k=0;k<2;k++) // Be persistent, do this a few times...
        {
        delay100(1);
        stp_i(net_scratchpad, "", acc_current_rec.acc_chargemode);
        net_msg_cmd_msg = net_scratchpad;
        vehicle_fn_commandhandler(FALSE, 10, net_msg_cmd_msg);
        }
      break;
    case ACC_STATE_CHARGEDONE:
      // Completed charging in a charge store area
      car_chargelimit_rangelimit = 0;
      car_chargelimit_soclimit = 0;
      break;
    case ACC_STATE_OVERRIDE:
      // ACC overridden with a manual charge
      break;
    }
  }

void acc_state_ticker1(void)
  {
  char *s;

  CHECKPOINT(0x63)

  switch (acc_state)
    {
    case ACC_STATE_FIRSTRUN:
      // First time run
      acc_current_loc = acc_find(&acc_current_rec,TRUE);
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
      if ((car_doors1bits.ChargePort)&&(car_chargesubstate != 0x07))
        {
        // The charge port has been opened, and we are not waiting for cable connection.
        acc_state_enter(ACC_STATE_CPDECIDE);
        }
      else if (CAR_IS_ON)
        {
        // We've started to drive
        // Kind of kludgy, but set state directly without 'entering' the state
        // so without activating homelink
        acc_state = ACC_STATE_DRIVINGIN;
        }
      break;
    case ACC_STATE_CPDECIDE:
      // Charge port has been opened, so decide what to do
      if ((! car_doors1bits.ChargePort)||(car_chargesubstate == 0x07))
        {
        // The charge port has been closed or cable not connected
        acc_state_enter(ACC_STATE_PARKEDIN);
        }
      else if (acc_current_rec.acc_flags.Cooldown)
        {
        acc_state_enter(ACC_STATE_COOLDOWN);
        }
      else if (acc_current_rec.acc_flags.ChargeAtPlugin)
        {
        acc_state_enter(ACC_STATE_CHARGINGIN);
        }
      else if ((acc_current_rec.acc_flags.ChargeAtTime)||
               (acc_current_rec.acc_flags.ChargeByTime))
        {
        acc_state_enter(ACC_STATE_WAKEUPWC);
        }
      break;
    case ACC_STATE_COOLDOWN:
      // Cooldown in a charge store area
      if (acc_timeout_ticks < 60)
        {
        // Stop timeout 1 minute into the 2 minutes
        acc_timeout_ticks = 0;
        acc_timeout_goto = 0;
        }
      if ((car_coolingdown < 0)||((!CAR_IS_CHARGING)&&(acc_timeout_ticks==0)))
        {
        // Cooldown has completed (or never started)
        if ((car_doors1bits.ChargePort)&&
            (car_chargesubstate != 0x07))
          {
          if (acc_current_rec.acc_flags.ChargeAtPlugin)
            {
            // Let's now do a normal charge...
            acc_state_enter(ACC_STATE_WAKEUPCIN);
            }
          else if ((acc_current_rec.acc_flags.ChargeAtTime)||
                   (acc_current_rec.acc_flags.ChargeByTime))
            {
            // Let's wait or a scheduled charge...
            acc_state_enter(ACC_STATE_WAKEUPWC);
            }
          else
            {
            // Stop charge...
            if (CAR_IS_CHARGING)
              {
              // Stop charge, but stay in current state
              vehicle_fn_commandhandler(FALSE, 12, NULL); // Stop charge
              }
            else
              {
              // If charge has stopped, get out of this state
              acc_state_enter(ACC_STATE_CHARGEDONE);
              }
            }
          }
        else
          {
          acc_state_enter(ACC_STATE_CHARGEDONE);
          }
        }
      break;
    case ACC_STATE_WAITCHARGE:
      // Waiting for charge time in a charge store area
      // Let ensure charge port is closed for at least 10 seconds, before getting out of here
      if (! car_doors1bits.ChargePort)
        {
        if (acc_timeout_goto == 0)
          {
          // Get out in 10 seconds, unless charge port is re-opened
          acc_timeout_ticks = 10;
          acc_timeout_goto = ACC_STATE_PARKEDIN;
          }
        }
      else
        {
        acc_timeout_ticks = 0;
        acc_timeout_goto = 0;
        }
      break;
    case ACC_STATE_CHARGINGIN:
      // Charging in a charge store area
      if (acc_timeout_ticks < 60)
        {
        // Stop timeout 1 minute into the 2 minutes
        acc_timeout_ticks = 0;
        acc_timeout_goto = 0;
        }
      if (! CAR_IS_CHARGING)
        {
        // Only monitor for charge done after 1 minute
        if (acc_timeout_ticks==0)
          acc_state_enter(ACC_STATE_CHARGEDONE);
        }
      else
        {
        if (car_chargemode != acc_current_rec.acc_chargemode)
          {
          stp_i(net_scratchpad, "", acc_current_rec.acc_chargemode);
          net_msg_cmd_msg = net_scratchpad;
          vehicle_fn_commandhandler(FALSE, 10, net_msg_cmd_msg);
          }
        if (car_chargelimit != acc_current_rec.acc_chargelimit)
          {
          stp_i(net_scratchpad, "", acc_current_rec.acc_chargelimit);
          net_msg_cmd_msg = net_scratchpad;
          vehicle_fn_commandhandler(FALSE, 15, net_msg_cmd_msg);
          }
        }
      break;
    case ACC_STATE_CHARGEDONE:
      // Completed charging in a charge store area
      if (CAR_IS_CHARGING)
        {
        acc_state_enter(ACC_STATE_CHARGINGIN);
        }
      else if (CAR_IS_ON)
        {
        acc_state_enter(ACC_STATE_DRIVINGIN);
        }
      else if (!car_doors1bits.ChargePort)
        {
        acc_state_enter(ACC_STATE_PARKEDIN);
        }
      break;
    case ACC_STATE_OVERRIDE:
      // ACC overridden with a manual charge
      if (acc_timeout_ticks < 60)
        {
        // Stop timeout 1 minute into the 2 minutes
        acc_timeout_ticks = 0;
        acc_timeout_goto = 0;
        }
      if (! CAR_IS_CHARGING)
        {
        // Only monitor for charge done after 1 minute
        if (acc_timeout_ticks==0)
          acc_state_enter(ACC_STATE_CHARGEDONE);
        }
      break;
    }
  }

void acc_state_ticker10(void)
  {
  unsigned long now;
  char *p;

  CHECKPOINT(0x64)

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
        acc_current_loc = acc_find(&acc_current_rec,TRUE);
        if (acc_current_loc > 0)
          {
          acc_state_enter(ACC_STATE_DRIVINGIN);
          }
        }
      break;
    case ACC_STATE_DRIVINGIN:
      // Driving in a charge store area
      // Poll to see if we're leaving an ACC location
      acc_current_loc = acc_find(&acc_current_rec,TRUE);
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
      if (CAR_IS_CHARGING)
        {
        // Stop charge, but stay in current state
        vehicle_fn_commandhandler(FALSE, 12, NULL); // Stop charge
        }
      // Check if charge is due
      p = par_get(PARAM_TIMEZONE);
      now = car_time + ((long)timestring_to_mins(p))*60;  // Date+Time in seconds, local time zone
      now = (now % 86400) / 60;  // In minutes past the start of the day
      if (now == acc_chargeminute)
        {
        // Time to charge!
        acc_state_enter(ACC_STATE_WAKEUPCIN);
        }
      break;
    case ACC_STATE_CHARGINGIN:
      // Charging in a charge store area
      break;
    case ACC_STATE_CHARGEDONE:
      // Completed charging in a charge store area
      break;
    case ACC_STATE_OVERRIDE:
      // ACC overridden with a manual charge
      break;
    }
  
  }

BOOL acc_handle_msg(BOOL msgmode, int code, char* msg)
  {
  CHECKPOINT(0x65)

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
      if (code == 11)
        {
        // Manual charge start
        // A bit of a kludge, but we're going to set a state timeout transition
        // so we can only monitor charge after a minute or so. We will never
        // let this timeout actually happen...
        acc_state_enter(ACC_STATE_OVERRIDE);
        acc_timeout_goto = ACC_STATE_OVERRIDE;
        acc_timeout_ticks = 120;
        }
      break;
    case ACC_STATE_COOLDOWN:
      // Cooldown in a charge store area
      break;
    case ACC_STATE_WAITCHARGE:
      // Waiting for charge time in a charge store area
      if (code == 11)
        {
        // Manual charge start
        acc_state_enter(ACC_STATE_WAKEUPCIN);
        }
      else if (code == 25)
        {
        // Manual cooldown
        acc_state_enter(ACC_STATE_COOLDOWN);
        }
      break;
    case ACC_STATE_CHARGINGIN:
      // Charging in a charge store area
      break;
    case ACC_STATE_CHARGEDONE:
      // Completed charging in a charge store area
      if (code == 11)
        {
        // Manual charge start
        // A bit of a kludge, but we're going to set a state timeout transition
        // so we can only monitor charge after a minute or so. We will never
        // let this timeout actually happen...
        acc_state_enter(ACC_STATE_OVERRIDE);
        acc_timeout_goto = ACC_STATE_OVERRIDE;
        acc_timeout_ticks = 120;
        }
      break;
    case ACC_STATE_OVERRIDE:
      // ACC overridden with a manual charge
      break;
    }

  return FALSE;
  }

void acc_initialise(void)        // ACC Initialisation
  {
  acc_state_enter(ACC_STATE_FIRSTRUN);
  }

BOOL acc_cmd_here(BOOL sms, char* caller, char* arguments)
  {
  // Turn on ACC here
  unsigned char k;
  char *s;
  struct acc_record ar;

  net_send_sms_start(caller);

  k = acc_find(&ar,FALSE);
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

BOOL acc_cmd_nothere(BOOL sms, char* caller, char *arguments)
  {
  // Turn off ACC here
  unsigned char k;
  char *s;
  unsigned char found = 0;
  struct acc_record ar;

  s = stp_rom(net_scratchpad, "ACC cleared");

  while ((k=acc_find(&ar,FALSE))>0)
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

BOOL acc_cmd_clear(BOOL sms, char* caller, char *arguments)
  {
  // Clear all ACC data
  unsigned char k;
  struct acc_record ar;
  memset(&ar,0,sizeof(ar));

  k = acc_get(&ar, arguments);
  if (k>0)
    {
    par_set(k+PARAM_ACC_S-1,NULL);
    }
  else
    {
    for (k=0;k<PARAM_ACC_COUNT;k++)
      {
      par_set(k+PARAM_ACC_S,NULL);
      }
    }

  net_send_sms_start(caller);
  net_puts_rom("ACC configuration cleared");
  return TRUE;
  }

BOOL acc_cmd_stat(BOOL sms, char* caller, char *arguments)
  {
  // Return ACC status
  struct acc_record ar;
  int k;
  char *s,*p;
  unsigned long r;

  k = acc_find(&ar,FALSE);

  net_send_sms_start(caller);
  s = stp_i(net_scratchpad,"ACC Status #",k);
  if ((k>0)&&(ar.acc_recversion == ACC_RECVERSION))
    {
    s = stp_latlon(s, "\r\n ", ar.acc_latitude);
    s = stp_latlon(s, ", ", ar.acc_longitude);
    s = stp_rom(s, (ar.acc_flags.AccEnabled)?"\r\n Enabled":"\r\n Disabled");
    }

  s = stp_rom(s, "\r\n State: ");
  switch (acc_state)
    {
    case ACC_STATE_FIRSTRUN:
      // First time run
      s = stp_rom(s, "First run");
      break;
    case ACC_STATE_FREE:
      // Outside a charge store area
      s = stp_rom(s, "Free");
      if (CAR_IS_ON) s = stp_rom(s, " (driving)");
      break;
    case ACC_STATE_DRIVINGIN:
      // Driving in a charge store area
      s = stp_rom(s, "Driving In");
      break;
    case ACC_STATE_PARKEDIN:
      // Parked in a charge store area
      s = stp_rom(s, "Parked In");
      break;
    case ACC_STATE_CPDECIDE:
      // Charge port has been opened, so decide what to do
      s = stp_rom(s, "ChargePort Open");
      break;
    case ACC_STATE_COOLDOWN:
      // Cooldown in a charge store area
      s = stp_rom(s, "Cooldown");
      s = stp_i(s,"\r\n ",car_coolingdown);
      s = stp_i(s," cycles, ",car_tbattery);
      s = stp_i(s,"C/",car_cooldown_tbattery);
      s = stp_i(s,"C, ",car_cooldown_timelimit);
      s = stp_rom(s,"mins remain");
      break;
    case ACC_STATE_WAITCHARGE:
      // Waiting for charge time in a charge store area
      s = stp_rom(s, "Wait Charge");
      s = stp_time(s, "\r\n Wake at: ",(unsigned long)acc_chargeminute * 60);
      s = stp_i(s, "\r\n Charge: ",(int)acc_current_rec.acc_chargelimit * VOLTS_ACC_ASSUMED);
      s = stp_i(s, "W\r\n Estimate: ",car_chargeestimate);
      s = stp_rom(s, "mins");
      break;
    case ACC_STATE_CHARGINGIN:
      // Charging in a charge store area
      s = stp_rom(s, "Charging In");
      s = stp_mode(s, "\r\n ",ar.acc_chargemode);
      s = stp_i(s, " ", car_linevoltage);
      s = stp_i(s, "V/", car_chargecurrent);
      s = stp_rom(s,"A");
      break;
    case ACC_STATE_WAKEUPCIN:
      // Wake up and charge in a charge store area
      s = stp_rom(s, "WakeUp to Charge");
      break;
    case ACC_STATE_WAKEUPWC:
      // Wake up and wait for charge
      s = stp_rom(s, "WakeUp to wait charge");
      break;
    case ACC_STATE_CHARGEDONE:
      // Completed charging in a charge store area
      s = stp_rom(s, "Charge Done");
      break;
    case ACC_STATE_OVERRIDE:
      // ACC overridden with a manual charge
      s = stp_rom(s, "Charge override");
      break;
    }

  net_puts_ram(net_scratchpad);
  return TRUE;
  }


BOOL acc_cmd_diag(BOOL sms, char* caller, char *arguments)
  {
  // Return ACC status
  char *s;

  net_send_sms_start(caller);

  s = stp_i(net_scratchpad,"ACC DIAG last:\r\n  chgmod: ",acc_last_chgmod);
  s = stp_i(s,"\r\n  wAvail: ",acc_last_wAvail);
  s = stp_i(s,"\r\n  ixEnd: ",acc_last_ixEnd);
  s = stp_i(s,"\r\n  pctEnd: ",acc_last_pctEnd);
  s = stp_i(s,"\r\n  idealRange: ",acc_last_car_idealrange);
  s = stp_i(s,"\r\n  CAC: ",acc_last_cac);
  s = stp_i(s,"\r\n  location: ",acc_last_loc);
  s = stp_i(s,"\r\n  estimate: ",acc_last_estimate);

  net_puts_ram(net_scratchpad);
  return TRUE;
  }

BOOL acc_cmd_enable(BOOL sms, char* caller, char *arguments, unsigned char enabled)
  {
  // Enable/Disable ACC
  struct acc_record ar;
  int k;
  char *s;

  if (arguments != NULL)
    {
    k = acc_get(&ar, arguments);
    }
  else
    {
    k = acc_find(&ar,FALSE);
    }

  net_send_sms_start(caller);
  if (k<0)
    {
    s = stp_rom(net_scratchpad,ACC_NOTHERE);
    }
  else
    {
    ar.acc_flags.AccEnabled = enabled;
    par_setbase64(k+PARAM_ACC_S-1,&ar,sizeof(ar));
    s = stp_rom(net_scratchpad,(enabled)?"ACC enabled":"ACC disabled");
    }

  net_puts_ram(net_scratchpad);

  acc_state_enter(ACC_STATE_FIRSTRUN);

  return TRUE;
  }

void acc_sms_params(int k, struct acc_record* ar)
  {
  // SMS ACC parameters
  char *s,*p;
  unsigned long r;

  s = stp_i(net_scratchpad,"ACC #",k);
  if ((k>0)&&(ar->acc_recversion == ACC_RECVERSION))
    {
    s = stp_latlon(s, "\r\n ", ar->acc_latitude);
    s = stp_latlon(s, ", ", ar->acc_longitude);
    s = stp_rom(s, (ar->acc_flags.AccEnabled)?"\r\n Enabled":"\r\n Disabled");
    if (ar->acc_flags.Cooldown)
      s = stp_rom(s, "\r\n Cooldown");
    if (ar->acc_flags.Homelink)
      s = stp_i(s, "\r\n Homelink #",ar->acc_homelink);
    if (ar->acc_flags.ChargeAtPlugin)
      s = stp_rom(s, "\r\n Charge at plugin");
    if (ar->acc_flags.ChargeAtTime)
      s = stp_time(s, "\r\n Charge at time ",(unsigned long)ar->acc_chargetime * 60);
    if (ar->acc_flags.ChargeByTime)
      s = stp_time(s, "\r\n Charge by time ",(unsigned long)ar->acc_chargetime * 60);
    s = stp_mode(s, "\r\n Mode: ",ar->acc_chargemode);
    s = stp_i(s, " (",ar->acc_chargelimit);
    s = stp_rom(s, "A)");
    if (ar->acc_stopsoc>0)
      {
      s = stp_i(s, "\r\n Stop at SOC ",ar->acc_stopsoc);
      s = stp_rom(s, "%");
      }
    if (ar->acc_stoprange>0)
      {
      r = (unsigned long)ar->acc_stoprange;
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
    if (ar->acc_radius>0)
      {
      r = (unsigned long)ar->acc_radius;
      if (can_mileskm=='M')
        {
        s = stp_l(s, "\r\n Radius ",r*3);
        s = stp_rom(s, "'");
        }
      else
        {
        s = stp_l(s, "\r\n Radius ",r);
        s = stp_rom(s, "m");
        }
      }
    }
  else
    {
    s = stp_rom(s," - No ACC defined here");
    }

  net_puts_ram(net_scratchpad);
  }

BOOL acc_cmd_params(BOOL sms, char* caller, char *arguments)
  {
  // Set ACC params
  struct acc_record ar;
  int k = 0;
  char p;
  char *s;

  if (arguments != NULL)
    {
    k = acc_get(&ar, arguments);
    if (k>0)
      arguments = net_sms_nextarg(arguments);
    else
      k = acc_find(&ar,FALSE);
    }

  net_send_sms_start(caller);
  if (k<=0)
    {
    s = stp_rom(net_scratchpad,ACC_NOTHERE);
    }
  else
    {
    while (arguments != NULL)
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
          { ar.acc_chargetime = timestring_to_mins(arguments); }
        }
      else if (strcmppgm2ram(arguments,"CHARGEBY")==0)
        {
        ar.acc_flags.ChargeAtPlugin = 0;
        ar.acc_flags.ChargeAtTime = 0;
        ar.acc_flags.ChargeByTime = 1;
        arguments = net_sms_nextarg(arguments);
        if (arguments != NULL)
          { ar.acc_chargetime = timestring_to_mins(arguments); }
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
      else if (strcmppgm2ram(arguments,"RADIUS")==0)
        {
        arguments = net_sms_nextarg(arguments);
        if (arguments != NULL)
          {
          if (can_mileskm=='M')
            ar.acc_radius = atoi(arguments)/3;
          else
            ar.acc_radius = atoi(arguments);
          }
        }
      if (arguments != NULL)
        arguments = net_sms_nextarg(arguments);
      }
    par_setbase64(k+PARAM_ACC_S-1,&ar,sizeof(ar));
    acc_sms_params(k, &ar);
    }

  acc_state_enter(ACC_STATE_FIRSTRUN);

  return TRUE;
  }

BOOL acc_cmd_paramsq(BOOL sms, char* caller, char *arguments)
  {
  // Return ACC status
  struct acc_record ar;
  int k;
  char *s,*p;
  unsigned long r;

  if (arguments != NULL)
    {
    k = acc_get(&ar, arguments);
    }
  else
    {
    k = acc_find(&ar,FALSE);
    }

  net_send_sms_start(caller);
  acc_sms_params(k, &ar);

  return TRUE;
  }

BOOL acc_cmd(char *caller, char *command, char *arguments, BOOL sms)
  {
  char *p = arguments;

  if (arguments == NULL)
    {
    net_send_sms_start(caller);
    net_puts_rom("ACC command not recognised");
    return TRUE;
    }

  strupr(arguments); // Convert command to upper case
  arguments = net_sms_nextarg(arguments);

  if (strcmppgm2ram(p,"HERE")==0)
    {
    return acc_cmd_here(sms, caller, arguments);
    }
  else if (strcmppgm2ram(p,"NOTHERE")==0)
    {
    return acc_cmd_nothere(sms, caller, arguments);
    }
  else if (strcmppgm2ram(p,"CLEAR")==0)
    {
    return acc_cmd_clear(sms, caller, arguments);
    }
  else if (strcmppgm2ram(p,"STAT")==0)
    {
    return acc_cmd_stat(sms, caller, arguments);
    }
  else if (strcmppgm2ram(p,"DIAG")==0)
    {
    return acc_cmd_diag(sms, caller, arguments);
    }
  else if (strcmppgm2ram(p,"ENABLE")==0)
    {
    return acc_cmd_enable(sms, caller, arguments, 1);
    }
  else if (strcmppgm2ram(p,"DISABLE")==0)
    {
    return acc_cmd_enable(sms, caller, arguments, 0);
    }
  else if (strcmppgm2ram(p,"PARAMS?")==0)
    {
    return acc_cmd_paramsq(sms, caller, arguments);
    }
  else if (strcmppgm2ram(p,"PARAMS")==0)
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
    acc_granular_tick -= 60;
    }
  }
