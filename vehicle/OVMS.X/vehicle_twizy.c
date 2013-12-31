/*******************************************************************************
;    Project:       Open Vehicle Monitor System
;    Date:          24 Nov 2012
;
;    Changes:
;    1.0  Initial release
;
;    1.1  2 Nov 2012 (Michael Balzer):
;           - Basic Twizy integration
;
;    1.2  9 Nov 2012 (Michael Balzer):
;           - CAN lockups fixed
;           - CAN data validation
;           - Charge+Key status from CAN 0x597 => reliable charge stop detection
;           - Range updates while charging
;           - Odometer
;           - Suppress SOC alert until CAN status valid
;
;    1.3  11 Nov 2012 (Michael Balzer):
;           - km to integer miles conversion with smaller error on re-conversion
;           - providing car_linevoltage (fix 230 V) & car_chargecurrent (fix 10 A)
;           - providing car VIN to be ready for auto provisioning
;           - FEATURE 10 >0: sufficient SOC charge monitor (percent)
;           - FEATURE 11 >0: sufficient range charge monitor (km/mi)
;           - FEATURE 12 >0: individual maximum ideal range (km/mi)
;           - chargestate=2 "topping off" when sufficiently charged or 97% SOC
;           - min SOC warning now triggers charge alert
;
;    1.4  16 Nov 2012 (Michael Balzer):
;           - Crash reset hardening w/o eating up EEPROM space by using SRAM
;           - Interrupt optimization
;
;    1.5  18 Nov 2012 (Michael Balzer):
;           - SMS cmd "STAT": moved specific output to vehicle module
;           - SMS cmd "HELP": adds twizy commands to std help
;           - SMS cmd "RANGE": set/query max ideal range
;           - SMS cmd "CA": set/query charge alerts
;           - SMS cmd "DEBUG": output internal state variables
;
;    1.6  24 Nov 2012 (Michael Balzer):
;           - unified charge alerts for MSG/SMS
;           - MSG integration for all commands (see capabilities)
;           - code cleanup & design pattern documentation
;
;    2.0  1 Dec 2012 (Michael Balzer):
;           - PEM and motor temperatures
;           - Battery cell temperatures: 7 cell modules of 2 cells each
;             (overall battery temperature = average of modules)
;           - Battery pack and cell voltages
;           - Battery monitoring system:
;             - history of min/max/max deviation during usage cycle
;             - ...tagging of suspicious cells (dev > std dev)
;             - ...tagging of cell alerts (dev > 2 * std dev)
;               (thresholds may need refinement)
;             - ...sends SMS+MSG on alert state change
;           - First draft of battery state data MSG protocol extension
;             (to be discussed on the developer list)
;           - Development/debug utility: CAN simulator w/ battery data
;             sim data can be injected any time by issuing the DEBUG command
;             with desired sim data chunk number (i.e. SMS "DEBUG 12" / MSG "200,12")
;           - Minor code cleanups & bug fixes
;
;    2.1  8 Dec 2012 (Michael Balzer):
;           - temperature function updated to new formula (A - 40)
;           - cell alert thresholds changed to fixed absolute deviations
;             3 °C temperature / 100 mV  (watch thresholds = stddev as before)
;           - battery monitor reset now bound to "key on" event
;             instead of full charge cycle
;           - cell mean value / deviation calculations now done with rounding
;           - battery text alert now also sent by MSG protocol ("PA")
;             and triggered immediately after every alert status change
;           - Twizy battery monitor compiler switch: OVMS_TWIZY_BATTMON
;             current ressource usage: 6% RAM (193 byte) + 10% ROM
;
;    2.2  16 Dec 2012 (Michael Balzer):
;           - Update MSG protocol for battery monitor to new "History" message type
;             using types 'PWR-BattPack' & 'PWR-BattCell'
;           - New function: now gathers power usage statistics. MSG command 206 /
;             SMS command "POWER" outputs statistics, MSG notify also done after
;             each power off. Statistics cover power usage in Wh for constant drive,
;             acceleration & deceleration + percentages each.
;           - Minor cleanups & fixes.
;
;    2.3  20 Dec 2012 (Michael Balzer):
;           - Replaced all sprintf() calls by new stp_* utils (to reduce stack usage)
;           - Bug fixes on power usage statistics
;
;    2.4  30 Dec 2012 (Michael Balzer):
;           - Added charge time estimation for commands STAT? and CA?
;           - Added GPS track logging using history MSG "RT-GPS-Log" (with STREAM support)
;           - Moved regular data updates to notification function
;           - Moved notifications to vehicle_poll() hook
;           - Moved common macros to common includes
;           - Source code reformatting in style ANSI / indent 2
;
;    2.5  1 Jan 2013 (Michael Balzer):
;           - Split up battery MSG and separated from DataUpdate (due to size)
;             (incl. implementation of MSG mode battery alerts)
;           - "CA?" now adds time estimation for full charge in any case
;
;    2.6  13 Jan 2013 (Michael Balzer):
;           - Renamed local Twizy variables to avoid name space conflicts
;           - Battery monitor extended by stddev alert mode
;           - Power stats extended by distance / efficiency
;           - Minor bug fixes & changes
;           Note: introduced second overlay section "vehicle_overlay_data2"
;
;    2.6.2  2 Feb 2013 (Michael Balzer):
;           - Power line plug-in detection (for 12V alert system)
;           - Pilot signal (able to charge) set from power line detection
;           - Support for car_time & car_parktime
;
;    2.6.3  29 Mar 2013 (Michael Balzer):
;           - Bug fix and acceleration of battery monitor sensor fetching
;
;    2.6.4  12 Apr 2013 (Michael Balzer):
;           - RT-GPS-Log extended by power data
;           - Fixed a bug with distance measurement (wrong power efficiency)
;           - Power statistics just show power sums if not driven
;           - Rounding of car_speed
;
;    2.6.5  7 May 2013 (Michael Balzer):
;           - FEATURE_STREAM changed to bit field: 2 = GPS log stream
;
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
#include <delays.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "ovms.h"
#include "params.h"
#include "led.h"
#include "utils.h"
#include "net_sms.h"
#include "net_msg.h"


/***************************************************************
 * Twizy definitions
 */

// Twizy specific features:
#define FEATURE_SUFFSOC      0x0A // Sufficient SOC
#define FEATURE_SUFFRANGE    0x0B // Sufficient range
#define FEATURE_MAXRANGE     0x0C // Maximum ideal range

// Twizy specific commands:
#define CMD_Debug                   200 // ()
#define CMD_QueryRange              201 // ()
#define CMD_SetRange                202 // (maxrange)
#define CMD_QueryChargeAlerts       203 // ()
#define CMD_SetChargeAlerts         204 // (range, soc)
#define CMD_BatteryAlert            205 // ()
#define CMD_BatteryStatus           206 // ()
#define CMD_PowerUsageNotify        207 // (mode)
#define CMD_PowerUsageStats         208 // ()

// Twizy module version & capabilities:
rom char vehicle_twizy_version[] = "3.0.1";

#ifdef OVMS_TWIZY_BATTMON
rom char vehicle_twizy_capabilities[] = "C6,C200-208";
#else
rom char vehicle_twizy_capabilities[] = "C6,C200-204,207-208";
#endif // OVMS_TWIZY_BATTMON


/***************************************************************
 * Twizy data types
 */

#ifdef OVMS_TWIZY_BATTMON

typedef struct battery_pack
{
  UINT volt_act; // current voltage in 1/10 V
  UINT volt_min; // charge cycle min voltage
  UINT volt_max; // charge cycle max voltage

  UINT volt_watches; // bitfield: dev > stddev
  UINT volt_alerts; // bitfield: dev > BATT_DEV_VOLT_ALERT
  UINT last_volt_alerts; // recognize alert state changes

  UINT8 temp_watches; // bitfield: dev > stddev
  UINT8 temp_alerts; // bitfield: dev > BATT_DEV_TEMP_ALERT
  UINT8 last_temp_alerts; // recognize alert state changes

  UINT cell_volt_stddev_max; // max cell voltage std deviation
  // => watch/alert bit #15 (1<<15)
  UINT8 cmod_temp_stddev_max; // max cmod temperature std deviation
  // => watch/alert bit #7 (1<<7)

} battery_pack;

typedef struct battery_cmod // cell module
{
  UINT8 temp_act; // current temperature in °F
  UINT8 temp_min; // charge cycle min temperature
  UINT8 temp_max; // charge cycle max temperature
  INT8 temp_maxdev; // charge cycle max temperature deviation

} battery_cmod;

typedef struct battery_cell
{
  UINT volt_act; // current voltage in 1/200 V
  UINT volt_min; // charge cycle min voltage
  UINT volt_max; // charge cycle max voltage
  INT volt_maxdev; // charge cycle max voltage deviation

  // NOTE: these might be compressable to UINT8
  //  if limiting the range to e.g. 2.5 .. 4.2 V
  //  => 1.7 V / 256 = ~ 1/150 V = should still be precise enough...

} battery_cell;

#endif // OVMS_TWIZY_BATTMON


typedef struct speedpwr // power usage statistics for accel/decel
{
  unsigned long dist; // distance sum in 1/10 m
  unsigned long use; // sum of power used
  unsigned long rec; // sum of power recovered (recuperation)

} speedpwr;

typedef struct levelpwr // power usage statistics for level up/down
{
  unsigned long dist; // distance sum in 1 m
  unsigned int hsum; // height sum in 1 m
  unsigned long use; // sum of power used
  unsigned long rec; // sum of power recovered (recuperation)

} levelpwr;


/***************************************************************
 * Twizy state variables
 */

#pragma udata overlay vehicle_overlay_data

unsigned char twizy_last_SOC; // sufficient charge SOC threshold helper
unsigned int twizy_last_idealrange; // sufficient charge range threshold helper

unsigned int twizy_soc; // detailed SOC (1/100 %)
unsigned int twizy_soc_min; // min SOC reached during last discharge
unsigned int twizy_soc_min_range; // twizy_range at min SOC
unsigned int twizy_soc_max; // max SOC reached during last charge

unsigned int twizy_range; // range in km
unsigned int twizy_speed; // current speed in 1/100 km/h
unsigned long twizy_odometer; // odometer in 1/100 km = 10 m
volatile unsigned int twizy_dist; // cyclic distance counter in 1/10 m = 10 cm

signed int twizy_power; // current power in 16*W, negative=charging

unsigned char twizy_status; // Car + charge status from CAN:
#define CAN_STATUS_KEYON        0x10        //  bit 4 = 0x10: 1 = Car ON (key turned)
#define CAN_STATUS_CHARGING     0x20        //  bit 5 = 0x20: 1 = Charging
#define CAN_STATUS_OFFLINE      0x40        //  bit 6 = 0x40: 1 = Switch-ON/-OFF phase / 0 = normal operation
#define CAN_STATUS_ONLINE       0x80        //  bit 7 = 0x80: 1 = CAN-Bus online (test flag to detect offline)

volatile unsigned char twizy_button_cnt; // will count key presses in STOP mode (msg 081)

// MSG notification queue (like net_notify for vehicle specific notifies)
volatile UINT8 twizy_notify; // bit set of...
#define SEND_BatteryAlert           0x01  // text alert: battery problem
#define SEND_PowerNotify            0x02  // text alert: power usage summary
#define SEND_DataUpdate             0x04  // regular data update (per minute)
#define SEND_StreamUpdate           0x08  // stream data update (per second)
#define SEND_BatteryStats           0x10  // separate battery stats (large)


// -----------------------------------------------
// RAM USAGE FOR STD VARS: 25 bytes (w/o DIAG)
// + 1 static CRC WORDS = 2 bytes
// = TOTAL: 27 bytes
// -----------------------------------------------


#ifdef OVMS_DIAGMODULE
volatile int twizy_sim = -1; // CAN simulator: -1=off, else read index in twizy_sim_data
#endif // OVMS_DIAGMODULE


/***************************************************************
 * Twizy POWER STATISTICS variables
 */

speedpwr twizy_speedpwr[3]; // speed power usage statistics
UINT8 twizy_speed_state; // speed state, one of:
#define CAN_SPEED_CONST         0           // constant speed
#define CAN_SPEED_ACCEL         1           // accelerating
#define CAN_SPEED_DECEL         2           // decelerating

#define CAN_SPEED_THRESHOLD     10          // speed change threshold for accel/decel

volatile unsigned int twizy_speed_distref; // distance reference for speed section


levelpwr twizy_levelpwr[2]; // level power usage statistics
#define CAN_LEVEL_UP            0           // uphill
#define CAN_LEVEL_DOWN          1           // downhill

unsigned long twizy_level_odo; // level section odometer reference
signed int twizy_level_alt; // level section altitude reference
volatile unsigned long twizy_level_use; // level section use collector
volatile unsigned long twizy_level_rec; // level section rec collector
#define CAN_LEVEL_MINSECTLEN    100         // min section length (in m)
#define CAN_LEVEL_THRESHOLD     1           // level change threshold (in percent)

// -----------------------------------------------
// TOTAL RAM USAGE FOR POWER STATS: 81 bytes
// + 1 static CRC WORDS = 2 bytes
// = TOTAL: 83 bytes
// -----------------------------------------------


/***************************************************************
 * Twizy BATTERY MONITORING variables
 */

#ifdef OVMS_TWIZY_BATTMON

// put the battmon vars into a separate section to prevent
// C18 "cannot fit the section" linker bug:
#pragma udata overlay vehicle_overlay_data2

#define BATT_PACKS      1
#define BATT_CELLS      14
#define BATT_CMODS      7
battery_pack twizy_batt[BATT_PACKS]; // size:  1 * 18 =  18 bytes
battery_cmod twizy_cmod[BATT_CMODS]; // size:  7 *  4 =  28 bytes
battery_cell twizy_cell[BATT_CELLS]; // size: 14 *  8 = 112 bytes
// ------------- = 158 bytes

// Battery cell/cmod deviation alert thresholds:
#define BATT_DEV_TEMP_ALERT         3       // = 3 °C
#define BATT_DEV_VOLT_ALERT         6       // = 30 mV

// ...thresholds for overall stddev:
#define BATT_STDDEV_TEMP_WATCH      2       // = 2 °C
#define BATT_STDDEV_TEMP_ALERT      3       // = 3 °C
#define BATT_STDDEV_VOLT_WATCH      3       // = 15 mV
#define BATT_STDDEV_VOLT_ALERT      5       // = 25 mV

// watch/alert flags for overall stddev:
#define BATT_STDDEV_TEMP_FLAG       0x80    // bit #7
#define BATT_STDDEV_VOLT_FLAG       0x8000  // bit #15


// STATE MACHINE: twizy_batt_sensors_state
//  A consistent state needs all 5 battery sensor messages
//  of one series (i.e. 554-6-7-E-F) to be read.
//  state=BATT_SENSORS_START begins a new fetch cycle.
//  poll1() will advance/reset states accordingly to incoming msgs.
//  ticker1() will not process the data until BATT_SENSORS_READY
//  has been reached, after processing it will reset state to _START.
volatile UINT8 twizy_batt_sensors_state;
#define BATT_SENSORS_START          0   // start group fetch
#define BATT_SENSORS_GOT556         3   // mask: lowest 2 bits (state)
#define BATT_SENSORS_GOT554         4   // bit
#define BATT_SENSORS_GOT557         8   // bit
#define BATT_SENSORS_GOT55E         16  // bit
#define BATT_SENSORS_GOT55F         32  // bit
#define BATT_SENSORS_GOTALL         61  // threshold: data complete
#define BATT_SENSORS_READY          63  // value: group complete


// -------------------------------------------------
// TOTAL RAM USAGE FOR BATTERY MONITOR: 159 bytes
// + 16 static CRC WORDS = 32 bytes
// = TOTAL: 191 bytes
// -------------------------------------------------

#pragma udata overlay vehicle_overlay_data

#endif // OVMS_TWIZY_BATTMON



/***************************************************************
 * Twizy CANopen SDO communication variables
 */

// SDO request:
volatile UINT8    twizy_sdo_control;      // control/status/command
volatile UINT     twizy_sdo_index;        // index
volatile UINT8    twizy_sdo_subindex;     // subindex
volatile UINT32   twizy_sdo_data;         // payload

/* NOTE:
 * the SDO comm could become common functionality by
 * adding the node id (fixed #1 for the Twizy SEVCON)
 */

// Convenience:
#define readsdo       vehicle_twizy_readsdo
#define writesdo      vehicle_twizy_writesdo
#define login         vehicle_twizy_login
#define configmode    vehicle_twizy_configmode


// I/O level CAN error codes / classes:

#define ERR_NoCANWrite          0x0001

#define ERR_ReadSDO             0x0002
#define ERR_ReadSDO_Timeout     0x0003
#define ERR_ReadSDO_SegXfer     0x0004

#define ERR_WriteSDO            0x0008
#define ERR_WriteSDO_Timeout    0x0009


// App level CAN error codes / classes:

#define ERR_LoginFailed         0x0010
#define ERR_CfgModeFailed       0x0020
#define ERR_Range               0x0030

// check if SDO may contain relevant error info:
#define ERROR_IN_SDO(c) (((c) & 0xfff0) != ERR_Range)



//#pragma udata   // return to default udata section -- why?


/***************************************************************
 * Twizy functions
 */

BOOL vehicle_twizy_poll0(void);
BOOL vehicle_twizy_poll1(void);

void vehicle_twizy_power_reset(void);
void vehicle_twizy_power_collect(void);
char vehicle_twizy_power_msgp(char stat, int cmd);
BOOL vehicle_twizy_power_cmd(BOOL msgmode, int cmd, char *arguments);
BOOL vehicle_twizy_power_sms(BOOL premsg, char *caller, char *command, char *arguments);

#ifdef OVMS_TWIZY_BATTMON
void vehicle_twizy_battstatus_reset(void);
void vehicle_twizy_battstatus_collect(void);
char vehicle_twizy_battstatus_msgp(char stat, int cmd);
BOOL vehicle_twizy_battstatus_cmd(BOOL msgmode, int cmd, char *arguments);
BOOL
vehicle_twizy_battstatus_sms(BOOL premsg, char *caller, char *command, char *arguments);
#endif // OVMS_TWIZY_BATTMON



/***************************************************************
 * Twizy / CANopen SDO utilities
 */

void vehicle_twizy_sendsdoreq(void)
{
  // wait for previous async send (if any) to finish:
  while (TXB0CONbits.TXREQ) {}

  // try to prevent bus flooding:
  delay5b();

  // init transmit:
  TXB0CON = 0;

  // dest ID 0x601 (SEVCON):
  TXB0SIDH = 0b11000000;
  TXB0SIDL = 0b00100000;

  // fill in from twizy_sdo:
  TXB0D0 = twizy_sdo_control;
  TXB0D1 = (twizy_sdo_index) & 0xff;
  TXB0D2 = (twizy_sdo_index >> 8) & 0xff;
  TXB0D3 = twizy_sdo_subindex;
  TXB0D4 = (twizy_sdo_data) & 0xff;
  TXB0D5 = (twizy_sdo_data >> 8) & 0xff;
  TXB0D6 = (twizy_sdo_data >> 16) & 0xff;
  TXB0D7 = (twizy_sdo_data >> 24) & 0xff;

  // data length (8):
  TXB0DLC = 0b00001000;

  // clear status to detect reply:
  twizy_sdo_control = 0;

  // send:
  TXB0CON = 0b00001000;
  while (TXB0CONbits.TXREQ) {}
}


// read from SDO:
UINT vehicle_twizy_readsdo(UINT index, UINT8 subindex)
{
  UINT8 timeout;

  if (sys_features[FEATURE_CANWRITE] == 0)
    return ERR_NoCANWrite;

  // SDO init upload request:
  twizy_sdo_control = 0b01000000;
  twizy_sdo_index = index;
  twizy_sdo_subindex = subindex;
  vehicle_twizy_sendsdoreq();

  // wait for reply:
  timeout = 400; // ~2000 ms
  do {
    delay5b();
  } while (twizy_sdo_control == 0 && --timeout);

  if (timeout == 0) {
    // timeout, abort request:
    twizy_sdo_control = 0b10000000; // abort
    twizy_sdo_data = 0x05040000; // protocol timed out
    vehicle_twizy_sendsdoreq();
    return ERR_ReadSDO_Timeout;
  }
  
  // check reply status:
  if ((twizy_sdo_control & 0b11100000) != 0b01000000) {
    // check for CANopen general error:
    if (twizy_sdo_data == 0x08000000) {
      // add SEVCON error code:
      timeout = twizy_sdo_control;
      readsdo(0x5310,0x00);
      twizy_sdo_control = timeout;
      twizy_sdo_index = index;
      twizy_sdo_subindex = subindex;
      twizy_sdo_data |= 0x08000000;
    }
    return ERR_ReadSDO;
  }

  // check if segmented xfer necessary (not supported yet):
  if ((twizy_sdo_control & 0b00000010) != 0b00000010) {
    // abort request:
    twizy_sdo_control = 0b10000000; // abort
    twizy_sdo_data = 0x05040005; // out of memory (for now)
    vehicle_twizy_sendsdoreq();
    return ERR_ReadSDO_SegXfer;
  }

  return 0;
}


// write to SDO without size indication:
UINT vehicle_twizy_writesdo(UINT index, UINT8 subindex, UINT32 data)
{
  UINT timeout;

  if (sys_features[FEATURE_CANWRITE] == 0)
    return ERR_NoCANWrite;

  // SDO init download request:
  twizy_sdo_control = 0b00100010; // no size needed, server is smart
  twizy_sdo_index = index;
  twizy_sdo_subindex = subindex;
  twizy_sdo_data = data;
  vehicle_twizy_sendsdoreq();

  // wait for reply:
  timeout = 400; // ~2000 ms
  do {
    delay5b();
  } while (twizy_sdo_control == 0 && --timeout);

  if (timeout == 0) {
    // timeout, abort request:
    twizy_sdo_control = 0b10000000; // abort
    twizy_sdo_data = 0x05040000; // protocol timed out
    vehicle_twizy_sendsdoreq();
    return ERR_WriteSDO_Timeout;
  }

  // check reply status:
  if ((twizy_sdo_control & 0b11100000) != 0b01100000) {
    // check for CANopen general error:
    if (twizy_sdo_data == 0x08000000) {
      // add SEVCON error code:
      timeout = twizy_sdo_control;
      readsdo(0x5310,0x00);
      twizy_sdo_control = timeout;
      twizy_sdo_index = index;
      twizy_sdo_subindex = subindex;
      twizy_sdo_data |= 0x08000000;
    }
    return ERR_WriteSDO;
  }

  return 0;
}


UINT vehicle_twizy_login(BOOL on)
{
  UINT err;

  // check login level:
  if (err = readsdo(0x5000,1))
    return ERR_LoginFailed + err;

  if (on && twizy_sdo_data != 4) {
    // login:
    writesdo(0x5000,3,0);
    if (err = writesdo(0x5000,2,0x4bdf))
      return ERR_LoginFailed + err;

    // check new level:
    if (err = readsdo(0x5000,1))
      return ERR_LoginFailed + err;
    if (twizy_sdo_data != 4)
      return ERR_LoginFailed;
  }

  else if (!on && twizy_sdo_data != 0) {
    // logout:
    writesdo(0x5000,3,0);
    if (err = writesdo(0x5000,2,0))
      return ERR_LoginFailed + err;

    // check new level:
    if (err = readsdo(0x5000,1))
      return ERR_LoginFailed + err;
    if (twizy_sdo_data != 0)
      return ERR_LoginFailed;
  }

  return 0;
}


UINT vehicle_twizy_configmode(BOOL on)
{
  UINT err, cnt;

  // login if necessary:
  if (err = login(1))
    return err;

  ClrWdt();

  if (!on) {

    // request operational state:
    if (err = writesdo(0x2800,0,0))
      return ERR_CfgModeFailed + err;

    // no need to check the new state if the write succeeded,
    // the SEVCON will go operational ASAP
  }
  
  else {

    // check controller status:
    if (err = readsdo(0x5110,0))
      return ERR_CfgModeFailed + err;

    if (twizy_sdo_data != 127) {

      // request preoperational state:
      if (err = writesdo(0x2800,0,1))
        return ERR_CfgModeFailed + err;

      // give controller some time:
      delay5(2);

      // check new status:
      if (err = readsdo(0x5110,0))
        return ERR_CfgModeFailed + err;

      if (twizy_sdo_data != 127) {
        // reset preop state request:
        if (err = writesdo(0x2800,0,0))
          return ERR_CfgModeFailed + err;
        return ERR_CfgModeFailed;
      }
    }
  }
  
  return 0;
}


/***********************************************************************
 * COMMAND CLASS: SEVCON CONTROLLER CONFIGURATION
 *
 *  MSG: ...todo...
 *  SMS: CFG [cmd]
 *
 */

char *vehicle_twizy_fmt_sdo(char *s)
{
  s = stp_rom(s, " SDO ");
  if ((twizy_sdo_control & 0b11100000) == 0b10000000)
    s = stp_rom(s, "ABORT ");
  s = stp_x(s, "0x", twizy_sdo_index);
  s = stp_sx(s, ".", twizy_sdo_subindex);
  if (twizy_sdo_data > 0x0ffff)
    s = stp_lx(s, ": 0x", twizy_sdo_data);
  else
    s = stp_x(s, ": 0x", twizy_sdo_data);
  return s;
}


UINT32 scale(UINT32 deflt, UINT32 from, UINT32 to, UINT32 min, UINT32 max)
{
  UINT32 val;

  if (to == from)
    return deflt;

  val = (deflt * to) / from;

  if (val < min)
    return min;
  else if (val > max)
    return max;
  else
    return val;
}


UINT vehicle_twizy_cfg_speed(int max_kph, int warn_kph)
// max_kph: 6..111, -1=reset to default (80)
// warn_kph: 6..111, -1=reset to default (89)
{
  UINT err;
  UINT32 peaktrq;
  UINT32 rpm, rpm2;

  // parameter validation:

  if (max_kph == -1)
    max_kph = 80;
  else if (max_kph < 6 || max_kph > 111)
    return ERR_Range + 1;

  if (warn_kph == -1)
    warn_kph = 89;
  else if (warn_kph < 6 || warn_kph > 111)
    return ERR_Range + 2;

  // we need pre-op state for the map manipulation (0x4611):
  if (err = configmode(1))
    return err;

  // get peak torque (for map scaling):
  if (err = readsdo(0x6076,0x00))
    return err;
  peaktrq = twizy_sdo_data;

  // references:
  //    7250 rpm = default max speed = ~80 kph
  //    8050 rpm = default overspeed warning trigger (STOP lamp ON) = ~89 kph
  //    8500 rpm = default overspeed brakedown trigger = ~94 kph
  //    10000 rpm = max neutral speed (0x3813.2d) = ~110 kph
  //    11000 rpm = severe overspeed fault (0x4624.00) = ~121 kph

  // calc fwd rpm:
  rpm = scale(7250,80,max_kph,400,10000);

  // set fwd rpm:
  if (err = writesdo(0x2920,0x05,rpm))
    return err;

  // adjust overspeed braking points (using fixed offsets):
  if (err = writesdo(0x3813,0x33,rpm+400)) // neutral braking start
    return err;
  if (err = writesdo(0x3813,0x35,rpm+800)) // neutral braking end
    return err;
  if (err = writesdo(0x3813,0x3b,MAX(rpm+1250,10900))) // drive brakedown trigger
    return err;

  // set overspeed warning range (STOP lamp):
  rpm2 = scale(8050,89,warn_kph,400,10900);
  if (err = writesdo(0x3813,0x34,rpm2)) // lamp ON
    return err;
  if (err = writesdo(0x3813,0x3c,rpm2-550)) // lamp OFF
    return err;

  // spread torque/speed map:

#if 0
  // is scaling all rpms any good? max points should be sufficient...
  // or just keep 2115 rpm (rated speed) and scale above?

  if (err = writesdo(0x4611,0x04,scale(2115,80,max_kph,400,10000)))
    return err;
  if (err = writesdo(0x4611,0x06,scale(2700,80,max_kph,400,10000)))
    return err;
  if (err = writesdo(0x4611,0x08,scale(3000,80,max_kph,400,10000)))
    return err;
  if (err = writesdo(0x4611,0x0a,scale(3500,80,max_kph,400,10000)))
    return err;
  if (err = writesdo(0x4611,0x0c,scale(4500,80,max_kph,400,10000)))
    return err;
  if (err = writesdo(0x4611,0x0e,scale(5500,80,max_kph,400,10000)))
    return err;
  if (err = writesdo(0x4611,0x10,scale(6500,80,max_kph,400,10000)))
    return err;
#endif

  // end speed minimum 7250:
  rpm2 = MAX(rpm, 7250);
  if (err = writesdo(0x4611,0x12,rpm2))
    return err;

  // end torque needs to be scaled up by peaktrq and down by fwdrpm:
  if (err = writesdo(0x4611,0x11,
    scale(scale(273,55000,peaktrq,160,1122),
      rpm2,7250,160,1122)))
    return err;

  // commit map changes:
  if (err = writesdo(0x4641,0x01,1))
    return err;
  delay5(10);

  return 0;
}


UINT vehicle_twizy_cfg_torque(int max_prc)
// max_prc: 10..100, -1=reset to default (78%)
//    100% = 70.125 Nm
{
  UINT err;
  UINT32 peaktrq;
  UINT32 fwdrpm;

  // parameter validation:

  if (max_prc == -1)
    max_prc = 78;
  else if (max_prc < 10 || max_prc > 100)
    return ERR_Range + 1;

  // we need pre-op state for the map manipulation (0x4611):
  if (err = configmode(1))
    return err;

  // get map end point rpm:
  if (err = readsdo(0x4611,0x12))
    return err;
  fwdrpm = twizy_sdo_data;

  // references:
  //    55 Nm (0x6076.0x00) = default peak torque to use (also in tq/speed map)
  //    57 Nm (0x2916.0x01) = ??? should be equal to 0x6076...
  //    70.125 Nm (0x4610.0x11) = max motor torque according to flux map

  // calc peak use torque:
  peaktrq = scale(55000,78,max_prc,10000,70125);

  // set peak use torque:
  if (err = writesdo(0x6076,0x00,peaktrq))
    return err;

  // set rated torque too?
  //if (err = writesdo(0x2916,0x01,scale(57000,78,max_prc,10000,70125)))
  //  return err;

  // scale torque/speed map:
  if (err = writesdo(0x4611,0x01,scale(880,78,max_prc,160,1122)))
    return err;
  if (err = writesdo(0x4611,0x03,scale(880,78,max_prc,160,1122)))
    return err;
  if (err = writesdo(0x4611,0x05,scale(659,78,max_prc,160,1122)))
    return err;
  if (err = writesdo(0x4611,0x07,scale(608,78,max_prc,160,1122)))
    return err;
  if (err = writesdo(0x4611,0x09,scale(516,78,max_prc,160,1122)))
    return err;
  if (err = writesdo(0x4611,0x0b,scale(421,78,max_prc,160,1122)))
    return err;
  if (err = writesdo(0x4611,0x0d,scale(360,78,max_prc,160,1122)))
    return err;
  if (err = writesdo(0x4611,0x0f,scale(307,78,max_prc,160,1122)))
    return err;

  // end torque needs to be scaled up by peaktrq and down by fwdrpm:
  if (err = writesdo(0x4611,0x11,
    scale(scale(273,55000,peaktrq,160,1122),
      fwdrpm,7250,160,1122)))
    return err;

  // commit map changes:
  if (err = writesdo(0x4641,0x01,1))
    return err;
  delay5(10);

  return 0;
}


UINT vehicle_twizy_cfg_tsmap(char map, int t1_prc, int t2_prc, int t3_prc, int t4_prc)
// map: 'D'=Drive 'N'=Neutral 'B'=Footbrake
// t1_prc: 1..100, -1=reset to default (D=100, N/B=100)
// t2_prc: 1..100, -1=reset to default (D=100, N/B=80)
// t3_prc: 1..100, -1=reset to default (D=100, N/B=50)
// t4_prc: 1..100, -1=reset to default (D=100, N/B=20)
{
  UINT err;
  UINT base;
  UINT32 val;

  // parameter validation:

  if (map != 'D' && map != 'N' && map != 'B')
    return ERR_Range + 1;

  if ((t1_prc != -1) && (t1_prc < 1 || t1_prc > 100))
    return ERR_Range + 2;

  if ((t2_prc != -1) && (t2_prc < 1 || t2_prc > 100))
    return ERR_Range + 3;

  if ((t3_prc != -1) && (t3_prc < 1 || t3_prc > 100))
    return ERR_Range + 4;

  if ((t4_prc != -1) && (t4_prc < 1 || t4_prc > 100))
    return ERR_Range + 5;

  // we need pre-op state:
  if (err = configmode(1))
    return err;

  // get map base subindex in SDO 0x3813:

  if (map=='B')
    base = 0x07;
  else if (map=='N')
    base = 0x1b;
  else // 'D'
    base = 0x24;

  // set:

  if (t1_prc >= 0)
    val = scale(32767,100,t1_prc,0,32767);
  else
    val = 32767;
  if (err = writesdo(0x3813,base+0,val))
    return err;

  if (t2_prc >= 0)
    val = scale(32767,100,t2_prc,0,32767);
  else
    val = (map=='D') ? 32767 : 26214;
  if (err = writesdo(0x3813,base+2,val))
    return err;

  if (t3_prc >= 0)
    val = scale(32767,100,t3_prc,0,32767);
  else
    val = (map=='D') ? 32767 : 16383;
  if (err = writesdo(0x3813,base+4,val))
    return err;

  if (t4_prc >= 0)
    val = scale(32767,100,t4_prc,0,32767);
  else
    val = (map=='D') ? 32767 : 6553;
  if (err = writesdo(0x3813,base+6,val))
    return err;

  return 0;
}


UINT vehicle_twizy_cfg_power(int max_prc)
// max_prc: 10..100, -1=reset to default (100)
{
  UINT err;

  // parameter validation:

  if (max_prc == -1)
    max_prc = 100;
  else if (max_prc < 10 || max_prc > 100)
    return ERR_Range + 1;

  // set:
  if (err = writesdo(0x2920,0x01,max_prc*10))
    return err;

  return 0;
}


UINT vehicle_twizy_cfg_recup(int neutral_prc, int brake_prc)
// neutral_prc: 0..100, -1=reset to default (18)
// brake_prc: 0..100, -1=reset to default (18)
{
  UINT err;

  // parameter validation:

  if (neutral_prc == -1)
    neutral_prc = 18;
  else if (neutral_prc < 0 || neutral_prc > 100)
    return ERR_Range + 1;

  if (brake_prc == -1)
    brake_prc = 18;
  else if (brake_prc < 0 || brake_prc > 100)
    return ERR_Range + 2;

  // set:
  if (err = writesdo(0x2920,0x03,scale(182,18,neutral_prc,0,1000)))
    return err;
  if (err = writesdo(0x2920,0x04,scale(182,18,brake_prc,0,1000)))
    return err;

  return 0;
}


UINT vehicle_twizy_cfg_ramps(int start_prc, int accel_prc, int decel_prc, int neutral_prc, int brake_prc)
// start_prc: 1..100, -1=reset to default (4)
// accel_prc: 1..100, -1=reset to default (25)
// decel_prc: 0..100, -1=reset to default (20)
// neutral_prc: 0..100, -1=reset to default (40)
// brake_prc: 0..100, -1=reset to default (40)
{
  UINT err;

  // parameter validation:

  if (start_prc == -1)
    start_prc = 4;
  else if (start_prc < 1 || start_prc > 100)
    return ERR_Range + 1;

  if (accel_prc == -1)
    accel_prc = 25;
  else if (accel_prc < 1 || accel_prc > 100)
    return ERR_Range + 2;

  if (decel_prc == -1)
    decel_prc = 20;
  else if (decel_prc < 0 || decel_prc > 100)
    return ERR_Range + 3;

  if (neutral_prc == -1)
    neutral_prc = 40;
  else if (neutral_prc < 0 || neutral_prc > 100)
    return ERR_Range + 4;

  if (brake_prc == -1)
    brake_prc = 40;
  else if (brake_prc < 0 || brake_prc > 100)
    return ERR_Range + 5;

  // set:

  if (err = writesdo(0x291c,0x02,scale(400,4,start_prc,10,10000)))
    return err;
  if (err = writesdo(0x2920,0x07,scale(2500,25,accel_prc,10,10000)))
    return err;
  if (err = writesdo(0x2920,0x0b,scale(2000,20,decel_prc,10,10000)))
    return err;
  if (err = writesdo(0x2920,0x0d,scale(4000,40,neutral_prc,10,10000)))
    return err;
  if (err = writesdo(0x2920,0x0e,scale(4000,40,brake_prc,10,10000)))
    return err;

  return 0;
}


UINT vehicle_twizy_cfg_smoothing(int prc)
// prc: 0..100, -1=reset to default (70)
{
  UINT err;

  // parameter validation:

  if (prc == -1)
    prc = 70;
  else if (prc < 0 || prc > 100)
    return ERR_Range + 1;

  // set:
  if (err = writesdo(0x290a,0x01,1+(prc/10)))
    return err;
  if (err = writesdo(0x290a,0x03,scale(800,70,prc,0,1000)))
    return err;

  return 0;
}


UINT vehicle_twizy_cfg_brakelight(int on_lev, int off_lev)
// *** NOT FUNCTIONAL WITHOUT HARDWARE MODIFICATION ***
// *** SEVCON cannot control Twizy brake lights ***
// on_lev: 0..100, -1=reset to default (100=off)
// off_lev: 0..100, -1=reset to default (100=off)
// on_lev must be >= off_lev
// ctrl bit in 0x2910.1 will be set/cleared accordingly
{
  UINT err;

  // parameter validation:

  if (on_lev == -1)
    on_lev = 100;
  else if (on_lev < 0 || on_lev > 100)
    return ERR_Range + 1;

  if (off_lev == -1)
    off_lev = 100;
  else if (off_lev < 0 || off_lev > 100)
    return ERR_Range + 2;

  if (on_lev < off_lev)
    return ERR_Range + 3;

  // we need pre-op state for manipulating 0x2910 (ctrl bit):
  if (err = configmode(1))
    return err;

  // set range:
  if (err = writesdo(0x3813,0x05,scale(1024,100,off_lev,64,1024)))
    return err;
  if (err = writesdo(0x3813,0x06,scale(1024,100,on_lev,64,1024)))
    return err;

  // set ctrl bit:
  if (err = readsdo(0x2910,0x01))
    return err;
  if (on_lev != 100 || off_lev != 100)
    twizy_sdo_data |= 0x2000;
  else
    twizy_sdo_data &= ~0x2000;
  if (err = writesdo(0x2910,0x01,twizy_sdo_data))
    return err;

  return 0;
}


// RESET all known parameters to default:
//  ATT: will not reset arbitrary WRITEs, just macro commands!
UINT vehicle_twizy_cfg_reset(void)
{
  UINT err;

  // clear watchdog timer:
  ClrWdt();

  // issue reset on every macro command:
  if (err = vehicle_twizy_cfg_speed(-1,-1))
    return err;
  if (err = vehicle_twizy_cfg_torque(-1))
    return err;
  if (err = vehicle_twizy_cfg_tsmap('D',-1,-1,-1,-1))
    return err;
  if (err = vehicle_twizy_cfg_tsmap('N',-1,-1,-1,-1))
    return err;
  if (err = vehicle_twizy_cfg_tsmap('B',-1,-1,-1,-1))
    return err;

  ClrWdt();

  if (err = vehicle_twizy_cfg_power(-1))
    return err;
  if (err = vehicle_twizy_cfg_recup(-1,-1))
    return err;
  if (err = vehicle_twizy_cfg_ramps(-1,-1,-1,-1,-1))
    return err;
  if (err = vehicle_twizy_cfg_smoothing(-1))
    return err;
  if (err = vehicle_twizy_cfg_brakelight(-1,-1))
    return err;

  return 0;
}


BOOL vehicle_twizy_cfg_sms(BOOL premsg, char *caller, char *command, char *arguments)
{
  char *s;
  char *cmd;
  UINT err;
  BOOL go_op_onexit = TRUE;

  if (!premsg)
    return FALSE;

  if (!caller || !*caller)
  {
    caller = par_get(PARAM_REGPHONE);
    if (!caller || !*caller)
      return FALSE;
  }

  if (sys_features[FEATURE_CANWRITE] == 0) {
    s = stp_rom(net_scratchpad, "Error: CAN bus in read only mode");
    net_send_sms_start(caller);
    net_puts_ram(net_scratchpad);
    return TRUE;
  }

  // get cmd:
  if (!arguments)
    return FALSE; // no cmd

  cmd = arguments;

  // convert cmd to upper-case:
  for (s=cmd; ((*s!=0)&&(*s!=' ')); s++)
    if ((*s > 0x60) && (*s < 0x7b)) *s=*s-0x20;


  // common SMS reply intro:
  s = stp_s(net_scratchpad, "CFG ", cmd);
  s = stp_rom(s, ": ");


  // login:
  if (err = login(1)) {
    s = stp_x(s, "LOGIN FAILED ERROR ", err);
    net_send_sms_start(caller);
    net_puts_ram(net_scratchpad);
    return TRUE;
  }

  //
  // cmd dispatcher:
  //

  if (strncmppgm2ram(cmd, "PRE", 3) == 0) {
    // PRE: enter config mode
    if (err = configmode(1)) {
      s = stp_x(s, "ERROR ", err);
      if (ERROR_IN_SDO(err))
        s = vehicle_twizy_fmt_sdo(s);
    }
    else {
      s = stp_rom(s, "OK");
    }
    go_op_onexit = FALSE;
  }

  else if (strncmppgm2ram(cmd, "OP", 2) == 0) {
    // OP: leave config mode
    if (err = configmode(0)) {
      s = stp_x(s, "ERROR ", err);
      if (ERROR_IN_SDO(err))
        s = vehicle_twizy_fmt_sdo(s);
    }
    else {
      s = stp_rom(s, "OK");
    }
    go_op_onexit = FALSE;
  }

  else if (strncmppgm2ram(cmd, "READ", 4) == 0) {
    // READ index_hex subindex_hex
    UINT index;
    UINT8 subindex;

    if (arguments = net_sms_nextarg(arguments))
      index = axtoul(arguments);
    if (arguments = net_sms_nextarg(arguments))
      subindex = axtoul(arguments);

    if (!arguments) {
      s = stp_rom(s, "ERROR: Too few args");
    }
    else {
      if (err = readsdo(index, subindex)) {
        s = stp_x(s, "ERROR ", err);
        if (ERROR_IN_SDO(err))
          s = vehicle_twizy_fmt_sdo(s);
      }
      else {
        s = vehicle_twizy_fmt_sdo(s);
        s = stp_ul(s, " = ", twizy_sdo_data);
      }
    }

    go_op_onexit = FALSE;
  }

  else if (strncmppgm2ram(cmd, "WRITE", 5) == 0) {
    // WRITE index_hex subindex_hex data_dec
    UINT index;
    UINT8 subindex;
    UINT32 data;

    if (arguments = net_sms_nextarg(arguments))
      index = axtoul(arguments);
    if (arguments = net_sms_nextarg(arguments))
      subindex = axtoul(arguments);
    if (arguments = net_sms_nextarg(arguments))
      data = atoul(arguments);

    if (!arguments) {
      s = stp_rom(s, "ERROR: Too few args");
    }
    else {
      // read old value:
      if (err = readsdo(index, subindex)) {
        s = stp_x(s, "ERROR ", err);
        if (ERROR_IN_SDO(err))
          s = vehicle_twizy_fmt_sdo(s);
      }
      else {
        // read ok:
        s = stp_rom(s, "OLD:");
        s = vehicle_twizy_fmt_sdo(s);
        s = stp_ul(s, " = ", twizy_sdo_data);

        // write new value:
        if (err = writesdo(index, subindex, data)) {
          s = stp_x(s, "ERROR ", err);
          if (ERROR_IN_SDO(err))
            s = vehicle_twizy_fmt_sdo(s);
        }
        else {
          s = stp_ul(s, " => NEW: ", data);
        }
      }
    }

    go_op_onexit = FALSE;
  }

  else if (strncmppgm2ram(cmd, "SPEED", 5) == 0) {
    // SPEED [max_kph] [warn_kph]
    int max_kph=-1, warn_kph=-1;

    if (arguments = net_sms_nextarg(arguments))
      max_kph = atoi(arguments);
    if (arguments = net_sms_nextarg(arguments))
      warn_kph = atoi(arguments);

    if (err = vehicle_twizy_cfg_speed(max_kph, warn_kph)) {
      s = stp_x(s, "ERROR ", err);
      if (ERROR_IN_SDO(err))
        s = vehicle_twizy_fmt_sdo(s);
    }
    else {
      s = stp_rom(s, "OK, power cycle to activate!");

      // debug:
      if (readsdo(0x2920,0x05) == 0)
        s = stp_ul(s, " fwd=", twizy_sdo_data);
      if (readsdo(0x3813,0x3c) == 0)
        s = stp_ul(s, " wrn=", twizy_sdo_data);
    }
  }

  else if (strncmppgm2ram(cmd, "TORQUE", 6) == 0) {
    // TORQUE [max_prc]
    int max_prc=-1;

    if (arguments = net_sms_nextarg(arguments))
      max_prc = atoi(arguments);

    if (err = vehicle_twizy_cfg_torque(max_prc)) {
      s = stp_x(s, "ERROR ", err);
      if (ERROR_IN_SDO(err))
        s = vehicle_twizy_fmt_sdo(s);
    }
    else {
      s = stp_rom(s, "OK, power cycle to activate!");

      // debug:
      if (readsdo(0x6076,0x00))
        s = stp_ul(s, " trq=", twizy_sdo_data);
    }
  }

  else if (strncmppgm2ram(cmd, "TSMAP", 5) == 0) {
    // TSMAP [map] [t1_prc] [t2_prc] [t3_prc] [t4_prc]
    char maps[4]={'D','N','B',0}, t1_prc=-1, t2_prc=-1, t3_prc=-1, t4_prc=-1;
    int i;

    if (arguments = net_sms_nextarg(arguments)) {
      for (i=0; i<3 && arguments[i]; i++)
        maps[i] = arguments[i] & ~0x20;
      maps[i] = 0;
    }
    if (arguments = net_sms_nextarg(arguments))
      t1_prc = atoi(arguments);
    if (arguments = net_sms_nextarg(arguments))
      t2_prc = atoi(arguments);
    if (arguments = net_sms_nextarg(arguments))
      t3_prc = atoi(arguments);
    if (arguments = net_sms_nextarg(arguments))
      t4_prc = atoi(arguments);

    for (i=0; maps[i]; i++) {
      if (err = vehicle_twizy_cfg_tsmap(maps[i], t1_prc, t2_prc, t3_prc, t4_prc))
        break;
    }

    if (err) {
      s = stp_x(s, "ERROR ", err);
      s = stp_rom(s, " IN MAP ");
      *s++ = maps[i]; *s = 0;
      if (ERROR_IN_SDO(err))
        s = vehicle_twizy_fmt_sdo(s);
    }
    else {
      s = stp_rom(s, "OK");

      // debug:
      if (readsdo(0x3813,0x24+6) == 0)
        s = stp_ul(s, " D4=", twizy_sdo_data);
      if (readsdo(0x3813,0x1b+6) == 0)
        s = stp_ul(s, " N4=", twizy_sdo_data);
      if (readsdo(0x3813,0x07+6) == 0)
        s = stp_ul(s, " B4=", twizy_sdo_data);
    }
  }

  else if (strncmppgm2ram(cmd, "POWER", 5) == 0) {
    // POWER [max_prc]
    int max_prc=-1;

    if (arguments = net_sms_nextarg(arguments))
      max_prc = atoi(arguments);

    if (err = vehicle_twizy_cfg_power(max_prc)) {
      s = stp_x(s, "ERROR ", err);
      if (ERROR_IN_SDO(err))
        s = vehicle_twizy_fmt_sdo(s);
    }
    else {
      s = stp_rom(s, "OK");

      // debug:
      if (readsdo(0x2920,0x01))
        s = stp_ul(s, " pow=", twizy_sdo_data);
    }
  }

  else if (strncmppgm2ram(cmd, "RECUP", 5) == 0) {
    // RECUP [neutral_prc] [brake_prc]
    int neutral_prc=-1, brake_prc=-1;

    if (arguments = net_sms_nextarg(arguments))
      neutral_prc = atoi(arguments);

    if (arguments = net_sms_nextarg(arguments))
      brake_prc = atoi(arguments);
    else
      brake_prc = neutral_prc;

    if (err = vehicle_twizy_cfg_recup(neutral_prc, brake_prc)) {
      s = stp_x(s, "ERROR ", err);
      if (ERROR_IN_SDO(err))
        s = vehicle_twizy_fmt_sdo(s);
    }
    else {
      s = stp_rom(s, "OK");

      // debug:
      if (readsdo(0x2920,0x03) == 0)
        s = stp_ul(s, " ntr=", twizy_sdo_data);
      if (readsdo(0x2920,0x04) == 0)
        s = stp_ul(s, " brk=", twizy_sdo_data);
    }
  }

  else if (strncmppgm2ram(cmd, "RAMPS", 5) == 0) {
    // RAMPS [start_prc] [accel_prc] [decel_prc] [neutral_prc] [brake_prc]
    int start_prc=-1, accel_prc=-1, decel_prc=-1, neutral_prc=-1, brake_prc=-1;

    if (arguments = net_sms_nextarg(arguments))
      start_prc = atoi(arguments);
    if (arguments = net_sms_nextarg(arguments))
      accel_prc = atoi(arguments);
    if (arguments = net_sms_nextarg(arguments))
      decel_prc = atoi(arguments);
    if (arguments = net_sms_nextarg(arguments))
      neutral_prc = atoi(arguments);
    if (arguments = net_sms_nextarg(arguments))
      brake_prc = atoi(arguments);

    if (err = vehicle_twizy_cfg_ramps(start_prc, accel_prc, decel_prc, neutral_prc, brake_prc)) {
      s = stp_x(s, "ERROR ", err);
      if (ERROR_IN_SDO(err))
        s = vehicle_twizy_fmt_sdo(s);
    }
    else {
      s = stp_rom(s, "OK");

      // debug:
      if (readsdo(0x291c,0x02) == 0)
        s = stp_ul(s, " rstr=", twizy_sdo_data);
      if (readsdo(0x2920,0x07) == 0)
        s = stp_ul(s, " racc=", twizy_sdo_data);
      if (readsdo(0x2920,0x0e) == 0)
        s = stp_ul(s, " rbrk=", twizy_sdo_data);
    }
  }

  else if (strncmppgm2ram(cmd, "SMOOTH", 6) == 0) {
    // SMOOTH [prc]
    int prc=-1;

    if (arguments = net_sms_nextarg(arguments))
      prc = atoi(arguments);

    if (err = vehicle_twizy_cfg_smoothing(prc)) {
      s = stp_x(s, "ERROR ", err);
      if (ERROR_IN_SDO(err))
        s = vehicle_twizy_fmt_sdo(s);
    }
    else {
      s = stp_rom(s, "OK");

      // debug:
      if (readsdo(0x290a,0x01))
        s = stp_ul(s, " scrv=", twizy_sdo_data);
      if (readsdo(0x290a,0x03))
        s = stp_ul(s, " srmp=", twizy_sdo_data);
    }
  }

  else if (strncmppgm2ram(cmd, "BRAKELIGHT", 10) == 0) {
    // BRAKELIGHT [on_lev] [off_lev]
    int on_lev=-1, off_lev=-1;

    if (arguments = net_sms_nextarg(arguments))
      on_lev = atoi(arguments);

    if (arguments = net_sms_nextarg(arguments))
      off_lev = atoi(arguments);
    else
      off_lev = on_lev;

    if (err = vehicle_twizy_cfg_brakelight(on_lev, off_lev)) {
      s = stp_x(s, "ERROR ", err);
      if (ERROR_IN_SDO(err))
        s = vehicle_twizy_fmt_sdo(s);
    }
    else {
      s = stp_rom(s, "OK");

      // debug:
      if (readsdo(0x3813,0x06) == 0)
        s = stp_ul(s, " bon=", twizy_sdo_data);
      if (readsdo(0x2910,0x01) == 0)
        s = stp_x(s, " flgs=", twizy_sdo_data);
    }
  }

  else if (strncmppgm2ram(cmd, "RESET", 5) == 0) {
    // RESET all macro commands to defaults
    if (err = vehicle_twizy_cfg_reset()) {
      s = stp_x(s, "ERROR ", err);
      if (ERROR_IN_SDO(err))
        s = vehicle_twizy_fmt_sdo(s);
    }
    else {
      s = stp_rom(s, "OK");

      // debug:
      if (readsdo(0x2920,0x05) == 0)
        s = stp_ul(s, " fwd=", twizy_sdo_data);
      if (readsdo(0x6076,0x00))
        s = stp_ul(s, " trq=", twizy_sdo_data);
      if (readsdo(0x2920,0x01) == 0)
        s = stp_ul(s, " pow=", twizy_sdo_data);
      if (readsdo(0x2920,0x03) == 0)
        s = stp_ul(s, " ntr=", twizy_sdo_data);
    }
  }

  else {
    // unknown command
    s = stp_rom(s, "Unknown command");
  }

  // go operational?
  if (go_op_onexit)
    configmode(0);

  // logout should not be needed here

  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE; // handled, but no SMS has been started

  net_send_sms_start(caller);
  net_puts_ram(net_scratchpad);

  return TRUE;
}





/**
 * vehicle_twizy_chargetime()
 *  Utility: calculate estimated charge time in minutes
 *  to reach dstsoc from current SOC
 *  (dstsoc in 1/100 %)
 */

// Charge time approximation constants:
// @ ~13 °C  -- temperature compensation needed?
#define CHARGETIME_CVSOC    9400    // CV phase normally begins at 93..95%
#define CHARGETIME_CC       180     // CC phase time (160..180 min.)
#define CHARGETIME_CV       40      // CV phase time (topoff) (20..40 min.)

int vehicle_twizy_chargetime(int dstsoc)
{
  int minutes;

  if (dstsoc > 10000)
    dstsoc = 10000;

  if (twizy_soc >= dstsoc)
    return 0;

  minutes = 0;

  if (dstsoc > CHARGETIME_CVSOC)
  {
    // CV phase
    if (twizy_soc < CHARGETIME_CVSOC)
      minutes += ((long) (dstsoc - CHARGETIME_CVSOC) * CHARGETIME_CV
              + ((10000-CHARGETIME_CVSOC)/2)) / (10000-CHARGETIME_CVSOC);
    else
      minutes += ((long) (dstsoc - twizy_soc) * CHARGETIME_CV
              + ((10000-CHARGETIME_CVSOC)/2)) / (10000-CHARGETIME_CVSOC);

    dstsoc = CHARGETIME_CVSOC;
  }

  // CC phase
  if (twizy_soc < dstsoc)
    minutes += ((long) (dstsoc - twizy_soc) * CHARGETIME_CC
            + (CHARGETIME_CVSOC/2)) / CHARGETIME_CVSOC;

  return minutes;
}

/**
 * vehicle_twizy_gps_msgp()
 *  GPS track logger
 *  using MSG "H", as only one MSG "L" will be kept on the server
 */
char vehicle_twizy_gpslog_msgp(char stat)
{
  static WORD crc;
  char *s;
  unsigned long pwr_dist, pwr_use, pwr_rec;

  // Read power stats:

  pwr_dist = twizy_speedpwr[CAN_SPEED_CONST].dist
          + twizy_speedpwr[CAN_SPEED_ACCEL].dist
          + twizy_speedpwr[CAN_SPEED_DECEL].dist;

  pwr_use = twizy_speedpwr[CAN_SPEED_CONST].use
          + twizy_speedpwr[CAN_SPEED_ACCEL].use
          + twizy_speedpwr[CAN_SPEED_DECEL].use;

  pwr_rec = twizy_speedpwr[CAN_SPEED_CONST].rec
          + twizy_speedpwr[CAN_SPEED_ACCEL].rec
          + twizy_speedpwr[CAN_SPEED_DECEL].rec;


  // H type "RT-GPS-Log", recno = odometer, keep for 1 day
  s = stp_ul(net_scratchpad, "MP-0 HRT-GPS-Log,", car_odometer); // in 1/10 mi
  s = stp_latlon(s, ",86400,", car_latitude);
  s = stp_latlon(s, ",", car_longitude);
  s = stp_i(s, ",", car_altitude);
  s = stp_i(s, ",", car_direction);
  s = stp_i(s, ",", car_speed); // in defined unit (mph or kph)
  s = stp_i(s, ",", car_gpslock);
  s = stp_i(s, ",", car_stale_gps);
  s = stp_i(s, ",", net_sq); // GPRS signal quality

  // Twizy specific (standard model candidates):
  s = stp_l(s, ",", (long) twizy_power * 16);     // current power (W)
  s = stp_ul(s, ",", (pwr_use + 11250) / 22500);  // power usage sum (Wh)
  s = stp_ul(s, ",", (pwr_rec + 11250) / 22500);  // recuperation sum (Wh)
  s = stp_ul(s, ",", (pwr_dist + 5) / 10);        // distance driven (m)

  return net_msg_encode_statputs(stat, &crc);
}


#ifdef OVMS_DIAGMODULE
/***************************************************************
 * CAN simulator: inject CAN messages for testing
 *
 * Usage:
 *  int: vehicle_twizy_simulator_run(chunknr)
 *  SMS: S DEBUG chunknr
 *  MSG: M 200,chunknr
 *
 * To create twizy_sim_data[] from CAN log:
 * cat canlog \
 *      | sed -e 's/\(..\)/0x\1,/g' \
 *      | sed -e 's/^0x\(.\)\(.\),0x\(.\)/0x0\1,0x\2\3,0x0/' \
 *      | sed -e 's/\(.*\),/  { \1 },/'
 *
 */

rom BYTE twizy_sim_data[][11] = {
  //{ 0, 0, 0 }, // chunk 0: init/on
  { 0x06, 0x9F, 0x04, 0xF0, 0x82, 0x87, 0x37}, // VIN
  { 0x05, 0x97, 0x08, 0x00, 0x95, 0x21, 0x41, 0x29, 0x00, 0x01, 0x35}, // STATUS
  { 0x01, 0x55, 0x08, 0x07, 0x97, 0xC7, 0x54, 0x97, 0x98, 0x00, 0x73}, // SOC
  { 0x05, 0x99, 0x08, 0x00, 0x00, 0x0D, 0x80, 0xFF, 0x46, 0x00, 0x00}, // RANGE
  { 0x05, 0x9E, 0x08, 0x00, 0x00, 0x0C, 0xF2, 0x29, 0x31, 0x00, 0x00}, // MOTOR TEMP

  { 0, 0, 1}, // chunk 1: charge
  { 0x05, 0x97, 0x08, 0x20, 0xA4, 0x03, 0xB1, 0x29, 0x00, 0x01, 0x64}, // STATUS

  { 0, 0, 2}, // chunk 2: off
  { 0x05, 0x97, 0x08, 0x00, 0xE4, 0x00, 0xD1, 0x29, 0x00, 0x01, 0x31}, // STATUS

  { 0, 0, 10}, // chunk 10: normal battery data
  { 0x05, 0x54, 0x08, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x00},
  { 0x05, 0x56, 0x08, 0x33, 0x73, 0x35, 0x33, 0x53, 0x35, 0x33, 0x5A},
  { 0x05, 0x57, 0x08, 0x33, 0x53, 0x35, 0x33, 0x53, 0x35, 0x33, 0x50},
  { 0x05, 0x5E, 0x08, 0x33, 0x53, 0x34, 0x33, 0x43, 0x36, 0x01, 0x59},
  { 0x05, 0x5F, 0x08, 0x00, 0xFE, 0x73, 0x00, 0x00, 0x23, 0xE2, 0x3E},

  { 0, 0, 11}, // chunk 11: battery alert test 1
  { 0x05, 0x54, 0x08, 0x33, 0x34, 0x33, 0x33, 0x33, 0x33, 0x33, 0x00}, // cmod 2 watch?
  { 0x05, 0x56, 0x08, 0x33, 0x73, 0x35, 0x33, 0x53, 0x35, 0x33, 0x5A},
  { 0x05, 0x57, 0x08, 0x33, 0x53, 0x35, 0x33, 0x53, 0x35, 0x33, 0x50},
  { 0x05, 0x5E, 0x08, 0x33, 0x53, 0x34, 0x33, 0x43, 0x36, 0x01, 0x59},
  { 0x05, 0x5F, 0x08, 0x00, 0xFE, 0x73, 0x00, 0x00, 0x23, 0xE2, 0x3E},

  { 0, 0, 12}, // chunk 12: battery alert test 2
  { 0x05, 0x54, 0x08, 0x33, 0x38, 0x33, 0x33, 0x34, 0x33, 0x34, 0x00}, // cmod 2 alert?
  { 0x05, 0x56, 0x08, 0x33, 0x73, 0x35, 0x33, 0x53, 0x35, 0x31, 0x5A}, // cell 5 alert?
  { 0x05, 0x57, 0x08, 0x33, 0x53, 0x35, 0x33, 0x53, 0x35, 0x33, 0x50},
  { 0x05, 0x5E, 0x08, 0x33, 0x53, 0x34, 0x33, 0x43, 0x36, 0x01, 0x59},
  { 0x05, 0x5F, 0x08, 0x00, 0xFE, 0x73, 0x00, 0x00, 0x23, 0xE2, 0x3E},

  { 0, 0, 13}, // chunk 13: battery stddev alert test
  { 0x05, 0x54, 0x08, 0x33, 0x35, 0x31, 0x32, 0x30, 0x37, 0x2E, 0x00},
  { 0x05, 0x56, 0x08, 0x33, 0xA3, 0x35, 0x32, 0x53, 0x36, 0x33, 0x1A},
  { 0x05, 0x57, 0x08, 0x32, 0x23, 0x34, 0x34, 0x03, 0x35, 0x33, 0x20},
  { 0x05, 0x5E, 0x08, 0x32, 0xA3, 0x30, 0x33, 0x43, 0x51, 0x01, 0x59},
  { 0x05, 0x5F, 0x08, 0x00, 0xFE, 0x73, 0x00, 0x00, 0x23, 0xE2, 0x3E},

  { 0, 0, 21}, // chunk 21: partial battery data #1 (state=1?)
  { 0x05, 0x54, 0x08, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x00},
  { 0, 0, 22}, // chunk 22: partial battery data #1 (state=2?)
  { 0x05, 0x56, 0x08, 0x33, 0x73, 0x35, 0x33, 0x53, 0x35, 0x33, 0x5A},
  { 0, 0, 23}, // chunk 23: partial battery data #1 (state=3?)
  { 0x05, 0x57, 0x08, 0x33, 0x53, 0x35, 0x33, 0x53, 0x35, 0x33, 0x50},
  { 0, 0, 24}, // chunk 24: partial battery data #1 (state=4?)
  { 0x05, 0x5E, 0x08, 0x33, 0x53, 0x34, 0x33, 0x43, 0x36, 0x01, 0x59},
  { 0, 0, 25}, // chunk 25: partial battery data #2 (state=5?)
  { 0x05, 0x5F, 0x08, 0x00, 0xFE, 0x73, 0x00, 0x00, 0x23, 0xE2, 0x3E},

  { 0, 0, 255} // end
};

void vehicle_twizy_simulator_run(int chunk)
{
  UINT8 cc;
  int line;
  char *s;

  for (line = 0, cc = 0; cc <= chunk; line++)
  {
    if ((twizy_sim_data[line][0] == 0) && (twizy_sim_data[line][1] == 0))
    {
      // next chunk:
      cc = twizy_sim_data[line][2];
    }

    else if (cc == chunk)
    {
      // this is the chunk: inject sim data
      if (net_state == NET_STATE_DIAGMODE)
      {
        s = stp_i(net_scratchpad, "# sim data line ", line);
        s = stp_rom(s, "\n");
        net_puts_ram(net_scratchpad);
      }

      // turn off CAN RX interrupts:
      PIE3bits.RXB0IE = 0;
      PIE3bits.RXB1IE = 0;

      // process sim data:
      twizy_sim = line;
      vehicle_twizy_poll0();
      vehicle_twizy_poll1();
      twizy_sim = -1;

      // turn on CAN RX interrupts:
      PIE3bits.RXB1IE = 1;
      PIE3bits.RXB0IE = 1;
    }
  }
}
#endif // OVMS_DIAGMODULE


////////////////////////////////////////////////////////////////////////
// twizy_poll()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to poll for data.
//
// See vehicle initialise() for buffer 0/1 filter setup.
//

// Poll buffer 0:

BOOL vehicle_twizy_poll0(void)
{
  unsigned char CANfilter;
  unsigned char CANsid;

  unsigned int t;
  unsigned char u;
  unsigned long v;


#ifdef OVMS_DIAGMODULE
  if (twizy_sim >= 0)
  {
    // READ SIMULATION DATA:
    UINT i;

    i = (((UINT) twizy_sim_data[twizy_sim][0]) << 8)
            + twizy_sim_data[twizy_sim][1];

    if (i == 0x155)
      CANfilter = 0;
    else
      return FALSE;

    can_datalength = twizy_sim_data[twizy_sim][2];
    for (i = 0; i < 8; i++)
      can_databuffer[i] = twizy_sim_data[twizy_sim][3 + i];
  }
  else
#endif // OVMS_DIAGMODULE
  {
    // READ CAN BUFFER:

    CANfilter = RXB0CON & 0x01;
    CANsid = (RXB0SIDH >> 5) & 0x0F;

    can_datalength = RXB0DLC & 0x0F; // number of received bytes
    can_databuffer[0] = RXB0D0;
    can_databuffer[1] = RXB0D1;
    can_databuffer[2] = RXB0D2;
    can_databuffer[3] = RXB0D3;
    can_databuffer[4] = RXB0D4;
    can_databuffer[5] = RXB0D5;
    can_databuffer[6] = RXB0D6;
    can_databuffer[7] = RXB0D7;

    RXB0CONbits.RXFUL = 0; // All bytes read, Clear flag
  }


  // HANDLE CAN MESSAGE:

  if (CANfilter == 0)
  {
    if (CANsid == 0x1)
    {
      /*****************************************************
       * FILTER 0:
       * CAN ID 0x155: sent every 10 ms (100 per second)
       */

      // Basic validation:
      // Byte 4:  0x94 = init/exit phase (CAN data invalid)
      //          0x54 = Twizy online (CAN data valid)
      if (can_databuffer[3] == 0x54)
      {
        // SOC:
        t = ((unsigned int) can_databuffer[4] << 8) + can_databuffer[5];
        if (t > 0 && t <= 40000)
        {
          twizy_soc = t >> 2;
          // car value derived in ticker1()

          // Remember maximum SOC for charging "done" distinction:
          if (twizy_soc > twizy_soc_max)
            twizy_soc_max = twizy_soc;

          // ...and minimum SOC for range calculation during charging:
          if (twizy_soc < twizy_soc_min)
          {
            twizy_soc_min = twizy_soc;
            twizy_soc_min_range = twizy_range;
          }
        }

        // POWER:
        t = ((unsigned int) (can_databuffer[1] & 0x0f) << 8) + can_databuffer[2];
        if (t > 0 && t < 0x0f00)
        {
          twizy_power = 2000 - (signed int) t;

          // calculate distance from ref:
          if (twizy_dist >= twizy_speed_distref)
            t = twizy_dist - twizy_speed_distref;
          else
            t = twizy_dist + (0x10000L - twizy_speed_distref);
          twizy_speed_distref = twizy_dist;

          // add to speed state:
          twizy_speedpwr[twizy_speed_state].dist += t;
          if (twizy_power > 0)
          {
            twizy_speedpwr[twizy_speed_state].use += twizy_power;
            twizy_level_use += twizy_power;
          }
          else
          {
            twizy_speedpwr[twizy_speed_state].rec += -twizy_power;
            twizy_level_rec += -twizy_power;
          }

          // do we need to take base power consumption into account?
          // i.e. for lights etc. -- varies...
        }
      }
    }
  }

  else
  {
    /*****************************************************
     * FILTER 1:
     * CAN ID 0x_81: CANopen message from SEVCON (Node #1)
     */

    if (CANsid == 0x0)
    {
      /*****************************************************
       * FILTER 1:
       * CAN ID 0x081: CANopen error message from SEVCON (Node #1)
       */

      // count errors to detect manual CFG RESET request:
      if ((CAN_BYTE(1)==0x10) && (CAN_BYTE(2)==0x01))
        twizy_button_cnt++;

    }

    else if (CANsid == 0x5)
    {
      /*****************************************************
       * FILTER 1:
       * CAN ID 0x581: CANopen SDO reply from SEVCON (Node #1)
       */
      // message structure:
      //    0 = control byte
      //    1+2 = index (little endian)
      //    3 = subindex
      //    4..7 = data (little endian)

      // check if index/subindex match our request:
      t = CAN_BYTE(1) + ((unsigned int)CAN_BYTE(2) << 8);
      u = CAN_BYTE(3);

      if (t == twizy_sdo_index && u == twizy_sdo_subindex)
      {
        // OK, get data:
        v = 0;
        switch (CAN_BYTE(0) & 0b00001100) // size / unused bytes
        {
        case 0b00000000:
          v |= (unsigned long)CAN_BYTE(7) << 24;
        case 0b00000100:
          v |= (unsigned long)CAN_BYTE(6) << 16;
        case 0b00001000:
          v |= (unsigned long)CAN_BYTE(5) << 8;
        case 0b00001100:
          v |= (unsigned long)CAN_BYTE(4);
        }
        twizy_sdo_data = v;

        // get status:
        twizy_sdo_control = CAN_BYTE(0);
      }

    }

  }

  return TRUE;
}



// Poll buffer 1:

BOOL vehicle_twizy_poll1(void)
{
  unsigned char CANfilter;
  unsigned char CANsid;

  unsigned int new_speed;


#ifdef OVMS_DIAGMODULE
  if (twizy_sim >= 0)
  {
    // READ SIMULATION DATA:
    UINT i;

    i = (((UINT) twizy_sim_data[twizy_sim][0]) << 8)
            + twizy_sim_data[twizy_sim][1];

    if ((i & 0x590) == 0x590)
      CANfilter = 2;
    else if ((i & 0x690) == 0x690)
      CANfilter = 3;
    else if ((i & 0x550) == 0x550)
      CANfilter = 4;
    else
      return FALSE;

    CANsid = i & 0x00f;

    can_datalength = twizy_sim_data[twizy_sim][2];
    for (i = 0; i < 8; i++)
      can_databuffer[i] = twizy_sim_data[twizy_sim][3 + i];
  }
  else
#endif // OVMS_DIAGMODULE
  {
    // READ CAN BUFFER:

    CANfilter = RXB1CON & 0x07;
    CANsid = ((RXB1SIDH & 0x01) << 3) + ((RXB1SIDL >> 5) & 0x0F);

    can_datalength = RXB1DLC & 0x0F; // number of received bytes
    can_databuffer[0] = RXB1D0;
    can_databuffer[1] = RXB1D1;
    can_databuffer[2] = RXB1D2;
    can_databuffer[3] = RXB1D3;
    can_databuffer[4] = RXB1D4;
    can_databuffer[5] = RXB1D5;
    can_databuffer[6] = RXB1D6;
    can_databuffer[7] = RXB1D7;

    RXB1CONbits.RXFUL = 0; // All bytes read, Clear flag
  }


  // HANDLE CAN MESSAGE:

  if (CANfilter == 2)
  {
    // Filter 2 = CAN ID GROUP 0x59_:

    switch (CANsid)
    {
      /*****************************************************
       * FILTER 2:
       * CAN ID 0x597: sent every 100 ms (10 per second)
       */
    case 0x07:

      // VEHICLE state:
      //  [0]: 0x20 = power line connected
      
      if (CAN_BYTE(0) & 0x20)
      {
        car_linevoltage = 230; // fix 230 V
        car_chargecurrent = 10; // fix 10 A
      }
      else
      {
        car_linevoltage = 0;
        car_chargecurrent = 0;
      }

      //  [1] bit 4 = 0x10 CAN_STATUS_KEYON: 1 = Car ON (key switch)
      //  [1] bit 5 = 0x20 CAN_STATUS_CHARGING: 1 = Charging
      //  [1] bit 6 = 0x40 CAN_STATUS_OFFLINE: 1 = Switch-ON/-OFF phase

      twizy_status = CAN_BYTE(1);
      // Translation to car_doors1 done in ticker1()

      // init cyclic distance counter on switch-on:
      if ((twizy_status & CAN_STATUS_KEYON) && (!car_doors1bits.CarON))
        twizy_dist = twizy_speed_distref = 0;

      // PEM temperature:
      if (CAN_BYTE(7) > 0 && CAN_BYTE(7) < 0xf0)
        car_tpem = (signed char) CAN_BYTE(7) - 40;

      break;



      /*****************************************************
       * FILTER 2:
       * CAN ID 0x599: sent every 100 ms (10 per second)
       */
    case 0x09:

      // RANGE:
      // we need to check for charging, as the Twizy
      // does not update range during charging
      if (((twizy_status & 0x60) == 0)
              && (can_databuffer[5] != 0xff) && (can_databuffer[5] > 0))
      {
        twizy_range = can_databuffer[5];
        // car values derived in ticker1()
      }

      // SPEED:
      new_speed = ((unsigned int) can_databuffer[6] << 8) + can_databuffer[7];
      if (new_speed != 0xffff)
      {
        int delta = (int) new_speed - (int) twizy_speed;

        if (delta >= CAN_SPEED_THRESHOLD)
          twizy_speed_state = CAN_SPEED_ACCEL;
        else if (delta <= -CAN_SPEED_THRESHOLD)
          twizy_speed_state = CAN_SPEED_DECEL;
        else
          twizy_speed_state = CAN_SPEED_CONST;

        twizy_speed = new_speed;
        // car value derived in ticker1()
      }

      break;


      /*****************************************************
       * FILTER 2:
       * CAN ID 0x59E: sent every 100 ms (10 per second)
       */
    case 0x0E:

      // CYCLIC DISTANCE COUNTER:
      twizy_dist = ((UINT) CAN_BYTE(0) << 8) + CAN_BYTE(1);

      // MOTOR TEMPERATURE:
      if (CAN_BYTE(5) > 40 && CAN_BYTE(5) < 0xf0)
        car_tmotor = CAN_BYTE(5) - 40;
      else
        car_tmotor = 0; // unsigned, no negative temps allowed...


      break;
    }

  }

  else if (CANfilter == 3)
  {
    // Filter 3 = CAN ID GROUP 0x69_:

    switch (CANsid)
    {
      /*****************************************************
       * FILTER 3:
       * CAN ID 0x69F: sent every 1000 ms (1 per second)
       */
    case 0x0f:
      // VIN: last 7 digits of real VIN, in nibbles, reverse:
      // (assumption: no hex digits)
      if (car_vin[7]) // we only need to process this once
      {
        car_vin[0] = '0' + CAN_NIB(7);
        car_vin[1] = '0' + CAN_NIB(6);
        car_vin[2] = '0' + CAN_NIB(5);
        car_vin[3] = '0' + CAN_NIB(4);
        car_vin[4] = '0' + CAN_NIB(3);
        car_vin[5] = '0' + CAN_NIB(2);
        car_vin[6] = '0' + CAN_NIB(1);
        car_vin[7] = 0;
      }

      break;
    }
  }

#ifdef OVMS_TWIZY_BATTMON
  else if (CANfilter == 4)
  {
    UINT8 i, state;

    // Filter 4 = CAN ID GROUP 0x55_: battery sensors.
    //
    // This group really needs to be processed as fast as possible;
    // though only delivered once per second (except 556), the complete
    // group comes at once and needs to be processed together to get
    // a consistent sensor state.

    // NEW INFO: group msgs can come in arbitrary order!
    //  => using faster (100ms) 0x556 msgs as examination window
    //    to detect mid-group fetch start

    // volatile optimization:
    state = twizy_batt_sensors_state;

    switch (CANsid)
    {
    case 0x06:
      // CAN ID 0x556: Battery cell voltages 1-5
      // 100 ms = 10 per second
      //  => used to clock examination window
      if ((CAN_BYTE(0) != 0x0ff) && (state != BATT_SENSORS_READY))
      {
        // store values:
        twizy_cell[0].volt_act = ((UINT) CAN_BYTE(0) << 4)
                | ((UINT) CAN_NIBH(1));
        twizy_cell[1].volt_act = ((UINT) CAN_NIBL(1) << 8)
                | ((UINT) CAN_BYTE(2));
        twizy_cell[2].volt_act = ((UINT) CAN_BYTE(3) << 4)
                | ((UINT) CAN_NIBH(4));
        twizy_cell[3].volt_act = ((UINT) CAN_NIBL(4) << 8)
                | ((UINT) CAN_BYTE(5));
        twizy_cell[4].volt_act = ((UINT) CAN_BYTE(6) << 4)
                | ((UINT) CAN_NIBH(7));

        // detect fetch completion/failure:
        if ((state & ~BATT_SENSORS_GOT556)
                == (BATT_SENSORS_READY & ~BATT_SENSORS_GOT556))
        {
          // read all sensor data: group complete
          twizy_batt_sensors_state = BATT_SENSORS_READY;
        }
        else if ((state & ~BATT_SENSORS_GOT556))
        {
          // read some sensor data: count 0x556 cycles
          i = (state & BATT_SENSORS_GOT556) + 1;

          if (i == 3)
            // not complete in 2 0x556s cycles: drop window (wait for next group)
            state = BATT_SENSORS_START;
          else
            // store new fetch window state:
            state = (state & ~BATT_SENSORS_GOT556) | i;

          twizy_batt_sensors_state = state;
        }

      }

      break;

    case 0x04:
      // CAN ID 0x554: Battery cell module temperatures
      // (1000 ms = 1 per second)
      if ((CAN_BYTE(0) != 0x0ff) && (state != BATT_SENSORS_READY))
      {
        for (i = 0; i < BATT_CMODS; i++)
          twizy_cmod[i].temp_act = CAN_BYTE(i);

        state |= BATT_SENSORS_GOT554;

        // detect fetch completion:
        if ((state & BATT_SENSORS_READY) >= BATT_SENSORS_GOTALL)
          state = BATT_SENSORS_READY;

        twizy_batt_sensors_state = state;
      }
      break;

    case 0x07:
      // CAN ID 0x557: Battery cell voltages 6-10
      // (1000 ms = 1 per second)
      if ((CAN_BYTE(0) != 0x0ff) && (state != BATT_SENSORS_READY))
      {
        twizy_cell[5].volt_act = ((UINT) CAN_BYTE(0) << 4)
                | ((UINT) CAN_NIBH(1));
        twizy_cell[6].volt_act = ((UINT) CAN_NIBL(1) << 8)
                | ((UINT) CAN_BYTE(2));
        twizy_cell[7].volt_act = ((UINT) CAN_BYTE(3) << 4)
                | ((UINT) CAN_NIBH(4));
        twizy_cell[8].volt_act = ((UINT) CAN_NIBL(4) << 8)
                | ((UINT) CAN_BYTE(5));
        twizy_cell[9].volt_act = ((UINT) CAN_BYTE(6) << 4)
                | ((UINT) CAN_NIBH(7));

        state |= BATT_SENSORS_GOT557;

        // detect fetch completion:
        if ((state & BATT_SENSORS_READY) >= BATT_SENSORS_GOTALL)
          state = BATT_SENSORS_READY;

        twizy_batt_sensors_state = state;
      }
      break;

    case 0x0E:
      // CAN ID 0x55E: Battery cell voltages 11-14
      // (1000 ms = 1 per second)
      if ((CAN_BYTE(0) != 0x0ff) && (state != BATT_SENSORS_READY))
      {
        twizy_cell[10].volt_act = ((UINT) CAN_BYTE(0) << 4)
                | ((UINT) CAN_NIBH(1));
        twizy_cell[11].volt_act = ((UINT) CAN_NIBL(1) << 8)
                | ((UINT) CAN_BYTE(2));
        twizy_cell[12].volt_act = ((UINT) CAN_BYTE(3) << 4)
                | ((UINT) CAN_NIBH(4));
        twizy_cell[13].volt_act = ((UINT) CAN_NIBL(4) << 8)
                | ((UINT) CAN_BYTE(5));

        state |= BATT_SENSORS_GOT55E;

        // detect fetch completion:
        if ((state & BATT_SENSORS_READY) >= BATT_SENSORS_GOTALL)
          state = BATT_SENSORS_READY;

        twizy_batt_sensors_state = state;
      }
      break;

    case 0x0F:
      // CAN ID 0x55F: Battery pack voltages
      // (1000 ms = 1 per second)
      if ((CAN_BYTE(5) != 0x0ff) && (state != BATT_SENSORS_READY))
      {
        // we still don't know why there are two pack voltages
        // best guess: take avg
        UINT v1, v2;

        v1 = ((UINT) CAN_BYTE(5) << 4)
                | ((UINT) CAN_NIBH(6));
        v2 = ((UINT) CAN_NIBL(6) << 8)
                | ((UINT) CAN_BYTE(7));

        twizy_batt[0].volt_act = (v1 + v2) >> 1;

        state |= BATT_SENSORS_GOT55F;

        // detect fetch completion:
        if ((state & BATT_SENSORS_READY) >= BATT_SENSORS_GOTALL)
          state = BATT_SENSORS_READY;

        twizy_batt_sensors_state = state;
      }
      break;

    } // switch (CANsid)
  }
#endif // OVMS_TWIZY_BATTMON

  else if (CANfilter == 5)
  {
    // Filter 5 = CAN ID GROUP 0x5D_: exact speed & odometer

    switch (CANsid)
    {
      /*****************************************************
       * FILTER 5:
       * CAN ID 0x5D7: sent every 100 ms (10 per second)
       */
    case 0x07:

      // ODOMETER:
      twizy_odometer = ((unsigned long) CAN_BYTE(5) >> 4)
              | ((unsigned long) CAN_BYTE(4) << 4)
              | ((unsigned long) CAN_BYTE(3) << 12)
              | ((unsigned long) CAN_BYTE(2) << 20);
      // car value derived in ticker1()

      break;
    }
  }

  return TRUE;
}


/***************************************************************
 * vehicle_twizy_notify: process twizy_notifies
 *  and send regular data updates
 *
 *  Note: if multiple send requests are queued, they will
 *    be sent one at a time (per call), in the order as coded
 *    here (ifs).
 */

void vehicle_twizy_notify(void)
{
  char *notify_channels;
  UINT8 notify_sms = 0, notify_msg = 0;
  char stat;

#ifdef OVMS_DIAGMODULE
  if ((net_state == NET_STATE_DIAGMODE))
    ; // disable connection check
  else
#endif // OVMS_DIAGMODULE

    // GPRS ready & available?
    if ((net_state != NET_STATE_READY))
    return;

  // Read user config: notification channels
  notify_channels = par_get(PARAM_NOTIFIES);
  if (strstrrampgm(notify_channels, "SMS"))
    notify_sms = 1;
  if (strstrrampgm(notify_channels, "IP"))
    notify_msg = 1;


#ifdef OVMS_TWIZY_BATTMON
  // Send battery alert:
  if ((twizy_notify & SEND_BatteryAlert))
  {
    if ((notify_msg) && (net_msg_serverok) && (net_msg_sendpending == 0))
    {
      vehicle_twizy_battstatus_cmd(FALSE, CMD_BatteryAlert, NULL);
      twizy_notify &= ~SEND_BatteryAlert;
    }

    if (notify_sms)
    {
      if (vehicle_twizy_battstatus_sms(TRUE, NULL, NULL, NULL))
        net_send_sms_finish();
      twizy_notify &= ~SEND_BatteryAlert;
    }
  }
#endif // OVMS_TWIZY_BATTMON


  // Send power usage statistics?
  if ((twizy_notify & SEND_PowerNotify))
  {
    if ((notify_msg) && (net_msg_serverok) && (net_msg_sendpending == 0))
    {
      vehicle_twizy_power_cmd(FALSE, CMD_PowerUsageNotify, NULL);
      twizy_notify &= ~SEND_PowerNotify;
    }

    if (notify_sms)
    {
      if (vehicle_twizy_power_sms(TRUE, NULL, NULL, NULL))
        net_send_sms_finish();
      twizy_notify &= ~SEND_PowerNotify;
    }
  }


#ifdef OVMS_TWIZY_BATTMON
  // Send battery status update:
  if ((twizy_notify & SEND_BatteryStats))
  {
    if ((net_msg_serverok == 1) && (net_msg_sendpending == 0))
    {
      vehicle_twizy_battstatus_cmd(FALSE, CMD_BatteryStatus, NULL);
      twizy_notify &= ~SEND_BatteryStats;
    }
  }
#endif // OVMS_TWIZY_BATTMON


  // Send regular data update:
  if ((twizy_notify & SEND_DataUpdate))
  {
    if ((net_msg_serverok == 1) && (net_msg_sendpending == 0))
    {
      stat = 2;
      stat = vehicle_twizy_power_msgp(stat, CMD_PowerUsageStats);
      stat = vehicle_twizy_gpslog_msgp(stat);
      if (stat != 2)
        net_msg_send();

      twizy_notify &= ~SEND_DataUpdate;

#ifdef OVMS_TWIZY_BATTMON
      // send battery status update separately, may need a full CIPSEND length:
      twizy_notify |= SEND_BatteryStats;
#endif // OVMS_TWIZY_BATTMON
    }
  }


  // Send streaming updates:
  if ((twizy_notify & SEND_StreamUpdate))
  {
    if ((net_msg_serverok == 1) && (net_msg_sendpending == 0))
    {
      if (vehicle_twizy_gpslog_msgp(2) != 2)
        net_msg_send();
      twizy_notify &= ~SEND_StreamUpdate;
    }
  }


}


/***********************************************************************
 * HOOK vehicle_fn_idlepoll:
 *  called from main loop after each net_poll()
 *  as net_poll() frees the msg_sendpending semaphore, this
 *  should be the place to do our own queued notifications...
 */
BOOL vehicle_twizy_idlepoll(void)
{
  // Send notifications and data updates:
  vehicle_twizy_notify();

  return TRUE;
}


////////////////////////////////////////////////////////////////////////
// twizy_state_ticker1()
// State Model: Per-second ticker
// This function is called approximately once per second, and gives
// the state a timeslice for activity.
//

BOOL vehicle_twizy_state_ticker1(void)
{
  int suffSOC, suffRange, maxRange;


  /***************************************************************************
   * Read feature configuration:
   */

  suffSOC = sys_features[FEATURE_SUFFSOC];
  suffRange = sys_features[FEATURE_SUFFRANGE];
  maxRange = sys_features[FEATURE_MAXRANGE];

  if (can_mileskm == 'K')
  {
    // convert user km to miles
    if (suffRange > 0)
      suffRange = MiFromKm(suffRange);
    if (maxRange > 0)
      maxRange = MiFromKm(maxRange);
  }


  /***************************************************************************
   * Convert & take over CAN values into CAR values:
   * (done here to keep interrupt fn small&fast)
   */

  // Count seconds:
  car_time++;

  // SOC: convert to percent:
  car_SOC = (twizy_soc + 50) / 100;

  // ODOMETER: convert to miles/10:
  car_odometer = MiFromKm(twizy_odometer / 10);

  // SPEED:
  if (can_mileskm == 'M')
    car_speed = MiFromKm((twizy_speed + 50) / 100); // miles/hour
  else
    car_speed = (twizy_speed + 50) / 100; // km/hour


  // STATUS: convert Twizy flags to car_doors1:
  // Door state #1
  //	bit2 = 0x04 Charge port (open=1/closed=0)
  //    bit3 = 0x08 Pilot signal present (able to charge)
  //	bit4 = 0x10 Charging (true=1/false=0)
  //	bit6 = 0x40 Hand brake applied (true=1/false=0)
  //	bit7 = 0x80 Car ON (true=1/false=0)
  //
  // ATT: bit 2 = 0x04 = needs to be set for net_sms_stat()!
  //
  // Twizy message: twizy_status
  //  bit 4 = 0x10 CAN_STATUS_KEYON: 1 = Car ON (key switch)
  //  bit 5 = 0x20 CAN_STATUS_CHARGING: 1 = Charging
  //  bit 6 = 0x40 CAN_STATUS_OFFLINE: 1 = Switch-ON/-OFF phase
  
  if (car_linevoltage > 0)
  {
    car_doors1bits.PilotSignal = 1;
    car_doors5bits.Charging12V = 1;
  }
  else
  {
    car_doors1bits.PilotSignal = 0;
    car_doors5bits.Charging12V = 0;
  }

  if ((twizy_status & 0x60) == 0x20)
  {
    car_doors1bits.ChargePort = 1;
    car_doors1bits.Charging = 1;
  }
  else
  {
    car_doors1bits.Charging = 0;
    // Port will be closed on next use start
  }

  if (twizy_status & CAN_STATUS_KEYON)
  {
    if (!car_doors1bits.CarON)
    {
      // CAR has just been turned ON
      car_doors1bits.CarON = 1;

      car_parktime = 0; // No longer parking
      net_req_notification(NET_NOTIFY_ENV);

#ifdef OVMS_TWIZY_BATTMON
      // reset battery monitor:
      vehicle_twizy_battstatus_reset();
#endif // OVMS_TWIZY_BATTMON

      // reset power statistics:
      vehicle_twizy_power_reset();

      // reset button cnt:
      twizy_button_cnt = 0;
    }
  }
  else
  {
    if (car_doors1bits.CarON)
    {
      // CAR has just been turned OFF
      car_doors1bits.CarON = 0;

      car_parktime = car_time-1; // Record it as 1 second ago, so non zero report
      net_req_notification(NET_NOTIFY_ENV);
      
      // send power statistics:
      if (twizy_speedpwr[CAN_SPEED_CONST].use > 22500)
        twizy_notify |= SEND_PowerNotify;

      // reset button cnt:
      twizy_button_cnt = 0;
    }
  }


  /***************************************************************************
   * Statistics subsystems:
   *
   */

#ifdef OVMS_TWIZY_BATTMON
  // collect battery statistics:
  vehicle_twizy_battstatus_collect();
#endif // OVMS_TWIZY_BATTMON

  // collect power statistics:
  vehicle_twizy_power_collect();


  /***************************************************************************
   * Charge notification + alerts:
   *
   * car_chargestate: 1=charging, 2=top off, 4=done, 21=stopped charging
   * car_chargesubstate: unused
   *
   */

  if (car_doors1bits.Charging)
  {
    /*******************************************************************
     * CHARGING
     */
    
    unsigned char twizy_curr_SOC = twizy_soc / 100; // unrounded SOC% for charge alerts


    // Calculate range during charging:
    // scale twizy_soc_min_range to twizy_soc
    if ((twizy_soc_min_range > 0) && (twizy_soc > 0) && (twizy_soc_min > 0))
    {
      // Update twizy_range:
      twizy_range =
              (((float) twizy_soc_min_range) / twizy_soc_min) * twizy_soc;

      if (twizy_range > 0)
        car_estrange = MiFromKm(twizy_range);

      if (maxRange > 0)
        car_idealrange = (((float) maxRange) * twizy_soc) / 10000;
      else
        car_idealrange = car_estrange;
    }


    // If charging has previously been interrupted...
    if (car_chargestate == 21)
    {
      // ...send charge alert:
      net_req_notification(NET_NOTIFY_CHARGE);
    }


    // If we've not been charging before...
    if (car_chargestate > 2)
    {
      // ...enter state 1=charging:
      car_chargestate = 1;

      // reset SOC max:
      twizy_soc_max = twizy_soc;

      // reset power sums:
      vehicle_twizy_power_reset();

      // Send charge stat:
      net_req_notification(NET_NOTIFY_STAT);
    }

    else
    {
      // We've already been charging:

      // check for crossing "sufficient SOC/Range" thresholds:
      if (
              ((twizy_soc > 0) && (suffSOC > 0)
              && (twizy_curr_SOC >= suffSOC) && (twizy_last_SOC < suffSOC))
              ||
              ((twizy_range > 0) && (suffRange > 0)
              && (car_idealrange >= suffRange) && (twizy_last_idealrange < suffRange))
              )
      {
        // ...enter state 2=topping off:
        car_chargestate = 2;

        // ...send charge alert:
        net_req_notification(NET_NOTIFY_CHARGE);
        net_req_notification(NET_NOTIFY_STAT);
      }

        // ...else set "topping off" from 94% SOC:
      else if ((car_chargestate != 2) && (twizy_soc >= 9400))
      {
        // ...enter state 2=topping off:
        car_chargestate = 2;
        net_req_notification(NET_NOTIFY_STAT);
      }

    }

    // update "sufficient" threshold helpers:
    twizy_last_SOC = twizy_curr_SOC;
    twizy_last_idealrange = car_idealrange;

  }

  else
  {
    /*******************************************************************
     * NOT CHARGING
     */


    // Calculate range:
    if (twizy_range > 0)
    {
      car_estrange = MiFromKm(twizy_range);

      if (maxRange > 0)
        car_idealrange = (((float) maxRange) * twizy_soc) / 10000;
      else
        car_idealrange = car_estrange;
    }


    // Check if we've been charging before:
    if (car_chargestate <= 2)
    {
      // yes, check if we've reached 100.00% SOC:
      if (twizy_soc_max >= 10000)
      {
        // yes, means "done"
        car_chargestate = 4;
      }
      else
      {
        // no, means "stopped"
        car_chargestate = 21;
      }

      // Send charge alert:
      net_req_notification(NET_NOTIFY_CHARGE);
      net_req_notification(NET_NOTIFY_STAT);

      // Notifications will be sent in about 1 second
      // and will need car_doors1 & 0x04 set for proper text.
      // We'll keep the flag until next car use.
    }

    else if (car_doors1bits.CarON && !car_doors1bits.Charging && car_doors1bits.ChargePort)
    {
      // Car on, not charging, charge port open:
      // beginning the next car usage cycle:

      // Close charging port:
      car_doors1bits.ChargePort = 0;

      // Set charge state to "done":
      car_chargestate = 4;

      // reset SOC minimum:
      twizy_soc_min = twizy_soc;
      twizy_soc_min_range = twizy_range;
    }
  }


  /***************************************************************************
   * Notifications:
   */

  // send stream updates (GPS log) while car is moving:
  // (every 5 seconds for debug/test, should be per second if possible...)
  if ((twizy_speed > 0) && (sys_features[FEATURE_STREAM] & 2)
          && ((can_granular_tick % 5) == 0))
  {
    twizy_notify |= SEND_StreamUpdate;
  }


  /***************************************************************************
   * Check for button presses in STOP mode => CFG RESET:
   */

  if (twizy_button_cnt >= 3)
  {
    char cmd[6];

    // do CFG RESET with SMS reply:
    strcpypgm2ram(cmd, "RESET");
    if (vehicle_twizy_cfg_sms(TRUE, NULL, NULL, cmd))
      net_send_sms_finish();

    // reset button cnt:
    twizy_button_cnt = 0;
  }

  else if (twizy_button_cnt >= 1)
  {
    // pre-op also sends a CAN error, so for button_cnt >= 1
    // check if we're stuck in pre-op state:
    if ((readsdo(0x5110,0x00) == 0) && (twizy_sdo_data == 0x7f)
      && (readsdo(0x5000,0x01) == 0) && (twizy_sdo_data != 4)) {
      // we're in pre-op but not logged in, try to solve:
      if ((login(1) == 0) && (configmode(0) == 0))
        twizy_button_cnt = 0; // solved
    }
  }


  return FALSE;
}


////////////////////////////////////////////////////////////////////////
// twizy_state_ticker10()
// State Model: 10 second ticker
// This function is called approximately once every 10 seconds (since state
// was first entered), and gives the state a timeslice for activity.
//

BOOL vehicle_twizy_state_ticker10(void)
{
  // Check if CAN-Bus has turned offline:
  if (!(twizy_status & CAN_STATUS_ONLINE))
  {
    // yes, offline: implies...
    twizy_status = CAN_STATUS_OFFLINE;
    car_linevoltage = 0;
    car_chargecurrent = 0;
    twizy_speed = 0;
    twizy_power = 0;
  }
  else
  {
    // Clear online flag to test for CAN activity:
    twizy_status &= ~CAN_STATUS_ONLINE;
  }

  return FALSE;
}


////////////////////////////////////////////////////////////////////////
// twizy_state_ticker60()
// State Model: Per-minute ticker
// This function is called approximately once per minute (since state
// was first entered), and gives the state a timeslice for activity.
//

BOOL vehicle_twizy_state_ticker60(void)
{
  // Queue regular data update:
  twizy_notify |= SEND_DataUpdate;

  return FALSE;
}




////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/*******************************************************************************
 * COMMAND PLUGIN CLASSES: CODE DESIGN PATTERN
 *
 * A command class handles a group of related commands (functions).
 *
 * STANDARD METHODS:
 *
 *  MSG status output:
 *      char newstat = _msgp( char stat, int cmd, ... )
 *      stat: chaining status pushes with optional crc diff checking
 *
 *  MSG command handler:
 *      BOOL handled = _cmd( BOOL msgmode, int cmd, char *arguments )
 *      msgmode: FALSE=just execute / TRUE=also output reply
 *      arguments: "," separated (see MSG protocol)
 *
 *  SMS command handler:
 *      BOOL handled = _sms( BOOL premsg, char *caller, char *command, char *arguments )
 *      premsg: TRUE=replace system handler / FALSE=append to system handler
 *          (framework will first try TRUE, if not handled fall back to system
 *          handler and afterwards try again with FALSE)
 *      arguments: " " separated / free form
 *
 * STANDARD BEHAVIOUR:
 *
 *  cmd=0 / command=NULL shall map to the default action (push status).
 *      (if a specific MSG protocol push ID has been assigned,
 *       _msgp() shall use that for cmd=0)
 *
 * PLUGGING IN:
 *
 *  - add _cmd() to vehicle fn_commandhandler()
 *  - add _sms() to vehicle sms_cmdtable[]/sms_hfntable[]
 *
 */
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/***********************************************************************
 * COMMAND CLASS: POWER
 *
 *  MSG: CMD_PowerUsageNotify ()
 *  SMS: POWER
 *      - output power usage stats as text alert
 *
 *  MSG: CMD_PowerUsageStats ()
 *  SMS: -
 *      - output power usage stats H-MSG type RT-PWR-UsageStats
 *
 */


void vehicle_twizy_power_reset(void)
{
  PIE3bits.RXB0IE = 0; // disable interrupts
  {
    memset(twizy_speedpwr, 0, sizeof twizy_speedpwr);
    twizy_speed_state = CAN_SPEED_CONST;
    twizy_speed_distref = twizy_dist;
    twizy_level_use = 0;
    twizy_level_rec = 0;
  }
  PIE3bits.RXB0IE = 1; // enable interrupt

  memset(twizy_levelpwr, 0, sizeof twizy_levelpwr);
  twizy_level_odo = 0;
  twizy_level_alt = 0;

}


// Collect power usage sections:

void vehicle_twizy_power_collect(void)
{
  long dist;
  int alt_diff, grade_perc;
  unsigned long coll_use, coll_rec;

  if ((twizy_status & CAN_STATUS_KEYON) == 0)
  {
    // CAR is turned off
    return;
  }
  else if (car_stale_gps == 0)
  {
    // no GPS for 2 minutes: reset section
    twizy_level_odo = 0;
    twizy_level_alt = 0;

    return;
  }
  else if (twizy_level_odo == 0)
  {
    // start new section:

    twizy_level_odo = twizy_odometer;
    twizy_level_alt = car_altitude;

    PIE3bits.RXB0IE = 0; // disable interrupt
    {
      twizy_level_use = 0;
      twizy_level_rec = 0;
    }
    PIE3bits.RXB0IE = 1; // enable interrupt

    return;
  }

  // calc section length:
  dist = (twizy_odometer - twizy_level_odo) * 10;
  if (dist < CAN_LEVEL_MINSECTLEN)
  {
    // section too short to collect
    return;
  }

  // OK, read + reset collected power sums:
  PIE3bits.RXB0IE = 0; // disable interrupt
  {
    coll_use = twizy_level_use;
    coll_rec = twizy_level_rec;
    twizy_level_use = 0;
    twizy_level_rec = 0;
  }
  PIE3bits.RXB0IE = 1; // enable interrupt

  // calc grade in percent:
  alt_diff = car_altitude - twizy_level_alt;
  grade_perc = (long) alt_diff * 100 / (long) dist;

  // set new section reference:
  twizy_level_odo = twizy_odometer;
  twizy_level_alt = car_altitude;

  // collect:
  if (grade_perc >= CAN_LEVEL_THRESHOLD)
  {
    twizy_levelpwr[CAN_LEVEL_UP].dist += dist; // in m
    twizy_levelpwr[CAN_LEVEL_UP].hsum += alt_diff; // in m
    twizy_levelpwr[CAN_LEVEL_UP].use += coll_use;
    twizy_levelpwr[CAN_LEVEL_UP].rec += coll_rec;
  }
  else if (grade_perc <= -CAN_LEVEL_THRESHOLD)
  {
    twizy_levelpwr[CAN_LEVEL_DOWN].dist += dist; // in m
    twizy_levelpwr[CAN_LEVEL_DOWN].hsum += -alt_diff; // in m
    twizy_levelpwr[CAN_LEVEL_DOWN].use += coll_use;
    twizy_levelpwr[CAN_LEVEL_DOWN].rec += coll_rec;
  }
}

void vehicle_twizy_power_prepmsg(char mode)
{
  unsigned long pwr_dist, pwr_use, pwr_rec;
  UINT8 prc_const = 0, prc_accel = 0, prc_decel = 0;
  char *s;

  // overall sums:
  pwr_dist = twizy_speedpwr[CAN_SPEED_CONST].dist
          + twizy_speedpwr[CAN_SPEED_ACCEL].dist
          + twizy_speedpwr[CAN_SPEED_DECEL].dist;

  pwr_use = twizy_speedpwr[CAN_SPEED_CONST].use
          + twizy_speedpwr[CAN_SPEED_ACCEL].use
          + twizy_speedpwr[CAN_SPEED_DECEL].use;

  pwr_rec = twizy_speedpwr[CAN_SPEED_CONST].rec
          + twizy_speedpwr[CAN_SPEED_ACCEL].rec
          + twizy_speedpwr[CAN_SPEED_DECEL].rec;

  // distribution:
  if (pwr_dist > 0)
  {
    prc_const = (twizy_speedpwr[CAN_SPEED_CONST].dist * 1000 / pwr_dist + 5) / 10;
    prc_accel = (twizy_speedpwr[CAN_SPEED_ACCEL].dist * 1000 / pwr_dist + 5) / 10;
    prc_decel = (twizy_speedpwr[CAN_SPEED_DECEL].dist * 1000 / pwr_dist + 5) / 10;
  }

  s = strchr(net_scratchpad, 0); // append to net_scratchpad

  if ((pwr_use == 0) || (pwr_dist <= 10))
  {
    // not driven: only output power totals:
    s = stp_ul(s, "Power -", (pwr_use + 11250) / 22500);
    s = stp_ul(s, " +", (pwr_rec + 11250) / 22500);
    s = stp_rom(s, " Wh");
  }
  else if (mode == 't')
  {
    // power totals:
    s = stp_ul(s, "Power -", (pwr_use + 11250) / 22500);
    s = stp_ul(s, " +", (pwr_rec + 11250) / 22500);
    s = stp_l2f(s, " Wh ", pwr_dist / 1000, 1);
    s = stp_i(s, " km\r Const ", prc_const);
    s = stp_ul(s, "% -", (twizy_speedpwr[CAN_SPEED_CONST].use + 11250) / 22500);
    s = stp_ul(s, " +", (twizy_speedpwr[CAN_SPEED_CONST].rec + 11250) / 22500);
    s = stp_i(s, " Wh\r Accel ", prc_accel);
    s = stp_ul(s, "% -", (twizy_speedpwr[CAN_SPEED_ACCEL].use + 11250) / 22500);
    s = stp_ul(s, " +", (twizy_speedpwr[CAN_SPEED_ACCEL].rec + 11250) / 22500);
    s = stp_i(s, " Wh\r Decel ", prc_decel);
    s = stp_ul(s, "% -", (twizy_speedpwr[CAN_SPEED_DECEL].use + 11250) / 22500);
    s = stp_ul(s, " +", (twizy_speedpwr[CAN_SPEED_DECEL].rec + 11250) / 22500);
    s = stp_ul(s, " Wh\r Up ", twizy_levelpwr[CAN_LEVEL_UP].hsum);
    s = stp_ul(s, "m -", (twizy_levelpwr[CAN_LEVEL_UP].use + 11250) / 22500);
    s = stp_ul(s, " +", (twizy_levelpwr[CAN_LEVEL_UP].rec + 11250) / 22500);
    s = stp_ul(s, " Wh\r Down ", twizy_levelpwr[CAN_LEVEL_DOWN].hsum);
    s = stp_ul(s, "m -", (twizy_levelpwr[CAN_LEVEL_DOWN].use + 11250) / 22500);
    s = stp_ul(s, " +", (twizy_levelpwr[CAN_LEVEL_DOWN].rec + 11250) / 22500);
    s = stp_rom(s, " Wh");
  }
  else // mode == 'e'
  {
    // power efficiency (default):
    long pwr;
    unsigned long dist;

    // speed distances are in 1/10 m

    dist = pwr_dist;
    pwr = pwr_use - pwr_rec;
    if ((pwr_use > 0) && (dist > 0))
    {
      s = stp_l(s, "Efficiency ", (pwr / dist * 10000 + 11250) / 22500);
      s = stp_i(s, " Wh/km R=", (pwr_rec * 1000 / pwr_use + 5) / 10);
      s = stp_rom(s, "%");
    }

    pwr_use = twizy_speedpwr[CAN_SPEED_CONST].use;
    pwr_rec = twizy_speedpwr[CAN_SPEED_CONST].rec;
    dist = twizy_speedpwr[CAN_SPEED_CONST].dist;
    pwr = pwr_use - pwr_rec;
    if ((pwr_use > 0) && (dist > 0))
    {
      s = stp_i(s, "\r Const ", prc_const);
      s = stp_l(s, "% ", (pwr / dist * 10000 + 11250) / 22500);
      s = stp_i(s, " Wh/km R=", (pwr_rec * 1000 / pwr_use + 5) / 10);
      s = stp_rom(s, "%");
    }

    pwr_use = twizy_speedpwr[CAN_SPEED_ACCEL].use;
    pwr_rec = twizy_speedpwr[CAN_SPEED_ACCEL].rec;
    dist = twizy_speedpwr[CAN_SPEED_ACCEL].dist;
    pwr = pwr_use - pwr_rec;
    if ((pwr_use > 0) && (dist > 0))
    {
      s = stp_i(s, "\r Accel ", prc_accel);
      s = stp_l(s, "% ", (pwr / dist * 10000 + 11250) / 22500);
      s = stp_i(s, " Wh/km R=", (pwr_rec * 1000 / pwr_use + 5) / 10);
      s = stp_rom(s, "%");
    }

    pwr_use = twizy_speedpwr[CAN_SPEED_DECEL].use;
    pwr_rec = twizy_speedpwr[CAN_SPEED_DECEL].rec;
    dist = twizy_speedpwr[CAN_SPEED_DECEL].dist;
    pwr = pwr_use - pwr_rec;
    if ((pwr_use > 0) && (dist > 0))
    {
      s = stp_i(s, "\r Decel ", prc_decel);
      s = stp_l(s, "% ", (pwr / dist * 10000 + 11250) / 22500);
      s = stp_i(s, " Wh/km R=", (pwr_rec * 1000 / pwr_use + 5) / 10);
      s = stp_rom(s, "%");
    }

    // level distances are in 1 m

    pwr_use = twizy_levelpwr[CAN_LEVEL_UP].use;
    pwr_rec = twizy_levelpwr[CAN_LEVEL_UP].rec;
    dist = twizy_levelpwr[CAN_LEVEL_UP].dist;
    pwr = pwr_use - pwr_rec;
    if ((pwr_use > 0) && (dist > 0))
    {
      s = stp_i(s, "\r Up ", twizy_levelpwr[CAN_LEVEL_UP].hsum);
      s = stp_l(s, "m ", (pwr / dist * 1000 + 11250) / 22500);
      s = stp_i(s, " Wh/km R=", (pwr_rec * 1000 / pwr_use + 5) / 10);
      s = stp_rom(s, "%");
    }

    pwr_use = twizy_levelpwr[CAN_LEVEL_DOWN].use;
    pwr_rec = twizy_levelpwr[CAN_LEVEL_DOWN].rec;
    dist = twizy_levelpwr[CAN_LEVEL_DOWN].dist;
    pwr = pwr_use - pwr_rec;
    if ((pwr_use > 0) && (dist > 0))
    {
      s = stp_i(s, "\r Down ", twizy_levelpwr[CAN_LEVEL_DOWN].hsum);
      s = stp_l(s, "m ", (pwr / dist * 1000 + 11250) / 22500);
      s = stp_i(s, " Wh/km R=", (pwr_rec * 1000 / pwr_use + 5) / 10);
      s = stp_rom(s, "%");
    }

  }
}

char vehicle_twizy_power_msgp(char stat, int cmd)
{
  static WORD crc;
  char *s;

  if (cmd == CMD_PowerUsageStats)
  {
    /* Output power usage statistics:
     *
     * MP-0 HRT-PWR-UsageStats,0,86400
     *  ,<speed_CONST_dist>,<speed_CONST_use>,<speed_CONST_rec>
     *  ,<speed_ACCEL_dist>,<speed_ACCEL_use>,<speed_ACCEL_rec>
     *  ,<speed_DECEL_dist>,<speed_DECEL_use>,<speed_DECEL_rec>
     *  ,<level_UP_dist>,<level_UP_hsum>,<level_UP_use>,<level_UP_rec>
     *  ,<level_DOWN_dist>,<level_DOWN_hsum>,<level_DOWN_use>,<level_DOWN_rec>
     *
     */

    s = stp_rom(net_scratchpad, "MP-0 HRT-PWR-UsageStats,0,86400");

    s = stp_ul(s, ",", twizy_speedpwr[CAN_SPEED_CONST].dist);
    s = stp_ul(s, ",", twizy_speedpwr[CAN_SPEED_CONST].use);
    s = stp_ul(s, ",", twizy_speedpwr[CAN_SPEED_CONST].rec);

    s = stp_ul(s, ",", twizy_speedpwr[CAN_SPEED_ACCEL].dist);
    s = stp_ul(s, ",", twizy_speedpwr[CAN_SPEED_ACCEL].use);
    s = stp_ul(s, ",", twizy_speedpwr[CAN_SPEED_ACCEL].rec);

    s = stp_ul(s, ",", twizy_speedpwr[CAN_SPEED_DECEL].dist);
    s = stp_ul(s, ",", twizy_speedpwr[CAN_SPEED_DECEL].use);
    s = stp_ul(s, ",", twizy_speedpwr[CAN_SPEED_DECEL].rec);

    s = stp_ul(s, ",", twizy_levelpwr[CAN_LEVEL_UP].dist);
    s = stp_ul(s, ",", twizy_levelpwr[CAN_LEVEL_UP].hsum);
    s = stp_ul(s, ",", twizy_levelpwr[CAN_LEVEL_UP].use);
    s = stp_ul(s, ",", twizy_levelpwr[CAN_LEVEL_UP].rec);

    s = stp_ul(s, ",", twizy_levelpwr[CAN_LEVEL_DOWN].dist);
    s = stp_ul(s, ",", twizy_levelpwr[CAN_LEVEL_DOWN].hsum);
    s = stp_ul(s, ",", twizy_levelpwr[CAN_LEVEL_DOWN].use);
    s = stp_ul(s, ",", twizy_levelpwr[CAN_LEVEL_DOWN].rec);

    stat = net_msg_encode_statputs(stat, &crc);
  }

  return stat;
}

BOOL vehicle_twizy_power_sms(BOOL premsg, char *caller, char *command, char *arguments)
{
  char argc1;

  if (!premsg)
    return FALSE;

  // check SMS notifies:
  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  if (!caller || !*caller)
  {
    caller = par_get(PARAM_REGPHONE);
    if (!caller || !*caller)
      return FALSE;
  }

  // argc1 = mode: 't' = totals, fallback 'e' = efficiency
  argc1 = (arguments) ? (arguments[0] | 0x20) : 'e';

  // prepare message:
  net_scratchpad[0] = 0;
  vehicle_twizy_power_prepmsg(argc1);
  cr2lf(net_scratchpad);

  // OK, send SMS:
  delay100(2);
  net_send_sms_start(caller);
  net_puts_ram(net_scratchpad);

  return TRUE; // handled
}

BOOL vehicle_twizy_power_cmd(BOOL msgmode, int cmd, char *arguments)
{
  char *s;
  char argc1;

  if (cmd == CMD_PowerUsageStats)
  {
    // send statistics:
    if (msgmode)
    {
      vehicle_twizy_power_msgp(0, cmd);

      // msg command response:
      s = stp_i(net_scratchpad, "MP-0 c", cmd);
      s = stp_rom(s, ",0");
      net_msg_encode_puts();
    }
    else
    {
      if (vehicle_twizy_power_msgp(2, cmd) != 2)
        net_msg_send();
    }

    return TRUE;
  }

  else // cmd == CMD_PowerUsageNotify(mode)
  {
    // argc1 = mode: 't' = totals, fallback 'e' = efficiency
    argc1 = (arguments) ? (arguments[0] | 0x20) : 'e';
    
    // send text alert:
    strcpypgm2ram(net_scratchpad, "MP-0 PA");
    vehicle_twizy_power_prepmsg(argc1);

    if (msgmode)
    {
      net_msg_encode_puts();

      // msg command response:
      s = stp_i(net_scratchpad, "MP-0 c", cmd);
      s = stp_rom(s, ",0");
      net_msg_encode_puts();
    }
    else
    {
      net_msg_start();
      net_msg_encode_puts();
      net_msg_send();
    }

    return TRUE;
  }

  return FALSE;
}



/***********************************************************************
 * COMMAND CLASS: DEBUG
 *
 *  MSG: CMD_Debug ()
 *  SMS: DEBUG
 *      - output internal state dump for debugging
 *
 */

char vehicle_twizy_debug_msgp(char stat, int cmd)
{
  //static WORD crc; // diff crc for push msgs
  char *s;

  stat = net_msgp_environment(stat);

  s = stp_rom(net_scratchpad, "MP-0 ");
  s = stp_i(s, "c", cmd ? cmd : CMD_Debug);
  s = stp_x(s, ",0,", twizy_status);
  s = stp_x(s, ",", car_doors1);
  s = stp_x(s, ",", car_doors5);
  s = stp_i(s, ",", car_chargestate);
  s = stp_i(s, ",", twizy_speed);
  s = stp_i(s, ",", twizy_power);
  s = stp_ul(s, ",", twizy_odometer);
  s = stp_i(s, ",", twizy_soc);
  s = stp_i(s, ",", twizy_soc_min);
  s = stp_i(s, ",", twizy_soc_max);
  s = stp_i(s, ",", twizy_range);
  s = stp_i(s, ",", twizy_soc_min_range);
  s = stp_i(s, ",", car_estrange);
  s = stp_i(s, ",", car_idealrange);
  s = stp_i(s, ",", can_minSOCnotified);

  net_msg_encode_puts();
  return (stat == 2) ? 1 : stat;
}

BOOL vehicle_twizy_debug_cmd(BOOL msgmode, int cmd, char *arguments)
{
#ifdef OVMS_DIAGMODULE
  if (arguments && *arguments)
  {
    // Run simulator:
    vehicle_twizy_simulator_run(atoi(arguments));
  }
#endif // OVMS_DIAGMODULE

  if (msgmode)
    vehicle_twizy_debug_msgp(0, cmd);

  return TRUE;
}

BOOL vehicle_twizy_debug_sms(BOOL premsg, char *caller, char *command, char *arguments)
{
  char *s;

#ifdef OVMS_DIAGMODULE
  if (arguments && *arguments)
  {
    // Run simulator:
    vehicle_twizy_simulator_run(atoi(arguments));
  }
#endif // OVMS_DIAGMODULE

  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  if (!caller || !*caller)
  {
    caller = par_get(PARAM_REGPHONE);
    if (!caller || !*caller)
      return FALSE;
  }

  if (premsg)
    net_send_sms_start(caller);

  // SMS PART:

  s = net_scratchpad;
  s = stp_x(s, " STS=", twizy_status);
  s = stp_i(s, " BC=", twizy_button_cnt);

  //s = stp_x(s, " DS1=", car_doors1);
  //s = stp_x(s, " DS5=", car_doors5);
  //s = stp_i(s, " MSN=", can_minSOCnotified);
  //s = stp_i(s, " CHG=", car_chargestate);
  //s = stp_i(s, " SPD=", twizy_speed);
  //s = stp_i(s, " PWR=", twizy_power);
  //s = stp_ul(s, " ODO=", twizy_odometer);
  //s = stp_ul(s, " di=", twizy_dist);
  //s = stp_ul(s, " dr=", twizy_speed_distref);
  //s = stp_i(s, " SOC=", twizy_soc);
  //s = stp_i(s, " SMIN=", twizy_soc_min);
  //s = stp_i(s, " SMAX=", twizy_soc_max);
  //s = stp_i(s, " CRNG=", twizy_range);
  //s = stp_i(s, " MRNG=", twizy_soc_min_range);
  //s = stp_i(s, " ERNG=", car_estrange);
  //s = stp_i(s, " IRNG=", car_idealrange);
  s = stp_i(s, " SQ=", net_sq);
  //s = stp_i(s, " NET=L", net_link);
  //s = stp_i(s, "/S", net_msg_serverok);
  //s = stp_i(s, "/A", net_apps_connected);
  //s = stp_x(s, " NTF=", twizy_notify);
  //s = stp_i(s, " MSP=", net_msg_sendpending);
  net_puts_ram(net_scratchpad);

#ifdef OVMS_DIAGMODULE
  // ...MORE IN DIAG MODE (serial port):
  if (net_state == NET_STATE_DIAGMODE)
  {
    s = net_scratchpad;
    s = stp_i(s, "\n# FIX=", car_gpslock);
    s = stp_lx(s, " LAT=", car_latitude);
    s = stp_lx(s, " LON=", car_longitude);
    s = stp_i(s, " ALT=", car_altitude);
    s = stp_i(s, " DIR=", car_direction);
    net_puts_ram(net_scratchpad);
  }
#endif // OVMS_DIAGMODULE

#ifdef OVMS_TWIZY_BATTMON
  // battery bit fields:
  s = net_scratchpad;
  s = stp_x(s, "\n# vw=", twizy_batt[0].volt_watches);
  s = stp_x(s, " va=", twizy_batt[0].volt_alerts);
  s = stp_x(s, " lva=", twizy_batt[0].last_volt_alerts);
  s = stp_x(s, " tw=", twizy_batt[0].temp_watches);
  s = stp_x(s, " ta=", twizy_batt[0].temp_alerts);
  s = stp_x(s, " lta=", twizy_batt[0].last_temp_alerts);
  s = stp_x(s, " ss=", twizy_batt_sensors_state);
  net_puts_ram(net_scratchpad);
#endif // OVMS_TWIZY_BATTMON


  return TRUE;
}



/***********************************************************************
 * COMMAND CLASS: CHARGE STATUS / ALERT
 *
 *  MSG: CMD_Alert()
 *  SMS: STAT
 *      - output charge status
 *
 */

// prepmsg: Generate STAT message for SMS & MSG mode
// - no charge mode
// - output estrange
// - output twizy_soc_min + twizy_soc_max
// - output odometer
//
// => cat to net_scratchpad (to be sent as SMS or MSG)
//    line breaks: default \r for MSG mode >> cr2lf >> SMS

void vehicle_twizy_stat_prepmsg(void)
{
  char *s;
  
  // append to net_scratchpad:
  s = strchr(net_scratchpad, 0);

  if (car_doors1bits.ChargePort)
  {
    // Charge port door is open, we are charging
    switch (car_chargestate)
    {
    case 0x01:
      s = stp_rom(s, "Charging");
      break;
    case 0x02:
      s = stp_rom(s, "Charging, Topping off");
      break;
    case 0x04:
      s = stp_rom(s, "Charging Done");
      break;
    default:
      s = stp_rom(s, "Charging Stopped");
    }

    // Power sum:
    s = stp_ul(s, "\r CHG: ", (twizy_speedpwr[CAN_SPEED_CONST].rec + 11250) / 22500);
    s = stp_rom(s, " Wh");

    if (car_chargestate != 4)
    {
      // Not done yet; estimated time remaining for 100%:
      s = stp_i(s, "\r Full: ", vehicle_twizy_chargetime(10000));
      s = stp_rom(s, " min.");
    }

  }
  else
  {
    // Charge port door is closed, not charging
    s = stp_rom(s, "Not charging");

    // Estimated charge time for 100%:
    if (twizy_soc < 10000)
    {
      s = stp_i(s, "\r Full charge: ", vehicle_twizy_chargetime(10000));
      s = stp_rom(s, " min.");
    }
  }


  // Estimated + Ideal Range:
  if (can_mileskm == 'M')
  {
    s = stp_i(s, "\r Range: ", car_estrange);
    s = stp_i(s, " - ", car_idealrange);
    s = stp_rom(s, " mi");
  }
  else
  {
    s = stp_i(s, "\r Range: ", KmFromMi(car_estrange));
    s = stp_i(s, " - ", KmFromMi(car_idealrange));
    s = stp_rom(s, " km");
  }


  // SOC + min/max:
  s = stp_l2f(s, "\r SOC: ", twizy_soc, 2);
  s = stp_l2f(s, "% (", twizy_soc_min, 2);
  s = stp_l2f(s, "%..", twizy_soc_max, 2);
  s = stp_rom(s, "%)");

  // ODOMETER:
  if (can_mileskm == 'M')
  {
    s = stp_ul(s, "\r ODO: ", car_odometer / 10);
    s = stp_rom(s, " mi");
  }
  else
  {
    s = stp_ul(s, "\r ODO: ", KmFromMi(car_odometer / 10));
    s = stp_rom(s, " km");
  }

}

BOOL vehicle_twizy_stat_sms(BOOL premsg, char *caller, char *command, char *arguments)
{
  // check for replace mode:
  if (!premsg)
    return FALSE;

  // check SMS notifies:
  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  if (!caller || !*caller)
  {
    caller = par_get(PARAM_REGPHONE);
    if (!caller || !*caller)
      return FALSE;
  }

  // prepare message:
  net_scratchpad[0] = 0;
  vehicle_twizy_stat_prepmsg();
  cr2lf(net_scratchpad);

  // OK, start SMS:
  delay100(2);
  net_send_sms_start(caller);
  net_puts_ram(net_scratchpad);

  return TRUE; // handled
}

BOOL vehicle_twizy_alert_cmd(BOOL msgmode, int cmd, char *arguments)
{
  // prepare message:
  strcpypgm2ram(net_scratchpad, (char const rom far*)"MP-0 PA");
  vehicle_twizy_stat_prepmsg();

  net_msg_encode_puts();

  return TRUE;
}



/***********************************************************************
 * COMMAND CLASS: MAX RANGE CONFIG
 *
 *  MSG: CMD_QueryRange()
 *  SMS: RANGE?
 *      - output current max ideal range
 *
 *  MSG: CMD_SetRange(range)
 *  SMS: RANGE [range]
 *      - set/clear max ideal range
 *      - range: in user units (mi/km)
 *
 */

char vehicle_twizy_range_msgp(char stat, int cmd)
{
  //static WORD crc; // diff crc for push msgs
  char *s;

  s = stp_rom(net_scratchpad, "MP-0 ");
  s = stp_i(s, "c", cmd ? cmd : CMD_QueryRange);
  s = stp_i(s, ",0,", sys_features[FEATURE_MAXRANGE]);

  //return net_msg_encode_statputs( stat, &crc );
  net_msg_encode_puts();
  return (stat == 2) ? 1 : stat;
}

BOOL vehicle_twizy_range_cmd(BOOL msgmode, int cmd, char *arguments)
{
  if (cmd == CMD_SetRange)
  {
    if (arguments && *arguments)
      sys_features[FEATURE_MAXRANGE] = atoi(arguments);
    else
      sys_features[FEATURE_MAXRANGE] = 0;

    par_set(PARAM_FEATURE_BASE + FEATURE_MAXRANGE, arguments);
  }

  // CMD_QueryRange
  if (msgmode)
    vehicle_twizy_range_msgp(0, cmd);

  return TRUE;
}

BOOL vehicle_twizy_range_sms(BOOL premsg, char *caller, char *command, char *arguments)
{
  char *s;

  if (!premsg)
    return FALSE;

  if (!caller || !*caller)
  {
    caller = par_get(PARAM_REGPHONE);
    if (!caller || !*caller)
      return FALSE;
  }

  // RANGE (maxrange)
  if (command && (command[5] != '?'))
  {
    if (arguments && *arguments)
      sys_features[FEATURE_MAXRANGE] = atoi(arguments);
    else
      sys_features[FEATURE_MAXRANGE] = 0;

    par_set(PARAM_FEATURE_BASE + FEATURE_MAXRANGE, arguments);
  }

  // RANGE?
  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE; // handled, but no SMS has been started

  // Reply current range:
  net_send_sms_start(caller);

  s = stp_rom(net_scratchpad, "Max ideal range: ");

  if (sys_features[FEATURE_MAXRANGE] == 0)
  {
    s = stp_rom(s, "UNSET");
  }
  else
  {
    s = stp_i(s, NULL, sys_features[FEATURE_MAXRANGE]);
    s = stp_rom(s, (can_mileskm == 'M') ? " mi" : " km");
  }

  net_puts_ram(net_scratchpad);

  return TRUE;
}


/***********************************************************************
 * COMMAND CLASS: CHARGE ALERT CONFIG
 *
 *  MSG: CMD_QueryChargeAlerts()
 *  SMS: CA?
 *      - output current charge alerts
 *
 *  MSG: CMD_SetChargeAlerts(range,soc)
 *  SMS: CA [range] [SOC"%"]
 *      - set/clear charge alerts
 *      - range: in user units (mi/km)
 *      - SMS recognizes 0-2 args, unit "%" = SOC
 *
 */

char vehicle_twizy_ca_msgp(char stat, int cmd)
{
  //static WORD crc; // diff crc for push msgs
  char *s;
  int etr_soc = 0, etr_range = 0, maxrange;

  // calculate ETR for SUFFSOC:
  etr_soc = vehicle_twizy_chargetime((int) sys_features[FEATURE_SUFFSOC]*100);

  // calculate ETR for SUFFRANGE:

  if (sys_features[FEATURE_MAXRANGE] > 0)
  {
    maxrange = sys_features[FEATURE_MAXRANGE];
  }
  else
  {
    if (twizy_soc_min && twizy_soc_min_range)
      maxrange = (((float) twizy_soc_min_range) / twizy_soc_min) * 10000;
    else
      maxrange = 0;
    if (can_mileskm == 'M')
      maxrange = MiFromKm(maxrange);
  }

  etr_range = (maxrange) ? vehicle_twizy_chargetime(
          (long) sys_features[FEATURE_SUFFRANGE] * 10000 / maxrange) : 0;

  // Send command reply:
  s = stp_rom(net_scratchpad, "MP-0 ");
  s = stp_i(s, "c", cmd ? cmd : CMD_QueryChargeAlerts);
  s = stp_i(s, ",0,", sys_features[FEATURE_SUFFRANGE]);
  s = stp_i(s, ",", sys_features[FEATURE_SUFFSOC]);
  s = stp_i(s, ",", etr_range);
  s = stp_i(s, ",", etr_soc);
  s = stp_i(s, ",", vehicle_twizy_chargetime(10000));

  //return net_msg_encode_statputs( stat, &crc );
  net_msg_encode_puts();
  return (stat == 2) ? 1 : stat;
}

BOOL vehicle_twizy_ca_cmd(BOOL msgmode, int cmd, char *arguments)
{
  if (cmd == CMD_SetChargeAlerts)
  {
    char *range = NULL, *soc = NULL;

    if (arguments && *arguments)
    {
      range = strtokpgmram(arguments, ",");
      soc = strtokpgmram(NULL, ",");
    }

    sys_features[FEATURE_SUFFRANGE] = range ? atoi(range) : 0;
    sys_features[FEATURE_SUFFSOC] = soc ? atoi(soc) : 0;

    // store new alerts into EEPROM:
    par_set(PARAM_FEATURE_BASE + FEATURE_SUFFRANGE, range);
    par_set(PARAM_FEATURE_BASE + FEATURE_SUFFSOC, soc);
  }

  // CMD_QueryChargeAlerts
  if (msgmode)
    vehicle_twizy_ca_msgp(0, cmd);

  return TRUE;
}

BOOL vehicle_twizy_ca_sms(BOOL premsg, char *caller, char *command, char *arguments)
{
  char *s;

  if (!premsg)
    return FALSE;

  if (!caller || !*caller)
  {
    caller = par_get(PARAM_REGPHONE);
    if (!caller || !*caller)
      return FALSE;
  }

  if (command && (command[2] != '?'))
  {
    // SET CHARGE ALERTS:

    int value;
    char unit;
    unsigned char f;
    char *arg_suffsoc = NULL, *arg_suffrange = NULL;

    // clear current alerts:
    sys_features[FEATURE_SUFFSOC] = 0;
    sys_features[FEATURE_SUFFRANGE] = 0;

    // read new alerts from arguments:
    while (arguments && *arguments)
    {
      value = atoi(arguments);
      unit = arguments[strlen(arguments) - 1];

      if (unit == '%')
      {
        arg_suffsoc = arguments;
        sys_features[FEATURE_SUFFSOC] = value;
      }
      else
      {
        arg_suffrange = arguments;
        sys_features[FEATURE_SUFFRANGE] = value;
      }

      arguments = net_sms_nextarg(arguments);
    }

    // store new alerts into EEPROM:
    par_set(PARAM_FEATURE_BASE + FEATURE_SUFFSOC, arg_suffsoc);
    par_set(PARAM_FEATURE_BASE + FEATURE_SUFFRANGE, arg_suffrange);
  }


  // REPLY current charge alerts and estimated charge time remaining:

  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE; // handled, but no SMS has been started

  net_send_sms_start(caller);

  s = stp_rom(net_scratchpad, "Charge Alert: ");

  if ((sys_features[FEATURE_SUFFSOC] == 0) && (sys_features[FEATURE_SUFFRANGE] == 0))
  {
    s = stp_rom(s, "OFF");
  }
  else
  {
    int etr_soc = 0, etr_range = 0, maxrange;

    if (sys_features[FEATURE_SUFFSOC] > 0)
    {
      // calculate ETR for SUFFSOC:
      etr_soc = vehicle_twizy_chargetime((int) sys_features[FEATURE_SUFFSOC]*100);

      // output SUFFSOC:
      s = stp_i(s, NULL, sys_features[FEATURE_SUFFSOC]);
      s = stp_rom(s, "%");
    }

    if (sys_features[FEATURE_SUFFRANGE] > 0)
    {
      // calculate ETR for SUFFRANGE:

      if (sys_features[FEATURE_MAXRANGE] > 0)
      {
        maxrange = sys_features[FEATURE_MAXRANGE];
      }
      else
      {
        if (twizy_soc_min && twizy_soc_min_range)
          maxrange = (((float) twizy_soc_min_range) / twizy_soc_min) * 10000;
        else
          maxrange = 0;
        if (can_mileskm == 'M')
          maxrange = MiFromKm(maxrange);
      }

      etr_range = (maxrange) ? vehicle_twizy_chargetime(
              (long) sys_features[FEATURE_SUFFRANGE] * 10000 / maxrange) : 0;

      // output SUFFRANGE:
      if (sys_features[FEATURE_SUFFSOC] > 0)
        s = stp_rom(s, " or ");
      s = stp_i(s, NULL, sys_features[FEATURE_SUFFRANGE]);
      s = stp_rom(s, (can_mileskm == 'M') ? " mi" : " km");
    }

    // output smallest ETR:
    if ((etr_range > 0) && ((etr_soc == 0) || (etr_range < etr_soc)))
      s = stp_i(s, "\n Time: ", etr_range);
    else if (etr_soc > 0)
      s = stp_i(s, "\n Time: ", etr_soc);
    s = stp_rom(s, " min.");

  }

  // Estimated time for 100%:
  s = stp_i(s, "\n Full charge: ", vehicle_twizy_chargetime(10000));
  s = stp_rom(s, " min.");

  net_puts_ram(net_scratchpad);

  return TRUE;
}




#ifdef OVMS_TWIZY_BATTMON

/***********************************************************************
 * COMMAND CLASS: BATTERY MONITORING
 *
 *  MSG: CMD_BatteryStatus()
 *  SMS: BATT
 *      - output current battery alert & watch status
 *
 *  SMS: BATT R
 *      - reset alerts & watches
 *
 *  SMS: BATT V
 *      - output current pack & cell voltages
 *
 *  SMS: BATT VD
 *      - output recorded max cell voltage deviations
 *
 *  SMS: BATT T
 *      - output current pack & cell temperatures
 *
 *  SMS: BATT TD
 *      - output recorded max cell temperature deviations
 *
 */

// Conversion utils:
#define CONV_PackVolt(m) ((UINT)(m) * (100 / 10))
#define CONV_CellVolt(m) ((UINT)(m) * (1000 / 200))
#define CONV_CellVoltS(m) ((INT)(m) * (1000 / 200))
#define CONV_Temp(m) ((INT)(m) - 40)


// Wait max. 1.1 seconds for consistent sensor data:

BOOL vehicle_twizy_battstatus_wait4sensors(void)
{
  UINT8 n;

  if ((twizy_status & CAN_STATUS_OFFLINE)) // only wait if CAN is online
    return TRUE;

  for (n = 11; (twizy_batt_sensors_state != BATT_SENSORS_READY) && (n > 0); n--)
    delay100(1);

  return (twizy_batt_sensors_state == BATT_SENSORS_READY);
}


// Reset battery monitor min/max/maxdev & alerts:

void vehicle_twizy_battstatus_reset(void)
{
  int i;

  vehicle_twizy_battstatus_wait4sensors();

  for (i = 0; i < BATT_CELLS; i++)
  {
    twizy_cell[i].volt_max = twizy_cell[i].volt_act;
    twizy_cell[i].volt_min = twizy_cell[i].volt_act;
    twizy_cell[i].volt_maxdev = 0;
  }

  for (i = 0; i < BATT_CMODS; i++)
  {
    twizy_cmod[i].temp_max = twizy_cmod[i].temp_act;
    twizy_cmod[i].temp_min = twizy_cmod[i].temp_act;
    twizy_cmod[i].temp_maxdev = 0;
  }

  for (i = 0; i < BATT_PACKS; i++)
  {
    twizy_batt[i].volt_min = twizy_batt[i].volt_act;
    twizy_batt[i].volt_max = twizy_batt[i].volt_act;
    twizy_batt[i].cell_volt_stddev_max = 0;
    twizy_batt[i].cmod_temp_stddev_max = 0;
    twizy_batt[i].temp_watches = 0;
    twizy_batt[i].temp_alerts = 0;
    twizy_batt[i].volt_watches = 0;
    twizy_batt[i].volt_alerts = 0;
    twizy_batt[i].last_temp_alerts = 0;
    twizy_batt[i].last_volt_alerts = 0;
  }

  twizy_batt_sensors_state = BATT_SENSORS_START;
}


// Collect battery voltages & temperatures:

void vehicle_twizy_battstatus_collect(void)
{
  UINT i, stddev, absdev;
  INT dev;
  UINT32 sum, sqrsum;
  float f, m;

  // only if consistent sensor state has been reached:
  if (twizy_batt_sensors_state != BATT_SENSORS_READY)
    return;

  /*
  if( (net_buf_mode == NET_BUF_CRLF) && (net_buf_pos == 0) )
  {
      char *s;
      s = stp_i( net_scratchpad, "# tic1: ss=", twizy_batt_sensors_state );
      s = stp_rom( s, "\r\n" );
      net_puts_ram( net_scratchpad );
  }
   */

  // *********** Temperatures: ************

  // build mean value & standard deviation:
  sum = 0;
  sqrsum = 0;

  for (i = 0; i < BATT_CMODS; i++)
  {
    // Validate:
    if ((twizy_cmod[i].temp_act == 0) || (twizy_cmod[i].temp_act >= 0x0f0))
      break;

    // Remember min:
    if ((twizy_cmod[i].temp_min == 0) || (twizy_cmod[i].temp_act < twizy_cmod[i].temp_min))
      twizy_cmod[i].temp_min = twizy_cmod[i].temp_act;

    // Remember max:
    if ((twizy_cmod[i].temp_max == 0) || (twizy_cmod[i].temp_act > twizy_cmod[i].temp_max))
      twizy_cmod[i].temp_max = twizy_cmod[i].temp_act;

    // build sums:
    sum += twizy_cmod[i].temp_act;
    sqrsum += SQR((UINT32) twizy_cmod[i].temp_act);
  }

  if (i == BATT_CMODS)
  {
    // All values valid, process:

    m = (float) sum / BATT_CMODS;

    car_tbattery = (signed char) m + 0.5 - 40;
    car_stale_temps = 120; // Reset stale indicator

    //stddev = (unsigned int) sqrt( ((float)sqrsum/BATT_CMODS) - SQR((float)sum/BATT_CMODS) ) + 1;
    // this is too complex for C18, we need to split this up:
    f = ((float) sqrsum / BATT_CMODS) - SQR(m);
    stddev = sqrt(f) + 0.5;
    if (stddev == 0)
      stddev = 1; // not enough precision to allow stddev 0

    // check max stddev:
    if (stddev > twizy_batt[0].cmod_temp_stddev_max)
    {
      twizy_batt[0].cmod_temp_stddev_max = stddev;

      // switch to overall stddev alert mode?
      // (resetting cmod flags to build new alert set)
      if (stddev >= BATT_STDDEV_TEMP_ALERT)
        twizy_batt[0].temp_alerts = BATT_STDDEV_TEMP_FLAG;
      else if (stddev >= BATT_STDDEV_TEMP_WATCH)
        twizy_batt[0].temp_watches = BATT_STDDEV_TEMP_FLAG;
    }

    // check cmod deviations:
    for (i = 0; i < BATT_CMODS; i++)
    {
      // deviation:
      dev = (twizy_cmod[i].temp_act - m)
              + ((twizy_cmod[i].temp_act >= m) ? 0.5 : -0.5);
      absdev = ABS(dev);

      // Set watch/alert flags:
      // (applying overall thresholds only in stddev alert mode)
      if ((twizy_batt[0].temp_alerts & BATT_STDDEV_TEMP_FLAG)
              && (absdev >= BATT_STDDEV_TEMP_ALERT))
        twizy_batt[0].temp_alerts |= (1 << i);
      else if (absdev >= BATT_DEV_TEMP_ALERT)
        twizy_batt[0].temp_alerts |= (1 << i);
      else if ((twizy_batt[0].temp_watches & BATT_STDDEV_TEMP_FLAG)
              && (absdev >= BATT_STDDEV_TEMP_WATCH))
        twizy_batt[0].temp_watches |= (1 << i);
      else if (absdev > stddev)
        twizy_batt[0].temp_watches |= (1 << i);

      // Remember max deviation:
      if (absdev > ABS(twizy_cmod[i].temp_maxdev))
        twizy_cmod[i].temp_maxdev = (INT8) dev;
    }

  } // if( i == BATT_CMODS )


  // ********** Voltages: ************

  // Battery pack:
  if ((twizy_batt[0].volt_min == 0) || (twizy_batt[0].volt_act < twizy_batt[0].volt_min))
    twizy_batt[0].volt_min = twizy_batt[0].volt_act;
  if ((twizy_batt[0].volt_max == 0) || (twizy_batt[0].volt_act > twizy_batt[0].volt_max))
    twizy_batt[0].volt_max = twizy_batt[0].volt_act;

  // Cells: build mean value & standard deviation:
  sum = 0;
  sqrsum = 0;

  for (i = 0; i < BATT_CELLS; i++)
  {
    // Validate:
    if ((twizy_cell[i].volt_act == 0) || (twizy_cell[i].volt_act >= 0x0f00))
      break;

    // Remember min:
    if ((twizy_cell[i].volt_min == 0) || (twizy_cell[i].volt_act < twizy_cell[i].volt_min))
      twizy_cell[i].volt_min = twizy_cell[i].volt_act;

    // Remember max:
    if ((twizy_cell[i].volt_max == 0) || (twizy_cell[i].volt_act > twizy_cell[i].volt_max))
      twizy_cell[i].volt_max = twizy_cell[i].volt_act;

    // build sums:
    sum += twizy_cell[i].volt_act;
    sqrsum += SQR((UINT32) twizy_cell[i].volt_act);
  }

  if (i == BATT_CELLS)
  {
    // All values valid, process:

    m = (float) sum / BATT_CELLS;

    //stddev = (unsigned int) sqrt( ((float)sqrsum/BATT_CELLS) - SQR((float)sum/BATT_CELLS) ) + 1;
    // this is too complex for C18, we need to split this up:
    f = ((float) sqrsum / BATT_CELLS) - SQR(m);
    stddev = sqrt(f) + 0.5;
    if (stddev == 0)
      stddev = 1; // not enough precision to allow stddev 0

    // check max stddev:
    if (stddev > twizy_batt[0].cell_volt_stddev_max)
    {
      twizy_batt[0].cell_volt_stddev_max = stddev;

      // switch to overall stddev alert mode?
      // (resetting cell flags to build new alert set)
      if (stddev >= BATT_STDDEV_VOLT_ALERT)
        twizy_batt[0].volt_alerts = BATT_STDDEV_VOLT_FLAG;
      else if (stddev >= BATT_STDDEV_VOLT_WATCH)
        twizy_batt[0].volt_watches = BATT_STDDEV_VOLT_FLAG;
    }

    // check cell deviations:
    for (i = 0; i < BATT_CELLS; i++)
    {
      // deviation:
      dev = (twizy_cell[i].volt_act - m)
              + ((twizy_cell[i].volt_act >= m) ? 0.5 : -0.5);
      absdev = ABS(dev);

      // Set watch/alert flags:
      // (applying overall thresholds only in stddev alert mode)
      if ((twizy_batt[0].volt_alerts & BATT_STDDEV_VOLT_FLAG)
              && (absdev >= BATT_STDDEV_VOLT_ALERT))
        twizy_batt[0].volt_alerts |= (1L << i);
      else if (absdev >= BATT_DEV_VOLT_ALERT)
        twizy_batt[0].volt_alerts |= (1L << i);
      else if ((twizy_batt[0].volt_watches & BATT_STDDEV_VOLT_FLAG)
              && (absdev >= BATT_STDDEV_VOLT_WATCH))
        twizy_batt[0].volt_watches |= (1L << i);
      else if (absdev > stddev)
        twizy_batt[0].volt_watches |= (1L << i);

      // Remember max deviation:
      if (absdev > ABS(twizy_cell[i].volt_maxdev))
        twizy_cell[i].volt_maxdev = (INT) dev;
    }

  } // if( i == BATT_CELLS )


  // Battery monitor update/alert:
  if ((twizy_batt[0].volt_alerts != twizy_batt[0].last_volt_alerts)
          || (twizy_batt[0].temp_alerts != twizy_batt[0].last_temp_alerts))
  {
    twizy_notify |= (SEND_BatteryAlert | SEND_BatteryStats);
  }


  // OK, sensors have been processed, start next fetch cycle:
  twizy_batt_sensors_state = BATT_SENSORS_START;

}



// Prep battery[p] alert message text for SMS/MSG in net_scratchpad:

void vehicle_twizy_battstatus_prepalert(int p, int smsmode)
{
  int c, val, maxlen;
  char *s;

  maxlen = (smsmode) ? 160 : 199;

  s = strchr(net_scratchpad, 0); // append to net_scratchpad

  // Voltage deviations:
  s = stp_rom(s, "Volts: ");

  // standard deviation:
  val = CONV_CellVolt(twizy_batt[p].cell_volt_stddev_max);
  if (twizy_batt[p].volt_alerts & BATT_STDDEV_VOLT_FLAG)
    s = stp_rom(s, "!");
  else if (twizy_batt[p].volt_watches & BATT_STDDEV_VOLT_FLAG)
    s = stp_rom(s, "?");
  s = stp_i(s, "SD:", val);
  s = stp_rom(s, "mV ");

  if ((twizy_batt[p].volt_alerts == 0) && (twizy_batt[p].volt_watches == 0))
  {
    s = stp_rom(s, "OK ");
  }
  else
  {
    for (c = 0; c < BATT_CELLS; c++)
    {
      // check length:
      if ((s-net_scratchpad) >= (maxlen-12))
      {
        s = stp_rom(s, "...");
        return;
      }

      // Alert / Watch?
      if (twizy_batt[p].volt_alerts & (1L << c))
        s = stp_rom(s, "!");
      else if (twizy_batt[p].volt_watches & (1L << c))
        s = stp_rom(s, "?");
      else
        continue;

      val = CONV_CellVoltS(twizy_cell[c].volt_maxdev);

      s = stp_i(s, "C", c + 1);
      if (val >= 0)
        s = stp_i(s, ":+", val);
      else
        s = stp_i(s, ":", val);
      s = stp_rom(s, "mV ");
    }
  }

  // check length:
  if ((s-net_scratchpad) >= (maxlen-20))
  {
    s = stp_rom(s, "...");
    return;
  }

  // Temperature deviations:
  s = stp_rom(s, "Temps: ");

  // standard deviation:
  val = twizy_batt[p].cmod_temp_stddev_max;
  if (twizy_batt[p].temp_alerts & BATT_STDDEV_TEMP_FLAG)
    s = stp_rom(s, "!");
  else if (twizy_batt[p].temp_watches & BATT_STDDEV_TEMP_FLAG)
    s = stp_rom(s, "?");
  s = stp_i(s, "SD:", val);
  s = stp_rom(s, "C ");
  
  if ((twizy_batt[p].temp_alerts == 0) && (twizy_batt[p].temp_watches == 0))
  {
    s = stp_rom(s, "OK ");
  }
  else
  {
    for (c = 0; c < BATT_CMODS; c++)
    {
      // check length:
      if ((s-net_scratchpad) >= (maxlen-8))
      {
        s = stp_rom(s, "...");
        return;
      }

      // Alert / Watch?
      if (twizy_batt[p].temp_alerts & (1 << c))
        s = stp_rom(s, "!");
      else if (twizy_batt[p].temp_watches & (1 << c))
        s = stp_rom(s, "?");
      else
        continue;

      val = twizy_cmod[c].temp_maxdev;

      s = stp_i(s, "M", c + 1);
      if (val >= 0)
        s = stp_i(s, ":+", val);
      else
        s = stp_i(s, ":", val);
      s = stp_rom(s, "C ");
    }
  }

  // alert text now ready to send in net_scratchpad
}

char vehicle_twizy_battstatus_msgp(char stat, int cmd)
{
  static WORD crc_alert[BATT_PACKS];
  static WORD crc_pack[BATT_PACKS];
  static WORD crc_cell[BATT_CELLS]; // RT: just one pack...
  UINT8 p, c;
  UINT8 tmin, tmax;
  int tact;
  UINT8 volt_alert, temp_alert;
  char *s;


  // Try to wait for consistent sensor data:
  if (vehicle_twizy_battstatus_wait4sensors() == FALSE)
  {
    // no sensor data available, abort if in diff mode:
    if (stat > 0)
      return stat;
  }


  if (cmd == CMD_BatteryAlert)
  {

    // Output battery packs (just one for Twizy up to now):
    for (p = 0; p < BATT_PACKS; p++)
    {
      // Output pack ALERT:
      if (twizy_batt[p].volt_alerts || twizy_batt[p].temp_alerts)
      {
        strcpypgm2ram(net_scratchpad, "MP-0 PA");
        vehicle_twizy_battstatus_prepalert(p, 0);
        stat = net_msg_encode_statputs(stat, &crc_alert[p]);
      }
      else
      {
        crc_alert[p] = 0; // reset crc for next alert
      }
    }

  } // cmd == CMD_BatteryAlert

  else // cmd == CMD_BatteryStatus (default)
  {
    // NOTE: this already needs a full CIPSEND length, needs to be 
    //        split up if extended further!

    // Prep: collect cell temperatures:
    tmin = 255;
    tmax = 0;
    tact = 0;
    for (c = 0; c < BATT_CMODS; c++)
    {
      tact += twizy_cmod[c].temp_act;
      if (twizy_cmod[c].temp_min < tmin)
        tmin = twizy_cmod[c].temp_min;
      if (twizy_cmod[c].temp_max > tmax)
        tmax = twizy_cmod[c].temp_max;
    }

    tact = (float) tact / BATT_CMODS + 0.5;

    // Output battery packs (just one for Twizy up to now):
    for (p = 0; p < BATT_PACKS; p++)
    {
      if (twizy_batt[p].volt_alerts)
        volt_alert = 3;
      else if (twizy_batt[p].volt_watches)
        volt_alert = 2;
      else
        volt_alert = 1;

      if (twizy_batt[p].temp_alerts)
        temp_alert = 3;
      else if (twizy_batt[p].temp_watches)
        temp_alert = 2;
      else
        temp_alert = 1;

      // Output pack STATUS:
      // MP-0 HRT-PWR-BattPack,<packnr>,86400
      //  ,<nr_of_cells>,<cell_startnr>
      //  ,<volt_alertstatus>,<temp_alertstatus>
      //  ,<soc>,<soc_min>,<soc_max>
      //  ,<volt_act>,<volt_act_cellnom>
      //  ,<volt_min>,<volt_min_cellnom>
      //  ,<volt_max>,<volt_max_cellnom>
      //  ,<temp_act>,<temp_min>,<temp_max>
      //  ,<cell_volt_stddev_max>,<cmod_temp_stddev_max>

      s = stp_rom(net_scratchpad, "MP-0 H");
      s = stp_i(s, "RT-PWR-BattPack,", p + 1);
      s = stp_i(s, ",86400,", BATT_CELLS);
      s = stp_i(s, ",", 1);
      s = stp_i(s, ",", volt_alert);
      s = stp_i(s, ",", temp_alert);
      s = stp_i(s, ",", twizy_soc);
      s = stp_i(s, ",", twizy_soc_min);
      s = stp_i(s, ",", twizy_soc_max);
      s = stp_i(s, ",", CONV_PackVolt(twizy_batt[p].volt_act));
      s = stp_i(s, ",", CONV_PackVolt(twizy_batt[p].volt_act) / BATT_CELLS);
      s = stp_i(s, ",", CONV_PackVolt(twizy_batt[p].volt_min));
      s = stp_i(s, ",", CONV_PackVolt(twizy_batt[p].volt_min) / BATT_CELLS);
      s = stp_i(s, ",", CONV_PackVolt(twizy_batt[p].volt_max));
      s = stp_i(s, ",", CONV_PackVolt(twizy_batt[p].volt_max) / BATT_CELLS);
      s = stp_i(s, ",", CONV_Temp(tact));
      s = stp_i(s, ",", CONV_Temp(tmin));
      s = stp_i(s, ",", CONV_Temp(tmax));
      s = stp_i(s, ",", CONV_CellVolt(twizy_batt[p].cell_volt_stddev_max));
      s = stp_i(s, ",", twizy_batt[p].cmod_temp_stddev_max);

      stat = net_msg_encode_statputs(stat, &crc_pack[p]);

      // Output cell status:
      for (c = 0; c < BATT_CELLS; c++)
      {
        if (twizy_batt[p].volt_alerts & (1L << c))
          volt_alert = 3;
        else if (twizy_batt[p].volt_watches & (1L << c))
          volt_alert = 2;
        else
          volt_alert = 1;

        if (twizy_batt[p].temp_alerts & (1 << (c >> 1)))
          temp_alert = 3;
        else if (twizy_batt[p].temp_watches & (1 << (c >> 1)))
          temp_alert = 2;
        else
          temp_alert = 1;

        // MP-0 HRT-PWR-BattCell,<cellnr>,86400
        //  ,<packnr>
        //  ,<volt_alertstatus>,<temp_alertstatus>,
        //  ,<volt_act>,<volt_min>,<volt_max>,<volt_maxdev>
        //  ,<temp_act>,<temp_min>,<temp_max>,<temp_maxdev>

        s = stp_rom(net_scratchpad, "MP-0 H");
        s = stp_i(s, "RT-PWR-BattCell,", c + 1);
        s = stp_i(s, ",86400,", p + 1);
        s = stp_i(s, ",", volt_alert);
        s = stp_i(s, ",", temp_alert);
        s = stp_i(s, ",", CONV_CellVolt(twizy_cell[c].volt_act));
        s = stp_i(s, ",", CONV_CellVolt(twizy_cell[c].volt_min));
        s = stp_i(s, ",", CONV_CellVolt(twizy_cell[c].volt_max));
        s = stp_i(s, ",", CONV_CellVoltS(twizy_cell[c].volt_maxdev));
        s = stp_i(s, ",", CONV_Temp(twizy_cmod[c >> 1].temp_act));
        s = stp_i(s, ",", CONV_Temp(twizy_cmod[c >> 1].temp_min));
        s = stp_i(s, ",", CONV_Temp(twizy_cmod[c >> 1].temp_max));
        s = stp_i(s, ",", twizy_cmod[c >> 1].temp_maxdev);

        stat = net_msg_encode_statputs(stat, &crc_cell[c]);
      }
    }

  } // cmd == CMD_BatteryStatus

  return stat;
}

BOOL vehicle_twizy_battstatus_cmd(BOOL msgmode, int cmd, char *arguments)
{
  char *s;

  if (msgmode)
  {
    vehicle_twizy_battstatus_msgp(0, cmd);

    s = stp_i(net_scratchpad, "MP-0 c", cmd ? cmd : CMD_BatteryStatus);
    s = stp_rom(s, ",0");
    net_msg_encode_puts();
  }
  else
  {
    if (vehicle_twizy_battstatus_msgp(2, cmd) != 2)
      net_msg_send();
  }

  return TRUE;
}

BOOL vehicle_twizy_battstatus_sms(BOOL premsg, char *caller, char *command, char *arguments)
{
  int p, c;
  char argc1, argc2;
  UINT8 tmin, tmax;
  int tact;
  UINT8 cc;
  char *s;

  if (!premsg)
    return FALSE;

  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  if (!caller || !*caller)
  {
    caller = par_get(PARAM_REGPHONE);
    if (!caller || !*caller)
      return FALSE;
  }

  // Try to wait for valid sensor data (but send info anyway)
  vehicle_twizy_battstatus_wait4sensors();

  // Start SMS:
  net_send_sms_start(caller);

  argc1 = (arguments) ? (arguments[0] | 0x20) : 0;
  argc2 = (argc1) ? (arguments[1] | 0x20) : 0;

  switch (argc1)
  {

  case 'r':
    // Reset:
    vehicle_twizy_battstatus_reset();
    net_puts_rom("Battery monitor reset.");
    break;


  case 't':
    // Current temperatures ("t") or deviations ("td"):

    // Prep: collect cell temperatures:
    tmin = 255;
    tmax = 0;
    tact = 0;
    for (c = 0; c < BATT_CMODS; c++)
    {
      tact += twizy_cmod[c].temp_act;
      if (twizy_cmod[c].temp_min < tmin)
        tmin = twizy_cmod[c].temp_min;
      if (twizy_cmod[c].temp_max > tmax)
        tmax = twizy_cmod[c].temp_max;
    }
    tact = (float) tact / BATT_CMODS + 0.5;

    // Output pack status:
    s = net_scratchpad;

    if (argc2 == 'd') // deviations
    {
      if (twizy_batt[0].temp_alerts & BATT_STDDEV_TEMP_FLAG)
        s = stp_rom(s, "!");
      else if (twizy_batt[0].temp_watches & BATT_STDDEV_TEMP_FLAG)
        s = stp_rom(s, "?");
      s = stp_i(s, "SD:", twizy_batt[0].cmod_temp_stddev_max);
      s = stp_rom(s, "C ");
    }
    else
    {
      s = stp_i(s, "P:", CONV_Temp(tact));
      s = stp_i(s, "C (", CONV_Temp(tmin));
      s = stp_i(s, "C..", CONV_Temp(tmax));
      s = stp_rom(s, "C) ");
    }

    // Output cmod status:
    for (c = 0; c < BATT_CMODS; c++)
    {
      // Alert?
      if (twizy_batt[0].temp_alerts & (1 << c))
        s = stp_rom(s, "!");
      else if (twizy_batt[0].temp_watches & (1 << c))
        s = stp_rom(s, "?");

      s = stp_i(s, NULL, c + 1);
      if (argc2 == 'd') // deviations
      {
        if (twizy_cmod[c].temp_maxdev >= 0)
          s = stp_i(s, ":+", twizy_cmod[c].temp_maxdev);
        else
          s = stp_i(s, ":", twizy_cmod[c].temp_maxdev);
        s = stp_rom(s, "C ");
      }
      else // current values
      {
        s = stp_i(s, ":", CONV_Temp(twizy_cmod[c].temp_act));
        s = stp_rom(s, "C ");
      }
    }

    net_puts_ram(net_scratchpad);

    break;


  case 'v':
    // Current voltage levels ("v") or deviations ("vd"):

    s = net_scratchpad;

    for (p = 0; p < BATT_PACKS; p++)
    {
      // Output pack status:

      if (argc2 == 'd') // deviations
      {
        if (twizy_batt[p].volt_alerts & BATT_STDDEV_VOLT_FLAG)
          s = stp_rom(s, "!");
        else if (twizy_batt[p].volt_watches & BATT_STDDEV_VOLT_FLAG)
          s = stp_rom(s, "?");
        s = stp_i(s, "SD:", CONV_CellVolt(twizy_batt[p].cell_volt_stddev_max));
        s = stp_rom(s, "mV ");
      }
      else
      {
        s = stp_l2f(s, "P:", CONV_PackVolt(twizy_batt[p].volt_act), 2);
        s = stp_rom(s, "V ");
      }

      // Output cell status:
      for (c = 0; c < BATT_CELLS; c++)
      {
        // Split SMS?
        if (s > (net_scratchpad + 160 - 13))
        {
          net_puts_ram(net_scratchpad);
          net_send_sms_finish();
          delay100(30);
          net_send_sms_start(caller);
          s = net_scratchpad;
        }

        // Alert?
        if (twizy_batt[p].volt_alerts & (1L << c))
          s = stp_rom(s, "!");
        else if (twizy_batt[p].volt_watches & (1L << c))
          s = stp_rom(s, "?");

        s = stp_i(s, NULL, c + 1);
        if (argc2 == 'd') // deviations
        {
          if (twizy_cell[c].volt_maxdev >= 0)
            s = stp_i(s, ":+", CONV_CellVoltS(twizy_cell[c].volt_maxdev));
          else
            s = stp_i(s, ":", CONV_CellVoltS(twizy_cell[c].volt_maxdev));
          s = stp_rom(s, "mV ");
        }
        else
        {
          s = stp_l2f(s, ":", CONV_CellVolt(twizy_cell[c].volt_act), 3);
          s = stp_rom(s, "V ");
        }
      }
    }

    net_puts_ram(net_scratchpad);

    break;


  default:
    // Battery alert status:

    net_scratchpad[0] = 0;
    vehicle_twizy_battstatus_prepalert(0, 1);
    cr2lf(net_scratchpad);
    net_puts_ram(net_scratchpad);

    // Remember last alert state notified:
    twizy_batt[0].last_volt_alerts = twizy_batt[0].volt_alerts;
    twizy_batt[0].last_temp_alerts = twizy_batt[0].temp_alerts;

    break;


  }

  return TRUE;
}

#endif // OVMS_TWIZY_BATTMON




////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
/****************************************************************
 *
 * COMMAND DISPATCHERS / FRAMEWORK HOOKS
 *
 */
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

BOOL vehicle_twizy_fn_commandhandler(BOOL msgmode, int cmd, char *msg)
{
  switch (cmd)
  {
    /************************************************************
     * STANDARD COMMAND OVERRIDES:
     */

  case CMD_Alert:
    return vehicle_twizy_alert_cmd(msgmode, cmd, msg);


    /************************************************************
     * CAR SPECIFIC COMMANDS:
     */

  case CMD_Debug:
    return vehicle_twizy_debug_cmd(msgmode, cmd, msg);

  case CMD_QueryRange:
  case CMD_SetRange /*(maxrange)*/:
    return vehicle_twizy_range_cmd(msgmode, cmd, msg);

  case CMD_QueryChargeAlerts:
  case CMD_SetChargeAlerts /*(range,soc)*/:
    return vehicle_twizy_ca_cmd(msgmode, cmd, msg);

#ifdef OVMS_TWIZY_BATTMON
  case CMD_BatteryAlert:
  case CMD_BatteryStatus:
    return vehicle_twizy_battstatus_cmd(msgmode, cmd, msg);
#endif // OVMS_TWIZY_BATTMON

  case CMD_PowerUsageNotify:
  case CMD_PowerUsageStats:
    return vehicle_twizy_power_cmd(msgmode, cmd, msg);

  }

  // not handled
  return FALSE;
}


////////////////////////////////////////////////////////////////////////
// This is the Twizy SMS command table
// (for implementation notes see net_sms::sms_cmdtable comment)
//
// First char = auth mode of command:
//   1:     the first argument must be the module password
//   2:     the caller must be the registered telephone
//   3:     the caller must be the registered telephone, or first argument the module password

BOOL vehicle_twizy_help_sms(BOOL premsg, char *caller, char *command, char *arguments);

rom char vehicle_twizy_sms_cmdtable[][NET_SMS_CMDWIDTH] = {
  "3DEBUG", // Twizy: output internal state dump for debug
  "3STAT", // override standard STAT
  "3RANGE", // Twizy: set/query max ideal range
  "3CA", // Twizy: set/query charge alerts
  "3HELP", // extend HELP output

#ifdef OVMS_TWIZY_BATTMON
  "3BATT", // Twizy: battery status
#endif // OVMS_TWIZY_BATTMON

  "3POWER", // Twizy: power usage statistics

  "3CFG", // Twizy: configuration tweaking

  ""
};

rom far BOOL(*vehicle_twizy_sms_hfntable[])(BOOL premsg, char *caller, char *command, char *arguments) = {
  &vehicle_twizy_debug_sms,
  &vehicle_twizy_stat_sms,
  &vehicle_twizy_range_sms,
  &vehicle_twizy_ca_sms,
  &vehicle_twizy_help_sms,

#ifdef OVMS_TWIZY_BATTMON
  &vehicle_twizy_battstatus_sms,
#endif // OVMS_TWIZY_BATTMON

  &vehicle_twizy_power_sms,

  &vehicle_twizy_cfg_sms
};

// SMS COMMAND DISPATCHER:
// premsg: TRUE=may replace, FALSE=may extend standard handler
// returns TRUE if handled

BOOL vehicle_twizy_fn_sms(BOOL checkauth, BOOL premsg, char *caller, char *command, char *arguments)
{
  int k;

  // Command parsing...
  for (k = 0; vehicle_twizy_sms_cmdtable[k][0] != 0; k++)
  {
    if (memcmppgm2ram(command,
                      (char const rom far*)vehicle_twizy_sms_cmdtable[k] + 1,
                      strlenpgm((char const rom far*)vehicle_twizy_sms_cmdtable[k]) - 1) == 0)
    {
      BOOL result;

      if (checkauth)
      {
        // we need to check the caller authorization:
        arguments = net_sms_initargs(arguments);
        if (!net_sms_checkauth(vehicle_twizy_sms_cmdtable[k][0], caller, &arguments))
          return FALSE; // failed
      }

      // Call sms handler:
      result = (*vehicle_twizy_sms_hfntable[k])(premsg, caller, command, arguments);

      if ((premsg) && (result))
      {
        // we're in charge + handled it; finish SMS:
        net_send_sms_finish();
      }

      return result;
    }
  }

  return FALSE; // no vehicle command
}

BOOL vehicle_twizy_fn_smshandler(BOOL premsg, char *caller, char *command, char *arguments)
{
  // called to extend/replace standard command: framework did auth check for us
  return vehicle_twizy_fn_sms(FALSE, premsg, caller, command, arguments);
}

BOOL vehicle_twizy_fn_smsextensions(char *caller, char *command, char *arguments)
{
  // called for specific command: we need to do the auth check
  return vehicle_twizy_fn_sms(TRUE, TRUE, caller, command, arguments);
}


////////////////////////////////////////////////////////////////////////
// Twizy specific SMS command output extension: HELP
// - add Twizy commands
//

BOOL vehicle_twizy_help_sms(BOOL premsg, char *caller, char *command, char *arguments)
{
  int k;

  if (premsg)
    return FALSE; // run only in extend mode

  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  net_puts_rom(" \r\nTwizy Commands:");

  for (k = 0; vehicle_twizy_sms_cmdtable[k][0] != 0; k++)
  {
    net_puts_rom(" ");
    net_puts_rom(vehicle_twizy_sms_cmdtable[k] + 1);
  }

  return TRUE;
}


////////////////////////////////////////////////////////////////////////
// vehicle_twizy_initialise()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to initialise itself.
//

BOOL vehicle_twizy_initialise(void)
{
  // car_type[] is uninitialised => distinct init from crash/reset:
  if ((car_type[0] != 'R') || (car_type[1] != 'T') || (car_type[2] != 0))
  {
    UINT8 i;

    // INIT VEHICLE VARIABLES:

    car_type[0] = 'R'; // Car is type RT - Renault Twizy
    car_type[1] = 'T';
    car_type[2] = 0;
    car_type[3] = 0;
    car_type[4] = 0;

    twizy_status = CAN_STATUS_OFFLINE;
    twizy_button_cnt = 0;

    twizy_soc = 5000;
    twizy_soc_min = 10000;
    twizy_soc_max = 0;

    car_SOC = 50;
    twizy_last_SOC = 0;

    twizy_range = 50;
    twizy_soc_min_range = 100;
    twizy_last_idealrange = 0;

    twizy_speed = 0;
    twizy_power = 0;
    twizy_odometer = 0;
    twizy_dist = 0;

#ifdef OVMS_TWIZY_BATTMON

    // Init battery monitor:
    memset(twizy_batt, 0, sizeof twizy_batt);
    twizy_batt[0].volt_min = 1000; // 100 V
    memset(twizy_cmod, 0, sizeof twizy_cmod);
    for (i = 0; i < BATT_CMODS; i++)
    {
      twizy_cmod[i].temp_act = 40;
      twizy_cmod[i].temp_min = 240;
    }
    memset(twizy_cell, 0, sizeof twizy_cell);
    for (i = 0; i < BATT_CELLS; i++)
    {
      twizy_cell[i].volt_min = 2000; // 10 V
    }

#endif // OVMS_TWIZY_BATTMON


    // init power statistics:
    memset(twizy_speedpwr, 0, sizeof twizy_speedpwr);
    twizy_speed_state = CAN_SPEED_CONST;
    twizy_speed_distref = 0;

    memset(twizy_levelpwr, 0, sizeof twizy_levelpwr);
    twizy_level_odo = 0;
    twizy_level_alt = 0;
    twizy_level_use = 0;
    twizy_level_rec = 0;
    
    twizy_notify = 0;

    car_time = 0;
    car_parktime = 0;
  }

#ifdef OVMS_TWIZY_BATTMON
  twizy_batt_sensors_state = BATT_SENSORS_START;
#endif // OVMS_TWIZY_BATTMON


  CANCON = 0b10010000; // Initialize CAN
  while (!CANSTATbits.OPMODE2); // Wait for Configuration mode

  // We are now in Configuration Mode.

  // ID masks and filters are 11 bit as High-8 + Low-MSB-3
  // (Filter bit n must match if mask bit n = 1)


  // RX buffer0 uses Mask RXM0 and filters RXF0, RXF1
  RXB0CON = 0b00000000;

  // Setup Filter0 and Mask for CAN ID 0x155

  // Mask0 =
  RXM0SIDH = 0b01011111;
  RXM0SIDL = 0b11100000;

  // Filter0 = ID 0x155 (Power + SOC):
  RXF0SIDH = 0b00001010;
  RXF0SIDL = 0b10100000;

  // Filter1 = ID 0x081 + 0x581 (CANopen message from SEVCON):
  RXF1SIDH = 0b00010000;
  RXF1SIDL = 0b00100000;


  // RX buffer1 uses Mask RXM1 and filters RXF2, RXF3, RXF4, RXF5
  RXB1CON = 0b00000000;

  // Mask1 = 0x7f0 = group filters for low volume IDs
  RXM1SIDH = 0b11111110;
  RXM1SIDL = 0b00000000;

  // Filter2 = GROUP 0x59_:
  RXF2SIDH = 0b10110010;
  RXF2SIDL = 0b00000000;

  // Filter3 = GROUP 0x69_:
  RXF3SIDH = 0b11010010;
  RXF3SIDL = 0b00000000;

  // Filter4 = GROUP 0x55_:
  RXF4SIDH = 0b10101010;
  RXF4SIDL = 0b00000000;

  // Filter5 = GROUP 0x5D_:
  RXF5SIDH = 0b10111010;
  RXF5SIDL = 0b00000000;


  // SET BAUDRATE (tool: Intrepid CAN Timing Calculator / 20 MHz)

  // 1 Mbps setting: tool says that's 3/3/3 + multisampling
  //BRGCON1 = 0x00;
  //BRGCON2 = 0xD2;
  //BRGCON3 = 0x02;

  // 500 kbps based on 1 Mbps + prescaling:
  //BRGCON1 = 0x01;
  //BRGCON2 = 0xD2;
  //BRGCON3 = 0x02;
  // => FAILS (Lockups)

  // 500 kbps -- tool recommendation + multisampling:
  //BRGCON1 = 0x00;
  //BRGCON2 = 0xFA;
  //BRGCON3 = 0x07;
  // => FAILS (Lockups)

  // 500 kbps -- according to
  // http://www.softing.com/home/en/industrial-automation/products/can-bus/more-can-open/timing/bit-timing-specification.php
  // - CANopen uses single sampling
  // - and 87,5% of the bit time for the sampling point
  // - Synchronization jump window from 85-90%
  // We can only approximate to this:
  // - sampling point at 85%
  // - SJW from 80-90%
  // (Tq=20, Prop=8, Phase1=8, Phase2=3, SJW=1)
  BRGCON1 = 0x00;
  BRGCON2 = 0xBF;
  BRGCON3 = 0x02;


  CIOCON = 0b00100000; // CANTX pin will drive VDD when recessive

  if (sys_features[FEATURE_CANWRITE] > 0)
  {
    CANCON = 0b00000000; // Normal mode
  }
  else
  {
    CANCON = 0b01100000; // Listen only mode, Receive bufer 0
  }

  // Hook in...

  vehicle_version = vehicle_twizy_version;
  can_capabilities = vehicle_twizy_capabilities;

  vehicle_fn_poll0 = &vehicle_twizy_poll0;
  vehicle_fn_poll1 = &vehicle_twizy_poll1;
  vehicle_fn_idlepoll = &vehicle_twizy_idlepoll;
  vehicle_fn_ticker1 = &vehicle_twizy_state_ticker1;
  vehicle_fn_ticker10 = &vehicle_twizy_state_ticker10;
  vehicle_fn_ticker60 = &vehicle_twizy_state_ticker60;
  vehicle_fn_smshandler = &vehicle_twizy_fn_smshandler;
  vehicle_fn_smsextensions = &vehicle_twizy_fn_smsextensions;
  vehicle_fn_commandhandler = &vehicle_twizy_fn_commandhandler;

  net_fnbits |= NET_FN_INTERNALGPS; // Require internal GPS
  net_fnbits |= NET_FN_12VMONITOR;    // Require 12v monitor
  net_fnbits |= NET_FN_SOCMONITOR;    // Require SOC monitor

  return TRUE;
}
