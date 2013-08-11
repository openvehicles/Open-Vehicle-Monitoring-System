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

#ifndef __OVMS_ACC_H
#define __OVMS_ACC_H

// STATES
#define ACC_STATE_FIRSTRUN   0x00  // First time run
#define ACC_STATE_FREE       0x10  // Outside a charge store area
#define ACC_STATE_DRIVINGIN  0x20  // Driving in a charge store area
#define ACC_STATE_PARKEDIN   0x30  // Parked in a charge store area
#define ACC_STATE_COOLDOWN   0x40  // Cooldown in a charge store area
#define ACC_STATE_WAITCHARGE 0x50  // Waiting for charge time in a charge store area
#define ACC_STATE_CHARGING   0x51  // Charging in a charge store area
#define ACC_STATE_CHARGEDONE 0x60  // Completed charging in a charge store area

// ACC data
extern unsigned char acc_state;                // The current state
extern unsigned char acc_state_vchar;          //   A per-state CHAR variable
extern unsigned int  acc_state_vint;           //   A per-state INT variable
extern unsigned char acc_timeout_goto;         // State to auto-transition to, after timeout
extern unsigned int  acc_timeout_ticks;        // Number of seconds before timeout auto-transition
extern unsigned int  acc_granular_tick;        // An internal ticker used to generate 1min, 5min, etc, calls

#define ACC_RANGE1 50
#define ACC_RANGE2 100

#define ACC_RECVERSION 1

struct acc_record
  {
  signed long acc_latitude;         // Latitude of ACC location
  signed long acc_longitude;        // Longitude of ACC location
  unsigned char acc_recversion;     // Encoding version of this record (ACC_RECVERSION)
  struct
    {
    unsigned AccEnabled:1;          // 0x01
    unsigned Cooldown:1;            // 0x02
    unsigned Homelink:1;            // 0x04
    unsigned :1;                    // 0x08
    unsigned ChargeAtPlugin:1;      // 0x10
    unsigned ChargeAtTime:1;        // 0x20
    unsigned ChargeByTime:1;        // 0x40
    unsigned :1;                    // 0x80
    } acc_flags;
  unsigned char acc_chargemode;     // 0=standard, 1=storage, 3=range, 4=performance
  unsigned char acc_chargelimit;    // Charge Limit (amps)
  unsigned int acc_chargetime;      // Time to start/stop charge
  unsigned int acc_stoprange;       // Range to stop charge at
  unsigned char acc_stopsoc;        // SOC to stop charge at
  unsigned char acc_homelink;       // Homelink to activate
  };

void acc_initialise(void);        // ACC Initialisation
void acc_ticker(void);            // ACC Ticker
BOOL acc_handle_sms(char *caller, char *command, char *arguments);

#endif // #ifndef __OVMS_ACC_H
