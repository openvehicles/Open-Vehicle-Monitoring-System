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
#include "p18f2680.h"
#include "UARTIntC.h"
#include "utils.h"
#include "params.h"
#include "can.h"
#include "net.h"

#define FEATURES_MAX 16
#define FEATURES_MAP_PARAM 8
#define FEATURE_SPEEDO       0x00 // Speedometer feature
#define FEATURE_STREAM       0x08 // Location streaming feature

#pragma udata
extern unsigned int car_linevoltage; // Line Voltage
extern unsigned char car_chargecurrent; // Charge Current
extern unsigned char car_chargestate; // 1=charging, 2=top off, 4=done, 13=preparing to charge, 21-25=stopped charging
extern unsigned char car_chargemode; // 0=standard, 1=storage, 3=range, 4=performance
extern unsigned char car_charging; // 1=yes/0=no
extern unsigned char car_stopped; // 1=yes,0=no
extern unsigned char car_doors1; //
extern unsigned char car_doors2; //
extern unsigned char car_lockstate; // Lock State
extern unsigned char car_speed; // speed in defined units (mph or kph)
extern unsigned char car_SOC; // State of Charge in %
extern unsigned int car_idealrange; // Ideal Range in miles
extern unsigned int car_estrange; // Estimated Range
extern unsigned long car_time; // UTC Time
extern unsigned long car_parktime; // UTC time car was parked (or 0 if not)
extern unsigned char car_vin[18]; // VIN
extern unsigned char car_tpem; // Tpem
extern unsigned char car_tmotor; // Tmotor
extern unsigned char car_tbattery; // Tbattery
extern unsigned char car_tpms_t[4]; // TPMS temperature
extern unsigned char car_tpms_p[4]; // TPMS pressure
extern unsigned int car_trip; // ODO trip in miles /10
extern unsigned long car_odometer; //Odometer in miles /10
extern signed long car_latitude; // Raw GPS Latitude
extern signed long car_longitude; // Raw GPS Longitude
extern unsigned char net_reg; // Network registration
extern unsigned char net_link; // Network link status
extern char net_apps_connected; // Network apps connected
extern char sys_features[FEATURES_MAX]; // System features
extern unsigned char net_sq; // GSM Network Signal Quality
#endif