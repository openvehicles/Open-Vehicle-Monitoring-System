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
#include "can.h"
#include "net.h"

#define OVMS_FIRMWARE_VERSION 1,3,3

#define FEATURES_MAX 16
#define FEATURES_MAP_PARAM 8
#define FEATURE_SPEEDO       0x00 // Speedometer feature
#define FEATURE_STREAM       0x08 // Location streaming feature
#define FEATURE_MINSOC       0x09 // Minimum SOC feature
#define FEATURE_CARBITS      0x0E // Various ON/OFF features (bitmap)
#define FEATURE_CANWRITE     0x0F // CAN bus can be written to

// The FEATURE_CARBITS feature is a set of ON/OFF bits to control different
// miscelaneous aspects of the system. The following bits are defined:
#define FEATURE_CB_2008      0x01 // Set to 1 to mark the car as 2008/2009
#define FEATURE_CB_SAD_SMS   0x02 // Set to 1 to suppress "Access Denied" SMS
#define FEATURE_CB_SOUT_SMS  0x04 // Set to 1 to suppress all outbound SMS

// The FEATURE_CANWRITE feature controls the CAN mode of the OVMS system
// Leaving it 0 (the default) will put the CAN bus in LISTEN mode. It
// will not be written to and no acknowledgements will be issued on the bus.
// Defining it >0 will put the CAN bus in NORMAL mode. This will allow the
// system to co-operate with the CAN bus (eg; issuing acknowledgements) and
// also give it the ability to transmit messages on the CAN bus.
// Leaving FEATURE_CANWRITE=0 is obviously less likely to interfere with
// the vehicle normal operations.

#pragma udata
extern unsigned char ovms_firmware[3]; // Firmware version
extern unsigned int car_linevoltage; // Line Voltage
extern unsigned char car_chargecurrent; // Charge Current
extern unsigned char car_chargelimit; // Charge Limit (amps)
extern unsigned int car_chargeduration; // Charge Duration (minutes)
extern unsigned char car_chargestate; // 1=charging, 2=top off, 4=done, 13=preparing to charge, 21-25=stopped charging
extern unsigned char car_chargesubstate;
extern unsigned char car_chargemode; // 0=standard, 1=storage, 3=range, 4=performance
extern unsigned char car_charge_b4; // B4 byte of charge state
extern unsigned char car_chargekwh; // KWh of charge
extern unsigned char car_doors1; //
extern unsigned char car_doors2; //
extern unsigned char car_doors3; //
extern unsigned char car_lockstate; // Lock State
extern unsigned char car_speed; // speed in defined units (mph or kph)
extern unsigned char car_SOC; // State of Charge in %
extern unsigned int car_idealrange; // Ideal Range in miles
extern unsigned int car_estrange; // Estimated Range
extern unsigned long car_time; // UTC Time
extern unsigned long car_parktime; // UTC time car was parked (or 0 if not)
extern signed char car_ambient_temp; // Ambient Temperature (celcius)
extern unsigned char car_vin[18]; // VIN
extern unsigned char car_type[5]; // Car Type
extern signed char car_tpem; // Tpem
extern unsigned char car_tmotor; // Tmotor
extern signed char car_tbattery; // Tbattery
extern signed char car_tpms_t[4]; // TPMS temperature
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
extern char net_apps_connected; // Network apps connected
extern char sys_features[FEATURES_MAX]; // System features
extern unsigned char net_sq; // GSM Network Signal Quality
extern unsigned int car_12vline; // 12V line level
#endif