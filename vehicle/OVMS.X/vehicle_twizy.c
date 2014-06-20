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

 *   3.0.0  30 Dec 2013 (Michael Balzer):
    Twizy: Major update: SEVCON controller configuration
      !HIGHLY EXPERIMENTAL FEATURE!
      read documentation (userguide) first!

    ATT: due to ROM limitations, this release cannot be
      combined with other vehicles and/or DIAG mode!

 *  3.0.1  3 Jan 2014 (Michael Balzer):
    - CAN simulator preprocessor flag renamed to OVMS_CANSIM
    - Read/write SDO: retries + watchdog clearing
    - Power map generator for SPEED / TORQUE
    - TORQUE command extended by max motor power parameter
    - Stack usage optimizations
    - Interrupt optimization (#pragma tmpdata => needed for all ISR - TODO!)
    - Minor fixes
    - Code cleanup
    - Added preprocessor flag OVMS_TWIZY_CFG for SEVCON functions

 * 3.0.2  4 Jan 2014 (Michael Balzer)
    - TSMAP: levels 2..4 now allow 0%

 * 3.1.0  9 Jan 2014 (Michael Balzer)
    - TSMAP: levels 1..4 now allow 0%
    - Power map generator extended for pwr max lo/hi, added sanity check
    - Renamed command "POWER" to "DRIVE"
    - Renamed command "TORQUE" to "POWER", changed param handling

 * 3.1.1  12 Jan 2014 (Michael Balzer)
    - SDO segmented upload support (readsdo_buf)
    - new command: CFG READS (read string)
    - new command: CFG WRITEO (write only)
    - Error counter hardening (counter reset by CFG command)
    - Code readability improvements
    - SEVCON alerts (highest active fault) via SMS & MSG

 * 3.1.2  18 Jan 2014 (Michael Balzer)
    - new command: MSG 209 = CMD_ResetLogs
    - new command: MSG 210 = CMD_QueryLogs

 * 3.1.3  25 Jan 2014 (Michael Balzer)
    - CFG support for Twizy45
    - new history msg RT-PWR-Log for long term logging of drives and charges

 * 3.2.0  16 Feb 2014 (Michael Balzer)
    - EEPROM CFG profile storage (3 custom profiles)
    - CFG profile management commands: LOAD, SAVE, INFO
    - CFG profile selection & display via simple switch/LED console
    - Twizy status flags extended by "GO", "D/N/R" and footbrake
    - Power map generation optimized (only done once on profile switch/reset)

 * 3.2.1  30 May 2014 (Michael Balzer)
    - Statistics: avg speed and accel/decel, trip length (car_trip) and SOC usage
    - New trip efficiency report after driving incl. new stats
    - RT-PWR-Log and RT-PWR-UsageStats records extended by new stats
    - New CFG RAMPL command to set ramp rpm/s limits
    - CFG TSMAP extended by speed coordinates

 * 3.2.2  02 Jun 2014 (Michael Balzer)
    - Bugfix: Trip report recuperation percentage calculation fixed

 * 3.2.3  20 Jun 2014 (Michael Balzer)
    - Added battery max drive/recup power from PDO 424 to RT-PWR-BattPack
    - Motor temperature now also available below zero (signed)
    - SEVCON auto-login on every switch-on (for SDO reads)
    - SDO functions now check component status


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
#define FEATURE_SUFFSOC             0x0A // Sufficient SOC
#define FEATURE_SUFFRANGE           0x0B // Sufficient range
#define FEATURE_MAXRANGE            0x0C // Maximum ideal range

// Twizy specific params:
// (Note: these collide with ACC params, but ACC module will not be used for Twizy)
#define PARAM_PROFILE               0x0F // current cfg profile nr (0..3)
#define PARAM_PROFILE_S             0x10 // custom profiles base
#define PARAM_PROFILE1              0x10 // custom profile #1 (binary, 2 slots)
#define PARAM_PROFILE2              0x12 // custom profile #2 (binary, 2 slots)
#define PARAM_PROFILE3              0x14 // custom profile #3 (binary, 2 slots)


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
#define CMD_ResetLogs               209 // (which, start)
#define CMD_QueryLogs               210 // (which, start)

// Twizy module version & capabilities:
rom char vehicle_twizy_version[] = "3.2.3";

#ifdef OVMS_TWIZY_BATTMON
rom char vehicle_twizy_capabilities[] = "C6,C200-210";
#else
rom char vehicle_twizy_capabilities[] = "C6,C200-204,C207-210";
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

  UINT8 max_drive_pwr; // in 500 W
  UINT8 max_recup_pwr; // in 500 W

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

// Conversion utils:
#define CONV_PackVolt(m) ((UINT)(m) * (100 / 10))
#define CONV_CellVolt(m) ((UINT)(m) * (1000 / 200))
#define CONV_CellVoltS(m) ((INT)(m) * (1000 / 200))
#define CONV_Temp(m) ((INT)(m) - 40)

#endif // OVMS_TWIZY_BATTMON


typedef struct speedpwr // speed+power usage statistics for const/accel/decel
{
  unsigned long dist; // distance sum in 1/10 m
  unsigned long use; // sum of power used
  unsigned long rec; // sum of power recovered (recuperation)
  
  unsigned long spdcnt; // count of speed sum entries
  unsigned long spdsum; // sum of speeds/deltas during driving

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
unsigned int twizy_soc_tripstart; // SOC at last power on

unsigned int twizy_range; // range in km
unsigned int twizy_speed; // current speed in 1/100 km/h
unsigned long twizy_odometer; // current odometer in 1/100 km = 10 m
unsigned long twizy_odometer_tripstart; // odometer at last power on
volatile unsigned int twizy_dist; // cyclic distance counter in 1/10 m = 10 cm

signed int twizy_power; // current power in 16*W, negative=charging

unsigned char twizy_status; // Car + charge status from CAN:
#define CAN_STATUS_FOOTBRAKE    0x01        //  bit 0 = 0x01: 1 = Footbrake
#define CAN_STATUS_MODE_D       0x02        //  bit 1 = 0x02: 1 = Forward mode "D"
#define CAN_STATUS_MODE_R       0x04        //  bit 2 = 0x04: 1 = Reverse mode "R"
#define CAN_STATUS_MODE         0x06        //  mask
#define CAN_STATUS_GO           0x08        //  bit 3 = 0x08: 1 = Motor ON / "GO"
#define CAN_STATUS_KEYON        0x10        //  bit 4 = 0x10: 1 = Car ON (key turned)
#define CAN_STATUS_CHARGING     0x20        //  bit 5 = 0x20: 1 = Charging
#define CAN_STATUS_OFFLINE      0x40        //  bit 6 = 0x40: 1 = Switch-ON/-OFF phase / 0 = normal operation
#define CAN_STATUS_ONLINE       0x80        //  bit 7 = 0x80: 1 = CAN-Bus online (test flag to detect offline)


#ifdef OVMS_TWIZY_CFG

volatile unsigned char twizy_button_cnt; // will count key presses in STOP mode (msg 081)

struct {
  unsigned type:1;                  // CFG: 0=Twizy80, 1=Twizy45
  unsigned profile_user:2;          // CFG: user selected profile: 0=Default, 1..3=Custom
  unsigned profile_cfgmode:2;       // CFG: profile, cfgmode params were last loaded from
  unsigned unsaved:1;               // CFG: RAM profile changed & not yet saved to EEPROM
  unsigned keystate:1;              // CFG: key state change detection
} twizy_cfg;

struct twizy_cfg_profile {
  UINT8       checksum;
  UINT8       speed, warn;
  UINT8       torque, power_low, power_high;
  UINT8       drive, neutral, brake;
  struct tsmap {
    UINT8       spd1, spd2, spd3, spd4;
    UINT8       prc1, prc2, prc3, prc4;
  }           tsmap[3]; // 0=D 1=N 2=B
  UINT8       ramp_start, ramp_accel, ramp_decel, ramp_neutral, ramp_brake;
  UINT8       smooth;
  UINT8       brakelight_on, brakelight_off;
  UINT8       ramplimit_accel, ramplimit_decel;
} twizy_cfg_profile;

// Macros for converting profile values:
// shift values by 1 (-1.. => 0..) to map param range to UINT8
#define cfgparam(NAME)  (((int)(twizy_cfg_profile.NAME))-1)
#define cfgvalue(VAL)   ((UINT8)((VAL)+1))

unsigned int twizy_max_rpm;         // CFG: max speed (RPM: 0..11000)
unsigned long twizy_max_trq;        // CFG: max torque (mNm: 0..70125)
unsigned int twizy_max_pwr_lo;      // CFG: max power low speed (W: 0..17000)
unsigned int twizy_max_pwr_hi;      // CFG: max power high speed (W: 0..17000)

unsigned int twizy_sevcon_fault;    // SEVCON highest active fault

#endif // OVMS_TWIZY_CFG



// MSG notification queue (like net_notify for vehicle specific notifies)
volatile UINT8 twizy_notify; // bit set of...
#define SEND_BatteryAlert           0x01  // text alert: battery problem
#define SEND_PowerNotify            0x02  // text alert: power usage summary
#define SEND_DataUpdate             0x04  // regular data update (per minute)
#define SEND_StreamUpdate           0x08  // stream data update (per second)
#define SEND_BatteryStats           0x10  // separate battery stats (large)
#define SEND_FaultsAlert            0x20  // text alert: SEVCON fault(s)
#define SEND_PowerLog               0x40  // RT-PWR-Log history entry


// -----------------------------------------------
// RAM USAGE FOR STD VARS: 25 bytes (w/o DIAG)
// + 1 static CRC WORDS = 2 bytes
// = TOTAL: 27 bytes
// -----------------------------------------------


#ifdef OVMS_CANSIM
volatile int twizy_sim = -1; // CAN simulator: -1=off, else read index in twizy_sim_data
#endif // OVMS_CANSIM


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



#ifdef OVMS_TWIZY_CFG

/***************************************************************
 * Twizy CANopen SDO communication variables
 */

// SDO request/response:

// message structure:
//    0 = control byte
//    1+2 = index (little endian)
//    3 = subindex
//    4..7 = data (little endian)

volatile union {

  // raw / segment data:
  UINT8   byte[8];

  // request / expedited:
  struct {
    UINT8   control;
    UINT    index;
    UINT8   subindex;
    UINT32  data;
  };

} twizy_sdo;


/* NOTE:
 * the SDO comm could become common functionality by
 * adding the node id (fixed #1 for the Twizy SEVCON)
 */

// Convenience:
#define readsdo       vehicle_twizy_readsdo
#define readsdo_buf   vehicle_twizy_readsdo_buf
#define writesdo      vehicle_twizy_writesdo
#define login         vehicle_twizy_login
#define configmode    vehicle_twizy_configmode

// SDO communication codes:
#define SDO_CommandMask             0b11100000
#define SDO_ExpeditedUnusedMask     0b00001100
#define SDO_Expedited               0b00000010
#define SDO_Abort                   0b10000000

#define SDO_Abort_SegMismatch       0x05030000
#define SDO_Abort_Timeout           0x05040000
#define SDO_Abort_OutOfMemory       0x05040005


#define SDO_InitUploadRequest       0b01000000
#define SDO_InitUploadResponse      0b01000000

#define SDO_UploadSegmentRequest    0b01100000
#define SDO_UploadSegmentResponse   0b00000000

#define SDO_InitDownloadRequest     0b00100000
#define SDO_InitDownloadResponse    0b01100000

#define SDO_SegmentToggle           0b00010000
#define SDO_SegmentUnusedMask       0b00001110
#define SDO_SegmentEnd              0b00000001


#define CAN_GeneralError            0x08000000

// I/O level CAN error codes / classes:

#define ERR_NoCANWrite              0x0001
#define ERR_Timeout                 0x000a
#define ERR_ComponentOffline        0x000b

#define ERR_ReadSDO                 0x0002
#define ERR_ReadSDO_Timeout         0x0003
#define ERR_ReadSDO_SegXfer         0x0004
#define ERR_ReadSDO_SegMismatch     0x0005

#define ERR_WriteSDO                0x0008
#define ERR_WriteSDO_Timeout        0x0009


// App level CAN error codes / classes:

#define ERR_LoginFailed             0x0010
#define ERR_CfgModeFailed           0x0020
#define ERR_Range                   0x0030
#define ERR_UnknownHardware         0x0040


// check if SDO may contain relevant error info:
#define ERROR_IN_SDO(c) (((c) & 0xfff0) != ERR_Range)

#endif // OVMS_TWIZY_CFG


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



#ifdef OVMS_TWIZY_CFG

/***************************************************************
 * Twizy / CANopen SDO utilities
 */

// asynchronous SDO request:
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
  TXB0D0 = twizy_sdo.byte[0];
  TXB0D1 = twizy_sdo.byte[1];
  TXB0D2 = twizy_sdo.byte[2];
  TXB0D3 = twizy_sdo.byte[3];
  TXB0D4 = twizy_sdo.byte[4];
  TXB0D5 = twizy_sdo.byte[5];
  TXB0D6 = twizy_sdo.byte[6];
  TXB0D7 = twizy_sdo.byte[7];

  // data length (8):
  TXB0DLC = 0b00001000;

  // clear status to detect reply:
  twizy_sdo.control = 0xff;

  // send:
  TXB0CON = 0b00001000;
  while (TXB0CONbits.TXREQ) {}
}

// synchronous SDO request (1000 ms timeout, 3 tries):
UINT vehicle_twizy_sendsdoreq_sync(void)
{
  UINT8 control, timeout, tries;
  UINT32 data;

  control = twizy_sdo.control;
  data = twizy_sdo.data;

  tries = 3;
  do {

    // send request:
    twizy_sdo.control = control;
    twizy_sdo.data = data;
    vehicle_twizy_sendsdoreq();

    // wait for reply:
    ClrWdt();
    timeout = 200; // ~1000 ms
    do {
      delay5b();
    } while (twizy_sdo.control == 0xff && --timeout);

    if (timeout != 0)
      break;

    // timeout, abort request:
    twizy_sdo.control = SDO_Abort;
    twizy_sdo.data = SDO_Abort_Timeout;
    vehicle_twizy_sendsdoreq();

    if (tries > 1)
      delay5(4);

  } while (--tries);

  if (timeout == 0)
    return ERR_Timeout;

  return 0; // ok, response in twizy_sdo_*
}


// read from SDO:
UINT vehicle_twizy_readsdo(UINT index, UINT8 subindex)
{
  UINT8 control;

  // check for CAN write access:
  if (sys_features[FEATURE_CANWRITE] == 0)
    return ERR_NoCANWrite;

  // check component status (currently only SEVCON):
  if ((twizy_status & CAN_STATUS_KEYON) == 0)
    return ERR_ComponentOffline;

  // request upload:
  twizy_sdo.control = SDO_InitUploadRequest;
  twizy_sdo.index = index;
  twizy_sdo.subindex = subindex;
  twizy_sdo.data = 0;
  if (vehicle_twizy_sendsdoreq_sync() != 0)
    return ERR_ReadSDO_Timeout;

  // check response:
  if ((twizy_sdo.control & SDO_CommandMask) != SDO_InitUploadResponse) {
    // check for CANopen general error:
    if (twizy_sdo.data == CAN_GeneralError && index != 0x5310) {
      // add SEVCON error code:
      control = twizy_sdo.control;
      readsdo(0x5310,0x00);
      twizy_sdo.control = control;
      twizy_sdo.index = index;
      twizy_sdo.subindex = subindex;
      twizy_sdo.data |= CAN_GeneralError;
    }
    return ERR_ReadSDO;
  }

  // if expedited xfer we're done now:
  if (twizy_sdo.control & SDO_Expedited)
    return 0;

  // segmented xfer necessary:
  twizy_sdo.control = SDO_Abort;
  twizy_sdo.data = SDO_Abort_OutOfMemory;
  vehicle_twizy_sendsdoreq();
  return ERR_ReadSDO_SegXfer; // caller needs to use readsdo_buf
}


// read from SDO into buffer (supporting segmented xfer):
UINT vehicle_twizy_readsdo_buf(UINT index, UINT8 subindex, BYTE *dst, BYTE *maxlen)
{
  UINT8 n, toggle, dlen;

  // check for CAN write access:
  if (sys_features[FEATURE_CANWRITE] == 0)
    return ERR_NoCANWrite;

  // check component status (currently only SEVCON):
  if ((twizy_status & CAN_STATUS_KEYON) == 0)
    return ERR_ComponentOffline;

  // request upload:
  twizy_sdo.control = SDO_InitUploadRequest;
  twizy_sdo.index = index;
  twizy_sdo.subindex = subindex;
  twizy_sdo.data = 0;
  if (vehicle_twizy_sendsdoreq_sync() != 0)
    return ERR_ReadSDO_Timeout;

  // check response:
  if ((twizy_sdo.control & SDO_CommandMask) != SDO_InitUploadResponse) {
    // check for CANopen general error:
    if (twizy_sdo.data == CAN_GeneralError && index != 0x5310) {
      // add SEVCON error code:
      n = twizy_sdo.control;
      readsdo(0x5310,0x00);
      twizy_sdo.control = n;
      twizy_sdo.index = index;
      twizy_sdo.subindex = subindex;
      twizy_sdo.data |= CAN_GeneralError;
    }
    return ERR_ReadSDO;
  }

  // check for expedited xfer:
  if (twizy_sdo.control & SDO_Expedited) {

    // copy twizy_sdo.data to dst:
    dlen = 8 - ((twizy_sdo.control & SDO_ExpeditedUnusedMask) >> 2);
    for (n = 4; n < dlen && (*maxlen) > 0; n++, (*maxlen)--)
      *dst++ = twizy_sdo.byte[n];

    return 0;
  }

  // segmented xfer necessary:

  toggle = 0;

  do {

    // request segment:
    twizy_sdo.control = (SDO_UploadSegmentRequest|toggle);
    twizy_sdo.index = 0;
    twizy_sdo.subindex = 0;
    twizy_sdo.data = 0;
    if (vehicle_twizy_sendsdoreq_sync() != 0)
      return ERR_ReadSDO_Timeout;

    // check response:
    if ((twizy_sdo.control & (SDO_CommandMask|SDO_SegmentToggle)) != (SDO_UploadSegmentResponse|toggle)) {
      // mismatch:
      twizy_sdo.control = SDO_Abort;
      twizy_sdo.data = SDO_Abort_SegMismatch;
      vehicle_twizy_sendsdoreq();
      return ERR_ReadSDO_SegMismatch;
    }

    // ok, copy response data to dst:
    dlen = 8 - ((twizy_sdo.control & SDO_SegmentUnusedMask) >> 1);
    for (n = 1; n < dlen && (*maxlen) > 0; n++, (*maxlen)--)
      *dst++ = twizy_sdo.byte[n];

    // last segment fetched? => success!
    if (twizy_sdo.control & SDO_SegmentEnd)
      return 0;

    // maxlen reached? => abort xfer
    if ((*maxlen) == 0) {
      twizy_sdo.control = SDO_Abort;
      twizy_sdo.data = SDO_Abort_OutOfMemory;
      vehicle_twizy_sendsdoreq();
      return 0; // consider this as success, we read as much as we could
    }

    // toggle toggle bit:
    toggle = toggle ? 0 : SDO_SegmentToggle;

  } while(1);
  // not reached
}


// write to SDO without size indication:
UINT vehicle_twizy_writesdo(UINT index, UINT8 subindex, UINT32 data)
{
  UINT8 control;

  // check for CAN write access:
  if (sys_features[FEATURE_CANWRITE] == 0)
    return ERR_NoCANWrite;

  // check component status (currently only SEVCON):
  if ((twizy_status & CAN_STATUS_KEYON) == 0)
    return ERR_ComponentOffline;

  // request download:
  twizy_sdo.control = SDO_InitDownloadRequest | SDO_Expedited; // no size needed, server is smart
  twizy_sdo.index = index;
  twizy_sdo.subindex = subindex;
  twizy_sdo.data = data;
  if (vehicle_twizy_sendsdoreq_sync() != 0)
    return ERR_WriteSDO_Timeout;

  // check response:
  if ((twizy_sdo.control & SDO_CommandMask) != SDO_InitDownloadResponse) {
    // check for CANopen general error:
    if (twizy_sdo.data == CAN_GeneralError) {
      // add SEVCON error code:
      control = twizy_sdo.control;
      readsdo(0x5310,0x00);
      twizy_sdo.control = control;
      twizy_sdo.index = index;
      twizy_sdo.subindex = subindex;
      twizy_sdo.data |= CAN_GeneralError;
    }
    return ERR_WriteSDO;
  }

  // success
  return 0;
}


// SEVCON login/logout (access level 4):
UINT vehicle_twizy_login(BOOL on)
{
  UINT err;

  // get SEVCON type (Twizy 80/45):
  if (err = readsdo(0x1018,0x02))
    return ERR_UnknownHardware + err;

  if (twizy_sdo.data == 0x0712302d)
    twizy_cfg.type = 0; // Twizy80
  else if (twizy_sdo.data == 0x0712301b)
    twizy_cfg.type = 1; // Twizy45
  else
    return ERR_UnknownHardware; // unknown controller type
  

  // check login level:
  if (err = readsdo(0x5000,1))
    return ERR_LoginFailed + err;

  if (on && twizy_sdo.data != 4) {
    // login:
    writesdo(0x5000,3,0);
    if (err = writesdo(0x5000,2,0x4bdf))
      return ERR_LoginFailed + err;

    // check new level:
    if (err = readsdo(0x5000,1))
      return ERR_LoginFailed + err;
    if (twizy_sdo.data != 4)
      return ERR_LoginFailed;
  }

  else if (!on && twizy_sdo.data != 0) {
    // logout:
    writesdo(0x5000,3,0);
    if (err = writesdo(0x5000,2,0))
      return ERR_LoginFailed + err;

    // check new level:
    if (err = readsdo(0x5000,1))
      return ERR_LoginFailed + err;
    if (twizy_sdo.data != 0)
      return ERR_LoginFailed;
  }

  return 0;
}


// SEVCON state change operational/pre-operational:
UINT vehicle_twizy_configmode(BOOL on)
{
  UINT err, cnt;

  // login if necessary:
  if (err = login(1))
    return err;

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

    if (twizy_sdo.data != 127) {

      // request preoperational state:
      if (err = writesdo(0x2800,0,1))
        return ERR_CfgModeFailed + err;

      // give controller some time:
      delay5(2);

      // check new status:
      if (err = readsdo(0x5110,0))
        return ERR_CfgModeFailed + err;

      if (twizy_sdo.data != 127) {
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

struct twizy_cfg_params {
  UINT8   DefaultKphMax;
  UINT    DefaultRpmMax;
  UINT    DeltaBrkStart;
  UINT    DeltaBrkEnd;
  UINT    DeltaBrkDown;

  UINT8   DefaultKphWarn;
  UINT    DefaultRpmWarn;
  UINT    DeltaWarnOff;

  UINT    DefaultTrq;
  UINT32  DefaultTrqLim;
  UINT    DeltaMapTrq;

  UINT    DefaultPwrLo;
  UINT    DefaultPwrLoLim;
  UINT    DefaultPwrHi;
  UINT    DefaultPwrHiLim;

  UINT8   DefaultRecup;
  UINT8   DefaultRecupPrc;

  UINT    DefaultRampStart;
  UINT8   DefaultRampStartPrc;
  UINT    DefaultRampAccel;
  UINT8   DefaultRampAccelPrc;

  UINT    DefaultPMAP[18];

  UINT    DefaultMapSpd[4];
};

rom struct twizy_cfg_params twizy_cfg_params[2] =
{
  {
    //
    // CFG[0] = TWIZY80:
    //
    //    55 Nm (0x4611.0x01) = default max power map (PMAP) torque
    //    55 Nm (0x6076.0x00) = default peak torque
    //    57 Nm (0x2916.0x01) = rated torque (??? should be equal to 0x6076...)
    //    70.125 Nm (0x4610.0x11) = max motor torque according to flux map
    //
    //    7250 rpm (0x2920.0x05) = default max fwd speed = ~80 kph
    //    8050 rpm = default overspeed warning trigger (STOP lamp ON) = ~89 kph
    //    8500 rpm = default overspeed brakedown trigger = ~94 kph
    //    10000 rpm = max neutral speed (0x3813.2d) = ~110 kph
    //    11000 rpm = severe overspeed fault (0x4624.00) = ~121 kph

    80,     // DefaultKphMax
    7250,   // DefaultRpmMax
    400,    // DeltaBrkStart
    800,    // DeltaBrkEnd
    1250,   // DeltaBrkDown

    89,     // DefaultKphWarn
    8050,   // DefaultRpmWarn
    550,    // DeltaWarnOff

    55000,  // DefaultTrq
    70125,  // DefaultTrqLim
    0,      // DeltaMapTrq

    12182,  // DefaultPwrLo
    17000,  // DefaultPwrLoLim
    13000,  // DefaultPwrHi
    17000,  // DefaultPwrHiLim

    182,    // DefaultRecup
    18,     // DefaultRecupPrc

    400,    // DefaultRampStart
    4,      // DefaultRampStartPrc
    2500,   // DefaultRampAccel
    25,     // DefaultRampAccelPrc

            // DefaultPMAP:
    { 880,0, 880,2115, 659,2700, 608,3000, 516,3500,
      421,4500, 360,5500, 307,6500, 273,7250 },

            // DefaultMapSpd:
    { 3000, 3500, 4500, 6000 }
  },
  {
    //
    // CFG[1] = TWIZY45:
    //
    //    32.5 Nm (0x4611.0x01) = default max power map (PMAP) torque
    //    33 Nm (0x6076.0x00) = default peak torque
    //    33 Nm (0x2916.0x01) = rated torque (??? should be equal to 0x6076...)
    //    36 Nm (0x4610.0x11) = max motor torque according to flux map
    //
    //    5814 rpm (0x2920.0x05) = default max fwd speed = ~45 kph
    //    7200 rpm = default overspeed warning trigger (STOP lamp ON) = ~56 kph
    //    8500 rpm = default overspeed brakedown trigger = ~66 kph
    //    10000 rpm = max neutral speed (0x3813.2d) = ~77 kph
    //    11000 rpm = severe overspeed fault (0x4624.00) = ~85 kph

    45,     // DefaultKphMax
    5814,   // DefaultRpmMax
    686,    // DeltaBrkStart
    1386,   // DeltaBrkEnd
    2686,   // DeltaBrkDown

    56,     // DefaultKphWarn
    7200,   // DefaultRpmWarn
    900,    // DeltaWarnOff

    32500,  // DefaultTrq
    36000,  // DefaultTrqLim
    500,    // DeltaMapTrq

    7050,   // DefaultPwrLo
    10000,  // DefaultPwrLoLim
    7650,   // DefaultPwrHi
    10000,  // DefaultPwrHiLim

    209,    // DefaultRecup
    21,     // DefaultRecupPrc

    300,    // DefaultRampStart
    3,      // DefaultRampStartPrc
    2083,   // DefaultRampAccel
    21,     // DefaultRampAccelPrc

            // DefaultPMAP:
    { 520,0, 520,2050, 437,2500, 363,3000, 314,3500,
      279,4000, 247,4500, 226,5000, 195,6000 },

            // DefaultMapSpd:
    { 4357, 5083, 6535, 8714 }
  }
};

// ROM parameter access macro:
#define CFG twizy_cfg_params[twizy_cfg.type]


// scale utility:
//  returns <deflt> value scaled <from> <to> limited by <min> and <max>
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


// vehicle_twizy_cfg_makepowermap():
//  Internal utility for cfg_speed() and cfg_power()
//  builds torque/speed curve (SDO 0x4611) for current max values...
//  - twizy_max_rpm
//  - twizy_max_trq
//  - twizy_max_pwr_lo
//  - twizy_max_pwr_hi
// See "Twizy Powermap Calculator" spreadsheet for details.

rom UINT8 pmap_fib[7] = { 1, 2, 3, 5, 8, 13, 21 };

UINT vehicle_twizy_cfg_makepowermap(void
  /* twizy_max_rpm, twizy_max_trq, twizy_max_pwr_lo, twizy_max_pwr_hi */)
{
  UINT err;
  UINT8 i;
  UINT rpm_0, rpm;
  UINT trq;
  UINT pwr;
  int rpm_d, pwr_d;

  if (twizy_max_rpm == CFG.DefaultRpmMax && twizy_max_trq == CFG.DefaultTrq
    && twizy_max_pwr_lo == CFG.DefaultPwrLo && twizy_max_pwr_hi == CFG.DefaultPwrHi) {

    // restore default map:

    for (i=0; i<18; i++) {
      if (err = writesdo(0x4611,0x01+i,CFG.DefaultPMAP[i]))
        return err;
    }
  }

  else {

    // calculate constant torque part:

    rpm_0 = (((UINT32)twizy_max_pwr_lo * 9549 + (twizy_max_trq>>1)) / twizy_max_trq);
    trq = (twizy_max_trq * 16 + 500) / 1000;

    if (err = writesdo(0x4611,0x01,trq))
      return err;
    if (err = writesdo(0x4611,0x02,0))
      return err;
    if (err = writesdo(0x4611,0x03,trq))
      return err;
    if (err = writesdo(0x4611,0x04,rpm_0))
      return err;

    // calculate constant power part:

    if (twizy_max_rpm > rpm_0)
      rpm_d = (twizy_max_rpm - rpm_0 + (pmap_fib[6]>>1)) / pmap_fib[6];
    else
      rpm_d = 0;

    pwr_d = ((int)twizy_max_pwr_hi - (int)twizy_max_pwr_lo + (pmap_fib[5]>>1)) / pmap_fib[5];

    for (i=0; i<7; i++) {

      if (i<6)
        rpm = rpm_0 + (pmap_fib[i] * rpm_d);
      else
        rpm = twizy_max_rpm;

      if (i<5)
        pwr = twizy_max_pwr_lo + (pmap_fib[i] * pwr_d);
      else
        pwr = twizy_max_pwr_hi;

      trq = ((((UINT32)pwr * 9549 + (rpm>>1)) / rpm) * 16 + 500) / 1000;

      if (err = writesdo(0x4611,0x05+(i<<1),trq))
        return err;
      if (err = writesdo(0x4611,0x06+(i<<1),rpm))
        return err;

    }

  }

  // commit map changes:
  if (err = writesdo(0x4641,0x01,1))
    return err;
  delay5(10);

  return 0;
}


UINT vehicle_twizy_cfg_speed(int max_kph, int warn_kph)
// max_kph: 6..111, -1=reset to default (80)
// warn_kph: 6..111, -1=reset to default (89)
{
  UINT err;
  UINT rpm;

  CHECKPOINT (41)

  // parameter validation:

  if (max_kph == -1)
    max_kph = CFG.DefaultKphMax;
  else if (max_kph < 6 || max_kph > 111)
    return ERR_Range + 1;

  if (warn_kph == -1)
    warn_kph = CFG.DefaultKphWarn;
  else if (warn_kph < 6 || warn_kph > 111)
    return ERR_Range + 2;

  // we need pre-op state for the power map manipulation (0x4611):
  if (err = configmode(1))
    return err;


  // get max torque for map scaling:

  if (twizy_max_trq == 0) {
    if (err = readsdo(0x6076,0x00))
      return err;
    twizy_max_trq = twizy_sdo.data - CFG.DeltaMapTrq;
  }

  // get max power for map scaling:
  //  the controller does not store max power, derive it from rpm/trq:

  if (twizy_max_pwr_lo == 0) {
    if (err = readsdo(0x4611,0x04))
      return err;
    rpm = twizy_sdo.data;
    if (err = readsdo(0x4611,0x03))
      return err;
    if (twizy_sdo.data == CFG.DefaultPMAP[2] && rpm == CFG.DefaultPMAP[3])
      twizy_max_pwr_lo = CFG.DefaultPwrLo;
    else
      twizy_max_pwr_lo = (UINT)((((twizy_sdo.data*1000)>>4) * rpm + (9549>>1)) / 9549);
  }

  if (twizy_max_pwr_hi == 0) {
    if (err = readsdo(0x4611,0x12))
      return err;
    rpm = twizy_sdo.data;
    if (err = readsdo(0x4611,0x11))
      return err;
    if (twizy_sdo.data == CFG.DefaultPMAP[16] && rpm == CFG.DefaultPMAP[17])
      twizy_max_pwr_hi = CFG.DefaultPwrHi;
    else
      twizy_max_pwr_hi = (UINT)((((twizy_sdo.data*1000)>>4) * rpm + (9549>>1)) / 9549);
  }

  
  // set overspeed warning range (STOP lamp):
  rpm = scale(CFG.DefaultRpmWarn,CFG.DefaultKphWarn,warn_kph,400,10900);
  if (err = writesdo(0x3813,0x34,rpm)) // lamp ON
    return err;
  if (err = writesdo(0x3813,0x3c,rpm-CFG.DeltaWarnOff)) // lamp OFF
    return err;

  // calc fwd rpm:
  rpm = scale(CFG.DefaultRpmMax,CFG.DefaultKphMax,max_kph,400,10000);

  // set fwd rpm:
  if (err = writesdo(0x2920,0x05,rpm))
    return err;

  // adjust overspeed braking points (using fixed offsets):
  if (err = writesdo(0x3813,0x33,rpm+CFG.DeltaBrkStart)) // neutral braking start
    return err;
  if (err = writesdo(0x3813,0x35,rpm+CFG.DeltaBrkEnd)) // neutral braking end
    return err;
  if (err = writesdo(0x3813,0x3b,LIMIT_MAX(rpm+CFG.DeltaBrkDown,10900))) // drive brakedown trigger
    return err;

  twizy_max_rpm = rpm;

  return 0;
}


UINT vehicle_twizy_cfg_power(int trq_prc, int pwr_lo_prc, int pwr_hi_prc)
// See "Twizy Powermap Calculator" spreadsheet.
// trq_prc: 10..130, -1=reset to default (100%)
// i.e. TWIZY80:
//    100% = 55.000 Nm
//    128% = 70.125 Nm (130 allowed for easy handling)
// pwr_lo_prc: 10..130, -1=reset to default (100%)
//    100% = 12182 W
// pwr_hi_prc: 10..130, -1=reset to default (100%)
//    100% = 13000 W
{
  UINT err;

  // parameter validation:

  if (trq_prc == -1)
    trq_prc = 100;
  else if (trq_prc < 10 || trq_prc > 130)
    return ERR_Range + 1;

  if (pwr_lo_prc == -1)
    pwr_lo_prc = 100;
  else if (pwr_lo_prc < 10 || pwr_lo_prc > 130)
    return ERR_Range + 2;

  if (pwr_hi_prc == -1)
    pwr_hi_prc = 100;
  else if (pwr_hi_prc < 10 || pwr_hi_prc > 130)
    return ERR_Range + 3;

  // we need pre-op state for the power map manipulation (0x4611):
  if (err = configmode(1))
    return err;

  // get max fwd rpm for map scaling:
  if (twizy_max_rpm == 0) {
    if (err = readsdo(0x2920,0x05))
      return err;
    twizy_max_rpm = twizy_sdo.data;
  }


  // calc peak use torque:
  twizy_max_trq = scale(CFG.DefaultTrq,100,trq_prc,10000,CFG.DefaultTrqLim);

  // set peak use torque:
  if (err = writesdo(0x6076,0x00,twizy_max_trq + CFG.DeltaMapTrq))
    return err;

  // set rated torque too?
  //if (err = writesdo(0x2916,0x01,scale(57000,100,max_prc,10000,70125)))
  //  return err;

  // calc peak use power:
  twizy_max_pwr_lo = scale(CFG.DefaultPwrLo,100,pwr_lo_prc,500,CFG.DefaultPwrLoLim);
  twizy_max_pwr_hi = scale(CFG.DefaultPwrHi,100,pwr_hi_prc,500,CFG.DefaultPwrHiLim);

  return 0;
}


UINT vehicle_twizy_cfg_tsmap(
  char map,
  INT8 t1_prc, INT8 t2_prc, INT8 t3_prc, INT8 t4_prc,
  INT8 t1_spd, INT8 t2_spd, INT8 t3_spd, INT8 t4_spd)
// map: 'D'=Drive 'N'=Neutral 'B'=Footbrake
//
// t1_prc: 0..100, -1=reset to default (D=100, N/B=100)
// t2_prc: 0..100, -1=reset to default (D=100, N/B=80)
// t3_prc: 0..100, -1=reset to default (D=100, N/B=50)
// t4_prc: 0..100, -1=reset to default (D=100, N/B=20)
//
// t1_spd: 0..120, -1=reset to default (D/N/B=33)
// t2_spd: 0..120, -1=reset to default (D/N/B=39)
// t3_spd: 0..120, -1=reset to default (D/N/B=50)
// t4_spd: 0..120, -1=reset to default (D/N/B=66)
{
  UINT err;
  UINT8 base;
  UINT val;

  // parameter validation:

  if (map != 'D' && map != 'N' && map != 'B')
    return ERR_Range + 1;

  // torque points:

  if ((t1_prc != -1) && (t1_prc < 0 || t1_prc > 100))
    return ERR_Range + 2;
  if ((t2_prc != -1) && (t2_prc < 0 || t2_prc > 100))
    return ERR_Range + 3;
  if ((t3_prc != -1) && (t3_prc < 0 || t3_prc > 100))
    return ERR_Range + 4;
  if ((t4_prc != -1) && (t4_prc < 0 || t4_prc > 100))
    return ERR_Range + 5;

  // speed points:

  if ((t1_spd != -1) && (t1_spd < 0 || t1_spd > 120))
    return ERR_Range + 6;
  if ((t2_spd != -1) && (t2_spd < 0 || t2_spd > 120))
    return ERR_Range + 7;
  if ((t3_spd != -1) && (t3_spd < 0 || t3_spd > 120))
    return ERR_Range + 8;
  if ((t4_spd != -1) && (t4_spd < 0 || t4_spd > 120))
    return ERR_Range + 9;

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

  // torque points:

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

  // speed points:

  if (t1_spd >= 0)
    val = scale(CFG.DefaultMapSpd[0],33,t1_spd,0,65535);
  else
    val = (map=='D') ? 3000 : CFG.DefaultMapSpd[0];
  if (err = writesdo(0x3813,base+1,val))
    return err;

  if (t2_spd >= 0)
    val = scale(CFG.DefaultMapSpd[1],39,t2_spd,0,65535);
  else
    val = (map=='D') ? 3500 : CFG.DefaultMapSpd[1];
  if (err = writesdo(0x3813,base+3,val))
    return err;

  if (t3_spd >= 0)
    val = scale(CFG.DefaultMapSpd[2],50,t3_spd,0,65535);
  else
    val = (map=='D') ? 4500 : CFG.DefaultMapSpd[2];
  if (err = writesdo(0x3813,base+5,val))
    return err;

  if (t4_spd >= 0)
    val = scale(CFG.DefaultMapSpd[3],66,t4_spd,0,65535);
  else
    val = (map=='D') ? 6000 : CFG.DefaultMapSpd[3];
  if (err = writesdo(0x3813,base+7,val))
    return err;

  return 0;
}


UINT vehicle_twizy_cfg_drive(int max_prc)
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
    neutral_prc = CFG.DefaultRecupPrc;
  else if (neutral_prc < 0 || neutral_prc > 100)
    return ERR_Range + 1;

  if (brake_prc == -1)
    brake_prc = CFG.DefaultRecupPrc;
  else if (brake_prc < 0 || brake_prc > 100)
    return ERR_Range + 2;

  // set:
  if (err = writesdo(0x2920,0x03,scale(CFG.DefaultRecup,CFG.DefaultRecupPrc,neutral_prc,0,1000)))
    return err;
  if (err = writesdo(0x2920,0x04,scale(CFG.DefaultRecup,CFG.DefaultRecupPrc,brake_prc,0,1000)))
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
    start_prc = CFG.DefaultRampStartPrc;
  else if (start_prc < 1 || start_prc > 100)
    return ERR_Range + 1;

  if (accel_prc == -1)
    accel_prc = CFG.DefaultRampAccelPrc;
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

  if (err = writesdo(0x291c,0x02,scale(CFG.DefaultRampStart,CFG.DefaultRampStartPrc,start_prc,10,10000)))
    return err;
  if (err = writesdo(0x2920,0x07,scale(CFG.DefaultRampAccel,CFG.DefaultRampAccelPrc,accel_prc,10,10000)))
    return err;
  if (err = writesdo(0x2920,0x0b,scale(2000,20,decel_prc,10,10000)))
    return err;
  if (err = writesdo(0x2920,0x0d,scale(4000,40,neutral_prc,10,10000)))
    return err;
  if (err = writesdo(0x2920,0x0e,scale(4000,40,brake_prc,10,10000)))
    return err;

  return 0;
}


UINT vehicle_twizy_cfg_rampl(int accel_prc, int decel_prc)
// accel_prc: 1..100, -1=reset to default (30)
// decel_prc: 0..100, -1=reset to default (30)
{
  UINT err;

  // parameter validation:

  if (accel_prc == -1)
    accel_prc = 30;
  else if (accel_prc < 1 || accel_prc > 100)
    return ERR_Range + 1;

  if (decel_prc == -1)
    decel_prc = 30;
  else if (decel_prc < 0 || decel_prc > 100)
    return ERR_Range + 2;

  // set:

  if (err = writesdo(0x2920,0x0f,scale(6000,30,accel_prc,0,20000)))
    return err;
  if (err = writesdo(0x2920,0x10,scale(6000,30,decel_prc,0,20000)))
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
    twizy_sdo.data |= 0x2000;
  else
    twizy_sdo.data &= ~0x2000;
  if (err = writesdo(0x2910,0x01,twizy_sdo.data))
    return err;

  return 0;
}


// vehicle_twizy_cfg_calc_checksum: get checksum for twizy_cfg_profile
//
// Note: for extendability of struct twizy_cfg_profile, 0-Bytes will
//  not affect the checksum, so new fields can simply be added at the end
//  without losing version compatibility.
//  (0-bytes translate to value -1 = default)
BYTE vehicle_twizy_cfg_calc_checksum(void)
{
  UINT checksum;
  BYTE i;

  checksum = 0x0101; // version tag

  for (i=1; i<sizeof(twizy_cfg_profile); i++)
    checksum += ((BYTE *)&twizy_cfg_profile)[i];

  if ((checksum & 0x0ff) == 0)
    checksum >>= 8;

  return (checksum & 0x0ff);
}


// vehicle_twizy_cfg_readprofile: read profile from params to twizy_cgf_profile
//    with checksum validation
//    it invalid checksum initialize to default config & return FALSE
BOOL vehicle_twizy_cfg_readprofile(char key)
{
  BYTE checksum;

  if (key >= 1 && key <= 3) {
    // read custom cfg from params:
    par_getbin(PARAM_PROFILE_S + ((key-1)<<1), &twizy_cfg_profile, sizeof twizy_cfg_profile);

    // check consistency:
    if (twizy_cfg_profile.checksum == vehicle_twizy_cfg_calc_checksum())
      return TRUE;
    else {
      // init to defaults:
      memset(&twizy_cfg_profile, 0, sizeof(twizy_cfg_profile));
      return FALSE;
    }
  }
  else {
    // no custom cfg: load defaults
    memset(&twizy_cfg_profile, 0, sizeof(twizy_cfg_profile));
    return TRUE;
  }
}


// vehicle_twizy_cfg_writeprofile: write from twizy_cgf_profile to params
//    with checksum calculation
BOOL vehicle_twizy_cfg_writeprofile(char key)
{
  if (key >= 1 && key <= 3) {
    twizy_cfg_profile.checksum = vehicle_twizy_cfg_calc_checksum();
    par_setbin(PARAM_PROFILE_S + ((key-1)<<1), &twizy_cfg_profile, sizeof twizy_cfg_profile);
    return TRUE;
  }
  else {
    return FALSE;
  }
}


// vehicle_twizy_cfg_switchprofile: load and configure a profile
//    return value: 0 = no error, else error code
//    sets: twizy_cfg.profile_cfgmode, twizy_cfg.profile_user
UINT vehicle_twizy_cfg_switchprofile(char key)
{
  UINT err;

  // check key:

  if (key < 0 || key > 3)
    return ERR_Range + 1;

  // load new profile:

  vehicle_twizy_cfg_readprofile(key);
  
  twizy_cfg.unsaved = 0;

  // login:

  if (err = login(1))
    return err;
  
  // update op (user) mode params:

  if (err = vehicle_twizy_cfg_drive(cfgparam(drive)))
    return err;

  if (err = vehicle_twizy_cfg_recup(cfgparam(neutral),cfgparam(brake)))
    return err;

  if (err = vehicle_twizy_cfg_ramps(cfgparam(ramp_start),cfgparam(ramp_accel),cfgparam(ramp_decel),cfgparam(ramp_neutral),cfgparam(ramp_brake)))
    return err;

  if (err = vehicle_twizy_cfg_rampl(cfgparam(ramplimit_accel),cfgparam(ramplimit_decel)))
    return err;

  if (err = vehicle_twizy_cfg_smoothing(cfgparam(smooth)))
    return err;

  // update user profile status:
  twizy_cfg.profile_user = key;


  // update pre-op (admin) mode params if configmode possible at the moment:

  // triple (redundancy) safety check:
  // only allow configmode attempt if
  // a) Twizy is not moving
  // b) and in "N" mode
  // c) and not in "GO" mode
  if ((twizy_speed != 0) || (twizy_status & (CAN_STATUS_MODE | CAN_STATUS_GO))) {
    err = ERR_CfgModeFailed;
  }
  else {
    // ok, try to enter configmode:
    if (err = configmode(1))
      return err;

    err = vehicle_twizy_cfg_speed(cfgparam(speed),cfgparam(warn));
    if (!err)
      err = vehicle_twizy_cfg_power(cfgparam(torque),cfgparam(power_low),cfgparam(power_high));
    if (!err)
      err = vehicle_twizy_cfg_makepowermap();

    if (!err)
      err = vehicle_twizy_cfg_tsmap('D',
              cfgparam(tsmap[0].prc1),cfgparam(tsmap[0].prc2),cfgparam(tsmap[0].prc3),cfgparam(tsmap[0].prc4),
              cfgparam(tsmap[0].spd1),cfgparam(tsmap[0].spd2),cfgparam(tsmap[0].spd3),cfgparam(tsmap[0].spd4));
    if (!err)
      err = vehicle_twizy_cfg_tsmap('N',
              cfgparam(tsmap[1].prc1),cfgparam(tsmap[1].prc2),cfgparam(tsmap[1].prc3),cfgparam(tsmap[1].prc4),
              cfgparam(tsmap[1].spd1),cfgparam(tsmap[1].spd2),cfgparam(tsmap[1].spd3),cfgparam(tsmap[1].spd4));
    if (!err)
      err = vehicle_twizy_cfg_tsmap('B',
              cfgparam(tsmap[2].prc1),cfgparam(tsmap[2].prc2),cfgparam(tsmap[2].prc3),cfgparam(tsmap[2].prc4),
              cfgparam(tsmap[2].spd1),cfgparam(tsmap[2].spd2),cfgparam(tsmap[2].spd3),cfgparam(tsmap[2].spd4));

    if (!err)
      err = vehicle_twizy_cfg_brakelight(cfgparam(brakelight_on),cfgparam(brakelight_off));

    delay5(10);
    configmode(0);

    // update cfgmode profile status:
    if (err == 0)
      twizy_cfg.profile_cfgmode = key;
  }

  // reset error cnt:
  twizy_button_cnt = 0;

  return err;
}


// RESET all known parameters to default:
//  ATT: will not reset arbitrary WRITEs, just macro commands!
UINT vehicle_twizy_cfg_reset(void)
{
  UINT err;

  // profile 0 = init default profile:
  
  vehicle_twizy_cfg_readprofile(0);

  if (twizy_cfg.profile_user == 0)
    twizy_cfg.unsaved = 0;
  else
    twizy_cfg.unsaved = 1;

  // issue reset on every macro command:

  if (err = vehicle_twizy_cfg_speed(-1,-1))
    return err;
  if (err = vehicle_twizy_cfg_power(-1,-1,-1))
    return err;
  if (err = vehicle_twizy_cfg_makepowermap())
    return err;

  if (err = vehicle_twizy_cfg_tsmap('D',-1,-1,-1,-1,-1,-1,-1,-1))
    return err;
  if (err = vehicle_twizy_cfg_tsmap('N',-1,-1,-1,-1,-1,-1,-1,-1))
    return err;
  if (err = vehicle_twizy_cfg_tsmap('B',-1,-1,-1,-1,-1,-1,-1,-1))
    return err;

  if (err = vehicle_twizy_cfg_drive(-1))
    return err;
  if (err = vehicle_twizy_cfg_recup(-1,-1))
    return err;

  if (err = vehicle_twizy_cfg_ramps(-1,-1,-1,-1,-1))
    return err;
  if (err = vehicle_twizy_cfg_rampl(-1,-1))
    return err;
  if (err = vehicle_twizy_cfg_smoothing(-1))
    return err;

  if (err = vehicle_twizy_cfg_brakelight(-1,-1))
    return err;

  twizy_cfg.profile_cfgmode = twizy_cfg.profile_user;

  // reset error cnt:
  twizy_button_cnt = 0;

  return 0;
}


// utility: output SDO to string:
char *vehicle_twizy_fmt_sdo(char *s)
{
  s = stp_rom(s, " SDO ");
  if ((twizy_sdo.control & 0b11100000) == 0b10000000)
    s = stp_rom(s, "ABORT ");
  s = stp_x(s, "0x", twizy_sdo.index);
  s = stp_sx(s, ".", twizy_sdo.subindex);
  if (twizy_sdo.data > 0x0ffff)
    s = stp_lx(s, ": 0x", twizy_sdo.data);
  else
    s = stp_x(s, ": 0x", twizy_sdo.data);
  return s;
}


// utility: output power map debug info to string:
char *vehicle_twizy_fmt_powermap(char *s)
{
  if (readsdo(0x4611,0x04) == 0) {
    s = stp_ul(s, " p2=", twizy_sdo.data);
    if (readsdo(0x4611,0x03) == 0)
      s = stp_ul(s, ":", twizy_sdo.data);
  }
  if (readsdo(0x4611,0x06) == 0) {
    s = stp_ul(s, " p3=", twizy_sdo.data);
    if (readsdo(0x4611,0x05) == 0)
      s = stp_ul(s, ":", twizy_sdo.data);
  }
  if (readsdo(0x4611,0x10) == 0) {
    s = stp_ul(s, " p8=", twizy_sdo.data);
    if (readsdo(0x4611,0x0f) == 0)
      s = stp_ul(s, ":", twizy_sdo.data);
  }
  if (readsdo(0x4611,0x12) == 0) {
    s = stp_ul(s, " p9=", twizy_sdo.data);
    if (readsdo(0x4611,0x11) == 0)
      s = stp_ul(s, ":", twizy_sdo.data);
  }
  return s;
}


//
// CFG SMS COMMAND MAIN (DISPATCHER):
//
BOOL vehicle_twizy_cfg_sms(BOOL premsg, char *caller, char *cmd, char *arguments)
{
  UINT err;
  char *s;
  BOOL go_op_onexit = TRUE;
  
  int arg[5] = {-1,-1,-1,-1,-1};
  INT8 arg2[4] = {-1,-1,-1,-1};
  long data;
  char maps[4] = {'D','N','B',0};
  UINT8 i;

  CHECKPOINT (40)

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
    // READS[TR] index_hex subindex_hex
    if (arguments = net_sms_nextarg(arguments))
      arg[0] = (int)axtoul(arguments);
    if (arguments = net_sms_nextarg(arguments))
      arg[1] = (int)axtoul(arguments);

    if (!arguments) {
      s = stp_rom(s, "ERROR: Too few args");
    }
    else {
      if (cmd[4] != 'S') {
        // READ:
        if (err = readsdo(arg[0], arg[1])) {
          s = stp_x(s, "ERROR ", err);
          if (ERROR_IN_SDO(err))
            s = vehicle_twizy_fmt_sdo(s);
        }
        else {
          s = vehicle_twizy_fmt_sdo(s);
          s = stp_ul(s, " = ", twizy_sdo.data);
        }
      }
      else {
        // READS: SMS intro 'CFG READS: 0x1234.56=' = 21 chars, 139 remaining
        if (err = readsdo_buf(arg[0], arg[1], net_msg_scratchpad, (i=139, &i))) {
          s = stp_x(s, "ERROR ", err);
          if (ERROR_IN_SDO(err))
            s = vehicle_twizy_fmt_sdo(s);
        }
        else {
          net_msg_scratchpad[139-i] = 0;
          s = stp_x(s, "0x", arg[0]);
          s = stp_sx(s, ".", arg[1]);
          s = stp_s(s, "=", net_msg_scratchpad);
        }
      }
    }

    go_op_onexit = FALSE;
  }

  else if (strncmppgm2ram(cmd, "WRITE", 5) == 0) {
    // WRITE index_hex subindex_hex data_dec
    // WRITEO[NLY] index_hex subindex_hex data_dec
    if (arguments = net_sms_nextarg(arguments))
      arg[0] = (int)axtoul(arguments);
    if (arguments = net_sms_nextarg(arguments))
      arg[1] = (int)axtoul(arguments);
    if (arguments = net_sms_nextarg(arguments))
      data = atol(arguments);

    if (!arguments) {
      s = stp_rom(s, "ERROR: Too few args");
    }
    else {

      if (cmd[5] == 'O') {
        // WRITEONLY:

        // write new value:
        if (err = writesdo(arg[0], arg[1], data)) {
          s = stp_x(s, "ERROR ", err);
          if (ERROR_IN_SDO(err))
            s = vehicle_twizy_fmt_sdo(s);
        }
        else {
          s = stp_rom(s, "OK: ");
          s = vehicle_twizy_fmt_sdo(s);
        }
      }
      
      else {
        // READ-WRITE:

        // read old value:
        if (err = readsdo(arg[0], arg[1])) {
          s = stp_x(s, "ERROR ", err);
          if (ERROR_IN_SDO(err))
            s = vehicle_twizy_fmt_sdo(s);
        }
        else {
          // read ok:
          s = stp_rom(s, "OLD:");
          s = vehicle_twizy_fmt_sdo(s);
          s = stp_ul(s, " = ", twizy_sdo.data);

          // write new value:
          if (err = writesdo(arg[0], arg[1], data)) {
            s = stp_x(s, " ERROR ", err);
            if (ERROR_IN_SDO(err))
              s = vehicle_twizy_fmt_sdo(s);
          }
          else {
            s = stp_ul(s, " => NEW: ", data);
          }
        }
      }
    }

    go_op_onexit = FALSE;
  }

  else if (strncmppgm2ram(cmd, "SPEED", 5) == 0) {
    // SPEED [max_kph] [warn_kph]

    if (arguments = net_sms_nextarg(arguments))
      arg[0] = atoi(arguments);
    if (arguments = net_sms_nextarg(arguments))
      arg[1] = atoi(arguments);

    if ((err = vehicle_twizy_cfg_speed(arg[0], arg[1])) == 0)
      err = vehicle_twizy_cfg_makepowermap();

    if (err) {
      s = stp_x(s, "ERROR ", err);
      if (ERROR_IN_SDO(err))
        s = vehicle_twizy_fmt_sdo(s);
    }
    else {
      // update profile:
      twizy_cfg_profile.speed = cfgvalue(arg[0]);
      twizy_cfg_profile.warn = cfgvalue(arg[1]);
      twizy_cfg.unsaved = 1;

      // success message:
      s = stp_rom(s, "OK, power cycle to activate!");

      // debug:
      if (readsdo(0x2920,0x05) == 0)
        s = stp_ul(s, " fwd=", twizy_sdo.data);
      if (readsdo(0x3813,0x34) == 0)
        s = stp_ul(s, " wrn=", twizy_sdo.data);
      s = vehicle_twizy_fmt_powermap(s);
    }
  }

  else if (strncmppgm2ram(cmd, "POWER", 5) == 0) {
    // POWER [trq_prc] [pwr_lo_prc] [pwr_hi_prc]

    if (arguments = net_sms_nextarg(arguments))
      arg[0] = atoi(arguments);

    if (arguments = net_sms_nextarg(arguments))
      arg[1] = atoi(arguments);
    else
      arg[1] = arg[0];

    if (arguments = net_sms_nextarg(arguments))
      arg[2] = atoi(arguments);
    else
      arg[2] = arg[1];

    if ((err = vehicle_twizy_cfg_power(arg[0], arg[1], arg[2])) == 0)
      err = vehicle_twizy_cfg_makepowermap();

    if (err) {
      s = stp_x(s, "ERROR ", err);
      if (ERROR_IN_SDO(err))
        s = vehicle_twizy_fmt_sdo(s);
    }
    else {
      // update profile:
      twizy_cfg_profile.torque = cfgvalue(arg[0]);
      twizy_cfg_profile.power_low = cfgvalue(arg[1]);
      twizy_cfg_profile.power_high = cfgvalue(arg[2]);
      twizy_cfg.unsaved = 1;

      // success message:
      s = stp_rom(s, "OK, power cycle to activate!");

      // debug:
      if (readsdo(0x6076,0x00) == 0)
        s = stp_ul(s, " trq=", twizy_sdo.data - CFG.DeltaMapTrq);
      s = vehicle_twizy_fmt_powermap(s);
    }
  }

  else if (strncmppgm2ram(cmd, "TSMAP", 5) == 0) {
    // TSMAP [maps] [t1_prc[@t1_spd]] [t2_prc[@t2_spd]] [t3_prc[@t3_spd]] [t4_prc[@t4_spd]]

    if (arguments = net_sms_nextarg(arguments)) {
      for (i=0; i<3 && arguments[i]; i++)
        maps[i] = arguments[i] & ~0x20;
      maps[i] = 0;
    }
    for (i=0; i<4; i++)
    {
      if (arguments = net_sms_nextarg(arguments))
      {
        arg[i] = atoi(arguments);
        if (cmd = strchr(arguments, '@'))
          arg2[i] = atoi(cmd+1);
      }
    }

    for (i=0; maps[i]; i++) {
      if (err = vehicle_twizy_cfg_tsmap(maps[i],
                  arg[0], arg[1], arg[2], arg[3],
                  arg2[0], arg2[1], arg2[2], arg2[3]))
        break;

      // update profile:
      err = (maps[i]=='D') ? 0 : ((maps[i]=='N') ? 1 : 2);
      twizy_cfg_profile.tsmap[err].prc1 = cfgvalue(arg[0]);
      twizy_cfg_profile.tsmap[err].prc2 = cfgvalue(arg[1]);
      twizy_cfg_profile.tsmap[err].prc3 = cfgvalue(arg[2]);
      twizy_cfg_profile.tsmap[err].prc4 = cfgvalue(arg[3]);
      twizy_cfg_profile.tsmap[err].spd1 = cfgvalue(arg2[0]);
      twizy_cfg_profile.tsmap[err].spd2 = cfgvalue(arg2[1]);
      twizy_cfg_profile.tsmap[err].spd3 = cfgvalue(arg2[2]);
      twizy_cfg_profile.tsmap[err].spd4 = cfgvalue(arg2[3]);
      err = 0;
    }

    if (err) {
      s = stp_x(s, "ERROR ", err);
      s = stp_rom(s, " IN MAP ");
      *s++ = maps[i]; *s = 0;
      if (ERROR_IN_SDO(err))
        s = vehicle_twizy_fmt_sdo(s);
    }
    else {
      twizy_cfg.unsaved = 1;

      s = stp_rom(s, "OK");

      // debug:
      if (readsdo(0x3813,0x24+6) == 0)
        s = stp_ul(s, " D4=", twizy_sdo.data);
      if (readsdo(0x3813,0x24+7) == 0)
        s = stp_ul(s, "@", twizy_sdo.data);
      if (readsdo(0x3813,0x1b+6) == 0)
        s = stp_ul(s, " N4=", twizy_sdo.data);
      if (readsdo(0x3813,0x1b+7) == 0)
        s = stp_ul(s, "@", twizy_sdo.data);
      if (readsdo(0x3813,0x07+6) == 0)
        s = stp_ul(s, " B4=", twizy_sdo.data);
      if (readsdo(0x3813,0x07+7) == 0)
        s = stp_ul(s, "@", twizy_sdo.data);
    }
  }

  else if (strncmppgm2ram(cmd, "DRIVE", 5) == 0) {
    // DRIVE [max_prc]

    if (arguments = net_sms_nextarg(arguments))
      arg[0] = atoi(arguments);

    if (err = vehicle_twizy_cfg_drive(arg[0])) {
      s = stp_x(s, "ERROR ", err);
      if (ERROR_IN_SDO(err))
        s = vehicle_twizy_fmt_sdo(s);
    }
    else {
      // update profile:
      twizy_cfg_profile.drive = cfgvalue(arg[0]);
      twizy_cfg.unsaved = 1;

      // success message:
      s = stp_rom(s, "OK");

      // debug:
      if (readsdo(0x2920,0x01) == 0)
        s = stp_ul(s, " pow=", twizy_sdo.data);
    }
  }

  else if (strncmppgm2ram(cmd, "RECUP", 5) == 0) {
    // RECUP [neutral_prc] [brake_prc]

    if (arguments = net_sms_nextarg(arguments))
      arg[0] = atoi(arguments);

    if (arguments = net_sms_nextarg(arguments))
      arg[1] = atoi(arguments);
    else
      arg[1] = arg[0];

    if (err = vehicle_twizy_cfg_recup(arg[0], arg[1])) {
      s = stp_x(s, "ERROR ", err);
      if (ERROR_IN_SDO(err))
        s = vehicle_twizy_fmt_sdo(s);
    }
    else {
      // update profile:
      twizy_cfg_profile.neutral = cfgvalue(arg[0]);
      twizy_cfg_profile.brake = cfgvalue(arg[1]);
      twizy_cfg.unsaved = 1;

      // success message:
      s = stp_rom(s, "OK");

      // debug:
      if (readsdo(0x2920,0x03) == 0)
        s = stp_ul(s, " ntr=", twizy_sdo.data);
      if (readsdo(0x2920,0x04) == 0)
        s = stp_ul(s, " brk=", twizy_sdo.data);
    }
  }

  else if (strncmppgm2ram(cmd, "RAMPS", 5) == 0) {
    // RAMPS [start_prc] [accel_prc] [decel_prc] [neutral_prc] [brake_prc]

    if (arguments = net_sms_nextarg(arguments))
      arg[0] = atoi(arguments);
    if (arguments = net_sms_nextarg(arguments))
      arg[1] = atoi(arguments);
    if (arguments = net_sms_nextarg(arguments))
      arg[2] = atoi(arguments);
    if (arguments = net_sms_nextarg(arguments))
      arg[3] = atoi(arguments);
    if (arguments = net_sms_nextarg(arguments))
      arg[4] = atoi(arguments);

    if (err = vehicle_twizy_cfg_ramps(arg[0], arg[1], arg[2], arg[3], arg[4])) {
      s = stp_x(s, "ERROR ", err);
      if (ERROR_IN_SDO(err))
        s = vehicle_twizy_fmt_sdo(s);
    }
    else {
      // update profile:
      twizy_cfg_profile.ramp_start = cfgvalue(arg[0]);
      twizy_cfg_profile.ramp_accel = cfgvalue(arg[1]);
      twizy_cfg_profile.ramp_decel = cfgvalue(arg[2]);
      twizy_cfg_profile.ramp_neutral = cfgvalue(arg[3]);
      twizy_cfg_profile.ramp_brake = cfgvalue(arg[4]);
      twizy_cfg.unsaved = 1;

      // success message:
      s = stp_rom(s, "OK");

      // debug:
      if (readsdo(0x291c,0x02) == 0)
        s = stp_ul(s, " rpst=", twizy_sdo.data);
      if (readsdo(0x2920,0x07) == 0)
        s = stp_ul(s, " rpac=", twizy_sdo.data);
      if (readsdo(0x2920,0x0e) == 0)
        s = stp_ul(s, " rpbk=", twizy_sdo.data);
    }
  }

  else if (strncmppgm2ram(cmd, "RAMPL", 5) == 0) {
    // RAMPL [accel_prc] [decel_prc]

    if (arguments = net_sms_nextarg(arguments))
      arg[0] = atoi(arguments);
    if (arguments = net_sms_nextarg(arguments))
      arg[1] = atoi(arguments);

    if (err = vehicle_twizy_cfg_rampl(arg[0], arg[1])) {
      s = stp_x(s, "ERROR ", err);
      if (ERROR_IN_SDO(err))
        s = vehicle_twizy_fmt_sdo(s);
    }
    else {
      // update profile:
      twizy_cfg_profile.ramplimit_accel = cfgvalue(arg[0]);
      twizy_cfg_profile.ramplimit_decel = cfgvalue(arg[1]);
      twizy_cfg.unsaved = 1;

      // success message:
      s = stp_rom(s, "OK");

      // debug:
      if (readsdo(0x2920,0x0f) == 0)
        s = stp_ul(s, " rlacc=", twizy_sdo.data);
      if (readsdo(0x2920,0x10) == 0)
        s = stp_ul(s, " rldec=", twizy_sdo.data);
    }
  }

  else if (strncmppgm2ram(cmd, "SMOOTH", 6) == 0) {
    // SMOOTH [prc]

    if (arguments = net_sms_nextarg(arguments))
      arg[0] = atoi(arguments);

    if (err = vehicle_twizy_cfg_smoothing(arg[0])) {
      s = stp_x(s, "ERROR ", err);
      if (ERROR_IN_SDO(err))
        s = vehicle_twizy_fmt_sdo(s);
    }
    else {
      // update profile:
      twizy_cfg_profile.smooth = cfgvalue(arg[0]);
      twizy_cfg.unsaved = 1;

      // success message:
      s = stp_rom(s, "OK");

      // debug:
      if (readsdo(0x290a,0x01) == 0)
        s = stp_ul(s, " scrv=", twizy_sdo.data);
      if (readsdo(0x290a,0x03) == 0)
        s = stp_ul(s, " srmp=", twizy_sdo.data);
    }
  }

  else if (strncmppgm2ram(cmd, "BRAKELIGHT", 10) == 0) {
    // BRAKELIGHT [on_lev] [off_lev]

    if (arguments = net_sms_nextarg(arguments))
      arg[0] = atoi(arguments);

    if (arguments = net_sms_nextarg(arguments))
      arg[1] = atoi(arguments);
    else
      arg[1] = arg[0];

    if (err = vehicle_twizy_cfg_brakelight(arg[0], arg[1])) {
      s = stp_x(s, "ERROR ", err);
      if (ERROR_IN_SDO(err))
        s = vehicle_twizy_fmt_sdo(s);
    }
    else {
      // update profile:
      twizy_cfg_profile.brakelight_on = cfgvalue(arg[0]);
      twizy_cfg_profile.brakelight_off = cfgvalue(arg[1]);
      twizy_cfg.unsaved = 1;

      // success message:
      s = stp_rom(s, "OK");

      // debug:
      if (readsdo(0x3813,0x06) == 0)
        s = stp_ul(s, " bon=", twizy_sdo.data);
      if (readsdo(0x2910,0x01) == 0)
        s = stp_x(s, " flgs=", twizy_sdo.data);
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
        s = stp_ul(s, " fwd=", twizy_sdo.data);
      if (readsdo(0x6076,0x00) == 0)
        s = stp_ul(s, " trq=", twizy_sdo.data - CFG.DeltaMapTrq);

      if (readsdo(0x4611,0x04) == 0)
        s = stp_ul(s, " p2=", twizy_sdo.data);
      if (readsdo(0x4611,0x03) == 0)
        s = stp_ul(s, ":", twizy_sdo.data);

      if (readsdo(0x2920,0x03) == 0)
        s = stp_ul(s, " ntr=", twizy_sdo.data);

      if (readsdo(0x291c,0x02) == 0)
        s = stp_ul(s, " rpst=", twizy_sdo.data);
    }
  }

  else if (strncmppgm2ram(cmd, "SAVE", 4) == 0) {
    // SAVE [nr]: save profile to EEPROM

    if (arguments = net_sms_nextarg(arguments))
      arg[0] = atoi(arguments);
    else
      arg[0] = twizy_cfg.profile_user; // save as current profile

    if (vehicle_twizy_cfg_writeprofile(arg[0]) == FALSE) {
      s = stp_i(s, "ERROR: can't save to #", arg[0]);
    }
    else {
      s = stp_i(s, "OK saved as #", arg[0]);

      // make destination new current:
      par_set(PARAM_PROFILE, s-1);
      twizy_cfg.profile_user = arg[0];
      twizy_cfg.profile_cfgmode = arg[0];
      twizy_cfg.unsaved = 0;
    }
  }

  else if (strncmppgm2ram(cmd, "LOAD", 4) == 0) {
    // LOAD [nr]: load/restore profile from EEPROM

    if (arguments = net_sms_nextarg(arguments))
      arg[0] = atoi(arguments);
    else
      arg[0] = twizy_cfg.profile_user; // restore current profile

    err = vehicle_twizy_cfg_switchprofile(arg[0]);

    if (err && (err != ERR_CfgModeFailed)) {
      s = stp_x(s, "ERROR ", err);
      if (ERROR_IN_SDO(err))
        s = vehicle_twizy_fmt_sdo(s);
    }
    else {
      if (err == ERR_CfgModeFailed) {
        s = stp_i(s, "PARTIAL #", arg[0]);
        par_set(PARAM_PROFILE, s-1);
        s = stp_rom(s, ": retry at stop!");
      }
      else {
        s = stp_i(s, "OK #", arg[0]);
        par_set(PARAM_PROFILE, s-1);

        s = stp_i(s, " SPEED ", cfgparam(speed));
        s = stp_i(s, " ", cfgparam(warn));
        s = stp_i(s, " POWER ", cfgparam(torque));
        s = stp_i(s, " ", cfgparam(power_low));
        s = stp_i(s, " ", cfgparam(power_high));
      }

      s = stp_i(s, " DRIVE ", cfgparam(drive));
      s = stp_i(s, " RECUP ", cfgparam(neutral));
      s = stp_i(s, " ", cfgparam(brake));
    }
  }

  else if (strncmppgm2ram(cmd, "INFO", 4) == 0) {
    // INFO: output main params

    s = stp_i(s, "#", twizy_cfg.profile_user);

    s = stp_i(s, " SPEED ", cfgparam(speed));
    s = stp_i(s, " ", cfgparam(warn));

    s = stp_i(s, " POWER ", cfgparam(torque));
    s = stp_i(s, " ", cfgparam(power_low));
    s = stp_i(s, " ", cfgparam(power_high));

    s = stp_i(s, " DRIVE ", cfgparam(drive));
    
    s = stp_i(s, " RECUP ", cfgparam(neutral));
    s = stp_i(s, " ", cfgparam(brake));

    s = stp_i(s, " RAMPS ", cfgparam(ramp_start));
    s = stp_i(s, " ", cfgparam(ramp_accel));
    s = stp_i(s, " ", cfgparam(ramp_decel));
    s = stp_i(s, " ", cfgparam(ramp_neutral));
    s = stp_i(s, " ", cfgparam(ramp_brake));

    s = stp_i(s, " SMOOTH ", cfgparam(smooth));
  }

  else {
    // unknown command
    s = stp_rom(s, "Unknown command");
  }

  // go operational?
  if (go_op_onexit)
    configmode(0);

  // logout should not be needed here

  // reset error counter to inhibit unwanted resets:
  twizy_button_cnt = 0;

  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE; // handled, but no SMS has been started

  net_send_sms_start(caller);
  net_puts_ram(net_scratchpad);

  CHECKPOINT (0)
  return TRUE;
}



void vehicle_twizy_faults_prepmsg(void)
{
  char *s;
  UINT8 len;

  // append to net_scratchpad:
  s = strchr(net_scratchpad, 0);

  // ...fault code:
  s = stp_x(s, "SEVCON ALERT 0x", twizy_sevcon_fault);

  // ...fault description:
  if (writesdo(0x5610,0x01,twizy_sevcon_fault) == 0) {
    if (readsdo_buf(0x5610,0x02,s+1,(len=50,&len)) == 0) {
      *s = ' ';
      s += 51-len;
      *s = 0;
    }
  }
}


/**
 * CMD_QueryLogs:
 * 
 * Arguments: which=1 [start=0]
 * 
 *  which: 1=Alerts, 2=Faults FIFO, 3=System FIFO, 4=Event counter, 5=Min/max monitor
 *    default=1
 *    reset: 1 cannot be reset, 99 = reset all
 *
 *  start: first entry to fetch, default=0 (returns max 10 entries per call)
 *    reset: start irrelevant
 * 
 */


// util: string-print SEVCON fault code + description:
char *stp_sevcon_fault(char *s, const rom char *prefix, UINT code)
{
  UINT8 len;
  
  // add fault code:
  s = stp_x(s, prefix, code);
  *s++ = ',';
  
  // add fault description (',' replaced by ';'):
  if ((code > 0) && (writesdo(0x5610,0x01,code) == 0)) {
    if (readsdo_buf(0x5610,0x02,s,(len=50,&len)) == 0) {
      for (; len<50 && *s; len++, s++)
        if (*s==',') *s=';';
    }
  }
  
  *s = 0;
  return s;
}


UINT vehicle_twizy_resetlogs_msgp(UINT8 which, UINT8 *retcnt)
{
  UINT err;
  UINT8 n, cnt=0;

  // login necessary:
  if (which > 1) {
    if (err = login(1))
      return err;
  }

  /* Alerts (active faults):
   * cannot be reset (just by power cycle)
   */
  if (which == 1 || which == 99) {
    // get cnt anyway:
    if (readsdo(0x5300,0x01) == 0)
      cnt = twizy_sdo.data;
  }

  /* Faults FIFO: */
  if (which == 2 || which == 99) {
    err = writesdo(0x4110,0x01,1);
    // get new cnt:
    if (readsdo(0x4110,0x02) == 0)
      cnt = twizy_sdo.data;
  }

  /* System FIFO: */
  if ((!err) && (which == 3 || which == 99)) {
    err = writesdo(0x4100,0x01,1);
    // get new cnt:
    if (readsdo(0x4100,0x02) == 0)
      cnt = twizy_sdo.data;
  }

  /* Event counter: */
  if ((!err) && (which == 4 || which == 99)) {
    err = writesdo(0x4200,0x01,1);
    cnt = 10;
  }

  /* Min / max monitor: */
  if ((!err) && (which == 5 || which == 99)) {
    err = writesdo(0x4300,0x01,1);
    cnt = 2;
  }

  if (retcnt)
    *retcnt = cnt;
  return err;
}


UINT vehicle_twizy_querylogs_msgp(UINT8 which, UINT8 start, UINT8 *retcnt)
{
  char *s;
  UINT err;
  UINT8 n, len, cnt;

  // login necessary for FIFOs and counters:
  if (which > 1 && which < 5) {
    if (err = login(1))
      return err;
  }


  /* Key time:
      MP-0 HRT-ENG-LogKeyTime
	,0,86400
	,<KeyHour>,<KeyMinSec>
   */
  s = stp_rom(net_scratchpad, "MP-0 HRT-ENG-LogKeyTime,0,86400");
  if (err = readsdo(0x5200,0x01)) return err;
  s = stp_i(s, ",", twizy_sdo.data);
  if (err = readsdo(0x5200,0x02)) return err;
  s = stp_i(s, ",", twizy_sdo.data);
  net_msg_encode_puts();


  /* Alerts (active faults):
      MP-0 HRT-ENG-LogAlerts
              ,<n>,86400
              ,<Code>,<Description>
   */
  if (which == 1) {
    if (readsdo(0x5300,0x01) == 0)
    cnt = twizy_sdo.data;
    for (n=start; n<cnt && n<(start+10); n++) {
      if (err = writesdo(0x5300,0x02,n)) break;
      if (err = readsdo(0x5300,0x03)) break;
      s = stp_i(net_scratchpad, "MP-0 HRT-ENG-LogAlerts,", n);
      s = stp_sevcon_fault(s, ",86400,", twizy_sdo.data);
      net_msg_encode_puts();
    }
  }


  /* Faults FIFO:
      MP-0 HRT-ENG-LogFaults
              ,<n>,86400
              ,<Code>,<Description>
              ,<TimeHour>,<TimeMinSec>
              ,<Data1>,<Data2>,<Data3>
   */
  else if (which == 2) {
    if (readsdo(0x4110,0x02) == 0)
    cnt = twizy_sdo.data;
    for (n=start; n<cnt && n<(start+10); n++) {
      if (err = writesdo(0x4111,0x00,n)) break;
      if (err = readsdo(0x4112,0x01)) break;
      s = stp_i(net_scratchpad, "MP-0 HRT-ENG-LogFaults,", n);
      s = stp_sevcon_fault(s, ",86400,", twizy_sdo.data);
      readsdo(0x4112,0x02);
      s = stp_i(s, ",", twizy_sdo.data);
      readsdo(0x4112,0x03);
      s = stp_i(s, ",", twizy_sdo.data);
      readsdo(0x4112,0x04);
      s = stp_sx(s, ",", twizy_sdo.data);
      readsdo(0x4112,0x05);
      s = stp_sx(s, ",", twizy_sdo.data);
      readsdo(0x4112,0x06);
      s = stp_sx(s, ",", twizy_sdo.data);
      net_msg_encode_puts();
    }
  }


  /* System FIFO:
      MP-0 HRT-ENG-LogSystem
              ,<n>,86400
              ,<Code>,<Description>
              ,<TimeHour>,<TimeMinSec>
              ,<Data1>,<Data2>,<Data3>
   */
  else if (which == 3) {
    if (readsdo(0x4100,0x02) == 0)
    cnt = twizy_sdo.data;
    for (n=start; n<cnt && n<(start+10); n++) {
      if (err = writesdo(0x4101,0x00,n)) break;
      if (err = readsdo(0x4102,0x01)) break;
      s = stp_i(net_scratchpad, "MP-0 HRT-ENG-LogSystem,", n);
      s = stp_sevcon_fault(s, ",86400,", twizy_sdo.data);
      readsdo(0x4102,0x02);
      s = stp_i(s, ",", twizy_sdo.data);
      readsdo(0x4102,0x03);
      s = stp_i(s, ",", twizy_sdo.data);
      readsdo(0x4102,0x04);
      s = stp_sx(s, ",", twizy_sdo.data);
      readsdo(0x4102,0x05);
      s = stp_sx(s, ",", twizy_sdo.data);
      readsdo(0x4102,0x06);
      s = stp_sx(s, ",", twizy_sdo.data);
      net_msg_encode_puts();
    }
  }


  /* Event counter:
      MP-0 HRT-ENG-LogCounts
	,<n>,86400
	,<Code>,<Description>
	,<LastTimeHour>,<LastTimeMinSec>
	,<FirstTimeHour>,<FirstTimeMinSec>
	,<Count>
   */
  else if (which == 4) {
    if (start > 10)
      start = 10;
    cnt = 10;
    for (n=start; n<10; n++) {
      if (err = readsdo(0x4201+n,0x01)) break;
      s = stp_i(net_scratchpad, "MP-0 HRT-ENG-LogCounts,", n);
      s = stp_sevcon_fault(s, ",86400,", twizy_sdo.data);
      readsdo(0x4201+n,0x04);
      s = stp_i(s, ",", twizy_sdo.data);
      readsdo(0x4201+n,0x05);
      s = stp_i(s, ",", twizy_sdo.data);
      readsdo(0x4201+n,0x02);
      s = stp_i(s, ",", twizy_sdo.data);
      readsdo(0x4201+n,0x03);
      s = stp_i(s, ",", twizy_sdo.data);
      readsdo(0x4201+n,0x06);
      s = stp_i(s, ",", twizy_sdo.data);
      net_msg_encode_puts();
    }
  }


  /* Min / max monitor:
      MP-0 HRT-ENG-LogMinMax
	,<n>,86400
	,<BatteryVoltageMin>,<BatteryVoltageMax>
	,<CapacitorVoltageMin>,<CapacitorVoltageMax>
	,<MotorCurrentMin>,<MotorCurrentMax>
	,<MotorSpeedMin>,<MotorSpeedMax>
	,<DeviceTempMin>,<DeviceTempMax>
   */
  else if (which == 5) {
    if (start > 2)
      start = 2;
    cnt = 2;
    for (n=start; n<2; n++) {
      if (err = readsdo(0x4300+n,0x02)) break;
      s = stp_i(net_scratchpad, "MP-0 HRT-ENG-LogMinMax,", n);
      s = stp_i(s, ",86400,", (int)twizy_sdo.data);
      readsdo(0x4300+n,0x03);
      s = stp_i(s, ",", (int)twizy_sdo.data);
      readsdo(0x4300+n,0x04);
      s = stp_i(s, ",", (int)twizy_sdo.data);
      readsdo(0x4300+n,0x05);
      s = stp_i(s, ",", (int)twizy_sdo.data);
      readsdo(0x4300+n,0x06);
      s = stp_i(s, ",", (int)twizy_sdo.data);
      readsdo(0x4300+n,0x07);
      s = stp_i(s, ",", (int)twizy_sdo.data);
      readsdo(0x4300+n,0x0a);
      s = stp_i(s, ",", (int)twizy_sdo.data);
      readsdo(0x4300+n,0x0b);
      s = stp_i(s, ",", (int)twizy_sdo.data);
      readsdo(0x4300+n,0x0c);
      s = stp_i(s, ",", (int)twizy_sdo.data);
      readsdo(0x4300+n,0x0d);
      s = stp_i(s, ",", (int)twizy_sdo.data);
      net_msg_encode_puts();
    }
  }

  
  if (retcnt)
    *retcnt = cnt;
  return err;
}


BOOL vehicle_twizy_logs_cmd(BOOL msgmode, int cmd, char *arguments)
{
  char *s;
  UINT err;
  UINT8 which=1, start=0, cnt;

  // process arguments:
  if (arguments && *arguments) {
    if (arguments = strtokpgmram(arguments, ","))
      which = atoi(arguments);
    if (arguments = strtokpgmram(NULL, ","))
      start = atoi(arguments);
  }

  if (!msgmode)
    net_msg_start();

  // execute:
  if (cmd == CMD_ResetLogs)
    err = vehicle_twizy_resetlogs_msgp(which, &cnt);
  else if (cmd == CMD_QueryLogs)
    err = vehicle_twizy_querylogs_msgp(which, start, &cnt);
  else
    err = ERR_Range;

  if (msgmode) {
    // msg command response:
    s = stp_i(net_scratchpad, "MP-0 c", cmd);
    if (err) {
      s = stp_rom(s, ",1,");
      s = stp_x(s, "ERROR ", err);
      if (ERROR_IN_SDO(err))
        s = vehicle_twizy_fmt_sdo(s);
    }
    else {
      // success: return which code + entry count
      s = stp_i(s, ",0,", which);
      s = stp_i(s, ",", cnt);
    }
    net_msg_encode_puts();
  }

  if (!msgmode)
    net_msg_send();

  return (err == 0);
}


#endif // OVMS_TWIZY_CFG



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


/**
 * vehicle_twizy_pwrlog_msgp()
 *  send RT-PWR-Log history entry
 */
char vehicle_twizy_pwrlog_msgp(char stat)
{
  //static WORD crc;
  char *s;
  unsigned long pwr_dist, pwr_use, pwr_rec;
  UINT8 c;
  UINT8 tmin, tmax;
  UINT tact;

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

  
#ifdef OVMS_TWIZY_BATTMON
  // Collect cell temperatures:

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

  tact = (tact + BATT_CMODS/2) / BATT_CMODS;
#endif // OVMS_TWIZY_BATTMON


  // Format history msg:

  s = stp_rom(net_scratchpad, "MP-0 HRT-PWR-Log,0,31536000");

  s = stp_l2f(s, ",", twizy_odometer, 2);
  s = stp_latlon(s, ",", car_latitude);
  s = stp_latlon(s, ",", car_longitude);
  s = stp_i(s, ",", car_altitude);

  s = stp_i(s, ",", car_doors1bits.ChargePort ? car_chargestate : 0);
  s = stp_i(s, ",", car_parktime ? (car_time-car_parktime+30)/60 : 0);

  s = stp_l2f(s, ",", twizy_soc, 2);
  s = stp_l2f(s, ",", twizy_soc_min, 2);
  s = stp_l2f(s, ",", twizy_soc_max, 2);

  s = stp_ul(s, ",", (pwr_use + 11250) / 22500);
  s = stp_ul(s, ",", (pwr_rec + 11250) / 22500);
  s = stp_ul(s, ",", (pwr_dist + 5) / 10);

  s = stp_i(s, ",", KmFromMi(car_estrange));
  s = stp_i(s, ",", KmFromMi(car_idealrange));

#ifdef OVMS_TWIZY_BATTMON
  s = stp_l2f(s, ",", twizy_batt[0].volt_act, 1);
  s = stp_l2f(s, ",", twizy_batt[0].volt_min, 1);
  s = stp_l2f(s, ",", twizy_batt[0].volt_max, 1);

  s = stp_i(s, ",", CONV_Temp(tact));
  s = stp_i(s, ",", CONV_Temp(tmin));
  s = stp_i(s, ",", CONV_Temp(tmax));
#else // OVMS_TWIZY_BATTMON
  s = stp_rom(s, ",,,,,,");
#endif // OVMS_TWIZY_BATTMON

  s = stp_i(s, ",", car_tmotor);
  s = stp_i(s, ",", car_tpem);

  // trip distance driven in km based on odometer (more real than pwr_dist):
  s = stp_l2f(s, ",", (twizy_odometer - twizy_odometer_tripstart), 2);
  // trip SOC diff:
  s = stp_l2f(s, ",", ABS((long)twizy_soc_tripstart - (long)twizy_soc), 2);

  // trip speed/delta statistics:
  s = stp_l2f(s, ",", twizy_speedpwr[CAN_SPEED_CONST].spdsum
          / twizy_speedpwr[CAN_SPEED_CONST].spdcnt, 2); // avg speed kph
  s = stp_l2f(s, ",", (twizy_speedpwr[CAN_SPEED_ACCEL].spdsum * 10)
          / twizy_speedpwr[CAN_SPEED_ACCEL].spdcnt, 2); // avg accel kph/s
  s = stp_l2f(s, ",", (twizy_speedpwr[CAN_SPEED_DECEL].spdsum * 10)
          / twizy_speedpwr[CAN_SPEED_DECEL].spdcnt, 2); // avg decel kph/s


  // send:

  if (stat == 2)
    net_msg_start();
  net_msg_encode_puts();

  return (stat == 2) ? 1 : stat;
}


#ifdef OVMS_CANSIM
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
#endif // OVMS_CANSIM


////////////////////////////////////////////////////////////////////////
// twizy_poll()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to poll for data.
//
// See vehicle initialise() for buffer 0/1 filter setup.
//

// Poll buffer 0:

// ISR optimization, see http://www.xargs.com/pic/c18-isr-optim.pdf
#pragma tmpdata high_isr_tmpdata

BOOL vehicle_twizy_poll0(void)
{
  unsigned int t;
  unsigned char u;


#ifdef OVMS_CANSIM
  if (twizy_sim >= 0)
  {
    // READ SIMULATION DATA:
    can_id = (((UINT) twizy_sim_data[twizy_sim][0]) << 8)
            + twizy_sim_data[twizy_sim][1];

    if (can_id == 0x155)
      can_filter = 0;
    else
      can_filter = 1;

    can_datalength = twizy_sim_data[twizy_sim][2];
    for (u = 0; u < 8; u++)
      can_databuffer[u] = twizy_sim_data[twizy_sim][3 + u];
  }
#endif // OVMS_CANSIM


  // HANDLE CAN MESSAGE:

  if (can_filter == 0)
  {
    if (can_id == 0x155)
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


#ifdef OVMS_TWIZY_CFG

  else
  {
    /*****************************************************
     * FILTER 1:
     * CAN ID 0x_81: CANopen message from SEVCON (Node #1)
     */

    if (can_id == 0x081)
    {
      /*****************************************************
       * FILTER 1:
       * CAN ID 0x081: CANopen error message from SEVCON (Node #1)
       */

      // count errors to detect manual CFG RESET request:
      if ((CAN_BYTE(1)==0x10) && (CAN_BYTE(2)==0x01))
        twizy_button_cnt++;

    }

    else if (can_id == 0x581)
    {
      /*****************************************************
       * FILTER 1:
       * CAN ID 0x581: CANopen SDO reply from SEVCON (Node #1)
       */

      // copy message into twizy_sdo object:
      for (u = 0; u < can_datalength; u++)
        twizy_sdo.byte[u] = CAN_BYTE(u);
      for (; u < 8; u++)
        twizy_sdo.byte[u] = 0;
    }

  }

#endif // OVMS_TWIZY_CFG

  return TRUE;
}



// Poll buffer 1:

BOOL vehicle_twizy_poll1(void)
{
  unsigned int new_speed;

#ifdef OVMS_CANSIM
  unsigned char i;

  if (twizy_sim >= 0)
  {
    // READ SIMULATION DATA:
    can_id = (((UINT) twizy_sim_data[twizy_sim][0]) << 8)
            + twizy_sim_data[twizy_sim][1];

    if ((can_id & 0x590) == 0x590)
      can_filter = 2;
    else if ((can_id & 0x690) == 0x690)
      can_filter = 3;
    else if ((can_id & 0x550) == 0x550)
      can_filter = 4;
    else
      return FALSE;

    can_datalength = twizy_sim_data[twizy_sim][2];
    for (i = 0; i < 8; i++)
      can_databuffer[i] = twizy_sim_data[twizy_sim][3 + i];
  }
#endif // OVMS_CANSIM


  // HANDLE CAN MESSAGE:

  if (can_filter == 2)
  {
    // Filter 2 = CAN ID GROUP 0x_9_:

    switch (can_id)
    {
      /*****************************************************
       * FILTER 2:
       * CAN ID 0x597: sent every 100 ms (10 per second)
       */
    case 0x597:

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

      // twizy_status high nibble:
      //  [1] bit 4 = 0x10 CAN_STATUS_KEYON: 1 = Car ON (key switch)
      //  [1] bit 5 = 0x20 CAN_STATUS_CHARGING: 1 = Charging
      //  [1] bit 6 = 0x40 CAN_STATUS_OFFLINE: 1 = Switch-ON/-OFF phase

      twizy_status = (twizy_status & 0x0F) | (CAN_BYTE(1) & 0xF0);
      // Translation to car_doors1 done in ticker1()

      // init cyclic distance counter on switch-on:
      if ((twizy_status & CAN_STATUS_KEYON) && (!car_doors1bits.CarON))
        twizy_dist = twizy_speed_distref = 0;

      // PEM temperature:
      if (CAN_BYTE(7) > 0 && CAN_BYTE(7) < 0xf0)
        car_tpem = (signed int) CAN_BYTE(7) - 40;
      else
        car_tpem = 0;

      break;


      /*****************************************************
       * FILTER 2:
       * CAN ID 0x599: sent every 100 ms (10 per second)
       */
    case 0x599:

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

        // switch speed state:
        if (delta >= CAN_SPEED_THRESHOLD)
          twizy_speed_state = CAN_SPEED_ACCEL;
        else if (delta <= -CAN_SPEED_THRESHOLD)
          twizy_speed_state = CAN_SPEED_DECEL;
        else
          twizy_speed_state = CAN_SPEED_CONST;

        // speed/delta sum statistics while driving:
        if (new_speed >= CAN_SPEED_THRESHOLD)
        {
          // overall speed avg:
          twizy_speedpwr[0].spdcnt++;
          twizy_speedpwr[0].spdsum += new_speed;
          
          // accel/decel speed avg:
          if (twizy_speed_state != 0)
          {
            twizy_speedpwr[twizy_speed_state].spdcnt++;
            twizy_speedpwr[twizy_speed_state].spdsum += ABS(delta);
          }
        }

        twizy_speed = new_speed;
        // car value derived in ticker1()
      }

      break;


      /*****************************************************
       * FILTER 2:
       * CAN ID 0x59B: sent every 100 ms (10 per second)
       */
    case 0x59B:

      // twizy_status low nibble:
      twizy_status = (twizy_status & 0xF0) | (CAN_BYTE(1) & 0x09);
      if (CAN_BYTE(0) == 0x80)
        twizy_status |= CAN_STATUS_MODE_D;
      else if (CAN_BYTE(0) == 0x08)
        twizy_status |= CAN_STATUS_MODE_R;

      break;


      /*****************************************************
       * FILTER 2:
       * CAN ID 0x59E: sent every 100 ms (10 per second)
       */
    case 0x59E:

      // CYCLIC DISTANCE COUNTER:
      twizy_dist = ((UINT) CAN_BYTE(0) << 8) + CAN_BYTE(1);

      // MOTOR TEMPERATURE:
      if (CAN_BYTE(5) > 0 && CAN_BYTE(5) < 0xf0)
        car_tmotor = (signed int) CAN_BYTE(5) - 40;
      else
        car_tmotor = 0;


      break;


      /*****************************************************
       * FILTER 2:
       * CAN ID 0x69F: sent every 1000 ms (1 per second)
       */
    case 0x69f:
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
  else if (can_filter == 3)
  {
    // Filter 3 = CAN ID GROUP 0x_2_:

    switch (can_id)
    {
      /*****************************************************
       * FILTER 3:
       * CAN ID 0x424: sent every 100 ms (10 per second)
       */
    case 0x424:
      // max drive (discharge) + recup (charge) power:
      if (CAN_BYTE(2) != 0xff)
        twizy_batt[0].max_recup_pwr = CAN_BYTE(2);
      if (CAN_BYTE(3) != 0xff)
        twizy_batt[0].max_drive_pwr = CAN_BYTE(3);

      break;
    }
  }

  else if (can_filter == 4)
  {
    UINT8 i, state;

    // Filter 4 = CAN ID GROUP 0x_5_: battery sensors.
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

    switch (can_id)
    {
    case 0x554:
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

    case 0x556:
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

    case 0x557:
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

    case 0x55E:
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

    case 0x55F:
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

  else if (can_filter == 5)
  {
    // Filter 5 = CAN ID GROUP 0x_D_: exact speed & odometer

    switch (can_id)
    {
      /*****************************************************
       * FILTER 5:
       * CAN ID 0x5D7: sent every 100 ms (10 per second)
       */
    case 0x5d7:

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

#pragma tmpdata


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


#ifdef OVMS_TWIZY_CFG
  // Send SEVCON faults alert:
  if ((twizy_notify & SEND_FaultsAlert) && (twizy_sevcon_fault != 0))
  {
    if ((notify_msg) && (net_msg_serverok) && (net_msg_sendpending == 0))
    {
      net_msg_start();
      // push alert:
      strcpypgm2ram(net_scratchpad, "MP-0 PA");
      vehicle_twizy_faults_prepmsg();
      net_msg_encode_puts();
      // add active faults history data:
      vehicle_twizy_querylogs_msgp(1, 0, NULL);
      net_msg_send();
      twizy_notify &= ~SEND_FaultsAlert;
    }

    if (notify_sms)
    {
      net_scratchpad[0] = 0;
      vehicle_twizy_faults_prepmsg();
      net_send_sms_start(par_get(PARAM_REGPHONE));
      net_puts_ram(net_scratchpad);
      net_send_sms_finish();
      twizy_notify &= ~SEND_FaultsAlert;
    }
  }
#endif // OVMS_TWIZY_CFG


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


  // Send power usage log?
  if ((twizy_notify & SEND_PowerLog))
  {
    if ((net_msg_serverok) && (net_msg_sendpending == 0))
    {
      stat = vehicle_twizy_pwrlog_msgp(2);
      if (stat != 2)
        net_msg_send();
      
      twizy_notify &= ~SEND_PowerLog;
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
  car_trip = MiFromKm((twizy_odometer - twizy_odometer_tripstart) / 10);

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
      
      // set trip references:
      twizy_odometer_tripstart = twizy_odometer;
      twizy_soc_tripstart = twizy_soc;

      net_req_notification(NET_NOTIFY_ENV);

#ifdef OVMS_TWIZY_BATTMON
      // reset battery monitor:
      vehicle_twizy_battstatus_reset();
#endif // OVMS_TWIZY_BATTMON

      // reset power statistics:
      vehicle_twizy_power_reset();

#ifdef OVMS_TWIZY_CFG
      // reset button cnt:
      twizy_button_cnt = 0;
      // login to SEVCON:
      login(1);
#endif // OVMS_TWIZY_CFG

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
        twizy_notify |= (SEND_PowerNotify | SEND_PowerLog);

#ifdef OVMS_TWIZY_CFG
      // reset button cnt:
      twizy_button_cnt = 0;
#endif // OVMS_TWIZY_CFG

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


#ifdef OVMS_TWIZY_CFG

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
    if ((readsdo(0x5110,0x00) == 0) && (twizy_sdo.data == 0x7f)
      && (readsdo(0x5000,0x01) == 0) && (twizy_sdo.data != 4)) {
      // we're in pre-op but not logged in, try to solve:
      if ((login(1) == 0) && (configmode(0) == 0))
        twizy_button_cnt = 0; // solved
    }
  }


#endif // OVMS_TWIZY_CFG


  return FALSE;
}


////////////////////////////////////////////////////////////////////////
// twizy_state_ticker10th()
// State Model: 1/10 second ticker
// This function is called approximately 10 times per second
//

BOOL vehicle_twizy_state_ticker10th(void)
{

#ifdef OVMS_TWIZY_CFG

  BYTE bits, key;
  char buf[2];

  if (!car_doors1bits.CarON || car_doors1bits.Charging) {
    // Twizy off or charging: switch off LEDs
    PORTC = (PORTC & 0b11110000);
  }
  else {
    // LED control:
    // TMR0H 0..0x4c = ~1 sec, so 0x13 = ~ 1/4 sec
    if (TMR0H < 0x13) {
      // flash cfgmode profile LED if it differs:
      if (twizy_cfg.profile_cfgmode != twizy_cfg.profile_user)
        PORTC |= (1 << twizy_cfg.profile_cfgmode);
      // clear user profile LED if unsaved:
      if (twizy_cfg.unsaved)
        PORTC &= ~(1 << twizy_cfg.profile_user);
    }
    else {
      // show user profile status:
      PORTC = (PORTC & 0b11110000) | (1 << twizy_cfg.profile_user);
    }

    // Read input port:
    bits = (PORTA & 0b00111100) >> 2;

    if (bits == 0) {
      twizy_cfg.keystate = 0;
    }
    else if (twizy_cfg.keystate == 0) {
      twizy_cfg.keystate = 1;

      // determine new key press:
      for (key=0; (bits&1)==0; key++)
        bits >>= 1;

      // User feedback for key press:
      PORTC |= (1 << key);

      // Switch profile?
      if ((twizy_cfg.profile_user != key) || (twizy_cfg.profile_cfgmode != key) || (twizy_cfg.unsaved)) {
        if (vehicle_twizy_cfg_switchprofile(key) != 0)
          // Error: flash all LEDs for 1/10 sec
          PORTC |= 0b00001111;

        // store selection:
        buf[0] = '0' + twizy_cfg.profile_user;
        buf[1] = 0;
        par_set(PARAM_PROFILE, buf);
      }
    }

  }

#endif // OVMS_TWIZY_CFG

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

    // Check for new SEVCON fault:
#ifdef OVMS_TWIZY_CFG
    if (readsdo(0x5320,0x00) == 0) {
      if (twizy_sdo.data == 0) {
        // reset fault status:
        twizy_sevcon_fault = 0;
      }
      else if (twizy_sdo.data != twizy_sevcon_fault) {
        // new fault:
        twizy_sevcon_fault = twizy_sdo.data;
        twizy_notify |= SEND_FaultsAlert;
      }
    }
#endif // #ifdef #ifdef OVMS_TWIZY_CFG
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
    // power efficiency trip report (default):
    long pwr;
    long dist;

    // speed distances are in ~ 1/10 m

    // Template:
    //   Trip 12.3km 12.3kph 123Wpk/12% SOC-12.3%=12.3%
    //   === 12% 123Wpk/12% +++ 12% 12.3kps 123Wpk/12%
    //   --- 12% 12.3kps 123Wpk/12% ^^^ 123m 123Wpk/12%
    //   vvv 123m 123Wpk/12%

    s = stp_l2f(s, "Trip ", (twizy_odometer - twizy_odometer_tripstart + 5) / 10, 1);
    s = stp_l2f(s, "km ", (twizy_speedpwr[CAN_SPEED_CONST].spdsum
            / twizy_speedpwr[CAN_SPEED_CONST].spdcnt + 5) / 10, 1); // avg speed kph
    s = stp_rom(s, "kph");

    dist = pwr_dist;
    pwr = pwr_use - pwr_rec;
    if ((pwr_use > 0) && (dist > 0))
    {
      s = stp_l(s, " ", (pwr / dist * 10000 + ((pwr>=0)?11250:-11250)) / 22500);
      s = stp_i(s, "Wpk/", (pwr_rec / (pwr_use/1000) + 5) / 10);
      s = stp_rom(s, "%");
    }

    if (twizy_soc <= twizy_soc_tripstart)
    {
      s = stp_l2f(s, " SOC-", (twizy_soc_tripstart - twizy_soc + 5) / 10, 1);
      s = stp_l2f(s, "%=", (twizy_soc + 5) / 10, 1);
      s = stp_rom(s, "%");
    }

    pwr_use = twizy_speedpwr[CAN_SPEED_CONST].use;
    pwr_rec = twizy_speedpwr[CAN_SPEED_CONST].rec;
    dist = twizy_speedpwr[CAN_SPEED_CONST].dist;
    pwr = pwr_use - pwr_rec;
    if ((pwr_use > 0) && (dist > 0))
    {
      s = stp_i(s, "\r=== ", prc_const);
      s = stp_l(s, "% ", (pwr / dist * 10000 + ((pwr>=0)?11250:-11250)) / 22500);
      s = stp_i(s, "Wpk/", (pwr_rec / (pwr_use/1000) + 5) / 10);
      s = stp_rom(s, "%");
    }

    pwr_use = twizy_speedpwr[CAN_SPEED_ACCEL].use;
    pwr_rec = twizy_speedpwr[CAN_SPEED_ACCEL].rec;
    dist = twizy_speedpwr[CAN_SPEED_ACCEL].dist;
    pwr = pwr_use - pwr_rec;
    if ((pwr_use > 0) && (dist > 0))
    {
      s = stp_i(s, "\r+++ ", prc_accel);
      s = stp_l2f(s, "% ", ((twizy_speedpwr[CAN_SPEED_ACCEL].spdsum * 10)
              / twizy_speedpwr[CAN_SPEED_ACCEL].spdcnt + 5) / 10, 1); // avg accel kph/s
      s = stp_l(s, "kps ", (pwr / dist * 10000 + ((pwr>=0)?11250:-11250)) / 22500);
      s = stp_i(s, "Wpk/", (pwr_rec / (pwr_use/1000) + 5) / 10);
      s = stp_rom(s, "%");
    }

    pwr_use = twizy_speedpwr[CAN_SPEED_DECEL].use;
    pwr_rec = twizy_speedpwr[CAN_SPEED_DECEL].rec;
    dist = twizy_speedpwr[CAN_SPEED_DECEL].dist;
    pwr = pwr_use - pwr_rec;
    if ((pwr_use > 0) && (dist > 0))
    {
      s = stp_i(s, "\r--- ", prc_decel);
      s = stp_l2f(s, "% ", ((twizy_speedpwr[CAN_SPEED_DECEL].spdsum * 10)
              / twizy_speedpwr[CAN_SPEED_DECEL].spdcnt + 5) / 10, 1); // avg decel kph/s
      s = stp_l(s, "kps ", (pwr / dist * 10000 + ((pwr>=0)?11250:-11250)) / 22500);
      s = stp_i(s, "Wpk/", (pwr_rec / (pwr_use/1000) + 5) / 10);
      s = stp_rom(s, "%");
    }

    // level distances are in 1 m

    pwr_use = twizy_levelpwr[CAN_LEVEL_UP].use;
    pwr_rec = twizy_levelpwr[CAN_LEVEL_UP].rec;
    dist = twizy_levelpwr[CAN_LEVEL_UP].dist;
    pwr = pwr_use - pwr_rec;
    if ((pwr_use > 0) && (dist > 0))
    {
      s = stp_i(s, "\r^^^ ", twizy_levelpwr[CAN_LEVEL_UP].hsum);
      s = stp_l(s, "m ", (pwr / dist * 1000 + ((pwr>=0)?11250:-11250)) / 22500);
      s = stp_i(s, "Wpk/", (pwr_rec / (pwr_use/1000) + 5) / 10);
      s = stp_rom(s, "%");
    }

    pwr_use = twizy_levelpwr[CAN_LEVEL_DOWN].use;
    pwr_rec = twizy_levelpwr[CAN_LEVEL_DOWN].rec;
    dist = twizy_levelpwr[CAN_LEVEL_DOWN].dist;
    pwr = pwr_use - pwr_rec;
    if ((pwr_use > 0) && (dist > 0))
    {
      s = stp_i(s, "\rvvv ", twizy_levelpwr[CAN_LEVEL_DOWN].hsum);
      s = stp_l(s, "m ", (pwr / dist * 1000 + ((pwr>=0)?11250:-11250)) / 22500);
      s = stp_i(s, "Wpk/", (pwr_rec / (pwr_use/1000) + 5) / 10);
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
     * Extension V3.2.1:
     *  ,<speed_CONST_cnt>,<speed_CONST_sum>
     *  ,<speed_ACCEL_cnt>,<speed_ACCEL_sum>
     *  ,<speed_DECEL_cnt>,<speed_DECEL_sum>
     * (cnt = 1/10 seconds, CONST_sum = speed, other sum = delta)
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

    s = stp_ul(s, ",", twizy_speedpwr[CAN_SPEED_CONST].spdcnt);
    s = stp_ul(s, ",", twizy_speedpwr[CAN_SPEED_CONST].spdsum);

    s = stp_ul(s, ",", twizy_speedpwr[CAN_SPEED_ACCEL].spdcnt);
    s = stp_ul(s, ",", twizy_speedpwr[CAN_SPEED_ACCEL].spdsum);

    s = stp_ul(s, ",", twizy_speedpwr[CAN_SPEED_DECEL].spdcnt);
    s = stp_ul(s, ",", twizy_speedpwr[CAN_SPEED_DECEL].spdsum);
    
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
#ifdef OVMS_CANSIM
  if (arguments && *arguments)
  {
    // Run simulator:
    vehicle_twizy_simulator_run(atoi(arguments));
  }
#endif // OVMS_CANSIM

  if (msgmode)
    vehicle_twizy_debug_msgp(0, cmd);

  return TRUE;
}

BOOL vehicle_twizy_debug_sms(BOOL premsg, char *caller, char *command, char *arguments)
{
  char *s;

#ifdef OVMS_CANSIM
  if (arguments && *arguments)
  {
    // Run simulator:
    vehicle_twizy_simulator_run(atoi(arguments));
  }
#endif // OVMS_CANSIM

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

#ifdef OVMS_TWIZY_CFG
  s = stp_i(s, " BC=", twizy_button_cnt);
#endif // OVMS_TWIZY_CFG

  //s = stp_x(s, " STS=", twizy_status);
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
  //s = stp_i(s, " SQ=", net_sq);
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

  }
#endif // OVMS_DIAGMODULE


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

BOOL vehicle_twizy_statalert_cmd(BOOL msgmode, int cmd, char *arguments)
{
  // send text alert:
  strcpypgm2ram(net_scratchpad, (char const rom far*)"MP-0 PA");
  vehicle_twizy_stat_prepmsg();
  net_msg_encode_puts();

  // send RT-PWR-Log entry:
  vehicle_twizy_pwrlog_msgp(0);

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

    car_tbattery = (signed int) m + 0.5 - 40;
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
      //  ,<max_drive_pwr>,<max_recup_pwr>

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
      s = stp_i(s, ",", (int) twizy_batt[0].max_drive_pwr * 500);
      s = stp_i(s, ",", (int) twizy_batt[0].max_recup_pwr * 500);

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
    return vehicle_twizy_statalert_cmd(msgmode, cmd, msg);


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

#ifdef OVMS_TWIZY_CFG
  case CMD_ResetLogs:
  case CMD_QueryLogs:
    return vehicle_twizy_logs_cmd(msgmode, cmd, msg);
#endif // OVMS_TWIZY_CFG

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
  "3DEBUG",       // Twizy: output internal state dump for debug
  "3STAT",        // override standard STAT
  "3RANGE",       // Twizy: set/query max ideal range
  "3CA",          // Twizy: set/query charge alerts
  "3POWER",       // Twizy: power usage statistics

#ifdef OVMS_TWIZY_BATTMON
  "3BATT",        // Twizy: battery status
#endif // OVMS_TWIZY_BATTMON

#ifdef OVMS_TWIZY_CFG
  "3CFG",         // Twizy: SEVCON configuration tweaking
#endif // OVMS_TWIZY_CFG

  "3HELP", // extend HELP output
  ""
};

rom far BOOL(*vehicle_twizy_sms_hfntable[])(BOOL premsg, char *caller, char *command, char *arguments) = {
  &vehicle_twizy_debug_sms,
  &vehicle_twizy_stat_sms,
  &vehicle_twizy_range_sms,
  &vehicle_twizy_ca_sms,
  &vehicle_twizy_power_sms,

#ifdef OVMS_TWIZY_BATTMON
  &vehicle_twizy_battstatus_sms,
#endif // OVMS_TWIZY_BATTMON

#ifdef OVMS_TWIZY_CFG
  &vehicle_twizy_cfg_sms,
#endif // OVMS_TWIZY_CFG

  &vehicle_twizy_help_sms
};


// SMS COMMAND DISPATCHERS:
// premsg: TRUE=may replace, FALSE=may extend standard handler
// returns TRUE if handled

BOOL vehicle_twizy_fn_smshandler(BOOL premsg, char *caller, char *command, char *arguments)
{
  // called to extend/replace standard command: framework did auth check for us

  UINT8 k;

  // Command parsing...
  for (k = 0; vehicle_twizy_sms_cmdtable[k][0] != 0; k++)
  {
    if (memcmppgm2ram(command,
                      (char const rom far*)vehicle_twizy_sms_cmdtable[k] + 1,
                      strlenpgm((char const rom far*)vehicle_twizy_sms_cmdtable[k]) - 1) == 0)
    {
      // Call sms handler:
      k = (*vehicle_twizy_sms_hfntable[k])(premsg, caller, command, arguments);

      if ((premsg) && (k))
      {
        // we're in charge + handled it; finish SMS:
        net_send_sms_finish();
      }

      return k;
    }
  }

  return FALSE; // no vehicle command
}

BOOL vehicle_twizy_fn_smsextensions(char *caller, char *command, char *arguments)
{
  // called for specific command: we need to do the auth check

  UINT8 k;

  // Command parsing...
  for (k = 0; vehicle_twizy_sms_cmdtable[k][0] != 0; k++)
  {
    if (memcmppgm2ram(command,
                      (char const rom far*)vehicle_twizy_sms_cmdtable[k] + 1,
                      strlenpgm((char const rom far*)vehicle_twizy_sms_cmdtable[k]) - 1) == 0)
    {
      // we need to check the caller authorization:
      arguments = net_sms_initargs(arguments);
      if (!net_sms_checkauth(vehicle_twizy_sms_cmdtable[k][0], caller, &arguments))
        return FALSE; // failed

      // Call sms handler:
      k = (*vehicle_twizy_sms_hfntable[k])(TRUE, caller, command, arguments);

      if (k)
      {
        // we're in charge + handled it; finish SMS:
        net_send_sms_finish();
      }

      return k;
    }
  }

  return FALSE; // no vehicle command
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

  net_puts_rom(" \r\nTWIZY:");

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

#ifdef OVMS_TWIZY_CFG

    twizy_button_cnt = 0;

    twizy_max_rpm = 0;
    twizy_max_trq = 0;
    twizy_max_pwr_lo = 0;
    twizy_max_pwr_hi = 0;

    twizy_sevcon_fault = 0;

    twizy_cfg.type = 0;
    
    // reload last selected user profile on init:
    twizy_cfg.profile_user = atoi(par_get(PARAM_PROFILE));
    twizy_cfg.profile_cfgmode = twizy_cfg.profile_user;
    vehicle_twizy_cfg_readprofile(twizy_cfg.profile_user);
    twizy_cfg.unsaved = 0;
    twizy_cfg.keystate = 0;

#endif // OVMS_TWIZY_CFG

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

  /*
  Mask0		010 1111 1111
  Filter0	x0x 0101 0101 		_55		155
  Filter1	x0x 1000 0001 		_81		081 581
  */

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

  /*
  Mask1         100 1111 0000 
  Filter2	1xx 1001 xxxx		_9_		597 599 59B 59E 69F
  Filter3	1xx 0010 xxxx		_2_		424 (423) (425) (426) (627) (628) (629)
  Filter4	1xx 0101 xxxx		_5_		554 556 557 55E 55F (659)
  Filter5	1xx 1101 xxxx		_D_		5D7
  */

  // Mask1 = 
  RXM1SIDH = 0b10011110;
  RXM1SIDL = 0b00000000;

  // Filter2 = GROUP 0x_9_:
  RXF2SIDH = 0b10010010;
  RXF2SIDL = 0b00000000;

  // Filter3 = GROUP 0x_2_:
  RXF3SIDH = 0b10000100;
  RXF3SIDL = 0b00000000;

  // Filter4 = GROUP 0x_5_:
  RXF4SIDH = 0b10001010;
  RXF4SIDL = 0b00000000;

  // Filter5 = GROUP 0x_D_:
  RXF5SIDH = 0b10011010;
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
  vehicle_fn_ticker10th = &vehicle_twizy_state_ticker10th;
  vehicle_fn_smshandler = &vehicle_twizy_fn_smshandler;
  vehicle_fn_smsextensions = &vehicle_twizy_fn_smsextensions;
  vehicle_fn_commandhandler = &vehicle_twizy_fn_commandhandler;

  net_fnbits |= NET_FN_INTERNALGPS; // Require internal GPS
  net_fnbits |= NET_FN_12VMONITOR;    // Require 12v monitor
  net_fnbits |= NET_FN_SOCMONITOR;    // Require SOC monitor

  return TRUE;
}
