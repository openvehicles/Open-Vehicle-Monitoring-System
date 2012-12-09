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
 * Twizy utilities
 */

// Integer Miles <-> Kilometer conversions
// 1 mile = 1.609344 kilometers
// smalles error approximation: mi = (km * 10 - 5) / 16 + 1
#define KM2MI(KM)       (((KM) <= 0) ? 0 : ( ( ( (KM) * 10 - 5 ) >> 4 ) + 1 ))
#define MI2KM(MI)       ( ( ( (MI) << 4 ) + 5 ) / 10 )

// Format scaled int into two ints for sprintf
// ...example: sprintf( buf, "%d.%02u", FMTINT(val,100) )
#define FMTINT(f,d)     (f) / (d), ((f) >= 0) ? ((f) % (d)) : (-(f) % (d))
#define FMTUINT(f,d)     (f) / (d), (f) % (d)

// can_databuffer nibble access macros: b=byte# 0..7 / n=nibble# 0..15
#define CAN_BYTE(b) can_databuffer[b]
#define CAN_NIBL(b) (can_databuffer[b] & 0x0f)
#define CAN_NIBH(b) (can_databuffer[b] >> 4)
#define CAN_NIB(n) (((n)&1) ? CAN_NIBL((n)>>1) : CAN_NIBH((n)>>1))

// Math utils:
#define SQR(n) ((n)*(n))
#define ABS(n) (((n) < 0) ? -(n) : (n))


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
#define CMD_BatteryStatus           205 // ()

// Twizy module version & capabilities:
rom char vehicle_twizy_version[] = "2.1";

#ifdef OVMS_TWIZY_BATTMON
rom char vehicle_twizy_capabilities[] = "C6,C200-205";
#else
rom char vehicle_twizy_capabilities[] = "C6,C200-204";
#endif // OVMS_TWIZY_BATTMON


/***************************************************************
 * Twizy data types
 */

#ifdef OVMS_TWIZY_BATTMON

typedef struct battery_pack
{
    UINT volt_act;                  // current voltage in 1/10 V
    UINT volt_min;                  // charge cycle min voltage
    UINT volt_max;                  // charge cycle max voltage

    UINT volt_watches;              // bitfield: dev > stddev
    UINT volt_alerts;               // bitfield: dev > 2*stddev
    UINT last_volt_alerts;          // recognize alert state changes

    UINT8 temp_watches;             // bitfield: dev > stddev
    UINT8 temp_alerts;              // bitfield: dev > 2*stddev
    UINT8 last_temp_alerts;         // recognize alert state changes

} battery_pack;

typedef struct battery_cmod         // cell module
{
    UINT8 temp_act;                 // current temperature in °F
    UINT8 temp_min;                 // charge cycle min temperature
    UINT8 temp_max;                 // charge cycle max temperature
    INT8 temp_maxdev;               // charge cycle max temperature deviation

} battery_cmod;

typedef struct battery_cell
{
    UINT volt_act;                   // current voltage in 1/200 V
    UINT volt_min;                   // charge cycle min voltage
    UINT volt_max;                   // charge cycle max voltage
    INT volt_maxdev;                 // charge cycle max voltage deviation

    // NOTE: these might be compressable to UINT8
    //  if limiting the range to e.g. 2.5 .. 4.2 V
    //  => 1.7 V / 256 = ~ 1/150 V = should still be precise enough...

} battery_cell;

#endif // OVMS_TWIZY_BATTMON


/***************************************************************
 * Twizy state variables
 */

#pragma udata overlay vehicle_overlay_data

unsigned char can_last_SOC;                 // sufficient charge SOC threshold helper
unsigned int can_last_idealrange;           // sufficient charge range threshold helper

unsigned int can_soc;                       // detailed SOC (1/100 %)
unsigned int can_soc_min;                   // min SOC reached during last discharge
unsigned int can_soc_min_range;             // can_range at min SOC
unsigned int can_soc_max;                   // max SOC reached during last charge
unsigned int can_range;                     // range in km
unsigned int can_speed;                     // current speed in 1/100 km/h
signed int can_power;                       // current power in W, negative=charging
unsigned long can_odometer;                 // odometer in km

unsigned char can_status;                   // Car + charge status from CAN:
#define CAN_STATUS_KEYON        0x10        //  bit 4 = 0x10: 1 = Car ON (key turned)
#define CAN_STATUS_CHARGING     0x20        //  bit 5 = 0x20: 1 = Charging
#define CAN_STATUS_OFFLINE      0x40        //  bit 6 = 0x40: 1 = Switch-ON/-OFF phase / 0 = normal operation


#ifdef OVMS_TWIZY_BATTMON

#define BATT_PACKS      1
#define BATT_CELLS      14
#define BATT_CMODS      7
battery_pack can_batt[BATT_PACKS];          // size:  1 * 15 =  15 bytes
battery_cmod can_cmod[BATT_CMODS];          // size:  7 *  4 =  28 bytes
battery_cell can_cell[BATT_CELLS];          // size: 14 *  8 = 112 bytes
                                            // ------------- = 155 bytes

// Battery cell/cmod deviation alert thresholds:
#define BATT_THRESHOLD_TEMP         3       // = 3 °C
#define BATT_THRESHOLD_VOLT         20      // = 100 mV

// STATE MACHINE: can_batt_sensors_state
//  A consistent state needs all 5 battery sensor messages
//  of one series (i.e. 554-6-7-E-F) to be read.
//  state=BATT_SENSORS_START begins a new fetch cycle.
//  poll1() will advance/reset states accordingly to incoming msgs.
//  ticker1() will not process the data until BATT_SENSORS_READY
//  has been reached, after processing it will reset state to _START.
volatile UINT8 can_batt_sensors_state;
#define BATT_SENSORS_START          0
#define BATT_SENSORS_GOT554         1
#define BATT_SENSORS_GOT556         2
#define BATT_SENSORS_GOT557         3
#define BATT_SENSORS_GOT55E         4
#define BATT_SENSORS_READY          5

#endif // OVMS_TWIZY_BATTMON


#pragma udata   // return to default udata section -- why?


/***************************************************************
 * Twizy functions
 */

BOOL vehicle_twizy_poll0(void);
BOOL vehicle_twizy_poll1(void);

#ifdef OVMS_TWIZY_BATTMON
void vehicle_twizy_battstatus_reset(void);
void vehicle_twizy_battstatus_checkalert(void);
#endif // OVMS_TWIZY_BATTMON



#ifdef OVMS_DIAGMODULE
/***************************************************************
 * CAN simulator: inject CAN messages for testing
 *
 * Usage:
 *  int: vehicle_twizy_simulator_run(chunknr)
 *  SMS: S DEBUG chunknr
 *  MSG: M 200,chunknr
 *
 * To create can_sim_data[] from CAN log:
 * cat canlog \
 *      | sed -e 's/\(..\)/0x\1,/g' \
 *      | sed -e 's/^0x\(.\)\(.\),0x\(.\)/0x0\1,0x\2\3,0x0/' \
 *      | sed -e 's/\(.*\),/  { \1 },/'
 *
 */

rom BYTE can_sim_data[][11] =
{
    //{ 0, 0, 0 }, // chunk 0: init/on
    { 0x06,0x9F,0x04,0xF0,0x82,0x87,0x37 }, // VIN
    { 0x05,0x97,0x08,0x00,0x95,0x21,0x41,0x29,0x00,0x01,0x35 }, // STATUS
    { 0x01,0x55,0x08,0x07,0x97,0xC7,0x54,0x97,0x98,0x00,0x73 }, // SOC
    { 0x05,0x99,0x08,0x00,0x00,0x0D,0x80,0xFF,0x46,0x00,0x00 }, // RANGE
    { 0x05,0x9E,0x08,0x00,0x00,0x0C,0xF2,0x29,0x31,0x00,0x00 }, // MOTOR TEMP
    
    { 0, 0, 1 }, // chunk 1: charge
    { 0x05,0x97,0x08,0x20,0xA4,0x03,0xB1,0x29,0x00,0x01,0x64 }, // STATUS

    { 0, 0, 10 }, // chunk 10: normal battery data
    { 0x05,0x54,0x08,0x33,0x33,0x33,0x33,0x33,0x33,0x33,0x00 },
    { 0x05,0x56,0x08,0x33,0x73,0x35,0x33,0x53,0x35,0x33,0x5A },
    { 0x05,0x57,0x08,0x33,0x53,0x35,0x33,0x53,0x35,0x33,0x50 },
    { 0x05,0x5E,0x08,0x33,0x53,0x34,0x33,0x43,0x36,0x01,0x59 },
    { 0x05,0x5F,0x08,0x00,0xFE,0x73,0x00,0x00,0x23,0xE2,0x3E },

    { 0, 0, 11 }, // chunk 11: battery alert test 1
    { 0x05,0x54,0x08,0x33,0x34,0x33,0x33,0x33,0x33,0x33,0x00 }, // cmod 2 watch?
    { 0x05,0x56,0x08,0x33,0x73,0x35,0x33,0x53,0x35,0x33,0x5A },
    { 0x05,0x57,0x08,0x33,0x53,0x35,0x33,0x53,0x35,0x33,0x50 },
    { 0x05,0x5E,0x08,0x33,0x53,0x34,0x33,0x43,0x36,0x01,0x59 },
    { 0x05,0x5F,0x08,0x00,0xFE,0x73,0x00,0x00,0x23,0xE2,0x3E },
    
    { 0, 0, 12 }, // chunk 12: battery alert test 2
    { 0x05,0x54,0x08,0x33,0x38,0x33,0x33,0x34,0x33,0x34,0x00 }, // cmod 2 alert?
    { 0x05,0x56,0x08,0x33,0x73,0x35,0x33,0x53,0x35,0x31,0x5A }, // cell 5 alert?
    { 0x05,0x57,0x08,0x33,0x53,0x35,0x33,0x53,0x35,0x33,0x50 },
    { 0x05,0x5E,0x08,0x33,0x53,0x34,0x33,0x43,0x36,0x01,0x59 },
    { 0x05,0x5F,0x08,0x00,0xFE,0x73,0x00,0x00,0x23,0xE2,0x3E },

    { 0, 0, 21 }, // chunk 13: partial battery data #1 (state=1?)
    { 0x05,0x54,0x08,0x33,0x33,0x33,0x33,0x33,0x33,0x33,0x00 },
    { 0, 0, 22 }, // chunk 13: partial battery data #1 (state=2?)
    { 0x05,0x56,0x08,0x33,0x73,0x35,0x33,0x53,0x35,0x33,0x5A },
    { 0, 0, 23 }, // chunk 13: partial battery data #1 (state=3?)
    { 0x05,0x57,0x08,0x33,0x53,0x35,0x33,0x53,0x35,0x33,0x50 },
    { 0, 0, 24 }, // chunk 13: partial battery data #1 (state=4?)
    { 0x05,0x5E,0x08,0x33,0x53,0x34,0x33,0x43,0x36,0x01,0x59 },
    { 0, 0, 25 }, // chunk 13: partial battery data #2 (state=5?)
    { 0x05,0x5F,0x08,0x00,0xFE,0x73,0x00,0x00,0x23,0xE2,0x3E },

    { 0, 0, 255 } // end
};

volatile int can_sim = -1;  // -1=off, else read index in can_sim_data

void vehicle_twizy_simulator_run(int chunk)
{
    UINT8 cc;
    int line;

    for( line=0, cc=0; cc <= chunk; line++ )
    {
        if( (can_sim_data[line][0]==0) && (can_sim_data[line][1]==0) )
        {
            // next chunk:
            cc = can_sim_data[line][2];
        }

        else if( cc == chunk )
        {
            // this is the chunk: inject sim data
            if( net_state == NET_STATE_DIAGMODE )
            {
                sprintf( net_scratchpad, "# sim data line %d\n", line );
                net_puts_ram( net_scratchpad );
            }

            // turn off CAN RX interrupts:
            PIE3bits.RXB0IE = 0;
            PIE3bits.RXB1IE = 0;

            // process sim data:
            can_sim = line;
            vehicle_twizy_poll0();
            vehicle_twizy_poll1();
            can_sim = -1;

            // turn on CAN RX interrupts:
            PIE3bits.RXB1IE = 1;
            PIE3bits.RXB0IE = 1;
        }
    }
}
#endif // OVMS_DIAGMODULE


////////////////////////////////////////////////////////////////////////
// can_poll()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to poll for data.
//
// See vehicle initialise() for buffer 0/1 filter setup.
//

// Poll buffer 0:
BOOL vehicle_twizy_poll0(void)
{
    unsigned char CANfilter;
    //unsigned char CANsidh;
    //unsigned char CANsidl;
    unsigned int new_soc;
    unsigned int new_power;


#ifdef OVMS_DIAGMODULE
    if( can_sim >= 0 )
    {
        // READ SIMULATION DATA:
        UINT i;

        i = (((UINT) can_sim_data[can_sim][0]) << 8)
                + can_sim_data[can_sim][1];

        if( i == 0x155 )
            CANfilter = 0;
        else
            return FALSE;

        can_datalength = can_sim_data[can_sim][2];
        for( i=0; i < 8; i++ )
            can_databuffer[i] = can_sim_data[can_sim][3+i];
    }
    else
#endif // OVMS_DIAGMODULE
    {
        // READ CAN BUFFER:

        CANfilter = RXB0CON & 0x01;
        //CANsidh = RXB0SIDH;
        //CANsidl = RXB0SIDL & 0b11100000;

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

    if( CANfilter == 0 )
    {
        /*****************************************************
         * FILTER 0:
         * CAN ID 0x155: sent every 10 ms (100 per second)
         */

        // Basic validation:
        // Byte 4:  0x94 = init/exit phase (CAN data invalid)
        //          0x54 = Twizy online (CAN data valid)
        if( can_databuffer[3] == 0x54 )
        {
            // SOC:
            new_soc = ((unsigned int) can_databuffer[4] << 8) + can_databuffer[5];
            if( new_soc > 0 && new_soc <= 40000 )
            {
                can_soc = new_soc >> 2;
                // car value derived in ticker1()

                // Remember maximum SOC for charging "done" distinction:
                if( can_soc > can_soc_max )
                    can_soc_max = can_soc;

                // ...and minimum SOC for range calculation during charging:
                if( can_soc < can_soc_min )
                {
                    can_soc_min = can_soc;
                    can_soc_min_range = can_range;
                }
            }

            // POWER:
            new_power = ((unsigned int) (can_databuffer[1] & 0x0f) << 8) + can_databuffer[2];
            if( new_power > 0 && new_power < 0x0f00 )
            {
                can_power = 2000 - (signed int) new_power;
            }
        
        }
    }

    // else CANfilter == 1 ...reserved...
    return TRUE;
}



// Poll buffer 1:
BOOL vehicle_twizy_poll1(void)
{
    unsigned char CANfilter;
    unsigned char CANsid;

    unsigned int new_speed;


#ifdef OVMS_DIAGMODULE
    if( can_sim >= 0 )
    {
        // READ SIMULATION DATA:
        UINT i;

        i = (((UINT) can_sim_data[can_sim][0]) << 8)
                + can_sim_data[can_sim][1];

        if( (i & 0x590) == 0x590 )
            CANfilter = 2;
        else if( (i & 0x690) == 0x690 )
            CANfilter = 3;
        else if( (i & 0x550) == 0x550 )
            CANfilter = 4;
        else
            return FALSE;

        CANsid = i & 0x00f;

        can_datalength = can_sim_data[can_sim][2];
        for( i=0; i < 8; i++ )
            can_databuffer[i] = can_sim_data[can_sim][3+i];
    }
    else
#endif // OVMS_DIAGMODULE
    {
        // READ CAN BUFFER:

        CANfilter = RXB1CON & 0x07;
        CANsid = ((RXB1SIDH & 0x01) << 3) + ((RXB1SIDL & 0xe0) >> 5);

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

    if( CANfilter == 2 )
    {
        // Filter 2 = CAN ID GROUP 0x59_:

        switch( CANsid )
        {
            /*****************************************************
             * FILTER 2:
             * CAN ID 0x597: sent every 100 ms (10 per second)
             */
            case 0x07:
                
                // VEHICLE state:
                //  [0]: 0x00=not charging (?), 0x20=charging (?)
                //  [1] bit 4 = 0x10 CAN_STATUS_KEYON: 1 = Car ON (key switch)
                //  [1] bit 5 = 0x20 CAN_STATUS_CHARGING: 1 = Charging
                //  [1] bit 6 = 0x40 CAN_STATUS_OFFLINE: 1 = Switch-ON/-OFF phase

                can_status = can_databuffer[1];
                // Translation to car_doors1 done in ticker1()

                // PEM temperature:
                if( CAN_BYTE(7) > 0 && CAN_BYTE(7) < 0xf0 )
                    car_tpem = (signed char) CAN_BYTE(7) - 40;

                break;
                
                
                
            /*****************************************************
             * FILTER 2:
             * CAN ID 0x599: sent every 100 ms (10 per second)
             */
            case 0x09:
                
                // ODOMETER:
                if( can_databuffer[0] != 0xff )
                {
                    can_odometer = can_databuffer[3]
                               + ((unsigned long) can_databuffer[2] << 8)
                               + ((unsigned long) can_databuffer[1] << 16)
                               + ((unsigned long) can_databuffer[0] << 24);
                    // car value derived in ticker1()
                }

                // RANGE:
                // we need to check for charging, as the Twizy
                // does not update range during charging
                if( ((can_status & 0x60) == 0)
                        && (can_databuffer[5] != 0xff) && (can_databuffer[5] > 0) )
                {
                    can_range = can_databuffer[5];
                    // car values derived in ticker1()
                }

                // SPEED:
                new_speed = ((unsigned int) can_databuffer[6] << 8) + can_databuffer[7];
                if( new_speed != 0xffff )
                {
                    can_speed = new_speed;
                    // car value derived in ticker1()
                }

                break;


            /*****************************************************
             * FILTER 2:
             * CAN ID 0x59E: sent every 100 ms (10 per second)
             */
            case 0x0E:

                // MOTOR TEMPERATURE:
                if( CAN_BYTE(5) > 40 && CAN_BYTE(5) < 0xf0 )
                    car_tmotor = CAN_BYTE(5) - 40;
                else
                    car_tmotor = 0; // unsigned, no negative temps allowed...


                break;
        }

    }
    
    else if( CANfilter == 3 )
    {
        // Filter 3 = CAN ID GROUP 0x69_:

        switch( CANsid )
        {
            /*****************************************************
             * FILTER 3:
             * CAN ID 0x69F: sent every 1000 ms (1 per second)
             */
            case 0x0f:
                // VIN: last 7 digits of real VIN, in nibbles, reverse:
                // (assumption: no hex digits)
                if( car_vin[7] ) // we only need to process this once
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
    else if( CANfilter == 4 )
    {
        // Filter 4 = CAN ID GROUP 0x55_: battery sensors.
        //
        // This group really needs to be processed as fast as possible;
        // though only delivered once per second (except 556), the complete
        // group comes at once an needs to be processed together to get
        // a consistent sensor state.

        if( CAN_BYTE(0) != 0x0ff ) // common msg validation
        switch( CANsid )
        {
            case 0x04:
                // CAN ID 0x554: Battery cell module temperatures
                // (1000 ms = 1 per second)
                if( can_batt_sensors_state != BATT_SENSORS_READY )
                {
                    UINT8 i;

                    for( i=0; i < BATT_CMODS; i++ )
                        can_cmod[i].temp_act = CAN_BYTE(i);

                    can_batt_sensors_state = BATT_SENSORS_GOT554;
                }
                break;

            case 0x06:
                // CAN ID 0x556: Battery cell voltages 1-5
                // (100 ms = 10 per second, for no apparent reason...)
                if( can_batt_sensors_state == BATT_SENSORS_GOT554 )
                {
                    can_cell[0].volt_act = ((UINT) CAN_BYTE(0) << 4)
                            | ((UINT) CAN_NIBH(1));
                    can_cell[1].volt_act = ((UINT) CAN_NIBL(1) << 8)
                            | ((UINT) CAN_BYTE(2));
                    can_cell[2].volt_act = ((UINT) CAN_BYTE(3) << 4)
                            | ((UINT) CAN_NIBH(4));
                    can_cell[3].volt_act = ((UINT) CAN_NIBL(4) << 8)
                            | ((UINT) CAN_BYTE(5));
                    can_cell[4].volt_act = ((UINT) CAN_BYTE(6) << 4)
                            | ((UINT) CAN_NIBH(7));

                    can_batt_sensors_state = BATT_SENSORS_GOT556;
                }
                break;

            case 0x07:
                // CAN ID 0x557: Battery cell voltages 6-10
                // (1000 ms = 1 per second)
                if( can_batt_sensors_state == BATT_SENSORS_GOT556 )
                {
                    can_cell[5].volt_act = ((UINT) CAN_BYTE(0) << 4)
                            | ((UINT) CAN_NIBH(1));
                    can_cell[6].volt_act = ((UINT) CAN_NIBL(1) << 8)
                            | ((UINT) CAN_BYTE(2));
                    can_cell[7].volt_act = ((UINT) CAN_BYTE(3) << 4)
                            | ((UINT) CAN_NIBH(4));
                    can_cell[8].volt_act = ((UINT) CAN_NIBL(4) << 8)
                            | ((UINT) CAN_BYTE(5));
                    can_cell[9].volt_act = ((UINT) CAN_BYTE(6) << 4)
                            | ((UINT) CAN_NIBH(7));

                    can_batt_sensors_state = BATT_SENSORS_GOT557;
                }
                break;

            case 0x0E:
                // CAN ID 0x55E: Battery cell voltages 11-14
                // (1000 ms = 1 per second)
                if( can_batt_sensors_state == BATT_SENSORS_GOT557 )
                {
                    can_cell[10].volt_act = ((UINT) CAN_BYTE(0) << 4)
                            | ((UINT) CAN_NIBH(1));
                    can_cell[11].volt_act = ((UINT) CAN_NIBL(1) << 8)
                            | ((UINT) CAN_BYTE(2));
                    can_cell[12].volt_act = ((UINT) CAN_BYTE(3) << 4)
                            | ((UINT) CAN_NIBH(4));
                    can_cell[13].volt_act = ((UINT) CAN_NIBL(4) << 8)
                            | ((UINT) CAN_BYTE(5));

                    can_batt_sensors_state = BATT_SENSORS_GOT55E;
                }
                break;

            case 0x0F:
                // CAN ID 0x55F: Battery pack voltages
                // (1000 ms = 1 per second)
                if( can_batt_sensors_state == BATT_SENSORS_GOT55E )
                {
                    // we still don't know why there are two pack voltages
                    // best guess: take avg
                    UINT v1, v2;

                    v1 = ((UINT) CAN_BYTE(5) << 4)
                            | ((UINT) CAN_NIBH(6));
                    v2 = ((UINT) CAN_NIBL(6) << 8)
                            | ((UINT) CAN_BYTE(7));
                    
                    can_batt[0].volt_act = (v1 + v2) >> 1;

                    can_batt_sensors_state = BATT_SENSORS_READY;
                }
                break;

        }
    }
#endif // OVMS_TWIZY_BATTMON

    return TRUE;
}



////////////////////////////////////////////////////////////////////////
// can_state_ticker1()
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

    if( can_mileskm == 'K' )
    {
        // convert user km to miles
        if( suffRange > 0 )
            suffRange = KM2MI( suffRange );
        if( maxRange > 0 )
            maxRange = KM2MI( maxRange );
    }


    /***************************************************************************
     * Convert & take over CAN values into CAR values:
     * (done here to keep interrupt fn small&fast)
     */

    // SOC: convert to percent:
    car_SOC = can_soc / 100;

    // ODOMETER: convert to miles/10:
    car_odometer = KM2MI( can_odometer * 10 );
    
    // SPEED:
    if( can_mileskm == 'M' )
        car_speed = KM2MI( can_speed / 100 ); // miles/hour
    else
        car_speed = can_speed / 100; // km/hour

    
    // STATUS: convert Twizy flags to car_doors1:
    // Door state #1
    //	bit2 = 0x04 Charge port (open=1/closed=0)
    //	bit4 = 0x10 Charging (true=1/false=0)
    //	bit6 = 0x40 Hand brake applied (true=1/false=0)
    //	bit7 = 0x80 Car ON (true=1/false=0)
    //
    // ATT: bit 2 = 0x04 = needs to be set for net_sms_stat()!
    //
    // Twizy message: can_status
    //  bit 4 = 0x10 CAN_STATUS_KEYON: 1 = Car ON (key switch)
    //  bit 5 = 0x20 CAN_STATUS_CHARGING: 1 = Charging
    //  bit 6 = 0x40 CAN_STATUS_OFFLINE: 1 = Switch-ON/-OFF phase

    if( (can_status & 0x60) == 0x20 )
        car_doors1 |= 0x14; // Charging ON, Port OPEN
    else
        car_doors1 &= ~0x10; // Charging OFF

    if( can_status & CAN_STATUS_KEYON )
    {
        if( (car_doors1 & 0x80) == 0 )
        {
            car_doors1 |= 0x80; // Car ON
            
#ifdef OVMS_TWIZY_BATTMON
            // reset battery monitor:
            vehicle_twizy_battstatus_reset();
#endif // OVMS_TWIZY_BATTMON

        }
    }
    else
    {
        car_doors1 &= ~0x80; // Car OFF
    }


#ifdef OVMS_TWIZY_BATTMON

    // Battery voltages & temperatures:
    //   only if consistent sensor state has been reached:

    /*
    if( (net_buf_mode == NET_BUF_CRLF) && (net_buf_pos == 0) )
    {
        sprintf( net_scratchpad, "# tic1: ss=%d\r\n", can_batt_sensors_state );
        net_puts_ram( net_scratchpad );
    }
    */

    if( can_batt_sensors_state == BATT_SENSORS_READY )
    {
        // Battery calculations:
        UINT i, stddev, absdev;
        INT dev;
        UINT32 sum, sqrsum;
        float f, m;


        // *********** Temperatures: ************

        // build mean value & standard deviation:
        sum = 0;
        sqrsum = 0;

        for( i=0; i < BATT_CMODS; i++ )
        {
            // Validate:
            if( (can_cmod[i].temp_act==0) || (can_cmod[i].temp_act>=0x0f0) )
                break;

            // Remember min:
            if( (can_cmod[i].temp_min==0) || (can_cmod[i].temp_act < can_cmod[i].temp_min) )
                can_cmod[i].temp_min = can_cmod[i].temp_act;

            // Remember max:
            if( (can_cmod[i].temp_max==0) || (can_cmod[i].temp_act > can_cmod[i].temp_max) )
                can_cmod[i].temp_max = can_cmod[i].temp_act;

            // build sums:
            sum += can_cmod[i].temp_act;
            sqrsum += SQR((UINT32)can_cmod[i].temp_act);
        }

        if( i == BATT_CMODS )
        {
            // All values valid, process:

            m = (float) sum / BATT_CMODS;
            
            car_tbattery = (signed char) m + 0.5 - 40;
            car_stale_temps = 120; // Reset stale indicator

            //stddev = (unsigned int) sqrt( ((float)sqrsum/BATT_CMODS) - SQR((float)sum/BATT_CMODS) ) + 1;
            // this is too complex for C18, we need to split this up:
            f = ((float)sqrsum/BATT_CMODS) - SQR(m);
            stddev = sqrt(f) + 1; // +1 against int precision errors (may need multiplication)
            //sprintf(net_scratchpad, "# temp sqrsum=%lu sum=%lu stddev=%u\n", sqrsum, sum, stddev);
            //net_puts_ram(net_scratchpad);

            for( i=0; i < BATT_CMODS; i++ )
            {
                // deviation:
                dev = (can_cmod[i].temp_act - m)
                        + ((can_cmod[i].temp_act >= m) ? 0.5 : -0.5);
                absdev = ABS(dev);

                // Set watch/alert flags:
                if( absdev >= BATT_THRESHOLD_TEMP )
                    can_batt[0].temp_alerts |= (1 << i);
                else if( absdev > stddev )
                    can_batt[0].temp_watches |= (1 << i);

                // Remember max deviation:
                if( absdev > ABS(can_cmod[i].temp_maxdev) )
                    can_cmod[i].temp_maxdev = (INT8) dev;
            }

        } // if( i == BATT_CMODS )


        // ********** Voltages: ************
        
        // Battery pack:
        if( (can_batt[0].volt_min==0) || (can_batt[0].volt_act < can_batt[0].volt_min) )
            can_batt[0].volt_min = can_batt[0].volt_act;
        if( (can_batt[0].volt_max==0) || (can_batt[0].volt_act > can_batt[0].volt_max) )
            can_batt[0].volt_max = can_batt[0].volt_act;

        // Cells: build mean value & standard deviation:
        sum = 0;
        sqrsum = 0;

        for( i=0; i < BATT_CELLS; i++ )
        {
            // Validate:
            if( (can_cell[i].volt_act==0) || (can_cell[i].volt_act>=0x0f00) )
                break;

            // Remember min:
            if( (can_cell[i].volt_min==0) || (can_cell[i].volt_act < can_cell[i].volt_min) )
                can_cell[i].volt_min = can_cell[i].volt_act;

            // Remember max:
            if( (can_cell[i].volt_max==0) || (can_cell[i].volt_act > can_cell[i].volt_max) )
                can_cell[i].volt_max = can_cell[i].volt_act;

            // build sums:
            sum += can_cell[i].volt_act;
            sqrsum += SQR((UINT32)can_cell[i].volt_act);
        }

        if( i == BATT_CELLS )
        {
            // All values valid, process:

            m = (float) sum / BATT_CELLS;

            //stddev = (unsigned int) sqrt( ((float)sqrsum/BATT_CELLS) - SQR((float)sum/BATT_CELLS) ) + 1;
            // this is too complex for C18, we need to split this up:
            f = ((float)sqrsum/BATT_CELLS) - SQR(m);
            stddev = sqrt(f) + 1; // +1 against int precision errors (may need multiplication)
            //sprintf(net_scratchpad, "# volt sqrsum=%lu sum=%lu stddev=%u\n", sqrsum, sum, stddev);
            //net_puts_ram(net_scratchpad);

            for( i=0; i < BATT_CELLS; i++ )
            {
                // deviation:
                dev = (can_cell[i].volt_act - m)
                        + ((can_cell[i].volt_act >= m) ? 0.5 : -0.5);
                absdev = ABS(dev);

                // Set watch/alert flags:
                if( absdev >= BATT_THRESHOLD_VOLT )
                    can_batt[0].volt_alerts |= (1 << i);
                else if( absdev > stddev )
                    can_batt[0].volt_watches |= (1 << i);

                // Remember max deviation:
                if( absdev > ABS(can_cell[i].volt_maxdev) )
                    can_cell[i].volt_maxdev = (INT) dev;
            }

        } // if( i == BATT_CELLS )


        // Battery monitor update/alert:
        vehicle_twizy_battstatus_checkalert();

        // OK, sensors have been processed, fetch next data:
        can_batt_sensors_state = BATT_SENSORS_START;

    } // if( can_batt_sensors_state == BATT_SENSORS_READY )

#endif // OVMS_TWIZY_BATTMON


    /***************************************************************************
     * Charge notification + alerts:
     *
     * car_chargestate: 1=charging, 2=top off, 4=done, 21=stopped charging
     * car_chargesubstate: unused
     *
     */

    if( car_doors1 & 0x10 )
    {
        /*******************************************************************
         * CHARGING
         */

        // Calculate range during charging:
        // scale can_soc_min_range to can_soc
        if( (can_soc_min_range > 0) && (can_soc > 0) && (can_soc_min > 0) )
        {
            // Update can_range:
            can_range =
                (((float) can_soc_min_range) / can_soc_min) * can_soc;

            if( can_range > 0 )
                car_estrange = KM2MI( can_range );

            if( maxRange > 0 )
                car_idealrange = (((float) maxRange) * can_soc) / 10000;
            else
                car_idealrange = car_estrange;
        }


        // If charging has previously been interrupted...
        if( car_chargestate == 21 )
        {
            // ...send charge alert:
            net_req_notification( NET_NOTIFY_CHARGE );
        }


        // If we've not been charging before...
        if( car_chargestate > 2 )
        {
            // ...enter state 1=charging:
            car_chargestate = 1;

            // reset SOC max:
            can_soc_max = can_soc;

            // Send charge stat:
            net_req_notification( NET_NOTIFY_STAT );
        }
        
        else
        {
            // We've already been charging:

            // check for crossing "sufficient SOC/Range" thresholds:
            if(
               ( (can_soc > 0) && (suffSOC > 0)
                    && (car_SOC >= suffSOC) && (can_last_SOC < suffSOC) )
                 ||
               ( (can_range > 0) && (suffRange > 0)
                    && (car_idealrange >= suffRange) && (can_last_idealrange < suffRange) )
              )
            {
                // ...enter state 2=topping off:
                car_chargestate = 2;

                // ...send charge alert:
                net_req_notification( NET_NOTIFY_CHARGE );
                net_req_notification( NET_NOTIFY_STAT );
            }
            
            // ...else set "topping off" from 97% SOC:
            else if( (car_chargestate != 2) && (can_soc >= 9700) )
            {
                // ...enter state 2=topping off:
                car_chargestate = 2;
                net_req_notification( NET_NOTIFY_STAT );
            }

        }

        // update "sufficient" threshold helpers:
        can_last_SOC = car_SOC;
        can_last_idealrange = car_idealrange;

    }

    else
    {
        /*******************************************************************
         * NOT CHARGING
         */


        // Calculate range:
        if( can_range > 0 )
        {
            car_estrange = KM2MI( can_range );

            if( maxRange > 0 )
                car_idealrange = (((float) maxRange) * can_soc) / 10000;
            else
                car_idealrange = car_estrange;
        }


        // Check if we've been charging before:
        if( car_chargestate <= 2 )
        {
            // yes, check if we've reached 100.00% SOC:
            if( can_soc_max >= 10000 )
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
            net_req_notification( NET_NOTIFY_CHARGE );
            net_req_notification( NET_NOTIFY_STAT );

            // Notifications will be sent in about 1 second
            // and will need car_doors1 & 0x04 set for proper text.
            // We'll keep the flag until next car use.
        }

        else if( (car_doors1 & 0x94) == 0x84 )
        {
            // Car on, not charging, charge port open:
            // beginning the next car usage cycle:

            // Close charging port:
            car_doors1 &= ~0x04;

            // Set charge state to "done":
            car_chargestate = 4;

            // reset SOC minimum:
            can_soc_min = can_soc;
            can_soc_min_range = can_range;
        }
    }


    /***************************************************************************
     * Resolve CAN lockups:
     * PIC manual 23.15.6.1 Receiver Overflow:
            An overflow condition occurs when the MAB has
            assembled a valid received message (the message
            meets the criteria of the acceptance filters) and the
            receive buffer associated with the filter is not available
            for loading of a new message. The associated
            RXBnOVFL bit in the COMSTAT register will be set to
            indicate the overflow condition.
            >>> This bit must be cleared by the MCU. <<< !!!
     * ...to be sure we're clearing all relevant flags...
     */
    if( COMSTATbits.RXB0OVFL )
    {
        RXB0CONbits.RXFUL = 0; // clear buffer full flag
        PIR3bits.RXB0IF = 0; // clear interrupt flag
        COMSTATbits.RXB0OVFL = 0; // clear buffer overflow bit
    }
    if( COMSTATbits.RXB1OVFL )
    {
        RXB1CONbits.RXFUL = 0; // clear buffer full flag
        PIR3bits.RXB1IF = 0; // clear interrupt flag
        COMSTATbits.RXB1OVFL = 0; // clear buffer overflow bit
    }


    return FALSE;
}


////////////////////////////////////////////////////////////////////////
// can_state_ticker60()
// State Model: Per-minute ticker
// This function is called approximately once per minute (since state
// was first entered), and gives the state a timeslice for activity.
//

BOOL vehicle_twizy_state_ticker60(void)
{
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
 * COMMAND CLASS: DEBUG
 *
 *  MSG: CMD_Debug ()
 *  SMS: DEBUG
 *      - output internal state dump for debugging
 *
 */

char vehicle_twizy_debug_msgp( char stat, int cmd )
{
    //static WORD crc; // diff crc for push msgs

    stat = net_msgp_environment( stat );

    sprintf( net_scratchpad,
            "MP-0 c%d,0,%x,%x,%u,%d,%d,%ld,%u,%u,%d,%u,%u,%u,%u"
            , cmd ? cmd : CMD_Debug
            , can_status, car_doors1, car_chargestate
            , can_speed, can_power, can_odometer
            , can_soc, can_soc_min, can_soc_max
            , can_range, can_soc_min_range
            , car_estrange, car_idealrange );

    //return net_msg_encode_statputs( stat, &crc );
    net_msg_encode_puts();
    return stat==2 ? 1 : stat;
}

BOOL vehicle_twizy_debug_cmd( BOOL msgmode, int cmd, char *arguments )
{
#ifdef OVMS_DIAGMODULE
    if( arguments && *arguments )
    {
        // Run simulator:
        vehicle_twizy_simulator_run( atoi(arguments) );
    }
#endif // OVMS_DIAGMODULE

    if( msgmode )
        vehicle_twizy_debug_msgp(0, cmd);

    return TRUE;
}

BOOL vehicle_twizy_debug_sms(BOOL premsg, char *caller, char *command, char *arguments)
{
#ifdef OVMS_DIAGMODULE
    if( arguments && *arguments )
    {
        // Run simulator:
        vehicle_twizy_simulator_run( atoi(arguments) );
    }
#endif // OVMS_DIAGMODULE

    if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
        return FALSE;

    if( premsg )
        net_send_sms_start(caller);

    // SMS PART:

    // ATT! net_scratchpad can in theory take up to 199 chars
    //      BUT sprintf() fails with strange effects if called
    //      with a too complex format template, i.e. >80 chars && >10 args!
    //      => KISS / D&C!

    sprintf( net_scratchpad,
            "STS=%x DS1=%x CHG=%u SPD=%d PWR=%d ODO=%ld SOC=%u SMIN=%u SMAX=%d"
            , can_status, car_doors1, car_chargestate
            , can_speed, can_power, can_odometer
            , can_soc, can_soc_min, can_soc_max
            );
    net_puts_ram( net_scratchpad );

    sprintf( net_scratchpad,
            " CRNG=%u MRNG=%u ERNG=%u IRNG=%u"
            , can_range, can_soc_min_range
            , car_estrange, car_idealrange
            );
    net_puts_ram( net_scratchpad );

#ifdef OVMS_DIAGMODULE
    // ...MORE IN DIAG MODE (serial port):
    if( net_state == NET_STATE_DIAGMODE )
    {
        sprintf( net_scratchpad,
                " FIX=%d LAT=%08lx LON=%08lx ALT=%d DIR=%d"
                , car_gpslock, car_latitude, car_longitude, car_altitude, car_direction );
        net_puts_ram( net_scratchpad );
    }
#endif // OVMS_DIAGMODULE

#ifdef OVMS_TWIZY_BATTMON
    // battery bit fields:
    sprintf( net_scratchpad,
            "\n# vw=%x va=%x lva=%x tw=%x ta=%x lta=%x ss=%u"
            , (UINT) can_batt[0].volt_watches
            , (UINT) can_batt[0].volt_alerts
            , (UINT) can_batt[0].last_volt_alerts
            , (UINT) can_batt[0].temp_watches
            , (UINT) can_batt[0].temp_alerts
            , (UINT) can_batt[0].last_temp_alerts
            , (UINT) can_batt_sensors_state
            );
    net_puts_ram( net_scratchpad );
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
// - output can_soc_min + can_soc_max
// - output odometer
//
// => cat to net_scratchpad (to be sent as SMS or MSG)
//    line breaks: default \r for MSG mode >> cr2lf >> SMS

void vehicle_twizy_stat_prepmsg(void)
{
    // Charge State:

    if (car_doors1 & 0x04)
    {
        // Charge port door is open, we are charging
        switch (car_chargestate)
        {
            case 0x01:
              strcatpgm2ram(net_scratchpad,(char const rom far *)"Charging");
              break;
            case 0x02:
              strcatpgm2ram(net_scratchpad,(char const rom far *)"Charging, Topping off");
              break;
            case 0x04:
              strcatpgm2ram(net_scratchpad,(char const rom far *)"Charging Done");
              break;
            default:
              strcatpgm2ram(net_scratchpad,(char const rom far *)"Charging Stopped");
        }
    }
    else
    {
        // Charge port door is closed, not charging
        strcatpgm2ram(net_scratchpad,(char const rom far *)"Not charging");
    }

    // Estimated + Ideal Range

    strcatpgm2ram(net_scratchpad,(char const rom far *)"\r Range: ");
    if (can_mileskm == 'M')
        sprintf(net_msg_scratchpad, (rom far char*) "%u - %u mi"
            , car_estrange
            , car_idealrange); // Miles
    else
        sprintf(net_msg_scratchpad, (rom far char*) "%u - %u km"
            , MI2KM(car_estrange)
            , MI2KM(car_idealrange)); // km
    strcat((char*)net_scratchpad,net_msg_scratchpad);

    // SOC + min/max:

    strcatpgm2ram(net_scratchpad,(char const rom far *)"\r SOC: ");
    sprintf(net_msg_scratchpad, (rom far char*) "%u%% (%d.%02u%%..%d.%02u%%)"
            , car_SOC
            , FMTUINT(can_soc_min, 100)
            , FMTUINT(can_soc_max, 100)
            );
    strcat(net_scratchpad,net_msg_scratchpad);

    // ODOMETER:

    strcatpgm2ram(net_scratchpad,(char const rom far *)"\r ODO: ");
    if (can_mileskm == 'M') // Km or Miles
        sprintf(net_msg_scratchpad, (rom far char*) "%lu mi"
                , car_odometer / 10); // Miles
    else
        sprintf(net_msg_scratchpad, (rom far char*) "%lu km"
                , MI2KM(car_odometer / 10)); // km
    strcat(net_scratchpad,net_msg_scratchpad);
}

BOOL vehicle_twizy_stat_sms(BOOL premsg, char *caller, char *command, char *arguments)
{
    // check for replace mode:
    if( !premsg )
        return FALSE;

    // check SMS notifies:
    if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
        return FALSE;

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

BOOL vehicle_twizy_alert_cmd( BOOL msgmode, int cmd, char *arguments )
{
    // prepare message:
    strcpypgm2ram(net_scratchpad,(char const rom far*)"MP-0 PA");
    vehicle_twizy_stat_prepmsg();

    if( msgmode )
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

char vehicle_twizy_range_msgp( char stat, int cmd )
{
    //static WORD crc; // diff crc for push msgs

    sprintf( net_scratchpad,
            "MP-0 c%d,0,%u"
            , cmd ? cmd : CMD_QueryRange
            , sys_features[FEATURE_MAXRANGE]
            );

    //return net_msg_encode_statputs( stat, &crc );
    net_msg_encode_puts();
    return stat==2 ? 1 : stat;
}

BOOL vehicle_twizy_range_cmd( BOOL msgmode, int cmd, char *arguments )
{
    if( cmd == CMD_SetRange )
    {
        if( arguments && *arguments )
            sys_features[FEATURE_MAXRANGE] = atoi( arguments );
        else
            sys_features[FEATURE_MAXRANGE] = 0;

        par_set( PARAM_FEATURE_BASE + FEATURE_MAXRANGE, arguments );
    }

    // CMD_QueryRange
    if( msgmode )
        vehicle_twizy_range_msgp(0, cmd);

    return TRUE;
}

BOOL vehicle_twizy_range_sms(BOOL premsg, char *caller, char *command, char *arguments)
{
    if( !premsg )
        return FALSE;

    // RANGE (maxrange)
    if( command && (command[5] != '?') )
    {
        if( arguments && *arguments )
            sys_features[FEATURE_MAXRANGE] = atoi( arguments );
        else
            sys_features[FEATURE_MAXRANGE] = 0;

        par_set( PARAM_FEATURE_BASE + FEATURE_MAXRANGE, arguments );
    }

    // RANGE?
    if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
        return FALSE; // handled, but no SMS has been started

    // Reply current range:
    net_send_sms_start( caller );

    net_puts_rom("Max ideal range: ");
    if( sys_features[FEATURE_MAXRANGE] == 0 )
    {
        net_puts_rom("UNSET");
    }
    else
    {
        sprintf( net_scratchpad, (rom far char*) "%u "
                , sys_features[FEATURE_MAXRANGE] );
        net_puts_ram( net_scratchpad );
        net_puts_rom( (can_mileskm == 'M') ? "mi" : "km" );
    }

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

char vehicle_twizy_ca_msgp( char stat, int cmd )
{
    //static WORD crc; // diff crc for push msgs

    sprintf( net_scratchpad,
            "MP-0 c%d,0,%u,%u"
            , cmd ? cmd : CMD_QueryChargeAlerts
            , sys_features[FEATURE_SUFFRANGE]
            , sys_features[FEATURE_SUFFSOC]
            );

    //return net_msg_encode_statputs( stat, &crc );
    net_msg_encode_puts();
    return stat==2 ? 1 : stat;
}

BOOL vehicle_twizy_ca_cmd( BOOL msgmode, int cmd, char *arguments )
{
    if( cmd == CMD_SetChargeAlerts )
    {
        char *range = NULL, *soc = NULL;

        if( arguments && *arguments )
        {
            range = strtokpgmram( arguments, "," );
            soc = strtokpgmram( NULL, "," );
        }

        sys_features[FEATURE_SUFFRANGE] = range ? atoi(range) : 0;
        sys_features[FEATURE_SUFFSOC] = soc ? atoi(soc) : 0;

        // store new alerts into EEPROM:
        par_set( PARAM_FEATURE_BASE + FEATURE_SUFFRANGE, range );
        par_set( PARAM_FEATURE_BASE + FEATURE_SUFFSOC, soc );
    }

    // CMD_QueryChargeAlerts
    if( msgmode )
        vehicle_twizy_ca_msgp(0, cmd);

    return TRUE;
}

BOOL vehicle_twizy_ca_sms(BOOL premsg, char *caller, char *command, char *arguments)
{
    if( !premsg )
        return FALSE;

    if( command && (command[2] != '?') )
    {
        // SET CHARGE ALERTS:

        int value;
        char unit;
        unsigned char f;
        char *arg_suffsoc=NULL, *arg_suffrange=NULL;

        // clear current alerts:
        sys_features[FEATURE_SUFFSOC] = 0;
        sys_features[FEATURE_SUFFRANGE] = 0;

        // read new alerts from arguments:
        while( arguments && *arguments )
        {
            value = atoi( arguments );
            unit = arguments[strlen(arguments)-1];

            if( unit == '%' )
            {
                arg_suffsoc = arguments;
                sys_features[FEATURE_SUFFSOC] = value;
            }
            else
            {
                arg_suffrange = arguments;
                sys_features[FEATURE_SUFFRANGE] = value;
            }

            arguments = net_sms_nextarg( arguments );
        }

        // store new alerts into EEPROM:
        par_set( PARAM_FEATURE_BASE + FEATURE_SUFFSOC, arg_suffsoc );
        par_set( PARAM_FEATURE_BASE + FEATURE_SUFFRANGE, arg_suffrange );
    }


    // REPLY current charge alerts:

    if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
        return FALSE; // handled, but no SMS has been started

    net_send_sms_start( caller );
    net_puts_rom("Charge Alert: ");

    if( (sys_features[FEATURE_SUFFSOC]==0) && (sys_features[FEATURE_SUFFRANGE]==0) )
    {
        net_puts_rom("OFF");
    }
    else
    {
        if( sys_features[FEATURE_SUFFSOC] > 0 )
        {
            sprintf( net_scratchpad, (rom far char*) "%u%%"
                    , sys_features[FEATURE_SUFFSOC] );
            net_puts_ram( net_scratchpad );
        }

        if( sys_features[FEATURE_SUFFRANGE] > 0 )
        {
            if( sys_features[FEATURE_SUFFSOC] > 0 )
                net_puts_rom( " or " );
            sprintf( net_scratchpad, (rom far char*) "%u "
                    , sys_features[FEATURE_SUFFRANGE] );
            net_puts_ram( net_scratchpad );
            net_puts_rom( (can_mileskm == 'M') ? "mi" : "km" );
        }
    }

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

    if( (can_status & CAN_STATUS_OFFLINE) ) // only wait if CAN is online
        return TRUE;

    for( n=11; (can_batt_sensors_state != BATT_SENSORS_READY) && (n>0); n-- )
        delay100(1);
    
    return( can_batt_sensors_state == BATT_SENSORS_READY );
}


// Reset battery monitor min/max/maxdev & alerts:
void vehicle_twizy_battstatus_reset(void)
{
    int i;

    vehicle_twizy_battstatus_wait4sensors();

    for( i=0; i < BATT_CELLS; i++ )
    {
        can_cell[i].volt_max = can_cell[i].volt_act;
        can_cell[i].volt_min = can_cell[i].volt_act;
        can_cell[i].volt_maxdev = 0;
    }

    for( i=0; i < BATT_CMODS; i++ )
    {
        can_cmod[i].temp_max = can_cmod[i].temp_act;
        can_cmod[i].temp_min = can_cmod[i].temp_act;
        can_cmod[i].temp_maxdev = 0;
    }

    for( i=0; i < BATT_PACKS; i++ )
    {
        can_batt[i].volt_min = can_batt[i].volt_act;
        can_batt[i].volt_max = can_batt[i].volt_act;
        can_batt[i].temp_watches = 0;
        can_batt[i].temp_alerts = 0;
        can_batt[i].volt_watches = 0;
        can_batt[i].volt_alerts = 0;
        can_batt[i].last_temp_alerts = 0;
        can_batt[i].last_volt_alerts = 0;
    }
    
    can_batt_sensors_state = BATT_SENSORS_START;
}


// Prep battery[p] alert message text for SMS/MSG in net_scratchpad:
void vehicle_twizy_battstatus_prepalert( int p )
{
    int c;

    // Voltage deviations:
    strcatpgm2ram( net_scratchpad, "Volts: " );
    if( (can_batt[p].volt_alerts==0) && (can_batt[p].volt_watches==0) )
    {
        strcatpgm2ram( net_scratchpad, "OK " );
    }
    else
    {
        for( c = 0; c < BATT_CELLS; c++ )
        {
            // Alert?
            if( can_batt[p].volt_alerts & (1<<c) )
                strcatpgm2ram( net_scratchpad, "!" );
            else if( can_batt[p].volt_watches & (1<<c) )
                strcatpgm2ram( net_scratchpad, "?" );
            else
                continue;

            sprintf( net_msg_scratchpad,
                    "C%u:%+dmV "
                    , c+1
                    , CONV_CellVoltS(can_cell[c].volt_maxdev)
                    );
            strcat( net_scratchpad, net_msg_scratchpad );
        }
    }

    // Temperature deviations:
    strcatpgm2ram( net_scratchpad, "Temps: " );
    if( (can_batt[p].temp_alerts==0) && (can_batt[p].temp_watches==0) )
    {
        strcatpgm2ram( net_scratchpad, "OK " );
    }
    else
    {
        for( c = 0; c < BATT_CMODS; c++ )
        {
            // Alert?
            if( can_batt[p].temp_alerts & (1<<c) )
                strcatpgm2ram( net_scratchpad, "!" );
            else if( can_batt[p].temp_watches & (1<<c) )
                strcatpgm2ram( net_scratchpad, "?" );
            else
                continue;

            sprintf( net_msg_scratchpad,
                    "M%u:%+dC "
                    , c+1
                    , can_cmod[c].temp_maxdev
                    );
            strcat( net_scratchpad, net_msg_scratchpad );
        }
    }

    // alert text now ready to send in net_scratchpad
}


char vehicle_twizy_battstatus_msgp( char stat, int cmd )
{
    static WORD crc_alert[BATT_PACKS];
    static WORD crc_pack[BATT_PACKS];
    static WORD crc_cell[BATT_CELLS]; // RT: just one pack...
    UINT8 p, c;
    UINT8 tmin, tmax;
    int tact;
    UINT8 volt_alert, temp_alert;

    // Try to wait for consistent sensor data:
    if( vehicle_twizy_battstatus_wait4sensors() == FALSE )
    {
        // no sensor data available, abort if in diff mode:
        if( stat > 0 )
            return stat;
    }

    // Prep: collect cell temperatures:
    tmin = 255;
    tmax = 0;
    tact = 0;
    for( c=0; c < BATT_CMODS; c++ )
    {
        tact += can_cmod[c].temp_act;
        if( can_cmod[c].temp_min < tmin )
            tmin = can_cmod[c].temp_min;
        if( can_cmod[c].temp_max > tmax )
            tmax = can_cmod[c].temp_max;
    }

    tact = (float) tact / BATT_CMODS + 0.5;

    // Output pack status:
    // MP-0 B,<packnr>,<nr_of_cells>
    //  ,<volt_alertstatus>,<temp_alertstatus>
    //  ,<soc>,<soc_min>,<soc_max>
    //  ,<volt_act>,<volt_act_cellnom>
    //  ,<volt_min>,<volt_min_cellnom>
    //  ,<volt_max>,<volt_max_cellnom>
    //  ,<temp_act>,<temp_min>,<temp_max>
    for( p=0; p < BATT_PACKS; p++ )
    {
        if( can_batt[p].volt_alerts )
            volt_alert = 3;
        else if( can_batt[p].volt_watches )
            volt_alert = 2;
        else
            volt_alert = 1;

        if( can_batt[p].temp_alerts )
            temp_alert = 3;
        else if( can_batt[p].temp_watches )
            temp_alert = 2;
        else
            temp_alert = 1;

        // alert:
        if( can_batt[p].volt_alerts || can_batt[p].temp_alerts )
        {
            strcpypgm2ram( net_scratchpad, "MP-0 PA" );
            vehicle_twizy_battstatus_prepalert( p );
            stat = net_msg_encode_statputs( stat, &crc_alert[p] );
        }
        else
        {
            crc_alert[p] = 0; // reset crc for next alert
        }

        // data:
        sprintf( net_scratchpad,
                "MP-0 B,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u"
                , p+1, BATT_CELLS
                , volt_alert
                , temp_alert
                , can_soc, can_soc_min, can_soc_max
                , CONV_PackVolt(can_batt[p].volt_act)
                , CONV_PackVolt(can_batt[p].volt_act / BATT_CELLS)
                , CONV_PackVolt(can_batt[p].volt_min)
                , CONV_PackVolt(can_batt[p].volt_min / BATT_CELLS)
                , CONV_PackVolt(can_batt[p].volt_max)
                , CONV_PackVolt(can_batt[p].volt_max / BATT_CELLS)
                );
        sprintf( net_msg_scratchpad,
                ",%d,%d,%d"
                , CONV_Temp(tact)
                , CONV_Temp(tmin)
                , CONV_Temp(tmax)
                );
        strcat( net_scratchpad, net_msg_scratchpad );
        stat = net_msg_encode_statputs( stat, &crc_pack[p] );

        // Output cell status:
        // MP-0 H,<packnr>,<cellnr>
        //  ,<volt_alertstatus>,<temp_alertstatus>,
        //  ,<volt_act>,<volt_min>,<volt_max>,<volt_maxdev>
        //  ,<temp_act>,<temp_min>,<temp_max>,<temp_maxdev>
        for( c=0; c < BATT_CELLS; c++ )
        {
            if( can_batt[p].volt_alerts & (1 << c) )
                volt_alert = 3;
            else if( can_batt[p].volt_watches & (1 << c) )
                volt_alert = 2;
            else
                volt_alert = 1;

            if( can_batt[p].temp_alerts & (1 << (c >> 1)) )
                temp_alert = 3;
            else if( can_batt[p].temp_watches & (1 << (c >> 1)) )
                temp_alert = 2;
            else
                temp_alert = 1;

            sprintf( net_scratchpad,
                    "MP-0 H,%u,%u,%u,%u,%u,%u,%u,%d"
                    , p+1, c+1
                    , volt_alert
                    , temp_alert
                    , CONV_CellVolt(can_cell[c].volt_act)
                    , CONV_CellVolt(can_cell[c].volt_min)
                    , CONV_CellVolt(can_cell[c].volt_max)
                    , CONV_CellVoltS(can_cell[c].volt_maxdev)
                    );
            sprintf( net_msg_scratchpad,
                    ",%d,%d,%d,%d"
                    , CONV_Temp(can_cmod[c>>1].temp_act)
                    , CONV_Temp(can_cmod[c>>1].temp_min)
                    , CONV_Temp(can_cmod[c>>1].temp_max)
                    , can_cmod[c>>1].temp_maxdev
                    );
            strcat( net_scratchpad, net_msg_scratchpad );
            stat = net_msg_encode_statputs( stat, &crc_cell[c] );
        }
    }

    return stat;
}

BOOL vehicle_twizy_battstatus_cmd( BOOL msgmode, int cmd, char *arguments )
{
    if( msgmode )
        vehicle_twizy_battstatus_msgp(0, cmd);

    return TRUE;
}

BOOL vehicle_twizy_battstatus_sms(BOOL premsg, char *caller, char *command, char *arguments)
{
    int p, c;
    char argc1, argc2;
    UINT8 tmin, tmax;
    int tact;
    UINT8 cc;

    if( !premsg )
        return FALSE;

    if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
        return FALSE;

    // Try to wait for valid sensor data (but send info anyway)
    vehicle_twizy_battstatus_wait4sensors();

    // Start SMS:
    net_send_sms_start( caller );

    argc1 = (arguments) ? (arguments[0] | 0x20) : 0;
    argc2 = (argc1) ? (arguments[1] | 0x20) : 0;

    switch( argc1 )
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
            for( c=0; c < BATT_CMODS; c++ )
            {
                tact += can_cmod[c].temp_act;
                if( can_cmod[c].temp_min < tmin )
                    tmin = can_cmod[c].temp_min;
                if( can_cmod[c].temp_max > tmax )
                    tmax = can_cmod[c].temp_max;
            }
            tact = (float) tact / BATT_CMODS + 0.5;

            // Output pack status:
            sprintf( net_scratchpad,
                    "P:%dC (%dC..%dC) "
                    , CONV_Temp( tact )
                    , CONV_Temp( tmin )
                    , CONV_Temp( tmax )
                    );
            net_puts_ram( net_scratchpad );

            // Output cmod status:
            for( c=0; c < BATT_CMODS; c++ )
            {
                // Alert?
                if( can_batt[0].temp_alerts & (1<<c) )
                    net_putc_ram('!');
                else if( can_batt[0].temp_watches & (1<<c) )
                    net_putc_ram('?');

                if( argc2 == 'd' ) // deviations
                {
                    sprintf( net_scratchpad,
                        "%u:%+dC "
                        , c+1
                        , can_cmod[c].temp_maxdev
                        );
                }
                else // current values
                {
                    sprintf( net_scratchpad,
                        "%u:%dC "
                        , c+1
                        , CONV_Temp( can_cmod[c].temp_act )
                        );
                }
                net_puts_ram( net_scratchpad );
            }

            break;


        case 'v':
            // Current voltage levels ("v") or deviations ("vd"):

            for( p=0; p < BATT_PACKS; p++ )
            {
                // Output pack status:
                cc = sprintf( net_scratchpad,
                        "P:%u.%02uV "
                        , FMTUINT( CONV_PackVolt(can_batt[p].volt_act), 100 )
                        );
                net_puts_ram( net_scratchpad );

                // Output cell status:
                for( c=0; c < BATT_CELLS; c++ )
                {
                    // Split SMS?
                    if( cc > (160-13) )
                    {
                        net_send_sms_finish();
                        delay100(30);
                        net_send_sms_start( caller );
                        cc = 0;
                    }

                    // Alert?
                    if( can_batt[p].volt_alerts & (1<<c) )
                    {
                        net_putc_ram('!');
                        cc++;
                    }
                    else if( can_batt[p].volt_watches & (1<<c) )
                    {
                        net_putc_ram('?');
                        cc++;
                    }

                    if( argc2 == 'd' ) // deviations
                    {
                        cc += sprintf( net_scratchpad,
                            "%u:%+dmV "
                            , c+1
                            , CONV_CellVoltS(can_cell[c].volt_maxdev)
                            );
                    }
                    else
                    {
                        cc += sprintf( net_scratchpad,
                            "%u:%u.%03uV "
                            , c+1
                            , FMTUINT( CONV_CellVolt(can_cell[c].volt_act), 1000 )
                            );
                    }
                    net_puts_ram( net_scratchpad );
                }
            }

            break;


        default:
            // Battery alert status:

            net_scratchpad[0] = 0;
            vehicle_twizy_battstatus_prepalert( 0 );
            cr2lf( net_scratchpad );
            net_puts_ram( net_scratchpad );

            // Remember last alert state notified:
            can_batt[0].last_volt_alerts = can_batt[0].volt_alerts;
            can_batt[0].last_temp_alerts = can_batt[0].temp_alerts;

            break;


    }

    return TRUE;
}

void vehicle_twizy_battstatus_checkalert(void)
{
    char *regphone;

    // Send diff MSG:

    if( (net_msg_cmd_code!=0) && (net_msg_serverok==1) && (net_msg_sendpending==0) )
    {
        if( vehicle_twizy_battstatus_msgp(2, 0) == 1 )
        {
            net_msg_send();
            delay100(10); // give modem time to send
        }
    }

    // Send SMS if voltage / temperature alert status changed:

    if (sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS)
        return;

    regphone = par_get(PARAM_REGPHONE);
    if( !regphone || !*regphone )
        return;

    if( (can_batt[0].volt_alerts != can_batt[0].last_volt_alerts)
     || (can_batt[0].temp_alerts != can_batt[0].last_temp_alerts) )
    {
        if( vehicle_twizy_battstatus_sms(TRUE, regphone, NULL, NULL) )
            net_send_sms_finish();
    }

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

BOOL vehicle_twizy_fn_commandhandler( BOOL msgmode, int cmd, char *msg )
{
    switch( cmd )
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
        case CMD_BatteryStatus:
            return vehicle_twizy_battstatus_cmd(msgmode, cmd, msg);
#endif // OVMS_TWIZY_BATTMON

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

rom char vehicle_twizy_sms_cmdtable[][NET_SMS_CMDWIDTH] =
{
    "3DEBUG",       // Twizy: output internal state dump for debug
    "3STAT",        // override standard STAT
    "3RANGE",       // Twizy: set/query max ideal range
    "3CA",          // Twizy: set/query charge alerts
    "3HELP",        // extend HELP output

#ifdef OVMS_TWIZY_BATTMON
    "3BATT",        // Twizy: battery status
#endif // OVMS_TWIZY_BATTMON

    ""
};

rom far BOOL (*vehicle_twizy_sms_hfntable[])(BOOL premsg, char *caller, char *command, char *arguments) =
{
    &vehicle_twizy_debug_sms,
    &vehicle_twizy_stat_sms,
    &vehicle_twizy_range_sms,
    &vehicle_twizy_ca_sms,
    &vehicle_twizy_help_sms,

#ifdef OVMS_TWIZY_BATTMON
    &vehicle_twizy_battstatus_sms
#endif // OVMS_TWIZY_BATTMON

};

// SMS COMMAND DISPATCHER:
// premsg: TRUE=may replace, FALSE=may extend standard handler
// returns TRUE if handled
BOOL vehicle_twizy_fn_sms(BOOL checkauth, BOOL premsg, char *caller, char *command, char *arguments)
{
    int k;

    // Command parsing...
    for( k=0; vehicle_twizy_sms_cmdtable[k][0] != 0; k++ )
    {
        if( memcmppgm2ram( command,
                (char const rom far*)vehicle_twizy_sms_cmdtable[k]+1,
                strlenpgm((char const rom far*)vehicle_twizy_sms_cmdtable[k])-1) == 0 )
        {
            BOOL result;

            if( checkauth )
            {
                // we need to check the caller authorization:
                arguments = net_sms_initargs(arguments);
                if (!net_sms_checkauth(vehicle_twizy_sms_cmdtable[k][0], caller, &arguments))
                    return FALSE; // failed
            }

            // Call sms handler:
            result = (*vehicle_twizy_sms_hfntable[k])(premsg, caller, command, arguments);

            if( (premsg) && (result) )
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

    if( premsg )
        return FALSE; // run only in extend mode

    if( sys_features[FEATURE_CARBITS] & FEATURE_CB_SOUT_SMS )
        return FALSE;

    net_puts_rom(" \r\nTwizy Commands:");

    for( k=0; vehicle_twizy_sms_cmdtable[k][0] != 0; k++ )
    {
        net_puts_rom(" ");
        net_puts_rom(vehicle_twizy_sms_cmdtable[k]+1);
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
    if( (car_type[0]!='R') || (car_type[1]!='T') || (car_type[2]!=0) )
    {
        UINT8 i;

        // INIT VEHICLE VARIABLES:

        car_type[0] = 'R'; // Car is type RT - Renault Twizy
        car_type[1] = 'T';
        car_type[2] = 0;
        car_type[3] = 0;
        car_type[4] = 0;

        car_linevoltage = 230; // fix
        car_chargecurrent = 10; // fix

        can_status = CAN_STATUS_OFFLINE;

        can_soc = 5000;
        can_soc_min = 10000;
        can_soc_max = 0;

        car_SOC = 50;
        can_last_SOC = 0;

        can_range = 50;
        can_soc_min_range = 100;
        can_last_idealrange = 0;

        can_speed = 0;
        can_power = 0;
        can_odometer = 0;

#ifdef OVMS_TWIZY_BATTMON

        // Init battery monitor:
        memset( can_batt, 0, sizeof can_batt );
        can_batt[0].volt_min = 1000; // 100 V
        memset( can_cmod, 0, sizeof can_cmod );
        for( i=0; i < BATT_CMODS; i++ )
        {
            can_cmod[i].temp_act = 40;
            can_cmod[i].temp_min = 240;
        }
        memset( can_cell, 0, sizeof can_cell );
        for( i=0; i < BATT_CELLS; i++ )
        {
            can_cell[i].volt_min = 2000; // 10 V
        }

#endif // OVMS_TWIZY_BATTMON

    }

#ifdef OVMS_TWIZY_BATTMON
    can_batt_sensors_state = BATT_SENSORS_START;
#endif // OVMS_TWIZY_BATTMON


    CANCON = 0b10010000; // Initialize CAN
    while (!CANSTATbits.OPMODE2); // Wait for Configuration mode

    // We are now in Configuration Mode.

    // ID masks and filters are 11 bit as High-8 + Low-MSB-3
    // (Filter bit n must match if mask bit n = 1)


    // RX buffer0 uses Mask RXM0 and filters RXF0, RXF1
    RXB0CON = 0b00000000;

    // Setup Filter0 and Mask for CAN ID 0x155

    // Mask0 = 0x7ff = exact ID filter match (high perf)
    RXM0SIDH = 0b11111111;
    RXM0SIDL = 0b11100000;

    // Filter0 = ID 0x155:
    RXF0SIDH = 0b00101010;
    RXF0SIDL = 0b10100000;

    // Filter1 = unused (reserved for another frequent ID)
    RXF1SIDH = 0b00000000;
    RXF1SIDL = 0b00000000;


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

    // Filter5 = unused
    RXF5SIDH = 0b00000000;
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

    if (sys_features[FEATURE_CANWRITE]>0)
    {
        CANCON = 0b00000000;  // Normal mode
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
    vehicle_fn_ticker1 = &vehicle_twizy_state_ticker1;
    vehicle_fn_ticker60 = &vehicle_twizy_state_ticker60;
    vehicle_fn_smshandler = &vehicle_twizy_fn_smshandler;
    vehicle_fn_smsextensions = &vehicle_twizy_fn_smsextensions;
    vehicle_fn_commandhandler = &vehicle_twizy_fn_commandhandler;

    net_fnbits |= NET_FN_INTERNALGPS;   // Require internal GPS

    return TRUE;
}
