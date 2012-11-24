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
#include "ovms.h"
#include "params.h"
#include "led.h"
#include "utils.h"
#include "net_sms.h"
#include "net_msg.h"

// Integer Miles <-> Kilometer conversions
// 1 mile = 1.609344 kilometers
// smalles error approximation: mi = (km * 10 - 5) / 16 + 1
// -- ATT: NO NEGATIVE VALUES ALLOWED DUE TO BIT SHIFTING!
#define KM2MI(KM)       ( ( ( (KM) * 10 - 5 ) >> 4 ) + 1 )
#define MI2KM(MI)       ( ( ( (MI) << 4 ) + 5 ) / 10 )

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

// Twizy module version & capabilities:
rom char vehicle_twizy_version[] = "1.6";
rom char vehicle_twizy_capabilities[] = "C6,C200-204";


#pragma udata overlay vehicle_overlay_data

//unsigned char can_lastspeedmsg[8];           // A buffer to store the last speed message
//unsigned char can_lastspeedrpt;              // A mechanism to repeat the tx of last speed message

unsigned char can_last_SOC = 0;              // sufficient charge SOC threshold helper
unsigned int can_last_idealrange = 0;        // sufficient charge range threshold helper

// Additional Twizy state variables:
unsigned int can_soc;                       // detailed SOC (1/100 %)
unsigned int can_soc_min;                   // min SOC reached during last discharge
unsigned int can_soc_min_range;             // can_range at min SOC
unsigned int can_soc_max;                   // max SOC reached during last charge
unsigned int can_range;                     // range in km
unsigned int can_speed = 0;                  // current speed in 1/100 km/h
signed int can_power = 0;                    // current power in W, negative=charging
unsigned long can_odometer;                 // odometer in km

unsigned char can_status = 0;               // Car + charge status from CAN:
#define CAN_STATUS_KEYON        0x10        //  bit 4 = 0x10: 1 = Car ON (key turned)
#define CAN_STATUS_CHARGING     0x20        //  bit 5 = 0x20: 1 = Charging
#define CAN_STATUS_OFFLINE      0x40        //  bit 6 = 0x40: 1 = Switch-ON/-OFF phase / 0 = normal operation

#pragma udata



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
    unsigned char CANfilter = RXB0CON & 0x01;
    //unsigned char CANsidh = RXB0SIDH;
    //unsigned char CANsidl = RXB0SIDL & 0b11100000;
    unsigned int new_soc;
    unsigned int new_power;


    // READ CAN BUFFER:

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
    unsigned char CANfilter = RXB1CON & 0x07;
    unsigned char CANsid;

    unsigned int new_speed;


    // READ CAN BUFFER:

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


    // HANDLE CAN MESSAGE:

    if( CANfilter == 2 )
    {
        // Filter 2 = CAN ID GROUP 0x59_:
        CANsid = ((RXB1SIDH & 0x01) << 3) + ((RXB1SIDL & 0xe0) >> 5);

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
                
        }

    }
    
    else if( CANfilter == 3 )
    {
        // Filter 3 = CAN ID GROUP 0x69_:
        CANsid = ((RXB1SIDH & 0x01) << 3) + ((RXB1SIDL & 0xe0) >> 5);

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
                    car_vin[0] = '0' + (can_databuffer[3] & 0x0f);
                    car_vin[1] = '0' + ((can_databuffer[3] >> 4) & 0x0f);
                    car_vin[2] = '0' + (can_databuffer[2] & 0x0f);
                    car_vin[3] = '0' + ((can_databuffer[2] >> 4) & 0x0f);
                    car_vin[4] = '0' + (can_databuffer[1] & 0x0f);
                    car_vin[5] = '0' + ((can_databuffer[1] >> 4) & 0x0f);
                    car_vin[6] = '0' + (can_databuffer[0] & 0x0f);
                    car_vin[7] = 0;
                }

                break;
        }
    }

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


    /*
     * First boot data sanitizing:
     */

    if( can_soc == 0 || can_soc > 10000 )
        can_soc = 5000; // fallback if ovms powered on with car powered off

    // init min+max to current soc if available:
    if( can_soc_min == 0 || can_soc_min > 10000 )
    {
        can_soc_min = can_status ? can_soc : 10000;
        can_soc_min_range = can_status ? can_range : 0;
    }
    if( can_soc_max == 0 || can_soc_max > 10000 )
        can_soc_max = can_status ? can_soc : 0;


    /*
     * Feature configuration:
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


    /*
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
        car_doors1 |= 0x80; // Car ON
    else
        car_doors1 &= ~0x80; // Car OFF


    /*
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


    /* Resolve CAN lockups:
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
    // Check CAN status:
    if( can_status == 0 )
        return FALSE; // no valid CAN data yet

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
 *      char newstat = _msgp( int cmd, char stat )
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

char vehicle_twizy_debug_msgp( int cmd, char stat )
{
    static WORD crc;

    sprintf( net_scratchpad,
            "MP-0 c%d,0,%x,%x,%u,%d,%d,%ld,%u,%u,%d,%u,%u,%u,%u"
            , cmd ? cmd : CMD_Debug
            , can_status, car_doors1, car_chargestate
            , can_speed, can_power, can_odometer
            , can_soc, can_soc_min, can_soc_max
            , can_range, can_soc_min_range
            , car_estrange, car_idealrange );

    return net_msg_encode_statputs( stat, &crc );
}

BOOL vehicle_twizy_debug_cmd( BOOL msgmode, int cmd, char *arguments )
{
    if( msgmode )
        vehicle_twizy_debug_msgp(cmd, 0);

    return TRUE;
}

BOOL vehicle_twizy_debug_sms(BOOL premsg, char *caller, char *command, char *arguments)
{
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
    sprintf(net_msg_scratchpad, (rom far char*) "%u%% (%u - %u)"
            , car_SOC
            , can_soc_min
            , can_soc_max);
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

char vehicle_twizy_range_msgp( int cmd, char stat )
{
    static WORD crc;

    sprintf( net_scratchpad,
            "MP-0 c%d,0,%u"
            , cmd ? cmd : CMD_QueryRange
            , sys_features[FEATURE_MAXRANGE]
            );

    return net_msg_encode_statputs( stat, &crc );
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
        vehicle_twizy_range_msgp(cmd, 0);

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

char vehicle_twizy_ca_msgp( int cmd, char stat )
{
    static WORD crc;

    sprintf( net_scratchpad,
            "MP-0 c%d,0,%u,%u"
            , cmd ? cmd : CMD_QueryChargeAlerts
            , sys_features[FEATURE_SUFFRANGE]
            , sys_features[FEATURE_SUFFSOC]
            );

    return net_msg_encode_statputs( stat, &crc );
}

BOOL vehicle_twizy_ca_cmd( BOOL msgmode, int cmd, char *arguments )
{
    if( cmd == CMD_SetChargeAlerts )
    {
        char *range = strtokpgmram( arguments, "," );
        char *soc = strtokpgmram( NULL, "," );

        sys_features[FEATURE_SUFFRANGE] = range ? atoi(range) : 0;
        sys_features[FEATURE_SUFFSOC] = soc ? atoi(soc) : 0;

        // store new alerts into EEPROM:
        par_set( PARAM_FEATURE_BASE + FEATURE_SUFFRANGE, range );
        par_set( PARAM_FEATURE_BASE + FEATURE_SUFFSOC, soc );
    }

    // CMD_QueryChargeAlerts
    if( msgmode )
        vehicle_twizy_ca_msgp(cmd, 0);

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
    ""
};

rom BOOL (*vehicle_twizy_sms_hfntable[])(BOOL premsg, char *caller, char *command, char *arguments) =
{
    &vehicle_twizy_debug_sms,
    &vehicle_twizy_stat_sms,
    &vehicle_twizy_range_sms,
    &vehicle_twizy_ca_sms,
    &vehicle_twizy_help_sms,
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
    char *p;

    car_type[0] = 'R'; // Car is type RT - Renault Twizy
    car_type[1] = 'T';
    car_type[2] = 0;
    car_type[3] = 0;
    car_type[4] = 0;

    car_linevoltage = 230; // fix
    car_chargecurrent = 10; // fix

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

    // Filter4 = unused
    RXF4SIDH = 0b00000000;
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
