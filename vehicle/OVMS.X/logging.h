/*
;    Project:       Open Vehicle Monitor System
;    Date:          24 April 2013
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

#ifndef __OVMS_LOGGING_H
#define __OVMS_LOGGING_H

// STATES
#define LOG_STATE_FIRSTRUN        0x00  // First time run
#define LOG_STATE_WAITDRIVE_DONE  0x01  // Wait for drive to end, then discard
#define LOG_STATE_WAITCHARGE_DONE 0x02  // Wait for charge to end, then discard
#define LOG_STATE_DRIVING         0x10  // Driving
#define LOG_STATE_CHARGING        0x20  // Charging
#define LOG_STATE_PARKED          0x30  // Parked

// LOG data
extern unsigned char log_state;                // The current state
extern unsigned char log_state_vchar;          //   A per-state CHAR variable
extern unsigned int  log_state_vint;           //   A per-state INT variable
extern unsigned char log_timeout_goto;         // State to auto-transition to, after timeout
extern unsigned int  log_timeout_ticks;        // Number of seconds before timeout auto-transition
extern unsigned int  log_granular_tick;        // An internal ticker used to generate 1min, 5min, etc, calls

#define LOG_TYPE_CHARGE         'C'     // A charge log record
#define LOG_TYPE_CHARGE_DEL     'c'     // A charge log record being delivered
#define LOG_TYPE_CHARGING       'h'     // A charging log record
#define LOG_TYPE_DRIVE          'D'     // A drive log record
#define LOG_TYPE_DRIVE_DEL      'd'     // A drive log record being delivered
#define LOG_TYPE_DRIVING        'r'     // A driving log record
#define LOG_TYPE_FREE           0       // A free log record

#define LOG_CHARGERESULT_OK     0       // Result if charge was ok
#define LOG_CHARGERESULT_STOP   1       // Result if charge was stopped
#define LOG_CHARGERESULT_FAIL   2       // Result if charge failed

#define LOG_RECORDSTORE         6       // Number of records that can be stored

struct logging_record
  {
  unsigned char type;               // One of LOG_TYPE_*
  unsigned long start_time;         // Time (car_time) log record created
  unsigned int  duration;           // Log record duration
  union
    {
    struct
      {
      unsigned char charge_mode;    // Mode vehicle was charging in
      signed long charge_latitude;  // Raw GPS Latitude
      signed long charge_longitude; // Raw GPS Longitude
      unsigned int charge_voltage;  // Largest line voltage during charge
      unsigned char charge_current; // Largest lien current during charge
      unsigned char charge_result;  // One of LOG_CHARGERESULT_*
      unsigned char start_SOC;
      unsigned int start_idealrange;
      unsigned char end_SOC;
      unsigned int end_idealrange;
      } charge;
    struct
      {
      unsigned char drive_mode;    // Mode vehicle was driven in
      signed long start_latitude;  // Raw GPS Latitude
      signed long start_longitude; // Raw GPS Longitude
      signed long end_latitude;    // Raw GPS Latitude
      signed long end_longitude;   // Raw GPS Longitude
      unsigned long distance;      // Distance driven (or start odometer for 'd')
      unsigned char start_SOC;
      unsigned int start_idealrange;
      unsigned char end_SOC;
      unsigned int end_idealrange;
      } drive;
    } record;
  };
  
extern struct logging_record log_recs[LOG_RECORDSTORE];

unsigned char logging_haspending(void); // Pending log messages
void logging_sendpending(void);         // Send pending log messages
void logging_serverconnect(void);       // Indication that server has connected
void logging_ack(unsigned char ack);    // A server acknowledgement
void logging_initialise(void);          // Logging Initialisation
void logging_ticker(void);              // Logging Ticker

#endif // #ifndef __OVMS_LOGGING_H
