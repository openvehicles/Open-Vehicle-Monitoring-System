////////////////////////////////////////////////////////////////////////////////
// Project:       Open Vehicle Monitor System
// Vehicle:       Kia Soul EV
// 
// CAN documentation:
//  CAN reverse engineering done by Tyrel Haveman and AlKl NoFx
//  Google Docs spreadsheet:
//  https://docs.google.com/spreadsheets/d/1YYlZ-IcTQlz-LzaYkHO-7a4SFM8QYs2BGNXiSU5_EwI/
// 
// History:
// 
// 0.1  7 Oct 2015  (Michael Balzer)
//  - Initial release, untested
//  - VIN, car status, doors & charging status, SOC, estimated range, speed
// 
// 0.2  16 Oct 2015  (Michael Balzer)
//  - OBD diag pages 01 & 05
//  - SMS command "DEBUG"
// 
// 0.3  8 Nov 2015  (Geir Øyvind Vælidalo)
//  - Proper SOC, Odometer, Chargepower
//  - Ideal range support (Add your maksimum range in Feature #12)
//  - Charge alert support (todo)
//  - BATT SMS. Returns battery info.
//  - BTMP SMS. Returns battery module temperatures.
//  - QRY SMS. Send QRY [can id in hex] to read can-message.
//  - BCV SMS. Battery cell voltages.
// 
// 0.31  10 Nov 2015  (Geir Øyvind Vælidalo)
//  - Fixed issues with BCV
//  - Fixed speed. 
//  - Added trip odo. (From park to park)
//
// 0.32  12 Nov 2015  (Geir Øyvind Vælidalo)
//  - Motor and MCU temperatures
//  - Better charge handling (Sense proper car on, car off, port open etc.)
//  - Added time to full charge
//
// 0.33  18 Nov 2015  (Geir Øyvind Vælidalo)
//  - TMPS-data in app
//  - TMPS-SMS
//  - Improved charge handling. Fetching proper voltages and amperes?
//  - Support for ChaDeMo-charging (untested)
//
// 0.4  20 Nov 2015  (Geir Øyvind Vælidalo)
//  - Fixed byte alignment errors and added correct value adjustments for TMPS-
//    data in both app and sms
//  - Fixed issue with detecting ChaDeMo-charging? (Still untested)
//  - Adjusted some of the values in BATT, as they was just 1/10th of actual
//    values.
//  - Added AVG-sms which returns average use per km/mi in total.
//
// 0.41  21 Nov 2015  (Geir Øyvind Vælidalo)
//  - Fixed some issues with charge detection
//  - Added three poll states: 0=parking, 1=driving, 2=charging
//  - Calculates charge limit for TYPE1/2 charging.
//  - Added car_chargekWh. 
//  - Minor cleanups 
//  - Fixed AVG-sms. Value was only 1/10 of actual value. 
//
// 0.42  25 Nov 2015  (Geir Øyvind Vælidalo)
//  - Fixed Trip + added trip to AVG sms (untested)
//  - chargefull_minsremaining calculation should calc to range or soc
//    specified in CA if present. On ChaDeMo 83% is always max (BMS SOC 80%)
//  - SMS-notification when starting charging is now back. Removed by mistake.
//  - "Changed" the poll states to 0=off, 1=on, 2=charging 
//  - ChaDeMo charging is properly detected
//
// 0.43  29 Nov 2015  (Geir Øyvind Vælidalo)
//  - Fixed trip in AVG sms + added discharge and recuperation for last trip.
//  - Changed AVG-sms to TRIP-sms
//  - Added ambient temperature 
//  - MaxRange is adjusted based on ambient temp and assumes half range at -20C  
//
// 0.44  06 Dec 2015  (Geir Øyvind Vælidalo)
//  - Removed unused bytes to make room for ACC
//  - Fixed a bug with speed 
//  - Fixed issue with CarOn and CarOff not being detected properly, causing
//    problems for trip.
//  - Fixed ODOmeter. Could sometimes be reset.
//  - Built with ACC, so more data is available for APP (E.g. Mins remaining 
//    until car will be full, Range Limits etc.) See protocol doc, and Tesla
//    Roadster OVMS user guide.
//  - Added more data to TRIP-sms
//
// 0.45  06 Jan 2016  (Geir Øyvind Vælidalo)
//  - Added charge type (J1772 and ChaDeMo)
//  - Minimized the BCV-sms to save 63 bytes of ram.
//    - BCV SMS-response is now very minimalized to make room for 32 values.  
//  - car_chargekWh was 10 times to high
//  - QRY is rewritten in order to save a few bytes of ram.
//
// 0.46  06 Mar 2016  (Geir Øyvind Vælidalo)
//  - Added synchronous can messages sender, that waits for response
//  - Added commandhandler
//  - Added Lock and unclock door
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include <stdlib.h>
#include <delays.h>
#include <string.h>
#include "ovms.h"
#include "params.h"
#include "net_msg.h"
#include "net_sms.h"

#define VEHICLE_POLL_TYPE_OBDII_IOCTRL_BY_ID 0x2F // InputOutputControlByIdentifier
#define CAN_ADJ -1  // To adjust for the 1 byte difference between the 
// can_databuffer and the google spreadsheet
#define H2D(c) (c >= 'A') ? (c - 'A' + 10) : (c - '0') //Simple hex to decimal


// Module version:
rom char vehicle_kiasoul_version[] = "0.45";

rom char vehicle_kiasoul_capabilities[] = "C6,C20-24";

////////////////////////////////////////////////////////////////////////////////
// Kia Soul state variables
// 

#pragma udata overlay vehicle_overlay_data

UINT8 ks_busactive; // indication that bus is active

car_doors1bits_t ks_doors1; // Main status flags
car_doors2bits_t ks_doors2;

UINT8 ks_last_soc;
UINT8 ks_bms_soc; // BMS SOC [1/10 %]
UINT8 ks_estrange; // Estimated range remaining [km]
UINT8 ks_last_ideal_range;
UINT ks_chargepower; // in [1/256 kW]
UINT8 ks_auxVoltage; // Voltage 12V aux battery [1/10 V]

UINT8 ks_odometer[3];
UINT8 ks_trip_start_odo[3]; // used to calculate trip
#define KS_TRIP_START_ODOMETER (((UINT32)ks_trip_start_odo[0]<<16) | ((UINT32)ks_trip_start_odo[1]<<8) | ((UINT32)ks_trip_start_odo[2]))
#define KS_ODOMETER            (((UINT32)ks_odometer[0]<<16)       | ((UINT32)ks_odometer[1]<<8)       | ((UINT32)ks_odometer[2]))

UINT32 ks_start_cdc; // Used to calculate trip power use (Cumulated discharge) 
UINT32 ks_start_cc; // Used to calculate trip recuperation (Cumulated charge)
UINT32 ks_cum_charge_start; // Used to calculate charged power.

struct {
  unsigned char ChargingChademo : 1;
  unsigned char ChargingJ1772 : 1;
  unsigned char : 1;
  unsigned char : 1;
  unsigned char FanStatus : 4;
} ks_charge_bits;

struct {
  unsigned char BCV_BlockToFetch : 2;
  unsigned char BCV_BlockFetched : 1;
  unsigned char BCV_BlockSent : 1;
  unsigned char NotifyCharge : 1;
  unsigned char : 1;
  unsigned char QRY_Read : 1;
  unsigned char QRY_Sent : 1;
} ks_sms_bits;

UINT8 ks_can_qry_databuffer[8]; // The can query data buffer
// Two first bytes is the request id
#define KS_REQUEST_ID (((UINT)ks_can_qry_databuffer[0]<<8) | ((UINT)ks_can_qry_databuffer[1]))

UINT8 ks_battery_module_temp[8];

UINT ks_battery_DC_voltage; //DC voltage                    02 21 01 -> 22 2+3
INT ks_battery_current; //Battery current               02 21 01 -> 21 7+22 1
UINT ks_battery_avail_charge; //Available charge              02 21 01 -> 21 2+3
UINT8 ks_battery_max_cell_voltage; //Max cell voltage              02 21 01 -> 23 6
UINT8 ks_battery_max_cell_voltage_no; //Max cell voltage no           02 21 01 -> 23 7
UINT8 ks_battery_min_cell_voltage; //Min cell voltage              02 21 01 -> 24 1
UINT8 ks_battery_min_cell_voltage_no; //Min cell voltage no           02 21 01 -> 24 2

UINT32 ks_battery_cum_charge_current; //Cumulated charge current    02 21 01 -> 24 6+7 & 25 1+2
UINT32 ks_battery_cum_discharge_current; //Cumulated discharge current 02 21 01 -> 25 3-6
UINT32 ks_battery_cum_charge; //Cumulated charge power      02 21 01 -> 25 7 + 26 1-3
UINT32 ks_battery_cum_discharge; //Cumulated discharge power   02 21 01 -> 26 4-7
UINT8 ks_battery_cum_op_time[3]; //Cumulated operating time    02 21 01 -> 27 1-4

UINT8 ks_battery_cell_voltage[32];

UINT8 ks_battery_min_temperature; //02 21 05 -> 21 7 
UINT8 ks_battery_inlet_temperature; //02 21 05 -> 21 6 
UINT8 ks_battery_max_temperature; //02 21 05 -> 22 1 
UINT8 ks_battery_heat_1_temperature; //02 21 05 -> 23 6
UINT8 ks_battery_heat_2_temperature; //02 21 05 -> 23 7

UINT ks_battery_max_detoriation; //02 21 05 -> 24 1+2
UINT8 ks_battery_max_detoriation_cell_no; //02 21 05 -> 24 3
UINT ks_battery_min_detoriation; //02 21 05 -> 24 4+5
UINT8 ks_battery_min_detoriation_cell_no; //02 21 05 -> 24 6

UINT8 ks_heatsink_temperature; //TODO Remove?
UINT8 ks_battery_fan_feedback;

//UINT8 ks_air_bag_hwire_duty;              //02 21 05 -> 23 5 //TODO Remove?
//UINT  ks_obc_ampere; //TODO Remove?
//UINT  ks_battery_avail_discharge;     //Available discharge           02 21 01 -> 21 4+5

UINT32 ks_tpms_id[4];
INT8 ks_tpms_temp[4];
UINT8 ks_tpms_pressure[4];

UINT8 ks_obc_volt;

typedef union {

  struct { //TODO Is this the correct order, or should it be swapped?
    unsigned char Park : 1;
    unsigned char Reverse : 1;
    unsigned char Neutral : 1;
    unsigned char Drive : 1;
    unsigned char Break : 1;
    unsigned char ECOOff : 1;
    unsigned char : 1;
    unsigned char : 1;
  };
  unsigned char value;
} KsShiftBits;
KsShiftBits ks_shift_bits;

struct {
  UINT8 byte[8];
  UINT8 status;
  UINT16 id;
}  ks_send_can;

UINT8 ks_door_byte; //TODO Temporary door byte for analysis purposes

#define KS_BUS_TIMEOUT 10     // bus activity timeout in seconds

// Kia Soul EV specific features:
#define FEATURE_MAXRANGE            0x0C // Maximum ideal range [Mi/km]
#define FEATURE_SUFFSOC             0x0A // Sufficient SOC [%]
#define FEATURE_SUFFRANGE           0x0B // Sufficient range [Mi/km]

////////////////////////////////////////////////////////////////////////////////
// OBDII polling table:
// 
//    typedef struct
//    {
//      unsigned int moduleid; // send request to (0 = end of list)
//      unsigned int rmoduleid; // expect response from (0 = broadcast)
//      unsigned char type; // see vehicle.h
//      unsigned int pid; // the PID to request
//      unsigned int polltime[VEHICLE_POLL_NSTATES]; // poll frequency
//    } vehicle_pid_t;
//
// Polling types ("modes") supported: see vehicle.h
// 
// polltime[state] = poll period in seconds, i.e. 10 = poll every 10 seconds
//
// state: 0..2; set by vehicle_poll_setstate()
//     0=off, 1=on, 2=charging
//

rom vehicle_pid_t vehicle_kiasoul_polls[] = {
  { 0x7e2, 0, VEHICLE_POLL_TYPE_OBDIIVEHICLE, 0x02,
    { 0, 120, 120}}, // VIN
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x01,
    { 30, 10, 10}}, // Diag page 01
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x02,
    { 0, 30, 10}}, // Diag page 02
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x03,
    { 0, 30, 10}}, // Diag page 03
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x04,
    { 0, 30, 10}}, // Diag page 04
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x05,
    { 120, 10, 10}}, // Diag page 05

  { 0x794, 0x79c, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x02,
    { 0, 0, 10}}, // On board charger

  { 0x7e2, 0x7ea, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x00,
    { 30, 10, 10}}, // VMCU  Shift-stick 
  { 0x7e2, 0x7ea, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x02,
    { 30, 10, 0}}, // Motor temp++

  { 0x7df, 0x7de, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x06,
    { 30, 10, 0}}, // TMPS

  { 0, 0, 0x00, 0x00,{ 0, 0, 0}}
};

// ISR optimization, see http://www.xargs.com/pic/c18-isr-optim.pdf
#pragma tmpdata high_isr_tmpdata

BOOL vehicle_kiasoul_checkpass(char *password)
{
  char *p = par_get(PARAM_MODULEPASS);
  if ((password != NULL)&&(*p && strncmp(p,password,strlen(p))==0))
    return TRUE;
  return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// Framework hook: _poll0 (ISR; process CAN buffer 0)
// 
// ATT: avoid complex calculations and data processing in ISR code!
//      store in state variables, process in _ticker or _idlepoll hooks
// 
// Kia Soul: buffer 0 = process OBDII polling result
// 
// Available framework variables:
//  can_id            = CAN RX ID (poll -> rmoduleid)
//  can_filter        = CAN RX filter number triggered (see init)
//  can_databuffer    = CAN RX data buffer (8 bytes)
//  can_datalength    = CAN RX data length (in data buffer)
//
// OBDII/UDS polling response:
//  vehicle_poll_type = last poll -> type
//  vehicle_poll_pid  = last poll -> pid
//
// Multi frame response: hook called once per frame
//  vehicle_poll_ml_frame   = frame counter (0..n)
//  can_datalength          = frame length
//                            (3 bytes in frame 0, max 7 bytes in frames 1..n)
//  vehicle_poll_ml_offset  = msg length including this frame
//  vehicle_poll_ml_remain  = msg length remaining after this frame
//

BOOL vehicle_kiasoul_poll0(void) {
  UINT8 i;
  UINT base;
  UINT32 val;

  // process OBDII data:
  switch (can_id) {
    /*case 0x779:
        while (TXB0CONbits.TXREQ) {} // Loop until TX is done
        TXB0CON = 0;
        TXB0SIDL = ((0x771) & 0x07)<<5;
        TXB0SIDH = ((0x771)>>3);
        TXB0D0 = 0x04; // flow control frame type
        TXB0D1 = 0x2f; // request all frames available
        TXB0D2 = 0xbc; // with 50ms send interval
        TXB0D3 = 0x11;
        TXB0D4 = 0x03;
        TXB0D5 = 0x00;
        TXB0D6 = 0x00;
        TXB0D7 = 0x00;
        TXB0DLC = 0b00001000; // data length (8)
        TXB0CON = 0b00001000; // mark for transmission

      break;*/
    case 0x7de: //TPMS
      switch (vehicle_poll_pid) {
        case 0x06:
          car_stale_tpms = 120;
          if (vehicle_poll_ml_frame == 0) {
            val = (UINT32) can_databuffer[7]
                    | ((UINT32) can_databuffer[6] << 8)
                    | ((UINT32) can_databuffer[5] << 16)
                    | ((UINT32) can_databuffer[4] << 24);
            if (val > 0) ks_tpms_id[0] = val;
          } else if (vehicle_poll_ml_frame == 1) {
            i = can_databuffer[1 + CAN_ADJ];
            if (i > 0) ks_tpms_pressure[0] = i;
            ks_tpms_temp[0] = can_databuffer[2 + CAN_ADJ];
            val = (ks_tpms_id[1] & 0x000000ff)
                    | ((UINT32) can_databuffer[7 + CAN_ADJ] << 8)
                    | ((UINT32) can_databuffer[6 + CAN_ADJ] << 16)
                    | ((UINT32) can_databuffer[5 + CAN_ADJ] << 24);
            if (val > 0) ks_tpms_id[1] = val;
          } else if (vehicle_poll_ml_frame == 2) {
            val = (UINT32) can_databuffer[1 + CAN_ADJ] | (ks_tpms_id[1] & 0xffffff00);
            if (val > 0) ks_tpms_id[1] = val;
            i = can_databuffer[2 + CAN_ADJ];
            if (i > 0) ks_tpms_pressure[1] = i;
            ks_tpms_temp[1] = can_databuffer[3 + CAN_ADJ];
            val = (ks_tpms_id[2] & 0x0000ffff)
                    | ((UINT32) can_databuffer[7 + CAN_ADJ] << 16)
                    | ((UINT32) can_databuffer[6 + CAN_ADJ] << 24);
            if (val > 0) ks_tpms_id[2] = val;
          } else if (vehicle_poll_ml_frame == 3) {
            val = (UINT32) can_databuffer[2 + CAN_ADJ]
                    | ((UINT32) can_databuffer[1 + CAN_ADJ] << 8)
                    | (ks_tpms_id[2] & 0xffff0000);
            if (val > 0) ks_tpms_id[2] = val;
            i = can_databuffer[3 + CAN_ADJ];
            if (i > 0) ks_tpms_pressure[2] = i;
            ks_tpms_temp[2] = can_databuffer[4 + CAN_ADJ];
            val = (ks_tpms_id[3] & 0x00ffffff)
                    | ((UINT32) can_databuffer[7 + CAN_ADJ] << 24);
            if (val > 0) ks_tpms_id[3] = val;
          } else if (vehicle_poll_ml_frame == 4) {
            val = (UINT32) can_databuffer[3 + CAN_ADJ]
                    | ((UINT32) can_databuffer[2 + CAN_ADJ] << 8)
                    | ((UINT32) can_databuffer[1 + CAN_ADJ] << 16)
                    | (ks_tpms_id[3] & 0xff000000);
            if (val > 0) ks_tpms_id[3] = val;
            i = can_databuffer[4 + CAN_ADJ];
            if (i > 0) ks_tpms_pressure[3] = i;
            ks_tpms_temp[3] = can_databuffer[5 + CAN_ADJ];
          }
          break;
      }
      break;

    case 0x79c: //OBC
      switch (vehicle_poll_pid) {
        case 0x02:
          if (vehicle_poll_ml_frame == 1) {
            ks_obc_volt = (UINT8) ((((UINT) can_databuffer[3 + CAN_ADJ] << 8)
                    | (UINT) can_databuffer[4 + CAN_ADJ]) / 10);
            //} else if (vehicle_poll_ml_frame == 2) {
            //ks_obc_ampere = ((UINT) can_databuffer[4 + CAN_ADJ] << 8) 
            //        | (UINT) can_databuffer[5 + CAN_ADJ];
          }
          break;
      }
      break;

    case 0x7ea:
      switch (vehicle_poll_pid) {
        case 0x00:
          if (vehicle_poll_ml_frame == 1) {
            ks_shift_bits.value = can_databuffer[4 + CAN_ADJ];
          }
          break;

        case 0x02:
          // VIN (multi-line response):
          // VIN length is 20 on Kia => skip first frame (3 bytes):
          if (vehicle_poll_ml_frame > 0 && vehicle_poll_type == VEHICLE_POLL_TYPE_OBDIIVEHICLE) {
            base = vehicle_poll_ml_offset - can_datalength - 3;
            for (i = 0; (i < can_datalength) && ((base + i)<(sizeof (car_vin) - 1)); i++)
              car_vin[base + i] = can_databuffer[i];
            if (vehicle_poll_ml_remain == 0)
              car_vin[base + i] = 0;
          }
          if (vehicle_poll_type == VEHICLE_POLL_TYPE_OBDIIGROUP) {
            if (vehicle_poll_ml_frame == 3) {
              car_tmotor = (signed int)can_databuffer[5 + CAN_ADJ] - 40;
              car_tpem = (signed int)can_databuffer[6 + CAN_ADJ] - 40;
              ks_heatsink_temperature = (signed int)can_databuffer[7 + CAN_ADJ] - 40;
              car_stale_temps = 120;
            }
          }
          break;
      }
      break;

    case 0x7ec:
      switch (vehicle_poll_pid) {
        case 0x01:
          // diag page 01: skip first frame (no data)
          if (vehicle_poll_ml_frame > 0) {
            if (vehicle_poll_ml_frame == 1) // 02 21 01 - 21
            {
              ks_battery_avail_charge = (UINT) can_databuffer[3 + CAN_ADJ]
                      | ((UINT) can_databuffer[2 + CAN_ADJ] << 8);
              //ks_battery_avail_discharge = (UINT8)(((UINT) can_databuffer[5 + CAN_ADJ] 
              //        | ((UINT) can_databuffer[4 + CAN_ADJ] << 8))>>2);
              i = can_databuffer[6 + CAN_ADJ];
              ks_doors1.PilotSignal = ((i & 0x80) > 0);
              ks_charge_bits.ChargingChademo = ((i & 0x40) > 0);
              ks_charge_bits.ChargingJ1772 = ((i & 0x20) > 0);
              ks_battery_current = (ks_battery_current & 0x00FF)
                      | ((UINT) can_databuffer[7 + CAN_ADJ] << 8);
            }
            if (vehicle_poll_ml_frame == 2) // 02 21 01 - 22
            {
              ks_battery_current = (ks_battery_current & 0xFF00)
                      | (UINT) can_databuffer[1 + CAN_ADJ];
              ks_battery_DC_voltage = (UINT) can_databuffer[3 + CAN_ADJ]
                      | ((UINT) can_databuffer[2 + CAN_ADJ] << 8);
              ks_battery_module_temp[0] = can_databuffer[4 + CAN_ADJ];
              ks_battery_module_temp[1] = can_databuffer[5 + CAN_ADJ];
              ks_battery_module_temp[2] = can_databuffer[6 + CAN_ADJ];
              ks_battery_module_temp[3] = can_databuffer[7 + CAN_ADJ];
            } else if (vehicle_poll_ml_frame == 3) // 02 21 01 - 23
            {
              ks_battery_module_temp[4] = can_databuffer[1 + CAN_ADJ];
              ks_battery_module_temp[5] = can_databuffer[2 + CAN_ADJ];
              ks_battery_module_temp[6] = can_databuffer[3 + CAN_ADJ];
              ks_battery_module_temp[7] = can_databuffer[5 + CAN_ADJ];
              car_stale_temps = 120; // Reset stale indicator 

              ks_battery_max_cell_voltage = can_databuffer[6 + CAN_ADJ];
              ks_battery_max_cell_voltage_no = can_databuffer[7 + CAN_ADJ];
            } else if (vehicle_poll_ml_frame == 4) // 02 21 01 - 24
            {
              ks_battery_min_cell_voltage = can_databuffer[1 + CAN_ADJ];
              ks_battery_min_cell_voltage_no = can_databuffer[2 + CAN_ADJ];
              ks_charge_bits.FanStatus = can_databuffer[4 + CAN_ADJ] & 0xF;
              ks_battery_fan_feedback = can_databuffer[3 + CAN_ADJ];
              ks_battery_cum_charge_current = (ks_battery_cum_charge_current & 0x0000FFFF)
                      | ((UINT32) can_databuffer[6 + CAN_ADJ] << 24)
                      | ((UINT32) can_databuffer[7 + CAN_ADJ] << 16);

              ks_auxVoltage = can_databuffer[5 + CAN_ADJ];
            } else if (vehicle_poll_ml_frame == 5) // 02 21 01 - 25
            {
              ks_battery_cum_charge_current = (ks_battery_cum_charge_current & 0xFFFF0000)
                      | ((UINT32) can_databuffer[1 + CAN_ADJ] << 8)
                      | ((UINT32) can_databuffer[2 + CAN_ADJ]);
              ks_battery_cum_discharge_current = (UINT32) can_databuffer[6 + CAN_ADJ]
                      | ((UINT32) can_databuffer[5 + CAN_ADJ] << 8)
                      | ((UINT32) can_databuffer[4 + CAN_ADJ] << 16)
                      | ((UINT32) can_databuffer[3 + CAN_ADJ] << 24);
              ks_battery_cum_charge = (ks_battery_cum_charge & 0xFFFFFF)
                      | ((UINT32) can_databuffer[7 + CAN_ADJ]) << 24;
            } else if (vehicle_poll_ml_frame == 6) // 02 21 01 - 26
            {
              ks_battery_cum_charge = (ks_battery_cum_charge & 0xFF000000)
                      | ((UINT32) can_databuffer[1 + CAN_ADJ] << 16)
                      | ((UINT32) can_databuffer[2 + CAN_ADJ] << 8)
                      | ((UINT32) can_databuffer[3 + CAN_ADJ]);
              ks_battery_cum_discharge = (UINT32) can_databuffer[7 + CAN_ADJ]
                      | ((UINT32) can_databuffer[6 + CAN_ADJ] << 8)
                      | ((UINT32) can_databuffer[5 + CAN_ADJ] << 16)
                      | ((UINT32) can_databuffer[4 + CAN_ADJ] << 24);
            } else if (vehicle_poll_ml_frame == 7) // 02 21 01 - 27
            {
              val = ((UINT32) can_databuffer[4 + CAN_ADJ]
                      | ((UINT32) can_databuffer[3 + CAN_ADJ] << 8)
                      | ((UINT32) can_databuffer[2 + CAN_ADJ] << 16)
                      | ((UINT32) can_databuffer[1 + CAN_ADJ] << 24)) / 3600;
              ks_battery_cum_op_time[0] = (val >> 16) & 0xff;
              ks_battery_cum_op_time[1] = (val >> 8) & 0xff;
              ks_battery_cum_op_time[2] = val & 0xff;
            }
          }
          break;

        case 0x02: //Cell voltages
        case 0x03:
        case 0x04:
          // diag page 02-04: skip first frame (no data)
          if (!ks_sms_bits.BCV_BlockFetched) {
            if (ks_sms_bits.BCV_BlockToFetch == vehicle_poll_pid - 2) {
              if (vehicle_poll_ml_frame >= 0) {
                base = vehicle_poll_ml_offset - can_datalength - 3;
                for (i = 0; i < can_datalength && ((base + i)<sizeof (ks_battery_cell_voltage)); i++)
                  ks_battery_cell_voltage[base + i] = can_databuffer[i];
                ks_sms_bits.BCV_BlockFetched = 1;
                ks_sms_bits.BCV_BlockSent = 0;
              }
            }
          }
          break;

        case 0x05:
          if (vehicle_poll_ml_frame == 1) {
            ks_battery_inlet_temperature = can_databuffer[6 + CAN_ADJ];
            ks_battery_min_temperature = can_databuffer[7 + CAN_ADJ];
          } else if (vehicle_poll_ml_frame == 2) {
            ks_battery_max_temperature = can_databuffer[1 + CAN_ADJ];
          } else if (vehicle_poll_ml_frame == 3) {
            //ks_air_bag_hwire_duty = can_databuffer[5 + CAN_ADJ];
            ks_battery_heat_1_temperature = can_databuffer[6 + CAN_ADJ];
            ks_battery_heat_2_temperature = can_databuffer[7 + CAN_ADJ];
          } else if (vehicle_poll_ml_frame == 4) {
            ks_battery_max_detoriation = (UINT) can_databuffer[2 + CAN_ADJ]
                    | ((UINT) can_databuffer[1 + CAN_ADJ] << 8);
            ks_battery_max_detoriation_cell_no = can_databuffer[3 + CAN_ADJ];
            ks_battery_min_detoriation = (UINT) can_databuffer[5 + CAN_ADJ]
                    | ((UINT) can_databuffer[4 + CAN_ADJ] << 8);
            ks_battery_min_detoriation_cell_no = can_databuffer[6 + CAN_ADJ];
          }
          break;
      }
      break;

  }
  return TRUE;
}


////////////////////////////////////////////////////////////////////////////////
// Framework hook: _poll1 (ISR; process CAN buffer 1)
// 
// ATT: avoid complex calculations and data processing in ISR code!
//      store in state variables, process in _ticker or _idlepoll hooks
// 
// Kia Soul: buffer 1 = direct CAN bus processing
// 
// Available framework variables:
//  can_id            = CAN RX ID
//  can_filter        = CAN RX filter number triggered (see init)
//  can_databuffer    = CAN RX data buffer (8 bytes)
//  can_datalength    = CAN RX data length (in data buffer)
//

BOOL vehicle_kiasoul_poll1(void) {
  long val;

  // reset bus activity timeout:
  ks_busactive = KS_BUS_TIMEOUT;

  // process CAN data:
  switch (can_id) {

    case 0x018:
      // Doors status:
      ks_doors1.ChargePort = ((CAN_BYTE(0) & 0x01) > 0);
      ks_doors1.FrontLeftDoor = ((CAN_BYTE(0) & 0x10) > 0);
      ks_doors1.FrontRightDoor = ((CAN_BYTE(0) & 0x80) > 0);
      //ks_door_byte = CAN_BYTE(0);
      break;

    case 0x120:
      if( CAN_BYTE(2)==0x20 ){
        if( CAN_BYTE(3)==0x20){
          ks_doors2.CarLocked = 0x01;
        }else if( CAN_BYTE(3)==0x10){
          ks_doors2.CarLocked = 0x00;         
        }
        ks_door_byte = CAN_BYTE(3);
      }
      break;
      
    case 0x200:
      // Estimated range:
      val = CAN_BYTE(2);
      if (val > 0)
        ks_estrange = val;
      break;

    case 0x433:
      // Parking brake status 
      ks_doors1.HandBrake = ((CAN_BYTE(2) & 0x10) > 0);
      break;

    case 0x4f0:
      // Odometer:
      if (can_datalength = 8 && (CAN_BYTE(7) || CAN_BYTE(6) || CAN_BYTE(5))) {
        ks_odometer[0] = CAN_BYTE(7);
        ks_odometer[1] = CAN_BYTE(6);
        ks_odometer[2] = CAN_BYTE(5);
      }
      break;

    case 0x4f2:
      // Speed:
      if (can_mileskm == 'M')
        car_speed = MiFromKm(CAN_BYTE(1) >> 1); // mph
      else
        car_speed = CAN_BYTE(1) >> 1; // kph      

      // 4f2 is one of the few can-messages that are sent while car is on and off.
      // Byte 2 and 7 are some sort of counters which runs while the car is on.
      if (CAN_BYTE(2) > 0 || CAN_BYTE(7) > 0) {
        ks_doors1.CarON = 1;
      } else if (CAN_BYTE(2) == 0 && CAN_BYTE(7) == 0 && car_speed == 0
              && ks_doors1.HandBrake == 1) {
        // Boths 2 and 7 and speed is 0, so we assumes the car is off.
        ks_doors1.CarON = 0;
      }
      break;

    case 0x542:
      // SOC:
      val = CAN_BYTE(0);
      if (val > 0)
        car_SOC = val >> 1;
      break;

    case 0x581:
      // Car is CHARGING:
      ks_chargepower = ((UINT) CAN_BYTE(7) << 8) | CAN_BYTE(6);
      break;

    case 0x594:
      // BMS SOC:
      val = (UINT) CAN_BYTE(5) * 5 + CAN_BYTE(6);
      if (val > 0)
        ks_bms_soc = (UINT8) (val / 10);
      break;

    case 0x653:
      // AMBIENT TEMPERATURE:
      car_ambient_temp = (((signed int) CAN_BYTE(5)) / 2) - 40;
      car_stale_ambient = 120;
      break;
  }
  
  // Check response from synchronous can message
  if(can_id==(ks_send_can.id+0x08) && ks_send_can.status==0xff){
  //if((can_id==0x771 || can_id==0x779) && ks_send_can.status==0xff){
    ks_send_can.status=3; 
    ks_send_can.byte[0]=CAN_BYTE(0);
    ks_send_can.byte[1]=CAN_BYTE(1);
    ks_send_can.byte[2]=CAN_BYTE(2);
    ks_send_can.byte[3]=CAN_BYTE(3);
    ks_send_can.byte[4]=CAN_BYTE(4);
    ks_send_can.byte[5]=CAN_BYTE(5);
    ks_send_can.byte[6]=CAN_BYTE(6);
    ks_send_can.byte[7]=CAN_BYTE(7);
  }

  // Fetch requested can id, as specified via QRY sms.
  if (!ks_sms_bits.QRY_Read) {
    if (can_id == KS_REQUEST_ID) {
      ks_sms_bits.QRY_Read = 1;
      for (val = 0; val < 8; val++) {
        ks_can_qry_databuffer[val] = CAN_BYTE(val);
      }
      ks_sms_bits.QRY_Sent = 0;
    }
  }

  return TRUE;
}

#pragma tmpdata

////////////////////////////////////////////////////////////////////////
// vehicle_Kia Soul EV get_maxrange: get MAXRANGE with temperature compensation
//

UINT8 vehicle_kiasoul_get_maxrange(void) {
  int maxRange;

  // Read user configuration:
  maxRange = sys_features[FEATURE_MAXRANGE];

  // Temperature compensation:
  //   - assumes standard maxRange specified at 20°C
  //   - Temperature halved at -20C. 
  if (maxRange != 0) {
    maxRange = (maxRange * (100 - (int) (ABS(20 - car_ambient_temp)* 1.25))) / 100;
  }
  return (UINT8) maxRange;
}

void vehicle_kiasoul_query_sms_response(void) {
  char *s;
  int i;

  // Start SMS:
  net_send_sms_start(par_get(PARAM_REGPHONE));

  // Format SMS:
  s = net_scratchpad;

  // output 
  s = stp_rom(s, "\nHex: ");
  for (i = 0; i < 8; i++)
    s = stp_sx(s, " ", ks_can_qry_databuffer[i]);

  s = stp_rom(s, "\nDec: ");
  for (i = 0; i < 8; i++)
    s = stp_i(s, " ", ks_can_qry_databuffer[i]);

  // Send SMS:
  net_puts_ram(net_scratchpad);
  net_send_sms_finish();
  ks_sms_bits.QRY_Sent = 1;
}

////////////////////////////////////////////////////////////////////////////////
// Send Battery cell voltage response SMS

void vehicle_kiasoul_bcv_sms_response(void) {
  char *s;
  int i;

  // Start SMS:
  net_send_sms_start(par_get(PARAM_REGPHONE));

  // Format SMS:
  s = net_scratchpad;

  // Format SMS:
  s = net_scratchpad;
  for (; i < sizeof (ks_battery_cell_voltage); i++) {
    s = stp_i(s,
            ((i % 8) == 7) ? "\n" : " ",
            ((UINT) ks_battery_cell_voltage[i] << 1));
  }
  // Send SMS:
  net_puts_ram(net_scratchpad);
  net_send_sms_finish();

  ks_sms_bits.BCV_BlockSent = 1;
}


////////////////////////////////////////////////////////////////////////////////
// Framework hook: _ticker1 (called about once per second)
// 
// Kia Soul: 
// 

BOOL vehicle_kiasoul_ticker1(void) {
  UINT8 maxRange, suffRange, suffSOC;
  float chargeTarget;
  UINT8 i;
  // 
  // Check CAN bus activity timeout:
  //

  if (ks_busactive > 0)
    ks_busactive--;

  if (ks_busactive == 0) {
    // CAN bus is inactive, switch OFF:
    ks_doors1.CarON = 0;
    ks_doors1.Charging = 0;
    ks_chargepower = 0;
    car_speed = 0; //Clear speed
  }

  /***************************************************************************
   * Read feature configuration:
   */

  suffSOC = (UINT8) sys_features[FEATURE_SUFFSOC];
  suffRange = (UINT8) sys_features[FEATURE_SUFFRANGE];
  maxRange = vehicle_kiasoul_get_maxrange();

  // 
  // Convert KS status data to OVMS framework:
  //

  car_estrange = MiFromKm((UINT) ks_estrange << 1);

  if (maxRange > 0)
    car_idealrange = MiFromKm((((float) maxRange) * car_SOC) / 100);
  else
    car_idealrange = car_estrange;

  if (ks_odometer[0] > 0 || ks_odometer[1] > 0 || ks_odometer[2] > 0) {
    car_odometer = MiFromKm(KS_ODOMETER);
  }

  car_tbattery = ((signed int) ks_battery_module_temp[0] + (signed int) ks_battery_module_temp[1] +
          (signed int) ks_battery_module_temp[2] + (signed int) ks_battery_module_temp[3] +
          (signed int) ks_battery_module_temp[4] + (signed int) ks_battery_module_temp[5] +
          (signed int) ks_battery_module_temp[6] + (signed int) ks_battery_module_temp[7]) / 8;

  for (i = 0; i < 4; i++) {
    car_tpms_p[i] = (UINT8) (ks_tpms_pressure[i]*0.68875); // Adjusting for value being 4 times the psi
    // and APP is multiplying by 2.755 (due to Tesla Roadster)  
    car_tpms_t[i] = ks_tpms_temp[i] - 15; // car_tpms_t = value-55+40. -55 to get actual temp. 
    // +40 to adjust for APP (due to Tesla Roadster)  
  }

  //
  // Check for car status changes:
  //
  if( car_doors1bits.FrontLeftDoor!=ks_doors1.FrontLeftDoor ||
      car_doors1bits.FrontRightDoor!=ks_doors1.FrontRightDoor ||
      car_doors1bits.ChargePort != ks_doors1.ChargePort ||
      car_doors1bits.HandBrake != ks_doors1.HandBrake ||
      car_doors2bits.CarLocked != ks_doors2.CarLocked    
      ){
    car_doors1bits.FrontLeftDoor = ks_doors1.FrontLeftDoor;
    car_doors1bits.FrontRightDoor = ks_doors1.FrontRightDoor;
    car_doors1bits.ChargePort = ks_doors1.ChargePort;
    car_doors1bits.HandBrake = ks_doors1.HandBrake;
    car_doors2bits.CarLocked = ks_doors2.CarLocked;
    net_req_notification(NET_NOTIFY_ENV);
  }

  if (ks_doors1.CarON) {
    // Car is ON
    if (car_doors1bits.CarON == 0) {
      car_doors1bits.CarON = 1;

      // Start trip, save current state
      //ODO at Start of trip
      ks_trip_start_odo[0] = ks_odometer[0];
      ks_trip_start_odo[1] = ks_odometer[1];
      ks_trip_start_odo[2] = ks_odometer[2];
      ks_start_cdc = ks_battery_cum_discharge; //Cumulated discharge
      ks_start_cc = ks_battery_cum_charge; //Cumulated charge
    }
    if (car_parktime != 0) {
      car_parktime = 0; // No longer parking
      net_req_notification(NET_NOTIFY_ENV);
    }
    car_trip = MiFromKm((UINT) (KS_ODOMETER - KS_TRIP_START_ODOMETER));
  } else {
    // Car is OFF
    car_doors1bits.CarON = 0;
    car_speed = 0;
    if (car_parktime == 0) {
      car_parktime = car_time - 1; // Record it as 1 second ago, so non zero report
      car_trip = MiFromKm((UINT) (KS_ODOMETER - KS_TRIP_START_ODOMETER));
      net_req_notification(NET_NOTIFY_ENV);
    }
  }

  if (ks_charge_bits.ChargingJ1772) { // J1772 - Type 1  charging
    car_linevoltage = ks_obc_volt;
    car_chargecurrent = (((long) ks_chargepower * 1000) >> 8) / car_linevoltage;
    car_chargemode = 0; // Standard      
    car_chargelimit = 6600 / car_linevoltage; // 6.6kW
    car_chargetype = J1772_TYPE1;
  } else if (ks_charge_bits.ChargingChademo) { //ChaDeMo charging            
    car_linevoltage = ks_battery_DC_voltage / 10;
    car_chargecurrent = (UINT8) ((-ks_battery_current) / 10);
    car_chargemode = 4; // Performance      
    car_chargelimit = (UINT) ((long) ks_battery_avail_charge * 100 / ks_battery_DC_voltage); // 250A - 100kW/400V
    car_chargetype = CHADEMO;
  }

  ks_doors1.Charging = ks_doors1.PilotSignal
          && (ks_charge_bits.ChargingChademo || ks_charge_bits.ChargingJ1772)
          && (car_chargecurrent > 0);

  vehicle_poll_setstate(ks_doors1.Charging ? 2 : (ks_doors1.CarON ? 1 : 0));
  //
  // Check for charging status changes:
  if (ks_doors1.Charging) {
    if (!car_doors1bits.Charging) {
      // Charging started:
      car_doors1bits.PilotSignal = 1;
      car_doors1bits.ChargePort = 1;
      car_doors1bits.Charging = 1;
      car_doors5bits.Charging12V = 1;
      car_chargefull_minsremaining = 0;
      car_chargeduration = 0; // Reset charge duration
      car_chargestate = 1; // Charging state
      car_chargemode = 0; // Standard charging. 
      car_chargekwh = 0; // kWh charged
      car_chargelimit = 0; // Charge limit
      ks_sms_bits.NotifyCharge = 1; // Send SMS when charging is fully initiated
      ks_cum_charge_start = ks_battery_cum_charge;
      net_req_notification(NET_NOTIFY_CHARGE);
      net_req_notification(NET_NOTIFY_STAT);
    } else {
      // Charging continues:
      car_chargeduration++;

      if (((car_SOC > 0) && (suffSOC > 0) && (car_SOC >= suffSOC) && (ks_last_soc < suffSOC)) ||
              ((ks_estrange > 0) && (suffRange > 0) && (car_idealrange >= suffRange) && (ks_last_ideal_range < suffRange))) {
        // ...enter state 2=topping off:
        car_chargestate = 2;

        // ...send charge alert:
        net_req_notification(NET_NOTIFY_CHARGE);
        net_req_notification(NET_NOTIFY_STAT);
      }        // ...else set "topping off" from 94% SOC:
      else if (car_SOC >= 95) {
        car_chargestate = 2; // Topping off
        net_req_notification(NET_NOTIFY_ENV);
      }
    }

    // Check if we have what is needed to calculate remaining minutes
    if (car_linevoltage > 0 && car_chargecurrent > 0) {
      car_chargestate = 1; // Charging state

      //Calculate remaining charge time
      if (suffSOC > 0) {
        chargeTarget = 270 * suffSOC;
      } else if (suffRange > 0) {
        chargeTarget = ((long) suffRange * 27000 / maxRange);
      } else {
        chargeTarget = 27000;
      }
      if (ks_charge_bits.ChargingChademo && chargeTarget > 22410) { //ChaDeMo charging            
        chargeTarget = 22410;
      }
      car_chargefull_minsremaining = (UINT)
              ((float) ((chargeTarget - (27000.0 * car_SOC) / 100.0)*60.0) /
              ((float) (car_linevoltage * car_chargecurrent)));
      if (car_chargefull_minsremaining > 1440) { //Maximum 24h charge time 
        car_chargefull_minsremaining = 1440;
      }

      if (ks_sms_bits.NotifyCharge == 1) { //Send Charge SMS after we have initialized the voltage and current settings 
        ks_sms_bits.NotifyCharge = 0;
        net_req_notification(NET_NOTIFY_CHARGE);
        net_req_notification(NET_NOTIFY_STAT);
      }
    } else {
      car_chargestate = 0x0f; //Heating?
    }
    car_chargekwh = (UINT) (ks_battery_cum_charge - ks_cum_charge_start);
    ks_last_soc = car_SOC;
    ks_last_ideal_range = (UINT8) car_idealrange;
  } else if (car_doors1bits.Charging) {
    // Charge completed or interrupted:
    car_doors1bits.PilotSignal = 0;
    car_doors1bits.Charging = 0;
    car_doors5bits.Charging12V = 0;
    car_chargecurrent = 0;
    if (car_SOC == 100)
      car_chargestate = 4; // Charge DONE
    else
      car_chargestate = 21; // Charge STOPPED
    car_chargelimit = 0;
    car_chargekwh = (UINT) (ks_battery_cum_charge - ks_cum_charge_start);
    ks_cum_charge_start = 0;
    net_req_notification(NET_NOTIFY_CHARGE);
    net_req_notification(NET_NOTIFY_STAT);
  }

  // Check if we have a can-message as a response to a QRY sms
  if (ks_sms_bits.QRY_Read && !ks_sms_bits.QRY_Sent) {
    //Only second MSbit is sat, so we have data and are ready to send
    vehicle_kiasoul_query_sms_response();
  }

  // Check if we have to send a response to BCV
  if (ks_sms_bits.BCV_BlockFetched && !ks_sms_bits.BCV_BlockSent) {
    vehicle_kiasoul_bcv_sms_response();
  }
}


// asynchronous can request:
void vehicle_kiasoul_send_can_message(void)
{
  if( sys_features[FEATURE_CANWRITE]>0 ){
    // wait for previous async send (if any) to finish:
    while (TXB0CONbits.TXREQ) {}

    // try to prevent bus flooding:
    delay5b();

    // init transmit:
    TXB0CON = 0;

    TXB0SIDL = (ks_send_can.id & 0x07) << 5;
    TXB0SIDH = (ks_send_can.id >> 3);

    // fill in the can message
    TXB0D0 = ks_send_can.byte[0];
    TXB0D1 = ks_send_can.byte[1];
    TXB0D2 = ks_send_can.byte[2];
    TXB0D3 = ks_send_can.byte[3];
    TXB0D4 = ks_send_can.byte[4];
    TXB0D5 = ks_send_can.byte[5];
    TXB0D6 = ks_send_can.byte[6];
    TXB0D7 = ks_send_can.byte[7];

    // data length (8):
    TXB0DLC = 0b00001000;

    // clear status to detect reply:
    ks_send_can.status = 0xff;

    // send:
    TXB0CON = 0b00001000;

    while (TXB0CONbits.TXREQ) {}
  }
}


// synchronous can request (1000 ms timeout):
BOOL vehicle_kiasoul_send_can_message_sync(UINT16 id, UINT8 count, 
        UINT8 serviceId, UINT8 b1, UINT8 b2, UINT8 b3, UINT8 b4,  
        UINT8 b5, UINT8 b6)
{
  UINT8 timeout;
  
  ks_send_can.id = id;
  ks_send_can.byte[0] = count;
  ks_send_can.byte[1] = serviceId;
  ks_send_can.byte[2] = b1;
  ks_send_can.byte[3] = b2;
  ks_send_can.byte[4] = b3;
  ks_send_can.byte[5] = b4;
  ks_send_can.byte[6] = b5;
  ks_send_can.byte[7] = b6;
          
  vehicle_kiasoul_send_can_message();
  
  ClrWdt();
  timeout = 200; // ~1000 ms
  do {
    delay5b();
  } while (ks_send_can.status == 0xff && --timeout);
 
  if(timeout != 0 ){
    if( ks_send_can.byte[1]==serviceId+0x40 ){
      return TRUE;
    }else{
      ks_send_can.status = 0x2;       
    }
  }else{
    ks_send_can.status = 0x1; 
  }
  
  return FALSE;
 }

////////////////////////////////////////////////////////////////////////////////
// Send a can message to set ECU in Diagnostic session mode
BOOL vehicle_kiasoul_set_temporary_session_mode(UINT16 id, UINT8 mode){
  return vehicle_kiasoul_send_can_message_sync(id, 2, 
          VEHICLE_POLL_TYPE_OBDIISESSION, mode,0,0,0,0,0);
}

// Open or lock doors
BOOL vehicle_kiasoul_set_door_lock(BOOL open, char* password){ 
  if( ks_shift_bits.Park ){
    if( vehicle_kiasoul_checkpass(password) ){
      if( vehicle_kiasoul_set_temporary_session_mode(0x771, 3)){
        return vehicle_kiasoul_send_can_message_sync(0x771, 4, 
                VEHICLE_POLL_TYPE_OBDII_IOCTRL_BY_ID, 0xbc, open?0x10:0x11, 0x03, 
                0,0,0 );
      }
    }
  }
  return FALSE;
}

////////////////////////////////////////////////////////////////////////
// Vehicle specific SMS command: DEBUG
//

BOOL vehicle_kiasoul_debug_sms(BOOL premsg, char *caller, char *command, char *arguments) {
  char *s;
  int i;

  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  if (!caller || !*caller) {
    caller = par_get(PARAM_REGPHONE);
    if (!caller || !*caller)
      return FALSE;
  }

  // Start SMS:
  if (premsg)
    net_send_sms_start(caller);

  // Format SMS:
  s = net_scratchpad;

  if (arguments && (arguments[0] == 'C')) {
    s = stp_rom(s, "VARS:");
    s = stp_i(s, "\nobc V=", ks_obc_volt);
    //s = stp_i(s, "\nobc A=", ks_obc_ampere);
    s = stp_i(s, "\nBMSsoc=", ks_bms_soc);
    s = stp_l2f(s, "%\nchgep=", (ks_chargepower * 1000) >> 8, 2);
    s = stp_i(s, "\nFan\nDuty: ", ks_charge_bits.FanStatus * 10);
    if (ks_charge_bits.FanStatus == 0) {
      s = stp_rom(s, "% ");
    } else if (ks_charge_bits.FanStatus == 1) {
      s = stp_rom(s, "% 90"); // i*900
    } else if (ks_charge_bits.FanStatus == 2) {
      s = stp_rom(s, "% 120"); // i*600
    } else if (ks_charge_bits.FanStatus == 3) {
      s = stp_rom(s, "% 150"); // i*500
    } else if (ks_charge_bits.FanStatus == 4) {
      s = stp_rom(s, "% 180"); // i*450
    } else if (ks_charge_bits.FanStatus == 5) {
      s = stp_rom(s, "% 210"); // i*420
    } else if (ks_charge_bits.FanStatus == 6) {
      s = stp_rom(s, "% 245"); // i*408,3
    } else if (ks_charge_bits.FanStatus == 7) {
      s = stp_rom(s, "% 260"); // i*433,333   
    } else if (ks_charge_bits.FanStatus == 8) {
      s = stp_rom(s, "% 280"); // i*350
    } else if (ks_charge_bits.FanStatus == 9) {
      s = stp_rom(s, "% 300"); // i*333,3333
    }
    s = stp_i(s, "0 rpm\nFeedback=", ks_battery_fan_feedback);
    s = stp_rom(s, "Hz");
  } else {
    // output status vars:
    s = stp_rom(s, "VARS:");
    s = stp_i(s, "\ndoors=", ks_door_byte);
    s = stp_i(s, "\nspeed=", car_speed);
    s = stp_i(s, "\nparkbr=", ks_doors1.HandBrake);
    s = stp_i(s, "\nStick=", ks_shift_bits.value);
    s = stp_l2f(s, "\n12v1=", ks_auxVoltage, 1);
    s = stp_l2f(s, "\n12v2=", car_12vline, 1);
    //s = stp_i(s, "\nABHD", ks_air_bag_hwire_duty);
    s = stp_i(s, "\nlock=", ks_doors2.CarLocked);
        
/*
    s = stp_x(s, "\nQRY: 0x", KS_REQUEST_ID);
    if (!ks_sms_bits.QRY_Read) {
      s = stp_rom(s, "\nUNREAD");
    } else if (!ks_sms_bits.QRY_Sent) {
      s = stp_rom(s, "\nUNSENT");
    } else {
      s = stp_rom(s, "\nSENT");
    }

    s = stp_x(s, "\nBCV: ", ks_sms_bits.BCV_BlockToFetch);
    if (!ks_sms_bits.BCV_BlockFetched) {
      s = stp_rom(s, "\nUNREAD");
    } else if (!ks_sms_bits.BCV_BlockSent) {
      s = stp_rom(s, "\nUNSENT");
    } else {
      s = stp_rom(s, "\nSENT");
    }
*/
    s = stp_x(s, "\nCAN: 0x", ks_send_can.id);
    if (ks_send_can.status==0) {
      s = stp_rom(s, "\nRD");
    } else if (ks_send_can.status==0xff) {
      s = stp_rom(s, "\nWAIT");
    } else if (ks_send_can.status==1) {
      s = stp_rom(s, "\nTO");
    } else if (ks_send_can.status==2) {
      s = stp_rom(s, "\nERR");
    } else {
      s = stp_rom(s, "\nUNKN");
    }
      // output 
    s = stp_rom(s, "\nHex: ");
    for (i = 0; i < 8; i++)
      s = stp_sx(s, " ", ks_send_can.byte[i]);


  }

  // Send SMS:
  net_puts_ram(net_scratchpad);

  return TRUE;
}

////////////////////////////////////////////////////////////////////////
// Vehicle specific SMS command: BATT
//

BOOL vehicle_kiasoul_battery_sms(BOOL premsg, char *caller, char *command, char *arguments) {
  char *s;
  int i;

  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  if (!caller || !*caller) {
    caller = par_get(PARAM_REGPHONE);
    if (!caller || !*caller)
      return FALSE;
  }

  // Start SMS:
  if (premsg)
    net_send_sms_start(caller);

  // Format SMS:
  s = net_scratchpad;

  s = stp_l2f(s, "Battery\n", (long) ks_battery_DC_voltage, 1);
  s = stp_l2f(s, "V ", (long) ks_battery_current, 1);
  s = stp_l2f(s, "A\nAC", (long) ks_battery_avail_charge, 2);
  //s = stp_l2f(s, "kW\nAD", long) ks_battery_avail_discharge, 2);
  s = stp_l2f(s, "kW\nC", (long) ks_battery_max_cell_voltage << 1, 2);
  s = stp_i(s, "V#", ks_battery_max_cell_voltage_no);
  s = stp_l2f(s, " ", (long) ks_battery_min_cell_voltage << 1, 2);
  s = stp_i(s, "V#", ks_battery_min_cell_voltage_no);
  s = stp_l2f(s, "\nCC", (long) ks_battery_cum_charge_current, 1);
  s = stp_l2f(s, "Ah ", (long) ks_battery_cum_charge, 1);
  s = stp_l2f(s, "kWh\nCD", (long) ks_battery_cum_discharge_current, 1);
  s = stp_l2f(s, "Ah ", (long) ks_battery_cum_discharge, 1);
  s = stp_l(s, "kWh\nOT", (long) ((UINT32) ks_battery_cum_op_time[0] << 16) |
          ((UINT32) ks_battery_cum_op_time[1] << 8) |
          ((UINT32) ks_battery_cum_op_time[2]));
  s = stp_l2f(s, "h\nDet", (long) ks_battery_max_detoriation, 1);
  s = stp_i(s, "%#", ks_battery_max_detoriation_cell_no);
  s = stp_l2f(s, " ", (long) ks_battery_min_detoriation, 1);
  s = stp_i(s, "%#", ks_battery_min_detoriation_cell_no);

  // Send SMS:
  net_puts_ram(net_scratchpad);

  return TRUE;
}

////////////////////////////////////////////////////////////////////////
// Vehicle specific SMS command: BTMP
//

BOOL vehicle_kiasoul_battery_temp_sms(BOOL premsg, char *caller, char *command, char *arguments) {
  char *s;
  int i;

  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  if (!caller || !*caller) {
    caller = par_get(PARAM_REGPHONE);
    if (!caller || !*caller)
      return FALSE;
  }

  // Start SMS:
  if (premsg)
    net_send_sms_start(caller);

  // Format SMS:
  s = net_scratchpad;
  s = stp_rom(s, "Batt. temp\nModules\n");
  for (i = 0; i < 8; i++) {
    s = stp_i(s, "#", (i + 1));
    s = stp_i(s, "=", ks_battery_module_temp[i]);
    s = stp_rom(s, ((i + 1) % 4) ? "C " : "C\n");
  }
  s = stp_i(s, "Max", ks_battery_max_temperature);
  s = stp_i(s, "C Min", ks_battery_min_temperature);
  s = stp_i(s, "C\nInlet=", ks_battery_inlet_temperature);
  s = stp_i(s, "C\nHeat1=", ks_battery_heat_1_temperature);
  s = stp_i(s, "C 2=", ks_battery_heat_2_temperature);
  //s = stp_rom(s, "C");
  s = stp_i(s, "C\nMotT=", car_tmotor);
  s = stp_i(s, "C\nMcuT=", car_tpem);
  s = stp_i(s, "C\nHST=", ks_heatsink_temperature);


  // Send SMS:
  net_puts_ram(net_scratchpad);
  return TRUE;
}

////////////////////////////////////////////////////////////////////////
// Vehicle specific SMS command: TMPS
//

BOOL vehicle_kiasoul_tpms_sms(BOOL premsg, char *caller, char *command, char *arguments) {
  char *s;
  int i;

  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  if (!caller || !*caller) {
    caller = par_get(PARAM_REGPHONE);
    if (!caller || !*caller)
      return FALSE;
  }

  // Start SMS:
  if (premsg)
    net_send_sms_start(caller);

  // Format SMS:
  s = net_scratchpad;
  s = stp_rom(s, "TPMS");
  for (i = 0; i < 4; i++) {
    s = stp_lx(s, "\nID", (long) ks_tpms_id[i]);
    s = stp_l2f(s, " ", (long) ks_tpms_pressure[i]*25, 2);
    s = stp_l2f(s, "psi ", (long) ks_tpms_temp[i]*100 - 5500, 2);
    s = stp_rom(s, "C");
  }

  // Send SMS:
  net_puts_ram(net_scratchpad);
  return TRUE;
}

////////////////////////////////////////////////////////////////////////
// Vehicle specific SMS command: AVG
//

BOOL vehicle_kiasoul_trip_sms(BOOL premsg, char *caller, char *command, char *arguments) {
  char *s;
  UINT32 discharge;
  UINT32 dischargeTotal;
  UINT32 trip, recuperation;

  // Start SMS:
  if (premsg)
    net_send_sms_start(caller);

  // Format SMS:
  s = net_scratchpad;

  recuperation = (ks_battery_cum_charge - ks_start_cc);
  dischargeTotal = (ks_battery_cum_discharge - ks_start_cdc);
  discharge = (dischargeTotal - recuperation);
  trip = (KS_ODOMETER - KS_TRIP_START_ODOMETER);
  if (can_mileskm == 'M') {
    if (trip > 0 && ks_start_cc != 0 && ks_start_cdc != 0) {
      trip = MiFromKm(trip);
      s = stp_l2f(s, "TRIP\n", trip, 1);
      s = stp_l2f(s, "mi\n", (long) (discharge * 10000.0 / trip), 2);
      s = stp_l2f(s, "kWh/100mi\n", (long) (trip * 100.0 / discharge), 2);
      s = stp_l2f(s, "mi/kWh\nDis ", (long) discharge, 1);
      s = stp_l2f(s, "kWh\nRec ", (long) recuperation, 1);
      s = stp_l2f(s, "kWh\nSum ", (long) dischargeTotal, 1);
      s = stp_rom(s, "kWh\n\n");
    }
    s = stp_l2f(s, "ODO\n", (long) (ks_battery_cum_discharge * 100) / (long) MiFromKm(KS_ODOMETER / 100), 2);
    s = stp_l2f(s, "kWh/100mi\n", (long) MiFromKm(KS_ODOMETER) / (long) ks_battery_cum_discharge / 100, 2);
    s = stp_l2f(s, "mi/kWh\nDisch ", (long) ks_battery_cum_discharge, 1);
    s = stp_rom(s, "kWh");
  } else {
    if (trip > 0 && ks_start_cc != 0 && ks_start_cdc != 0) {
      s = stp_l2f(s, "TRIP\n", trip, 1);
      s = stp_l2f(s, "km\n", (long) (discharge * 10000.0 / trip), 2);
      s = stp_l2f(s, "kWh/100km\n", (long) (trip * 100.0 / discharge), 2);
      s = stp_l2f(s, "km/kWh\nDis ", (long) discharge, 1);
      s = stp_l2f(s, "kWh\nRec ", (long) recuperation, 1);
      s = stp_l2f(s, "kWh\nSum ", (long) dischargeTotal, 1);
      s = stp_rom(s, "kWh\n\n");
    }
    s = stp_l2f(s, "ODO\n", (long) KS_ODOMETER, 1);
    s = stp_l2f(s, "km\n", (long) (ks_battery_cum_discharge * 100) / (long) (KS_ODOMETER / 100), 2);
    s = stp_l2f(s, "kWh/100km\n", (long) (KS_ODOMETER) / (long) (ks_battery_cum_discharge / 100), 2);
    s = stp_l2f(s, "km/kWh\nDis ", (long) ks_battery_cum_discharge, 1);
    s = stp_rom(s, "kWh");
  }

  // Send SMS:
  net_puts_ram(net_scratchpad);
  return TRUE;
}

////////////////////////////////////////////////////////////////////////
// Vehicle specific SMS command: BCV
//

BOOL vehicle_kiasoul_battery_cells_sms(BOOL premsg, char *caller, char *command, char *arguments) {
  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  if (!caller || !*caller) {
    caller = par_get(PARAM_REGPHONE);
    if (!caller || !*caller)
      return FALSE;
  }

  if (arguments && arguments[0] >= '0' && arguments[0] <= '3') {
    ks_sms_bits.BCV_BlockToFetch = (arguments[0] - '0');
    ks_sms_bits.BCV_BlockFetched = 0;
    ks_sms_bits.BCV_BlockSent = 0;
    return TRUE;
  }
  return FALSE;
}

////////////////////////////////////////////////////////////////////////
// Vehicle specific SMS command: RANGE
// Usage: RANGE? Get current range
//        RANGE 12334 Set new maximum range 

BOOL vehicle_kiasoul_range_sms(BOOL premsg, char *caller, char *command, char *arguments) {
  char *s;
  int i;

  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  if (!caller || !*caller) {
    caller = par_get(PARAM_REGPHONE);
    if (!caller || !*caller)
      return FALSE;
  }

  // Start SMS:
  if (premsg)
    net_send_sms_start(caller);

  // Format SMS:
  s = net_scratchpad;

  if (arguments && *arguments) {
    // Set new maximum range value
    sys_features[FEATURE_MAXRANGE] = atoi(arguments);
  }

  // output current maximum range setting
  s = stp_i(s, " Range = ", vehicle_kiasoul_get_maxrange());

  // Send SMS:
  net_puts_ram(net_scratchpad);

  return TRUE;
}

////////////////////////////////////////////////////////////////////////
// Vehicle specific SMS command: CA
// Usage: CA?           Get current settings
//        CA 123        Set new charge alert range  
//        CA 80%        Set new charge alert   
//        CA 123 80%    Set new charge alerts 
//        CA            Resets the value 

BOOL vehicle_kiasoul_charge_alert_sms(BOOL premsg, char *caller, char *command, char *arguments) {
  char *s;
  int i;
  //TODO IKKE IMPLEMENTERT ENNÅ
  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  if (!caller || !*caller) {
    caller = par_get(PARAM_REGPHONE);
    if (!caller || !*caller)
      return FALSE;
  }

  // Start SMS:
  if (premsg)
    net_send_sms_start(caller);

  // Format SMS:
  s = net_scratchpad;

  if (command && (command[2] != '?')) {
    // SET CHARGE ALERTS:
    int value;
    char unit;
    unsigned char f;
    char *arg_suffsoc = NULL, *arg_suffrange = NULL;

    // clear current alerts:
    sys_features[FEATURE_SUFFSOC] = 0;
    sys_features[FEATURE_SUFFRANGE] = 0;

    // read new alerts from arguments:
    while (arguments && *arguments) {
      value = atoi(arguments);
      unit = arguments[strlen(arguments) - 1];

      if (unit == '%') {
        arg_suffsoc = arguments;
        sys_features[FEATURE_SUFFSOC] = value;
      } else {
        arg_suffrange = arguments;
        sys_features[FEATURE_SUFFRANGE] = value;
      }

      arguments = net_sms_nextarg(arguments);
    }

    // store new alerts into EEPROM:
    par_set(PARAM_FEATURE_BASE + FEATURE_SUFFSOC, arg_suffsoc);
    par_set(PARAM_FEATURE_BASE + FEATURE_SUFFRANGE, arg_suffrange);
  }

  //TODO vehicle_kiasoul_calculate_chargetimes();

  s = stp_rom(net_scratchpad, "Charge Alert: ");

  if ((sys_features[FEATURE_SUFFSOC] == 0) && (sys_features[FEATURE_SUFFRANGE] == 0)) {
    s = stp_rom(s, "OFF");
  } else {
    if (sys_features[FEATURE_SUFFSOC] > 0) {
      // output SUFFSOC:
      s = stp_i(s, NULL, sys_features[FEATURE_SUFFSOC]);
      s = stp_rom(s, "%");
    }

    if (sys_features[FEATURE_SUFFRANGE] > 0) {
      // output SUFFRANGE:
      s = stp_i(s, (sys_features[FEATURE_SUFFSOC] > 0) ? " or " : NULL,
              sys_features[FEATURE_SUFFRANGE]);
      s = stp_rom(s, (can_mileskm == 'M') ? " mi" : " km");
    }

    // output smallest ETR:
    s = stp_i(s, "\n Time: ", ((car_chargelimit_minsremaining_range >= 0)
            && (car_chargelimit_minsremaining_range < car_chargelimit_minsremaining_soc))
            ? car_chargelimit_minsremaining_range
            : car_chargelimit_minsremaining_soc);
    s = stp_rom(s, " min.");
  }

  // Estimated time for 100%:
  s = stp_i(s, "\n Full charge: ", car_chargefull_minsremaining);
  s = stp_rom(s, " min.\n");


  // Charge status:
  if (car_doors1bits.ChargePort) {
    // Charge port door is open, we are charging
    switch (car_chargestate) {
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
  } else {
    // Charge port door is closed, not charging
    s = stp_rom(s, "Not charging");
  }


  // Estimated + Ideal Range:
  if (can_mileskm == 'M') {
    s = stp_i(s, "\n Range: ", car_estrange);
    s = stp_i(s, " - ", car_idealrange);
    s = stp_rom(s, " mi");
  } else {
    s = stp_i(s, "\n Range: ", KmFromMi(car_estrange));
    s = stp_i(s, " - ", KmFromMi(car_idealrange));
    s = stp_rom(s, " km");
  }

  // SOC:
  s = stp_l2f(s, "\n SOC: ", car_SOC * 10, 1);
  s = stp_rom(s, "%");


  // Send SMS:
  net_puts_ram(net_scratchpad);

  return TRUE;
}

////////////////////////////////////////////////////////////////////////
// Vehicle specific SMS command: QRY
//

BOOL vehicle_kiasoul_query_sms(BOOL premsg, char *caller, char *command, char *arguments) {
  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;
  
  if (arguments && *arguments) {
    ks_can_qry_databuffer[0] = H2D(arguments[0]);
    ks_can_qry_databuffer[1] = (H2D(arguments[1])) << 4;
    ks_can_qry_databuffer[1] += H2D(arguments[2]);

    ks_sms_bits.QRY_Read = 0;
    ks_sms_bits.QRY_Sent = 0;
  }

  return TRUE;
}

////////////////////////////////////////////////////////////////////////
// This is the Kia Soul SMS command table
// (for implementation notes see net_sms::sms_cmdtable comment)
//
// First char = auth mode of command:
//   1:     the first argument must be the module password
//   2:     the caller must be the registered telephone
//   3:     the caller must be the registered telephone, or first argument the module password

BOOL vehicle_kiasoul_help_sms(BOOL premsg, char *caller, char *command, char *arguments);

rom char vehicle_kiasoul_sms_cmdtable[][NET_SMS_CMDWIDTH] = {
  "3DEBUG", // Output debug data
  "3HELP", // extend HELP output
  "3RANGE", // Range setting
  "3CA", // Charge Alert
  "3QRY", // Query CAN  
  "3BTMP", // Battery temp
  "3BATT", // Battery info
  "3BCV", // Battery cell voltages
  "3TPMS", // Tire pressures
  "3TRIP", // Trip info
  ""
};

rom far BOOL(*vehicle_kiasoul_sms_hfntable[])(BOOL premsg, char *caller, char *command, char *arguments) = {
  &vehicle_kiasoul_debug_sms,
  &vehicle_kiasoul_help_sms,
  &vehicle_kiasoul_range_sms,
  &vehicle_kiasoul_charge_alert_sms,
  &vehicle_kiasoul_query_sms,
  &vehicle_kiasoul_battery_temp_sms,
  &vehicle_kiasoul_battery_sms,
  &vehicle_kiasoul_battery_cells_sms,
  &vehicle_kiasoul_tpms_sms,
  &vehicle_kiasoul_trip_sms
};


// SMS COMMAND DISPATCHERS:
// premsg: TRUE=may replace, FALSE=may extend standard handler
// returns TRUE if handled

BOOL vehicle_kiasoul_fn_smshandler(BOOL premsg, char *caller, char *command, char *arguments) {
  // called to extend/replace standard command: framework did auth check for us

  UINT8 k;

  // Command parsing...
  for (k = 0; vehicle_kiasoul_sms_cmdtable[k][0] != 0; k++) {
    if (starts_with(command, (char const rom far*) vehicle_kiasoul_sms_cmdtable[k] + 1)) {
      // Call sms handler:
      k = (*vehicle_kiasoul_sms_hfntable[k])(premsg, caller, command, arguments);

      if ((premsg) && (k)) {
        // we're in charge + handled it; finish SMS:
        net_send_sms_finish();
      }

      return k;
    }
  }

  return FALSE; // no vehicle command
}

BOOL vehicle_kiasoul_fn_smsextensions(char *caller, char *command, char *arguments) {
  // called for specific command: we need to do the auth check

  UINT8 k;

  // Command parsing...
  for (k = 0; vehicle_kiasoul_sms_cmdtable[k][0] != 0; k++) {
    if (starts_with(command, (char const rom far*) vehicle_kiasoul_sms_cmdtable[k] + 1)) {
      // we need to check the caller authorization:
      arguments = net_sms_initargs(arguments);
      if (!net_sms_checkauth(vehicle_kiasoul_sms_cmdtable[k][0], caller, &arguments))
        return FALSE; // failed

      // Call sms handler:
      k = (*vehicle_kiasoul_sms_hfntable[k])(TRUE, caller, command, arguments);

      if (k) {
        // we're in charge + handled it; finish SMS:
        net_send_sms_finish();
      }

      return k;
    }
  }

  return FALSE; // no vehicle command
}


////////////////////////////////////////////////////////////////////////
// Vehicle specific SMS command output extension: HELP
//

BOOL vehicle_kiasoul_help_sms(BOOL premsg, char *caller, char *command, char *arguments) {
  int k;

  //if (premsg)
  //  return FALSE; // run only in extend mode

  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  net_puts_rom("\nKIA:");

  for (k = 0; vehicle_kiasoul_sms_cmdtable[k][0] != 0; k++) {
    net_puts_rom(" ");
    net_puts_rom(vehicle_kiasoul_sms_cmdtable[k] + 1);
  }

  return TRUE;
}

BOOL vehicle_kiasoul_fn_commandhandler(BOOL msgmode, int cmd, char *msg) {
  switch (cmd) {
      /************************************************************
       * STANDARD COMMAND OVERRIDES:
       */
    
    case CMD_Alert:
      //return vehicle_twizy_statalert_cmd(msgmode, cmd, msg);
      return FALSE;

    case CMD_Lock:
      //TODO Check if it is ok to lock/unlock
      if( vehicle_kiasoul_set_door_lock(TRUE, msg)){
        //STP_OK(net_scratchpad, cmd);
        return TRUE;
      }
      return FALSE;

    case CMD_UnLock:
      //TODO Check if it is ok to lock/unlock
      if( vehicle_kiasoul_set_door_lock(FALSE, msg)){
        //STP_OK(net_scratchpad, cmd);
        return TRUE;
      }
      return FALSE;

    case CMD_ValetOn:
    case CMD_ValetOff:
      return FALSE;

    case CMD_Homelink:
      return FALSE;

  }

  // not handled
  return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// Framework hook: _initialise (initialise vars & CAN interface)
// 
// Kia Soul: 
// 

BOOL vehicle_kiasoul_initialise(void) {

  //
  // Initialize vehicle status:
  // 

  car_type[0] = 'K'; // Car is type KS - Kia Soul
  car_type[1] = 'S';
  car_type[2] = 0;
  car_type[3] = 0;
  car_type[4] = 0;

  ks_busactive = 0;

  ks_doors1.CarON = 0;
  ks_doors1.Charging = 0;
  ks_doors1.FrontLeftDoor = 0;
  ks_doors1.FrontRightDoor = 0;
  ks_doors1.HandBrake = 0;
  ks_doors1.PilotSignal = 0;
  
  ks_doors2.CarLocked = 0;

  car_SOC = 100; // avoid low SOC alerts
  ks_last_soc = 100;
  ks_bms_soc = 100;

  car_12vline_ref = 122; //TODO What should this value be? 
  ks_auxVoltage = 0;

  ks_estrange = 0;
  ks_last_ideal_range = 0;
  ks_chargepower = 0;

  car_speed = 0;
  memset(ks_odometer, 0, sizeof (ks_odometer));
  memset(ks_trip_start_odo, 0, sizeof (ks_trip_start_odo));

  memset(ks_can_qry_databuffer, 0, sizeof (ks_can_qry_databuffer));

  ks_door_byte = 0;

  memset(ks_battery_module_temp, 0, sizeof (ks_battery_module_temp));

  ks_battery_DC_voltage = 0;
  ks_battery_current = 0;
  ks_battery_avail_charge = 0;
  ks_battery_max_cell_voltage = 0;
  ks_battery_max_cell_voltage_no = 0;
  ks_battery_min_cell_voltage = 0;
  ks_battery_min_cell_voltage_no = 0;
  ks_battery_cum_charge_current = 0;
  ks_battery_cum_discharge_current = 0;
  ks_battery_cum_charge = 0;
  ks_battery_cum_discharge = 0;
  memset(ks_battery_cum_op_time, 0, sizeof (ks_battery_cum_op_time));

  memset(ks_battery_cell_voltage, 0, sizeof (ks_battery_cell_voltage));

  ks_battery_min_temperature = 0;
  ks_battery_inlet_temperature = 0;
  ks_battery_max_temperature = 0;
  ks_battery_heat_1_temperature = 0;
  ks_battery_heat_2_temperature = 0;
  ks_battery_max_detoriation = 0;
  ks_battery_max_detoriation_cell_no = 0;
  ks_battery_min_detoriation = 0;
  ks_battery_min_detoriation_cell_no = 0;
  ks_heatsink_temperature = 0;
  ks_charge_bits.ChargingChademo = 0;
  ks_charge_bits.ChargingJ1772 = 0;
  ks_charge_bits.FanStatus = 0;
  ks_battery_fan_feedback = 0;
  ks_sms_bits.NotifyCharge = 0;
  ks_sms_bits.BCV_BlockToFetch = 0;
  ks_sms_bits.BCV_BlockFetched = 1;
  ks_sms_bits.BCV_BlockSent = 1;
  ks_obc_volt = 0;
  ks_shift_bits.value = 0;

  car_ambient_temp = 0;

  memset(ks_tpms_id, 0, sizeof (ks_tpms_id));
  memset(ks_tpms_pressure, 0, sizeof (ks_tpms_pressure));
  memset(ks_tpms_temp, 0, sizeof (ks_tpms_temp));

  ks_send_can.id=0;
  ks_send_can.status=0;
  memset(ks_send_can.byte, 0, sizeof (ks_send_can.byte));

  //
  // Initialize CAN interface:
  // 

  CANCON = 0b10010000;
  while (!CANSTATbits.OPMODE2); // Wait for Configuration mode

  // We are now in Configuration Mode

  // Filters: low byte bits 7..5 are the low 3 bits of the filter, and bits 4..0 are all zeros
  //          high byte bits 7..0 are the high 8 bits of the filter
  //          So total is 11 bits

  // Buffer 0 (filters 0, 1) for OBDII responses
  RXB0CON = 0b00000000;

  RXM0SIDL = 0b00000000; // Mask0   = 111 1111 1000
  RXM0SIDH = 0b11110001;

  RXF0SIDL = 0b00000000; // Filter0 = 111 1110 1000 (0x7[8-E][8-F])
  RXF0SIDH = 0b11110001;

  //RXF1SIDL = 0b00000000; // Filter1 = 111 1111 1000 (0x7F8 .. 0x7FF)
  //RXF1SIDH = 0b11111111; // (should not be triggered)
  RXF1SIDL = 0b00000000;
  RXF1SIDH = 0b11100000; // Filter1 = 111 1111 1000 (0x700 .. 0x777)

  // 0x018, 0x200, 0x433, 0x4f0, 0x4f2, 0x542, 0x581, 0x594

  // Buffer 1 (filters 2, 3, 4 and 5) for direct can bus messages
  RXB1CON = 0b00000000; // RX buffer1 uses Mask RXM1 and filters RXF2, RXF3, RXF4, RXF5

  RXM1SIDL = 0b00000000;
  //  RXM1SIDH = 0b11100000; // Set Mask1 to 111 0000 0000
  RXM1SIDH = 0b11000000; // Set Mask1 to 110 0000 0000

  RXF2SIDL = 0b00000000; // Filter2 = CAN ID 0x600 - 0x7FF
  // RXF2SIDH = 0b10100000;
  RXF2SIDH = 0b11000000;

  RXF3SIDL = 0b00000000; // Filter3 = CAN ID 0x200 - 0x3FF
  RXF3SIDH = 0b01000000;

  RXF4SIDL = 0b00000000; // Filter4 = CAN ID 0x400 - 0x5FF
  RXF4SIDH = 0b10000000;

  RXF5SIDL = 0b00000000; // Filter5 = CAN ID 0x000 - 0x1FF
  RXF5SIDH = 0b00000000;

  // CAN bus baud rate

  BRGCON1 = 0x01; // SET BAUDRATE to 500 Kbps
  BRGCON2 = 0xD2;
  BRGCON3 = 0x02;

  CIOCON = 0b00100000; // CANTX pin will drive VDD when recessive
  CANCON = 0b00000000; // Force Normal mode and CAN_WRITE
  sys_features[FEATURE_CANWRITE] |= 1; // We must be able to write to the CAN

  sys_features[FEATURE_CARBITS] |= FEATURE_CB_SSMSTIME; //Disable SMS-time

  // Hook in...
  vehicle_version = vehicle_kiasoul_version;
  can_capabilities = vehicle_kiasoul_capabilities;

  vehicle_fn_poll0 = &vehicle_kiasoul_poll0;
  vehicle_fn_poll1 = &vehicle_kiasoul_poll1;
  vehicle_fn_ticker1 = &vehicle_kiasoul_ticker1;
  vehicle_fn_smshandler = &vehicle_kiasoul_fn_smshandler;
  vehicle_fn_smsextensions = &vehicle_kiasoul_fn_smsextensions;
  vehicle_fn_commandhandler = &vehicle_kiasoul_fn_commandhandler;

  vehicle_poll_setpidlist(vehicle_kiasoul_polls);
  vehicle_poll_setstate(0);

  net_fnbits |= NET_FN_INTERNALGPS; // Require internal GPS
  net_fnbits |= NET_FN_12VMONITOR; // Require 12v monitor
  net_fnbits |= NET_FN_SOCMONITOR; // Require SOC monitor
  net_fnbits |= NET_FN_CARTIME; // Get real UTC time from modem

  
  return TRUE;
}

