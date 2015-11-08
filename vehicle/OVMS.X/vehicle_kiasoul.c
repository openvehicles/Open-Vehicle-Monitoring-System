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

// Module version:
rom char vehicle_kiasoul_version[] = "0.3";

#define CAN_ADJ -1  // To adjust for the 1 byte difference between the can_databuffer
                    // and the google spreadsheet

////////////////////////////////////////////////////////////////////////////////
// Kia Soul state variables
// 

#pragma udata overlay vehicle_overlay_data

UINT8 ks_busactive;           // indication that bus is active

car_doors1bits_t ks_doors1;   // Main status flags

UINT ks_soc;                  // SOC [1/10 %]
UINT ks_last_soc;
UINT ks_bms_soc;              // BMS SOC [1/10 %]
UINT ks_estrange;             // Estimated range remaining [km]
UINT ks_last_ideal_range;
UINT ks_chargepower;          // in [1/256 kW]
UINT ks_speed;                // in [kph]
UINT ks_speed2;                // in [kph]
UINT ks_auxVoltage;           // Voltage 12V aux battery [1/10 V]

UINT32 ks_odometer;           // Odometer [km]

//UINT8 ks_diag_01[0x3D-3];     // diag data page 01
UINT8 ks_diag_05[0x2C-3];     // diag data page 05

// test/debug:
//UINT8 ks_diag_01_len;
UINT8 ks_diag_05_len;

UINT8 ks_parking_brake_status;

UINT  ks_can_request_id;                             // The ID to request
UINT8 ks_can_request_status;                         // The status of the can request
unsigned char  ks_can_response_databuffer[8];        // The response

UINT8 ks_charge_byte3; //TODO Temporary temperature variables for analysis purposes.
UINT8 ks_charge_byte5;
// Not charging:        byte 3=0         Byte 5=0
// Charging 16A 230V:   byte 3=8 b1000   Byte 5=15 b1111  
// Fast charger:        byte 3=?         Byte 5=?

UINT8 ks_temp_1; //TODO Temporary temperature variables for analysis purposes.
UINT8 ks_temp_2;
UINT8 ks_temp_3;
UINT8 ks_temp_4;
UINT8 ks_temp_5;

UINT8 ks_door_byte; //TODO Temporary door byte for analysis purposes

signed char ks_battery_module_temp[8];     

UINT  ks_battery_DC_voltage;                //DC voltage                    02 21 01 -> 22  2+3
UINT  ks_battery_current;                   //Battery current               02 21 01 -> 21 7+22 1
UINT  ks_battery_avail_charge_power;        //Available charge power        02 21 01 -> 21 2+3
UINT  ks_battery_avail_discharge_power;     //Available discharge power     02 21 01 -> 21 4+5
UINT  ks_battery_max_cell_voltage;          //Max cell voltage              02 21 01 -> 23  6
UINT8 ks_battery_max_cell_voltage_no;       //Max cell voltage no           02 21 01 -> 23  7
UINT  ks_battery_min_cell_voltage;          //Min cell voltage              02 21 01 -> 24 1
UINT8 ks_battery_min_cell_voltage_no;       //Min cell voltage no           02 21 01 -> 24 2

UINT32 ks_battery_acc_charge_current;       //Accumulated charge current    02 21 01 -> 24 6+7 & 25 1+2
UINT32 ks_battery_acc_discharge_current;    //Accumulated discharge current 02 21 01 -> 25 3-6
UINT32 ks_battery_acc_charge_power;         //Accumulated charge power      02 21 01 -> 25 7 + 26 1-3
UINT32 ks_battery_acc_discharge_power;      //Accumulated discharge power   02 21 01 -> 26 4-7
UINT32 ks_battery_acc_op_time;              //Accumulated operating time    02 21 01 -> 27 1-4

UINT8 ks_battery_cell_voltage[96];
//181 bytes
UINT8 ks_battery_soh;                       

UINT8 ks_battery_min_temperature;           //02 21 05 -> 21 7 
UINT8 ks_battery_inlet_temperature;         //02 21 05 -> 21 6 
UINT8 ks_battery_max_temperature;           //02 21 05 -> 22 1 
UINT8 ks_air_bag_hwire_duty;                //02 21 05 -> 23 5
UINT8 ks_battery_heat_1_temperature;        //02 21 05 -> 23 6
UINT8 ks_battery_heat_2_temperature;        //02 21 05 -> 23 7
UINT  ks_battery_max_detoriation;           //02 21 05 -> 24 1+2
UINT8 ks_battery_max_detoriation_cell_no;   //02 21 05 -> 24 3
UINT  ks_battery_min_detoriation;           //02 21 05 -> 24 4+5
UINT8 ks_battery_min_detoriation_cell_no;   //02 21 05 -> 24 6

UINT8 ks_motor_temperature;                 
UINT8 ks_mcu_temperature; 
UINT8 ks_heatsink_temperature;

#pragma udata

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
//      i.e. 0=parking, 1=driving
//

rom vehicle_pid_t vehicle_kiasoul_polls[] = 
{
  { 0x7e2, 0, VEHICLE_POLL_TYPE_OBDIIVEHICLE, 0x02, {  120, 120, 0 } }, // VIN
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIISESSION, 0x90, {  60, 10, 0 } }, // Start diag session
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x01, {  60, 10, 0 } }, // Diag page 01
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x02, {  60, 10, 0 } }, // Diag page 02
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x03, {  60, 10, 0 } }, // Diag page 03
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x04, {  60, 10, 0 } }, // Diag page 04
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x05, {  60, 10, 0 } }, // Diag page 05
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIISESSION, 0x00, {  60, 10, 0 } }, // End diag session (?)
 
  /*{ 0x7e4, 0x7ea, VEHICLE_POLL_TYPE_OBDIISESSION, 0x90, {  60, 10, 0 } }, // Start diag session
  { 0x7e4, 0x7ea, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x02, {  60, 10, 0 } }, // Diag page 01
  { 0x7e4, 0x7ea, VEHICLE_POLL_TYPE_OBDIISESSION, 0x00, {  60, 10, 0 } }, // End diag session (?)
  */
  { 0, 0, 0x00, 0x00, { 0, 0, 0 } }
};

// ISR optimization, see http://www.xargs.com/pic/c18-isr-optim.pdf
#pragma tmpdata high_isr_tmpdata


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

BOOL vehicle_kiasoul_poll0(void)
  {
  UINT8 i;
  UINT base;

  // process OBDII data:
  switch (can_id)
    {
    
    case 0x7ea:
      switch (vehicle_poll_pid)
        {
        case 0x02:
          // VIN (multi-line response):
          // VIN length is 20 on Kia => skip first frame (3 bytes):
          if (vehicle_poll_ml_frame > 0)
          {
            base = vehicle_poll_ml_offset - can_datalength - 3;
            for (i=0; (i<can_datalength) && ((base+i)<(sizeof(car_vin)-1)); i++)
              car_vin[base+i] = can_databuffer[i];
            if (vehicle_poll_ml_remain == 0)
              car_vin[base+i] = 0;
          }
          else if(vehicle_poll_ml_frame==3){
              ks_motor_temperature = can_databuffer[5+CAN_ADJ];     //TODO Not correct?
              ks_mcu_temperature = can_databuffer[6+CAN_ADJ];       //TODO Not correct?
              ks_heatsink_temperature = can_databuffer[7+CAN_ADJ];  //TODO Not correct?
              car_stale_temps=120;
          }
          break;
        }
      break;
  
    case 0x7ec:
      switch (vehicle_poll_pid)
        {
        case 0x01:
          // diag page 01: skip first frame (no data)
          if (vehicle_poll_ml_frame > 0)
            {
            if(vehicle_poll_ml_frame==1) // 02 21 01 - 21
               {                
               ks_battery_avail_charge_power     = (UINT)can_databuffer[3+CAN_ADJ] + ((UINT)can_databuffer[2+CAN_ADJ]<<8); 
               ks_battery_avail_discharge_power  = (UINT)can_databuffer[5+CAN_ADJ] + ((UINT)can_databuffer[4+CAN_ADJ]<<8);
               //TODO Probably not correct
               ks_battery_current                = (ks_battery_current & 0x00FF) + ((UINT)can_databuffer[7+CAN_ADJ]<<8);
               }
            if(vehicle_poll_ml_frame==2) // 02 21 01 - 22
               {                
               ks_battery_current       = (ks_battery_current & 0xFF00) + (UINT)can_databuffer[1+CAN_ADJ];
               ks_battery_DC_voltage    = (UINT)can_databuffer[3+CAN_ADJ] + ((UINT)can_databuffer[2+CAN_ADJ]<<8); 
               ks_battery_module_temp[0] = can_databuffer[4+CAN_ADJ];  
               ks_battery_module_temp[1] = can_databuffer[5+CAN_ADJ]; 
               ks_battery_module_temp[2] = can_databuffer[6+CAN_ADJ]; 
               ks_battery_module_temp[3] = can_databuffer[7+CAN_ADJ]; 
               }
            else if(vehicle_poll_ml_frame==3)  // 02 21 01 - 23
               {
               ks_battery_module_temp[4] = can_databuffer[1+CAN_ADJ]; 
               ks_battery_module_temp[5] = can_databuffer[2+CAN_ADJ];  
               ks_battery_module_temp[6] = can_databuffer[3+CAN_ADJ];  
               ks_battery_module_temp[7] = can_databuffer[5+CAN_ADJ];  
               car_stale_temps = 120; // Reset stale indicator 

               ks_battery_max_cell_voltage     = ((UINT)can_databuffer[6+CAN_ADJ])*2;
               ks_battery_max_cell_voltage_no  = can_databuffer[7+CAN_ADJ];
               }
            else if(vehicle_poll_ml_frame==4)   // 02 21 01 - 24
               {
               ks_battery_min_cell_voltage    = ((UINT)can_databuffer[1+CAN_ADJ])*2;
               ks_battery_min_cell_voltage_no = can_databuffer[2+CAN_ADJ];
               //TODO Probably not correct
               ks_battery_acc_charge_current  = (ks_battery_acc_charge_current & 0x0000FFFF) + ((UINT32)can_databuffer[6+CAN_ADJ]<<24) + ((UINT32)can_databuffer[7+CAN_ADJ]<<16);
               
               ks_auxVoltage = can_databuffer[5+CAN_ADJ]; 
               }
            else if(vehicle_poll_ml_frame==5)   // 02 21 01 - 25
               {
               ks_battery_acc_charge_current    = (ks_battery_acc_charge_current &0xFFFF0000) + ((UINT32)can_databuffer[1+CAN_ADJ]<<8) + ((UINT32)can_databuffer[2+CAN_ADJ]);
               ks_battery_acc_discharge_current = (UINT32)can_databuffer[6+CAN_ADJ] + ((UINT32)can_databuffer[5+CAN_ADJ]<<8) + ((UINT32)can_databuffer[4+CAN_ADJ]<<16) + ((UINT32)can_databuffer[3+CAN_ADJ]<<24);
               ks_battery_acc_charge_power      = (ks_battery_acc_charge_power &0xFFFFFF) + ((UINT32)can_databuffer[7+CAN_ADJ])<<24;
               }
            else if(vehicle_poll_ml_frame==6)   // 02 21 01 - 26
               {
               ks_battery_acc_charge_power    = (ks_battery_acc_charge_power &0xFF000000) + ((UINT32)can_databuffer[1+CAN_ADJ]<<16) + ((UINT32)can_databuffer[2+CAN_ADJ]<<8) + ((UINT32)can_databuffer[3+CAN_ADJ]);
               ks_battery_acc_discharge_power = (UINT32)can_databuffer[7+CAN_ADJ] + ((UINT32)can_databuffer[6+CAN_ADJ]<<8) + ((UINT32)can_databuffer[5+CAN_ADJ]<<16) + ((UINT32)can_databuffer[4+CAN_ADJ]<<24);
               }
            else if(vehicle_poll_ml_frame==7)   // 02 21 01 - 27
               {
               ks_battery_acc_op_time = (UINT32)can_databuffer[4+CAN_ADJ] + ((UINT32)can_databuffer[3+CAN_ADJ]<<8) + ((UINT32)can_databuffer[2+CAN_ADJ]<<16) + ((UINT32)can_databuffer[1+CAN_ADJ]<<24);
               }
          }
          break;

        case 0x02: //Cell voltages
        case 0x03:
        case 0x04:
          // diag page 02-04: skip first frame (no data)
          if (vehicle_poll_ml_frame > 0)
          {
            base = ((vehicle_poll_pid-2)<<6) + vehicle_poll_ml_offset - can_datalength - 3;
            for (i=0; i<can_datalength && ((base+i)<sizeof(ks_battery_cell_voltage)); i++)
                ks_battery_cell_voltage[base+i] = can_databuffer[i];
          }
          break;
          
        case 0x05:
          // diag page 05: skip first frame (no data)
          if (vehicle_poll_ml_frame > 0)
            {
            base = vehicle_poll_ml_offset - can_datalength - 3;
            for (i=0; (i<can_datalength) && ((base+i)<sizeof(ks_diag_05)); i++)
              ks_diag_05[base+i] = can_databuffer[i];
            ks_diag_05_len = vehicle_poll_ml_offset;
            }
          if( vehicle_poll_ml_frame==1){
            ks_battery_inlet_temperature = can_databuffer[6+CAN_ADJ]; 
            ks_battery_min_temperature = can_databuffer[7+CAN_ADJ];                 
          }else if( vehicle_poll_ml_frame==2){
            ks_battery_max_temperature = can_databuffer[1+CAN_ADJ];
          }else if( vehicle_poll_ml_frame==3){
            ks_air_bag_hwire_duty = can_databuffer[5+CAN_ADJ];
            ks_battery_heat_1_temperature = can_databuffer[6+CAN_ADJ];
            ks_battery_heat_2_temperature = can_databuffer[7+CAN_ADJ];
          }else if( vehicle_poll_ml_frame==4){
            ks_battery_max_detoriation = (UINT)can_databuffer[2+CAN_ADJ] + ((UINT)can_databuffer[1+CAN_ADJ]<<8);
            ks_battery_max_detoriation_cell_no = can_databuffer[3+CAN_ADJ];
            ks_battery_min_detoriation = (UINT)can_databuffer[5+CAN_ADJ] + ((UINT)can_databuffer[4+CAN_ADJ]<<8);
            ks_battery_min_detoriation_cell_no = can_databuffer[6+CAN_ADJ];
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

BOOL vehicle_kiasoul_poll1(void)
  {
  UINT val1;
  int i;
  
  // reset bus activity timeout:
  ks_busactive = KS_BUS_TIMEOUT;

  // process CAN data:
  switch (can_id)
    {
    
    case 0x018:
      // Car is ON:
      ks_doors1.CarON = 1;
      //ks_doors1.Charging = 0;
      // Doors status:
      ks_doors1.FrontLeftDoor = ((CAN_BYTE(0) & 0x10) > 0);
      ks_doors1.FrontRightDoor = ((CAN_BYTE(0) & 0x80) > 0);
      
      ks_door_byte = CAN_BYTE(0);
      break;
      
    case 0x200:
      // Estimated range:
      val1 = (UINT)CAN_BYTE(2) << 1;
      if (val1 > 0)
        ks_estrange = val1;
      break;
      
    case 0x433:
       // Parking brake status 
       ks_parking_brake_status = (CAN_BYTE(2)>>4) & 0x1;
       break;
       
    case 0x4f0:
       // Odometer:
       ks_odometer = ((UINT32)CAN_BYTE(7) << 16) + ((UINT32)CAN_BYTE(6) << 8) + (UINT32)CAN_BYTE(5);
       ks_speed2 = ((UINT)CAN_BYTE(1) << 1) + ((CAN_BYTE(2) & 0x80) >> 7);
       break;
    
    case 0x4f2:
       // Speed:
       ks_speed = ((UINT)CAN_BYTE(1) << 1) + ((CAN_BYTE(2) & 0x80) >> 7);
       break; 
      
    case 0x542:
      // SOC:
      val1 = (UINT)CAN_BYTE(0) * 5;
      if (val1 > 0)
         ks_soc = val1;
      break;
  
    case 0x581:
      // Car is CHARGING:
      ks_doors1.CarON = 0;
      ks_doors1.Charging = (CAN_BYTE(3) != 0);
      ks_chargepower = ((UINT)CAN_BYTE(7) << 8) + CAN_BYTE(6); 
      ks_charge_byte3 = CAN_BYTE(3);
      ks_charge_byte5 = CAN_BYTE(5);
      break;
      
    case 0x594:
      // BMS SOC:
      val1 = (UINT)CAN_BYTE(5) * 5 + CAN_BYTE(6);
      if (val1 > 0)
        ks_bms_soc = val1;
      break;
      
    case 0x595:
      // Possible temp value 1
      ks_temp_1 = CAN_BYTE(0);
      break;

    case 0x596:
      // Possible temp value 2 and 3
      ks_temp_2 = CAN_BYTE(6);
      ks_temp_3 = CAN_BYTE(7);
      break;

    case 0x598:
      // Possible temp value 5 and 7
      ks_temp_4 = CAN_BYTE(6);
      ks_temp_5 = CAN_BYTE(7);
      break;
  
    }
  
  // Fetch requested can id, as specified via QRY sms.
  if( ks_can_request_status==1 ){
      if( can_id == ks_can_request_id  ){        
        for(i=0; i<8; i++){
            ks_can_response_databuffer[i]=CAN_BYTE(i);
        }
        ks_can_request_status=2;
      }
  }
  
  return TRUE;
  }

#pragma tmpdata

////////////////////////////////////////////////////////////////////////
// vehicle_Kia Soul EV get_maxrange: get MAXRANGE with temperature compensation
//

int vehicle_kiasoul_get_maxrange(void)
{
  int maxRange;
  
  // Read user configuration:
  maxRange = sys_features[FEATURE_MAXRANGE];

  // Temperature compensation:
  //   - assumes standard maxRange specified at 20°C
  //   - temperature influence approximation: 0.6 km / °C
  //TODO Should find our own data for the KIA. These values are for the Twizy.
  if (maxRange != 0)
    maxRange -= ((20 - car_tbattery) * 6) / 10;
  
  return maxRange;
}

void vehicle_kiasoul_query_sms_response(void){
    char *s;
    int i;
 
    // Start SMS:
    net_send_sms_start(par_get(PARAM_REGPHONE));

    // Format SMS:
    s = net_scratchpad;

    // output 
    s = stp_x(s, "Can: 0x", ks_can_request_id);
    s = stp_rom(s, "\nHex: ");
    for (i=0; i<8; i++)
      s = stp_sx(s, " ", ks_can_response_databuffer[i]);

    s = stp_rom(s, "\nDec: ");
    for (i=0; i<8; i++)
      s = stp_i(s, " ", ks_can_response_databuffer[i]);

    // Send SMS:
    net_puts_ram(net_scratchpad);
    net_send_sms_finish();
    ks_can_request_status=0;
}


////////////////////////////////////////////////////////////////////////////////
// Framework hook: _ticker1 (called about once per second)
// 
// Kia Soul: 
// 

BOOL vehicle_kiasoul_ticker1(void)
  {
  int maxRange, suffSOC, suffRange;
  // 
  // Check CAN bus activity timeout:
  //
  
  if (ks_busactive > 0)
    ks_busactive--;
  
  if (ks_busactive == 0)
    {
    // CAN bus is inactive, switch OFF:
    ks_doors1.CarON = 0;
    ks_doors1.Charging = 0;
    ks_chargepower = 0;
    ks_speed = 0;
    }
  
  /***************************************************************************
   * Read feature configuration:
   */

  suffSOC   = sys_features[FEATURE_SUFFSOC];
  suffRange = sys_features[FEATURE_SUFFRANGE];
  maxRange  = vehicle_kiasoul_get_maxrange();
  
  // 
  // Convert KS status data to OVMS framework:
  //
  
  car_SOC = ks_soc / 10; // no rounding to avoid 99.5 = 100
  
  car_estrange = MiFromKm(ks_estrange);

  if (maxRange > 0)
     car_idealrange = MiFromKm((((float) maxRange) * ks_soc) / 1000);
  else
     car_idealrange = car_estrange;
  
  car_odometer = MiFromKm(ks_odometer); 

  if (can_mileskm == 'M')
    car_speed = MiFromKm(ks_speed); // mph
  else
    car_speed = ks_speed; // kph
  
  car_tbattery = ((signed int)ks_battery_module_temp[0] + (signed int)ks_battery_module_temp[1] + 
                  (signed int)ks_battery_module_temp[2] + (signed int)ks_battery_module_temp[3] + 
                  (signed int)ks_battery_module_temp[4] + (signed int)ks_battery_module_temp[5] + 
                  (signed int)ks_battery_module_temp[6] + (signed int)ks_battery_module_temp[7]) / 8;
  
  car_tmotor = ks_motor_temperature;
  
  car_tpem = ks_mcu_temperature;
  
  //
  // Check for car status changes:
  //
  
  car_doors1bits.FrontLeftDoor = ks_doors1.FrontLeftDoor;
  car_doors1bits.FrontRightDoor = ks_doors1.FrontRightDoor;
  
  if (ks_doors1.CarON)
    {
    // Car is ON
    //vehicle_poll_setstate(1);
    if(!ks_doors1.Charging)  
        car_doors1bits.ChargePort = 0;
    car_doors1bits.HandBrake = ks_parking_brake_status;
    car_doors1bits.CarON = 1;
    if (car_parktime != 0)
      {
      car_parktime = 0; // No longer parking
      net_req_notification(NET_NOTIFY_ENV);
      }
    }
  else
    {
    // Car is OFF
    //vehicle_poll_setstate(0);
    car_doors1bits.HandBrake = ks_parking_brake_status;
    car_doors1bits.CarON = 0;
    car_speed = 0;
    if (car_parktime == 0)
      {
      car_parktime = car_time-1; // Record it as 1 second ago, so non zero report
      net_req_notification(NET_NOTIFY_ENV);
      }
    }
  
  
  //
  // Check for charging status changes:
  //
  if (ks_doors1.Charging)
    {
    if (!car_doors1bits.Charging)
      {
      // Charging started:
      car_doors1bits.PilotSignal = 1;
      car_doors1bits.ChargePort = 1;
      car_doors1bits.Charging = 1;
      car_doors5bits.Charging12V = 1;
      car_chargeduration = 0; // Reset charge duration
      car_chargestate = 1; // Charging state
      car_chargemode = 0; //Standard charging. 
      net_req_notification(NET_NOTIFY_CHARGE);
      net_req_notification(NET_NOTIFY_STAT);
      }
    else
      {
      // Charging continues:
      car_chargeduration++;

      if (
        ((car_SOC > 0) && (suffSOC > 0) && (car_SOC >= suffSOC) && (ks_last_soc < suffSOC))
        ||
        ((ks_estrange > 0) && (suffRange > 0) && (car_idealrange >= suffRange) && (ks_last_ideal_range < suffRange))
        )
      {
        // ...enter state 2=topping off:
        car_chargestate = 2;

        // ...send charge alert:
        net_req_notification(NET_NOTIFY_CHARGE);
        net_req_notification(NET_NOTIFY_STAT);
      }      
      // ...else set "topping off" from 94% SOC:
      else if (car_SOC >= 95)
        {
        car_chargestate = 2; // Topping off
        net_req_notification(NET_NOTIFY_ENV);
        }
      }
      
      //Setting "virtual" charge mode 
      car_linevoltage = 230;    //TODO Can we find the correct voltage somewhere?
      if( ks_chargepower < 600 ){        //Roughly 10A
        car_chargemode = 0;              //Standard charging      
      }else if( ks_chargepower < 1900 ){ //Roughly 32A
        car_chargemode = 3;              //Range      
      }else{             
        car_linevoltage = 400;    //TODO Can we find the correct voltage somewhere?
        car_chargemode = 4;       //Performance      
      }
      car_chargecurrent = (((long)ks_chargepower*1000)>>8)/car_linevoltage; 
      
      //Calculate remaining charge time
      //TODO If CA is sat, we should calculate for CA and not total
      car_chargefull_minsremaining = (60*27000-(60*27000*(long)ks_soc/1000))/(((long)ks_chargepower*1000)>>8);                  
      // car_chargefull_minsremaining = ((float)((2700-(27*car_SOC))*60))/(((float)ks_chargepower)/(float)256);
      
      ks_last_soc = car_SOC;
      ks_last_ideal_range = car_idealrange;
    }
  else if (car_doors1bits.Charging)
    {
    // Charge completed or interrupted:
    car_doors1bits.PilotSignal = 0;
    car_doors1bits.Charging = 0;
    car_doors5bits.Charging12V = 0;
    car_chargecurrent = 0;
    if (car_SOC == 100)
      car_chargestate = 4; // Charge DONE
    else
      car_chargestate = 21; // Charge STOPPED
    net_req_notification(NET_NOTIFY_CHARGE);
    net_req_notification(NET_NOTIFY_STAT);
    }
  
  // Check if we have a can-message as a response to a QRY sms
  if( ks_can_request_status==2 ){
     vehicle_kiasoul_query_sms_response();
   }
  
  }

////////////////////////////////////////////////////////////////////////
// Vehicle specific SMS command: DEBUG
//

BOOL vehicle_kiasoul_debug_sms(BOOL premsg, char *caller, char *command, char *arguments)
  {
  char *s;
  int i;
  
  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  if (!caller || !*caller)
    {
    caller = par_get(PARAM_REGPHONE);
    if (!caller || !*caller)
      return FALSE;
    }

  // Start SMS:
  if (premsg)
    net_send_sms_start(caller);

  // Format SMS:
  s = net_scratchpad;

  if (arguments && (arguments[0]=='1'))
    {
    // output ks_diag_01:
    //s = stp_i(s, "DIAG_01:", ks_diag_01_len);
    //for (i=0; i<sizeof(ks_diag_01); i++)
    //  s = stp_sx(s, (i%7)?"":"\n", ks_diag_01[i]);
    }
  else if (arguments && (arguments[0]=='5'))
    {
    // output ks_diag_05:
    s = stp_i(s, "DIAG_05:", ks_diag_05_len);
    for (i=0; i<sizeof(ks_diag_05); i++)
      s = stp_sx(s, (i%7)?"":"\n", ks_diag_05[i]);
    }
  else
    {
    // output status vars:
    s = stp_rom(s, "VARS:");
    s = stp_i(s, "\ndoors=", ks_door_byte);
    s = stp_i(s, "\nsoc=", ks_soc);
    s = stp_i(s, "\nsoc2=", ks_bms_soc);
    s = stp_i(s, "\nestr=", ks_estrange);
    s = stp_i(s, "\nchgep=", ks_chargepower);
    s = stp_i(s, "\nspeed=", ks_speed);
    s = stp_i(s, "\nspeed2=", ks_speed2);
    s = stp_i(s, "\nparkbr=", ks_parking_brake_status);
    s = stp_l2f(s, "\n12v1=", (long)ks_auxVoltage,1);
    s = stp_l2f(s, "\n12v2=", (long)car_12vline,1);
    s = stp_i(s, "\nchg b3=", ks_charge_byte3);
    s = stp_i(s, "\nchg b5=", ks_charge_byte5);
    s = stp_i(s, "\nt=", ks_temp_1);
    s = stp_i(s, ", ", ks_temp_2);
    s = stp_i(s, ", ", ks_temp_3);
    s = stp_i(s, ", ", ks_temp_4);
    s = stp_i(s, ", ", ks_temp_5);
    }
  
  // Send SMS:
  net_puts_ram(net_scratchpad);

  return TRUE;
  }

////////////////////////////////////////////////////////////////////////
// Vehicle specific SMS command: BATT
//

BOOL vehicle_kiasoul_battery_sms(BOOL premsg, char *caller, char *command, char *arguments)
  {
  char *s;
  int i;
  
  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  if (!caller || !*caller)
    {
    caller = par_get(PARAM_REGPHONE);
    if (!caller || !*caller)
      return FALSE;
    }

  // Start SMS:
  if (premsg)
    net_send_sms_start(caller);

  // Format SMS:
  s = net_scratchpad;

  s = stp_rom(s, "Battery"); 
  s = stp_l2f(s, "\n", ks_battery_DC_voltage,1);
  s = stp_l2f(s, "V ", ks_battery_current,1);
  s = stp_l2f(s, "A\nAvC", ks_battery_avail_charge_power,2);
  s = stp_l2f(s, "kW\nAvD", ks_battery_avail_discharge_power,2);
  s = stp_l2f(s, "kW\nC", ks_battery_max_cell_voltage,2);
  s = stp_i(s, "V#", ks_battery_max_cell_voltage_no);
  s = stp_l2f(s, " ", ks_battery_min_cell_voltage,2);
  s = stp_i(s, "V#", ks_battery_min_cell_voltage_no);
  s = stp_l2f(s, "\nAC", ks_battery_acc_charge_current,2);
  s = stp_l2f(s, "Ah\nAD", ks_battery_acc_discharge_current,2);
  s = stp_l2f(s, "Ah\nAcC", ks_battery_acc_charge_power,2);
  s = stp_l2f(s, "kWh\nAcD", ks_battery_acc_discharge_power,2);
  s = stp_l(s, "kWh\nAcOT", ks_battery_acc_op_time/3600);
  s = stp_l2f(s, "h\nMDet", ks_battery_max_detoriation,2);
  s = stp_i(s, "%#", ks_battery_max_detoriation_cell_no);
  s = stp_l2f(s, "\nmDet", ks_battery_min_detoriation,2);
  s = stp_i(s, "%#", ks_battery_min_detoriation_cell_no);
  s = stp_i(s, "\nABHD", ks_air_bag_hwire_duty);
  s = stp_rom(s, "%"); 

  // Send SMS:
  net_puts_ram(net_scratchpad);

  return TRUE;
  }

////////////////////////////////////////////////////////////////////////
// Vehicle specific SMS command: BTMP
//

BOOL vehicle_kiasoul_battery_temp_sms(BOOL premsg, char *caller, char *command, char *arguments)
  {
  char *s;
  int i;
  
  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  if (!caller || !*caller)
    {
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
  for(i=0; i<8; i++){
    s = stp_i(s, "#", (i+1));
    s = stp_i(s, "=", ks_battery_module_temp[i]);
    s = stp_rom(s, ((i+1)%4)?"C ":"C\n");
  }  
  s = stp_i(s, "Max", ks_battery_max_temperature);
  s = stp_i(s, "C Min", ks_battery_min_temperature);
  s = stp_i(s, "C\nInlet=", ks_battery_inlet_temperature);
  s = stp_i(s, "C\nHeat1=", ks_battery_heat_1_temperature);
  s = stp_i(s, "C 2=", ks_battery_heat_2_temperature);
  s = stp_rom(s, "C\0");

  // Send SMS:
  net_puts_ram(net_scratchpad);
  return TRUE;
  }

////////////////////////////////////////////////////////////////////////
// Vehicle specific SMS command: BCV
//
void vehicle_kiasoul_battey_cells_sms_response(BOOL premsg, char *caller, int i, int to){
  char *s;
  
  // Start SMS:
  if (premsg)
    net_send_sms_start(caller);

  // Format SMS:
  s = net_scratchpad;
  s = stp_rom(s, "Cell V");
  for(; i<to && i<96; i++){
    if((i%5)==0){
        s = stp_ulp(s, "\n", (i+1),2,'0');        
    }  
    s = stp_l2f(s, " ", ((UINT)ks_battery_cell_voltage[i]<<1),2);
  }  
  // Send SMS:
  net_puts_ram(net_scratchpad);   
  net_send_sms_finish();
  delay100(5);
}

BOOL vehicle_kiasoul_battery_cells_sms(BOOL premsg, char *caller, char *command, char *arguments)
{
  UINT8 start=0;  
  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  if (!caller || !*caller)
  {
    caller = par_get(PARAM_REGPHONE);
    if (!caller || !*caller)
      return FALSE;
  }
  
  if( arguments && arguments[0]>='0' && arguments[0]<='3'){
      start=(arguments[0]-'0')*25;
  }
  vehicle_kiasoul_battey_cells_sms_response(premsg, caller, start, start+25);

  return TRUE;
}

////////////////////////////////////////////////////////////////////////
// Vehicle specific SMS command: RANGE
// Usage: RANGE? Get current range
//        RANGE 12334 Set new maximum range 

BOOL vehicle_kiasoul_range_sms(BOOL premsg, char *caller, char *command, char *arguments)
  {
  char *s;
  int i;
  
  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  if (!caller || !*caller)
    {
    caller = par_get(PARAM_REGPHONE);
    if (!caller || !*caller)
      return FALSE;
    }

  // Start SMS:
  if (premsg)
    net_send_sms_start(caller);

  // Format SMS:
  s = net_scratchpad;

  if (arguments && *arguments)
  {
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

BOOL vehicle_kiasoul_charge_alert_sms(BOOL premsg, char *caller, char *command, char *arguments)
  {
  char *s;
  int i;
  //TODO IKKE IMPLEMENTERT ENNÅ
  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  if (!caller || !*caller)
    {
    caller = par_get(PARAM_REGPHONE);
    if (!caller || !*caller)
      return FALSE;
    }

  // Start SMS:
  if (premsg)
    net_send_sms_start(caller);

  // Format SMS:
  s = net_scratchpad;

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
  
  //TODO vehicle_kiasoul_calculate_chargetimes();
  
  s = stp_rom(net_scratchpad, "Charge Alert: ");

  if ((sys_features[FEATURE_SUFFSOC] == 0) && (sys_features[FEATURE_SUFFRANGE] == 0))
  {
    s = stp_rom(s, "OFF");
  }
  else
  {
    if (sys_features[FEATURE_SUFFSOC] > 0)
    {
      // output SUFFSOC:
      s = stp_i(s, NULL, sys_features[FEATURE_SUFFSOC]);
      s = stp_rom(s, "%");
    }

    if (sys_features[FEATURE_SUFFRANGE] > 0)
    {
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
  }
  else
  {
    // Charge port door is closed, not charging
    s = stp_rom(s, "Not charging");
  }


  // Estimated + Ideal Range:
  if (can_mileskm == 'M')
  {
    s = stp_i(s, "\n Range: ", car_estrange);
    s = stp_i(s, " - ", car_idealrange);
    s = stp_rom(s, " mi");
  }
  else
  {
    s = stp_i(s, "\n Range: ", KmFromMi(car_estrange));
    s = stp_i(s, " - ", KmFromMi(car_idealrange));
    s = stp_rom(s, " km");
  }

  // SOC:
  s = stp_l2f(s, "\n SOC: ", ks_soc, 1);
  s = stp_rom(s, "%");


  // Send SMS:
  net_puts_ram(net_scratchpad);

  return TRUE;
  }

////////////////////////////////////////////////////////////////////////
// Vehicle specific SMS command: QRY
//

BOOL vehicle_kiasoul_query_sms(BOOL premsg, char *caller, char *command, char *arguments)
  {
  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  if (arguments && *arguments)
  {
    ks_can_request_id = ((UINT)((arguments[0] >= 'A') ? (arguments[0] - 'A' + 10) : (arguments[0] - '0'))<<8)+
                        (((arguments[1] >= 'A') ? (UINT)(arguments[1] - 'A' + 10) : (UINT)(arguments[1] - '0'))<<4)+
                        (((arguments[2] >= 'A') ? (UINT32)(arguments[2] - 'A' + 10) : (UINT)(arguments[2] - '0')));

    ks_can_request_status = 1;
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

rom char vehicle_kiasoul_sms_cmdtable[][NET_SMS_CMDWIDTH] =
  {
  "3DEBUG",       // Output debug data
  "3HELP",        // extend HELP output
  "3RANGE",       // Range setting
  "3CA",          // Charge Alert
  "3QRY",         // Query CAN  
  "3BTMP",        // Battery temp
  "3BATT",        // Battery info
  "3BCV",         // Battery cell voltages
  ""
  };

rom far BOOL(*vehicle_kiasoul_sms_hfntable[])(BOOL premsg, char *caller, char *command, char *arguments) =
  {
  &vehicle_kiasoul_debug_sms,
  &vehicle_kiasoul_help_sms,
  &vehicle_kiasoul_range_sms,
  &vehicle_kiasoul_charge_alert_sms,
  &vehicle_kiasoul_query_sms,
  &vehicle_kiasoul_battery_temp_sms,
  &vehicle_kiasoul_battery_sms,
  &vehicle_kiasoul_battery_cells_sms
};


// SMS COMMAND DISPATCHERS:
// premsg: TRUE=may replace, FALSE=may extend standard handler
// returns TRUE if handled

BOOL vehicle_kiasoul_fn_smshandler(BOOL premsg, char *caller, char *command, char *arguments)
  {
  // called to extend/replace standard command: framework did auth check for us

  UINT8 k;

  // Command parsing...
  for (k = 0; vehicle_kiasoul_sms_cmdtable[k][0] != 0; k++)
    {
    if (starts_with(command, (char const rom far*)vehicle_kiasoul_sms_cmdtable[k] + 1))
      {
      // Call sms handler:
      k = (*vehicle_kiasoul_sms_hfntable[k])(premsg, caller, command, arguments);

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

BOOL vehicle_kiasoul_fn_smsextensions(char *caller, char *command, char *arguments)
  {
  // called for specific command: we need to do the auth check

  UINT8 k;

  // Command parsing...
  for (k = 0; vehicle_kiasoul_sms_cmdtable[k][0] != 0; k++)
    {
    if (starts_with(command, (char const rom far*)vehicle_kiasoul_sms_cmdtable[k] + 1))
      {
      // we need to check the caller authorization:
      arguments = net_sms_initargs(arguments);
      if (!net_sms_checkauth(vehicle_kiasoul_sms_cmdtable[k][0], caller, &arguments))
        return FALSE; // failed

      // Call sms handler:
      k = (*vehicle_kiasoul_sms_hfntable[k])(TRUE, caller, command, arguments);

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
// Vehicle specific SMS command output extension: HELP
//

BOOL vehicle_kiasoul_help_sms(BOOL premsg, char *caller, char *command, char *arguments)
  {
  int k;

  //if (premsg)
  //  return FALSE; // run only in extend mode

  if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
    return FALSE;

  net_puts_rom("\nKIA:");

  for (k = 0; vehicle_kiasoul_sms_cmdtable[k][0] != 0; k++)
    {
    net_puts_rom(" ");
    net_puts_rom(vehicle_kiasoul_sms_cmdtable[k] + 1);
    }

  return TRUE;
  }


////////////////////////////////////////////////////////////////////////////////
// Framework hook: _initialise (initialise vars & CAN interface)
// 
// Kia Soul: 
// 

BOOL vehicle_kiasoul_initialise(void)
  {

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
  
  ks_soc = 500; // avoid low SOC alerts
  ks_last_soc = 500;
  
  car_12vline_ref = 122; //TODO What should this value be? 

  ks_estrange = 0;
  ks_chargepower = 0;
  
  ks_speed = 0;
  
  ks_parking_brake_status = 0;
  
  //memset(ks_diag_01, 0, sizeof(ks_diag_01));
  memset(ks_diag_05, 0, sizeof(ks_diag_05));

  //ks_diag_01_len = 0;
  ks_diag_05_len = 0;
  
  ks_temp_1 = 20;
  ks_temp_2 = 20;
  ks_temp_3 = 20;
  ks_temp_4 = 20;
  ks_temp_5 = 20;
  
  ks_can_request_id = 0; 
  ks_can_request_status = 0; 

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

  RXM0SIDL = 0b00000000;        // Mask0   = 111 1111 1000
  RXM0SIDH = 0b11111111;

  RXF0SIDL = 0b00000000;        // Filter0 = 111 1110 1000 (0x7E8 .. 0x7EF)
  RXF0SIDH = 0b11111101;        // (OBDII responses)

  RXF1SIDL = 0b00000000;        // Filter1 = 111 1111 1000 (0x7F8 .. 0x7FF)
  RXF1SIDH = 0b11111111;        // (should not be triggered)
  
  
  // Buffer 1 (filters 2, 3, 4 and 5) for direct can bus messages
  RXB1CON  = 0b00000000;	// RX buffer1 uses Mask RXM1 and filters RXF2, RXF3, RXF4, RXF5

  RXM1SIDL = 0b00000000;
  RXM1SIDH = 0b11100000;	// Set Mask1 to 111 0000 0000

  RXF2SIDL = 0b00000000;	// Filter2 = CAN ID 0x500 - 0x5FF
  RXF2SIDH = 0b10100000;

  RXF3SIDL = 0b00000000;	// Filter3 = CAN ID 0x200 - 0x2FF
  RXF3SIDH = 0b01000000;

  RXF4SIDL = 0b00000000;        // Filter4 = CAN ID 0x400 - 0x4FF
  RXF4SIDH = 0b10000000;

  RXF5SIDL = 0b00000000;        // Filter5 = CAN ID 0x000 - 0x0FF
  RXF5SIDH = 0b00000000;

  // CAN bus baud rate

  BRGCON1 = 0x01; // SET BAUDRATE to 500 Kbps
  BRGCON2 = 0xD2;
  BRGCON3 = 0x02;

  CIOCON = 0b00100000; // CANTX pin will drive VDD when recessive
  if (sys_features[FEATURE_CANWRITE]>0)
    {
    CANCON = 0b00000000;  // Normal mode
    }
  else
    {
    CANCON = 0b01100000; // Listen only mode, Receive bufer 0
    }

  // Hook in...
  
  vehicle_version = vehicle_kiasoul_version;
  
  vehicle_fn_poll0 = &vehicle_kiasoul_poll0;
  vehicle_fn_poll1 = &vehicle_kiasoul_poll1;
  vehicle_fn_ticker1 = &vehicle_kiasoul_ticker1;
  vehicle_fn_smshandler = &vehicle_kiasoul_fn_smshandler;
  vehicle_fn_smsextensions = &vehicle_kiasoul_fn_smsextensions;
  
  if (sys_features[FEATURE_CANWRITE]>0)
    vehicle_poll_setpidlist(vehicle_kiasoul_polls);
  vehicle_poll_setstate(0);

  net_fnbits |= NET_FN_INTERNALGPS;   // Require internal GPS
  net_fnbits |= NET_FN_12VMONITOR;    // Require 12v monitor
  net_fnbits |= NET_FN_SOCMONITOR;    // Require SOC monitor
  net_fnbits |= NET_FN_CARTIME;       // Get real UTC time from modem

  return TRUE;
  }

