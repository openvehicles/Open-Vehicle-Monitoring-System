/*
;    Project:       Open Vehicle Monitor System
;    Date:          2 Nov 2012
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

// Integer Miles <-> Kilometer conversions
// 1 mile = 1.609344 kilometers
// smalles error approximation: mi = (km * 10 - 5) / 16 + 1
// -- ATT: NO NEGATIVE VALUES ALLOWED DUE TO BIT SHIFTING!
#define KM2MI(KM)       ( ( ( (KM) * 10 - 5 ) >> 4 ) + 1 )
#define MI2KM(MI)       ( ( ( (MI) << 4 ) + 5 ) / 10 )

// Maybe not so Twizy specific feature extensions:
#define FEATURE_SUFFSOC      0x0A // Sufficient SOC feature
#define FEATURE_SUFFRANGE    0x0B // Sufficient range feature
#define FEATURE_MAXRANGE     0x0C // Maximum ideal range feature

#pragma udata overlay vehicle_overlay_data

//unsigned char can_lastspeedmsg[8];           // A buffer to store the last speed message
//unsigned char can_lastspeedrpt;              // A mechanism to repeat the tx of last speed message
//unsigned char k = 0;

unsigned char can_last_SOC = 0;              // sufficient charge SOC threshold helper
unsigned int can_last_idealrange = 0;        // sufficient charge range threshold helper

// Additional Twizy state variables:
unsigned int can_soc = 0;                    // detailed SOC (1/100 %)
unsigned int can_soc_min = 10000;            // min SOC reached during last discharge
unsigned int can_soc_max = 0;                // max SOC reached during last charge
unsigned int can_range = 0;                  // range in km
unsigned int can_speed = 0;                  // current speed in 1/100 km/h
signed int can_power = 0;                    // current power in W, negative=charging
unsigned long can_odometer = 0;              // odometer in km

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
         * CAN ID 0x155:
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
                car_SOC = can_soc / 100;

                // Remember maximum SOC for charging "done" distinction:
                if( can_soc > can_soc_max )
                    can_soc_max = can_soc;
                // ...and minimum SOC for range calculation during charging:
                if( can_soc < can_soc_min )
                    can_soc_min = can_soc;
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
             * CAN ID 0x597:
             */
            case 0x07:
                
                // VEHICLE state:
                //
                // Door state #1
                //	bit2 = 0x04 Charge port (open=1/closed=0)
                //	bit4 = 0x10 Charging (true=1/false=0)
                //	bit6 = 0x40 Hand brake applied (true=1/false=0)
                //	bit7 = 0x80 Car ON (true=1/false=0)
                //
                // ATT: net module needs bit4 to know about charging states!
                //
                // Twizy message:
                //  [0]: 0x00=not charging (?), 0x20=charging (?)
                //  [1] bit 4 = 0x10 CAN_STATUS_KEYON: 1 = Car ON (key switch)
                //  [1] bit 5 = 0x20 CAN_STATUS_CHARGING: 1 = Charging
                //  [1] bit 6 = 0x40 CAN_STATUS_OFFLINE: 1 = Switch-ON/-OFF phase

                can_status = can_databuffer[1];

                if( (can_status & 0x60) == 0x20 )
                    car_doors1 |= 0x14; // Charging ON, Port OPEN
                else
                    car_doors1 &= ~0x10; // Charging OFF

                if( can_status & CAN_STATUS_KEYON )
                    car_doors1 |= 0x80; // Car ON
                else
                    car_doors1 &= ~0x80; // Car OFF

                break;
                
                
                
            /*****************************************************
             * FILTER 2:
             * CAN ID 0x599:
             */
            case 0x09:
                
                // ODOMETER:
                if( can_databuffer[0] != 0xff )
                {
                    can_odometer = can_databuffer[3]
                               + ((unsigned long) can_databuffer[2] << 8)
                               + ((unsigned long) can_databuffer[1] << 16)
                               + ((unsigned long) can_databuffer[0] << 24);
                    // convert to miles/10:
                    car_odometer = KM2MI( can_odometer * 10 );
                }

                // RANGE: ... bad: firmware needs value in miles
                // = we loose precision ... change?
                // also we need to check for charging, as the Twizy
                // does not update range during charging
                if( ((car_doors1 & 0x04) == 0)
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

                    if( can_mileskm == 'M' )
                        car_speed = KM2MI( can_speed / 100 ); // miles/hour
                    else
                        car_speed = can_speed / 100; // km/hour
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
             * CAN ID 0x69F:
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
     * RESET workaround against "lost range" problem:
     */

    if( can_range == 0 )
    {
        // Read last known can_range from FEATURE13:
        can_range = sys_features[13];
    }
    else if( can_range != sys_features[13] )
    {
        // Write last known can_range into FEATURE13:
        sys_features[13] = can_range;
        sprintf( net_scratchpad, "%d", sys_features[13] );
        par_set( PARAM_FEATURE13, net_scratchpad );
    }


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
     * Charge notification + alerts:
     *
     * car_chargestate: 1=charging, 2=top off, 4=done, 13=preparing to charge, 21-25=stopped charging
     * car_chargesubstate: 0x07 = ? see net_msg.c
     *
     * Door state #1
     *	bit2 = 0x04 Charge port (open=1/closed=0)
     *	bit4 = 0x10 Charging (true=1/false=0)
     *	bit6 = 0x40 Hand brake applied (true=1/false=0)
     *	bit7 = 0x80 Car ON (true=1/false=0)
     *
     * ATT: bit 2 = 0x04 = needs to be set for net_sms_stat()!
     *
     */

    if( car_doors1 & 0x10 )
    {
        /*******************************************************************
         * CHARGING
         */

        // Calculate range during charging:
        // scale last known range from can_soc_min to can_soc
        if( (can_range > 0) && (can_soc > 0) && (can_soc_min > 0) )
        {
            unsigned long chg_range =
                (((float) can_range) / can_soc_min) * can_soc;

            if( chg_range > 0 )
                car_estrange = KM2MI( chg_range );

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
        }
    }


#ifdef OVMS_DEBUG

    // CAN debug log via DIAG port SIM908 command echo:
    // ATT: could be a problem for running data transfers,
    //      should not be enabled for production/live firmware images!
    net_puts_rom( "ATE1\r" );

    sprintf( net_scratchpad,
            (rom far char*) "# STS=%x SOC=%u RNG=%u SPD=%d PWR=%d\r\n"
            , can_status, can_soc, can_range, can_speed, can_power );
    net_puts_ram( net_scratchpad );

    sprintf( net_scratchpad,
            (rom far char*) "# DS1=%x CHG=%u SND=%x SOCMIN=%u SOCMAX=%d ESTRNG=%u\r\n"
            , car_doors1, car_chargestate, net_notify, can_soc_min, can_soc_max, car_estrange );
    net_puts_ram( net_scratchpad );

    /*sprintf( net_scratchpad,
            (rom far char*) "# FIX=%d LAT=%08lx LON=%08lx ALT=%d DIR=%d\r\n"
            , car_gpslock, car_latitude, car_longitude, car_altitude, car_direction );
    net_puts_ram( net_scratchpad );*/

    if( COMSTAT )
    {
        // CAN debug log via DIAG port SIM908 command echo:
        // ATT: could be a problem for running data transfers,
        //      should not be enabled for production/live firmware images!
        sprintf( net_scratchpad,
                (rom far char*) "# CAN COMSTAT=%x RXERRCNT=%u TXERRCNT=%u\r\n",
                COMSTAT, RXERRCNT, TXERRCNT );
        net_puts_ram( net_scratchpad );
    }

#endif


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
    vehicle_fn_poll0 = &vehicle_twizy_poll0;
    vehicle_fn_poll1 = &vehicle_twizy_poll1;
    vehicle_fn_ticker1 = &vehicle_twizy_state_ticker1;
    vehicle_fn_ticker60 = &vehicle_twizy_state_ticker60;

    return TRUE;
}
