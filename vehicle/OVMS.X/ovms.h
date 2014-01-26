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

#ifndef __OVMS_MAIN
#define __OVMS_MAIN

#include "ovms.def"
#ifdef OVMS_HW_V1
#include "p18f2680.h"
#elif OVMS_HW_V2
#include "p18f2685.h"
#endif // #ifdef
#include "UARTIntC.h"
#include "utils.h"
#include "params.h"
#include "vehicle.h"
#include "net.h"

#define OVMS_FIRMWARE_VERSION 2,6,2

#define FEATURES_MAX 16
#define FEATURES_MAP_PARAM 8
#define FEATURE_STREAM       0x08 // Location streaming feature
#define FEATURE_MINSOC       0x09 // Minimum SOC feature
#define FEATURE_OPTIN        0x0D // Features to opt-in to
#define FEATURE_CARBITS      0x0E // Various ON/OFF features (bitmap)
#define FEATURE_CANWRITE     0x0F // CAN bus can be written to

// The FEATURE_OPTIN feature is a set of ON/OFF bits to control different
// miscelaneous aspects of the system that must be opted in to. The following
// bits are defined:
#define FEATURE_OI_SPEEDO    0x01 // Set to 1 to enable digital speedo
#define FEATURE_OI_LOGDRIVES 0x02 // Set to 1 to enable logging of drives
#define FEATURE_OI_LOGCHARGE 0x04 // Set to 1 to enable logging of charges

// The FEATURE_CARBITS feature is a set of ON/OFF bits to control different
// miscelaneous aspects of the system. The following bits are defined:
#define FEATURE_CB_2008      0x01 // Set to 1 to mark the car as 2008/2009
#define FEATURE_CB_SAD_SMS   0x02 // Set to 1 to suppress "Access Denied" SMS
#define FEATURE_CB_SOUT_SMS  0x04 // Set to 1 to suppress all outbound SMS
#define FEATURE_CB_SVALERTS  0x08 // Set to 1 to suppress vehicle alerts

// The FEATURE_CANWRITE feature controls the CAN mode of the OVMS system
// Leaving it 0 (the default) will put the CAN bus in LISTEN mode. It
// will not be written to and no acknowledgements will be issued on the bus.
// Defining it >0 will put the CAN bus in NORMAL mode. This will allow the
// system to co-operate with the CAN bus (eg; issuing acknowledgements) and
// also give it the ability to transmit messages on the CAN bus.
// Leaving FEATURE_CANWRITE=0 is obviously less likely to interfere with
// the vehicle normal operations.

#define MAX_ICCID 24
#define COOLDOWN_DEFAULT_TEMPLIMIT 31
#define COOLDOWN_DEFAULT_TIMELIMIT 60

#pragma udata
extern rom unsigned char ovms_firmware[3]; // Firmware version
extern unsigned int car_linevoltage; // Line Voltage
extern unsigned char car_chargecurrent; // Charge Current
extern unsigned char car_chargelimit; // Charge Limit (amps)
extern unsigned int car_chargeduration; // Charge Duration (minutes)
extern unsigned char car_chargestate; // 1=charging, 2=top off, 4=done, 13=preparing to charge, 21-25=stopped charging
extern unsigned char car_chargesubstate;
extern unsigned char car_chargemode; // 0=standard, 1=storage, 3=range, 4=performance
extern unsigned char car_charge_b4; // B4 byte of charge state
extern unsigned char car_chargekwh; // KWh of charge


//
// CAR STATUS BITS:
// see developer manual on "Virtual Vehicle / Environment" for more info
//

extern unsigned char car_doors1;
typedef struct {
  unsigned FrontLeftDoor:1;     // 0x01
  unsigned FrontRightDoor:1;    // 0x02
  unsigned ChargePort:1;        // 0x04
  unsigned PilotSignal:1;       // 0x08
  unsigned Charging:1;          // 0x10
  unsigned :1;                  // 0x20
  unsigned HandBrake:1;         // 0x40
  unsigned CarON:1;             // 0x80
} car_doors1bits_t;
#define car_doors1bits (*((car_doors1bits_t*)&car_doors1))

extern unsigned char car_doors2;
typedef struct {
  unsigned :1;                  // 0x01
  unsigned :1;                  // 0x02
  unsigned :1;                  // 0x04
  unsigned CarLocked:1;         // 0x08
  unsigned ValetMode:1;         // 0x10
  unsigned Headlights:1;        // 0x20
  unsigned Bonnet:1;            // 0x40
  unsigned Trunk:1;             // 0x80
} car_doors2bits_t;
#define car_doors2bits (*((car_doors2bits_t*)&car_doors2))

extern unsigned char car_doors3;
typedef struct {
  unsigned :1;                  // 0x01
  unsigned CoolingPump:1;       // 0x02
  unsigned :1;                  // 0x04
  unsigned :1;                  // 0x08
  unsigned :1;                  // 0x10
  unsigned :1;                  // 0x20
  unsigned :1;                  // 0x40
  unsigned :1;                  // 0x80
} car_doors3bits_t;
#define car_doors3bits (*((car_doors3bits_t*)&car_doors3))

extern unsigned char car_doors4;
typedef struct {
  unsigned :1;                  // 0x01
  unsigned :1;                  // 0x02
  unsigned AlarmSounds:1;       // 0x04
  unsigned :1;                  // 0x08
  unsigned :1;                  // 0x10
  unsigned :1;                  // 0x20
  unsigned :1;                  // 0x40
  unsigned :1;                  // 0x80
} car_doors4bits_t;
#define car_doors4bits (*((car_doors4bits_t*)&car_doors4))

extern unsigned char car_doors5;
typedef struct {
  unsigned RearLeftDoor:1;      // 0x01
  unsigned RearRightDoor:1;     // 0x02
  unsigned Frunk:1;             // 0x04
  unsigned :1;                  // 0x08
  unsigned Charging12V:1;       // 0x10
  unsigned :1;                  // 0x20
  unsigned :1;                  // 0x40
  unsigned HVAC:1;              // 0x80
} car_doors5bits_t;
#define car_doors5bits (*((car_doors5bits_t*)&car_doors5))


extern unsigned char car_lockstate; // Lock State
extern unsigned char car_speed; // speed in defined units (mph or kph)
extern unsigned char car_SOC; // State of Charge in %
extern unsigned int car_idealrange; // Ideal Range in miles
extern unsigned int car_estrange; // Estimated Range
extern unsigned long car_time; // UTC Time
extern unsigned long car_parktime; // UTC time car was parked (or 0 if not)
extern signed int car_ambient_temp; // Ambient Temperature (celcius)
extern unsigned char car_vin[18]; // VIN
extern unsigned char car_type[5]; // Car Type
extern signed int car_tpem; // Tpem
extern signed int car_tmotor; // Tmotor
extern signed int car_tbattery; // Tbattery
extern signed int car_tpms_t[4]; // TPMS temperature
extern unsigned char car_tpms_p[4]; // TPMS pressure
extern unsigned int car_trip; // ODO trip in miles /10
extern unsigned long car_odometer; //Odometer in miles /10
extern signed long car_latitude; // Raw GPS Latitude
extern signed long car_longitude; // Raw GPS Longitude
extern unsigned int car_direction; // GPS direction of the car
extern signed int car_altitude; // GPS altitude of the car
extern signed char car_timermode; // Timer mode (0=onplugin, 1=timer)
extern unsigned int car_timerstart; // Timer start
extern unsigned char car_gpslock; // GPS lock status
extern signed char car_stale_ambient; // 0 = Ambient temperature is stale
extern signed char car_stale_temps; // 0 = Powertrain temperatures are stale
extern signed char car_stale_gps; // 0 = gps is stale
extern signed char car_stale_tpms; // 0 = tpms is stale
extern signed char car_stale_timer; // 0 = timer is stale
extern unsigned char net_reg; // Network registration
extern unsigned char net_link; // Network link status
extern unsigned char net_iccid[MAX_ICCID]; // ICCID
extern char net_apps_connected; // Network apps connected
extern char sys_features[FEATURES_MAX]; // System features
extern unsigned char net_sq; // GSM Network Signal Quality
extern unsigned char car_12vline; // 12V line level
extern unsigned char car_12vline_ref; // 12V line level reference
extern unsigned char car_gsmcops[9]; // GSM provider
extern unsigned int car_cac100; // CAC (x100)
extern signed int car_chargefull_minsremaining;  // Minutes of charge remaining
extern signed int car_chargelimit_minsremaining; // Minutes of charge remaining
extern unsigned int car_chargelimit_rangelimit;  // Range limit (in vehicle units)
extern unsigned char car_chargelimit_soclimit;   // SOC% limit
extern signed char car_coolingdown;              // >=0 if car is cooling down
extern unsigned char car_cooldown_chargemode;    // 0=standard, 1=storage, 3=range, 4=performance
extern unsigned char car_cooldown_chargelimit;   // Charge Limit (amps)
extern signed int car_cooldown_tbattery;         // Cooldown temperature limit
extern unsigned int car_cooldown_timelimit;      // Cooldown time limit (minutes) remaining
extern unsigned char car_cooldown_wascharging;   // TRUE if car was charging when cooldown started
extern int car_chargeestimate;                   // Charge minute estimate
extern unsigned char car_SOCalertlimit;          // Limit of SOC at which alert should be raised

// Helpers

#define CAR_IS_ON (car_doors1bits.CarON)
#define CAR_IS_CHARGING (car_doors1bits.Charging)
#define CAR_IS_HEATING (car_chargestate==0x0f)

// DEBUG / QA stats:
extern UINT8 debug_crashcnt;           // crash counter, cleared on normal power up
extern UINT8 debug_crashreason;        // last saved reset reason (bit set)
extern UINT8 debug_checkpoint;         // number of last checkpoint before crash
#define CHECKPOINT(n) if ((debug_crashreason & 0x80)==0) debug_checkpoint = n;

#endif
