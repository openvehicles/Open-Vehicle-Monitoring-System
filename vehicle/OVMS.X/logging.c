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

#include "ovms.h"
#include "logging.h"
#include "net_msg.h"

// LOGGING data
#pragma udata LOGGING
unsigned char log_state = 0;                // The current state
unsigned char log_state_vchar = 0;          //   A per-state CHAR variable
unsigned int  log_state_vint = 0;           //   A per-state INT variable
unsigned char log_timeout_goto = 0;         // State to auto-transition to, after timeout
unsigned int  log_timeout_ticks = 0;        // Number of seconds before timeout auto-transition
unsigned int  log_granular_tick = 0;        // An internal ticker used to generate 1min, 5min, etc, calls

struct logging_record log_recs[LOG_RECORDSTORE];
signed char logging_pos = -1;
signed char logging_pending = 0;

signed char log_getfreerecord(void)
  {
  unsigned char x;

  for (x=0;x<LOG_RECORDSTORE;x++)
    {
    if (log_recs[x].type == LOG_TYPE_FREE)
      return x;
    }

  return -1;
  }

void log_state_enter(unsigned char newstate)
  {
  char *p;
  struct logging_record *rec;

  log_state = newstate;

  switch (log_state)
    {
    case LOG_STATE_FIRSTRUN:
      // First time run
      if (car_doors1bits.CarON)
        { log_state = LOG_STATE_WAITDRIVE; }
      else if (car_doors1bits.Charging)
        { log_state = LOG_STATE_WAITCHARGE; }
      break;
    case LOG_STATE_DRIVING:
      // A drive has just started...
      logging_pos = log_getfreerecord();
      if (logging_pos < 0)
        {
        // Overflow...
        log_state = LOG_STATE_WAITDRIVE;
        return;
        }
      rec = &log_recs[logging_pos];
      rec->type = LOG_TYPE_DRIVING;
      rec->start_time = car_time;
      rec->record.drive.drive_mode = car_chargemode;
      rec->record.drive.start_latitude = car_latitude;
      rec->record.drive.start_longitude = car_longitude;
      rec->record.drive.distance = car_odometer;
      rec->record.drive.start_SOC = car_SOC;
      rec->record.drive.start_idealrange = car_idealrange;
      break;
    case LOG_STATE_CHARGING:
      // A charge has just started...
      logging_pos = log_getfreerecord();
      if (logging_pos < 0)
        {
        // Overflow...
        log_state = LOG_STATE_WAITCHARGE;
        return;
        }
      rec = &log_recs[logging_pos];
      rec->type = LOG_TYPE_CHARGING;
      rec->start_time = car_time;
      rec->record.charge.charge_mode = car_chargemode;
      rec->record.charge.charge_latitude = car_latitude;
      rec->record.charge.charge_longitude = car_longitude;
      rec->record.charge.charge_voltage = car_linevoltage;
      rec->record.charge.charge_current = car_chargecurrent;
      rec->record.charge.start_SOC = car_SOC;
      rec->record.charge.start_idealrange = car_idealrange;
      break;
    }
  }

void log_state_ticker1(void)
  {
  struct logging_record *rec;

  switch (log_state)
    {
    case LOG_STATE_WAITDRIVE:
      if (car_doors1bits.CarON == 0)
        log_state_enter(LOG_STATE_PARKED);
      break;
    case LOG_STATE_WAITCHARGE:
      if ((car_doors1bits.Charging == 0)||(car_doors1bits.CarON))
        log_state_enter(LOG_STATE_PARKED);
      break;
    case LOG_STATE_DRIVING:
      if (logging_pos<0)
        {
        // We shouldn't be here without a log record
        log_state_enter(LOG_STATE_WAITDRIVE);
        break;
        }
      if (car_doors1bits.CarON == 0)
        {
        // Drive has finished
        rec = &log_recs[logging_pos];
        logging_pos = -1;
        logging_pending++;
        rec->type = LOG_TYPE_DRIVE;
        rec->duration = car_time - rec->start_time;
        rec->record.drive.end_latitude = car_latitude;
        rec->record.drive.end_longitude = car_longitude;
        rec->record.drive.distance = car_odometer - rec->record.drive.distance;
        rec->record.drive.end_SOC = car_SOC;
        rec->record.drive.end_idealrange = car_idealrange;
        log_state_enter(LOG_STATE_PARKED);
        }
      break;
    case LOG_STATE_CHARGING:
      if (logging_pos<0)
        {
        // We shouldn't be here without a log record
        log_state_enter(LOG_STATE_WAITCHARGE);
        break;
        }
      rec = &log_recs[logging_pos];
      if (car_linevoltage > rec->record.charge.charge_voltage)
        rec->record.charge.charge_voltage = car_linevoltage;
      if (car_chargecurrent > rec->record.charge.charge_current)
        rec->record.charge.charge_current = car_chargecurrent;
      if ((car_doors1bits.Charging == 0)||(car_doors1bits.CarON))
        {
        // Charge has finished
        logging_pos = -1;
        logging_pending++;
        rec->type = LOG_TYPE_DRIVE;
        rec->duration = car_time - rec->start_time;
        rec->record.charge.charge_mode = car_chargemode;
        if (car_chargestate == 4)
          rec->record.charge.charge_result = LOG_CHARGERESULT_OK;
        else if (car_chargesubstate == 3)
          rec->record.charge.charge_result = LOG_CHARGERESULT_STOP;
        else
          rec->record.charge.charge_result = LOG_CHARGERESULT_FAIL;
        rec->record.charge.end_SOC = car_SOC;
        rec->record.charge.end_idealrange = car_idealrange;
        log_state_enter(LOG_STATE_PARKED);
        }
      break;
    case LOG_STATE_PARKED:
      if (car_doors1bits.CarON)
        { log_state_enter(LOG_STATE_DRIVING); }
      else if (car_doors1bits.Charging)
        { log_state_enter(LOG_STATE_CHARGING); }
      break;
    }
  }

unsigned char logging_haspending(void)
  {
  // Pending log messages
  return logging_pending;
  }

void logging_sendpending(void)
  {
  // Send pending log messages

  char *s;
  unsigned char x;
  struct logging_record *rec;
  unsigned char dr,cr;

  dr=0;
  cr=0;

  for (x=0;x<LOG_RECORDSTORE;x++)
    {
    rec = &log_recs[x];
    if (rec->type == LOG_TYPE_DRIVE)
      {
      s = stp_i(net_scratchpad, "MP-0 HRT-Log-Drive,", dr++);
      s = stp_rom(s, ",31536000,");
      s = stp_l(s, ",", rec->start_time);
      s = stp_i(s, ",", rec->duration);
      s = stp_i(s, ",",rec->record.drive.drive_mode);
      s = stp_latlon(s, ",", rec->record.drive.start_latitude);
      s = stp_latlon(s, ",", rec->record.drive.start_longitude);
      s = stp_latlon(s, ",", rec->record.drive.end_latitude);
      s = stp_latlon(s, ",", rec->record.drive.end_longitude);
      s = stp_l(s, ",", rec->record.drive.distance);
      s = stp_i(s, ",", rec->record.drive.start_SOC);
      s = stp_i(s, ",", rec->record.drive.start_idealrange);
      s = stp_i(s, ",", rec->record.drive.end_SOC);
      s = stp_i(s, ",", rec->record.drive.end_idealrange);
      net_msg_encode_puts();
      rec->type = LOG_TYPE_FREE;
      }
    else if (rec->type == LOG_TYPE_CHARGE)
      {
      s = stp_i(net_scratchpad, "MP-0 HRT-Log-Charge,", cr++);
      s = stp_rom(s, ",31536000,");
      s = stp_l(s, ",", rec->start_time);
      s = stp_i(s, ",", rec->duration);
      s = stp_i(s, ",",rec->record.charge.charge_mode);
      s = stp_latlon(s, ",", rec->record.charge.charge_latitude);
      s = stp_latlon(s, ",", rec->record.charge.charge_longitude);
      s = stp_i(s, ",", rec->record.charge.charge_voltage);
      s = stp_i(s, ",", rec->record.charge.charge_current);
      s = stp_i(s, ",", rec->record.charge.charge_result);
      s = stp_i(s, ",", rec->record.charge.start_SOC);
      s = stp_i(s, ",", rec->record.charge.start_idealrange);
      s = stp_i(s, ",", rec->record.charge.end_SOC);
      s = stp_i(s, ",", rec->record.charge.end_idealrange);
      net_msg_encode_puts();
      rec->type = LOG_TYPE_FREE;
      }
    }
  logging_pending = 0;
  }

void log_state_ticker60(void)
  {
  }

void log_state_ticker3600(void)
  {
  }

void logging_initialise(void)        // Logging Initialisation
  {
  }

void logging_ticker(void)            // Logging Ticker
  {
  // This ticker is called once every second
  log_granular_tick++;
  if ((log_timeout_goto > 0)&&(log_timeout_ticks-- == 0))
    {
    log_state_enter(log_timeout_goto);
    }
  else
    {
    log_state_ticker1();
    }
  if ((log_granular_tick % 60)==0)    log_state_ticker60();
  if ((log_granular_tick % 3600)==0)
    {
    log_state_ticker3600();
    log_granular_tick -= 3600;
    }
  }
