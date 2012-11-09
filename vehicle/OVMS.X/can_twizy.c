/*
;    Project:       Open Vehicle Monitor System
;    Date:          2 Nov 2012
;
;    Changes:
;    1.0  Initial release
;    1.1  2 Nov 2012 (Michael Balzer):
;           - Basic Twizy integration
;    1.2  9 Nov 2012 (Michael Balzer):
;           - CAN lockups fixed
;           - CAN data validation
;           - Charge+Key status from CAN 0x597 => reliable charge stop detection
;           - Range updates while charging
;           - Odometer
;           - Suppress SOC alert until CAN status valid
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

#pragma udata

unsigned char can_datalength;                // The number of valid bytes in the can_databuffer
unsigned char can_databuffer[8];             // A buffer to store the current CAN message

//unsigned char can_lastspeedmsg[8];           // A buffer to store the last speed message
//unsigned char can_lastspeedrpt;              // A mechanism to repeat the tx of last speed message
//unsigned char k = 0;

unsigned char can_minSOCnotified = 0;        // minSOC notified flag
unsigned int  can_granular_tick = 0;         // An internal ticker used to generate 1min, 5min, etc, calls
unsigned char can_mileskm = 'M';             // Miles of Kilometers

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
// CAN Interrupt Service Routine (High Priority)
//
// Interupts here will interrupt Uart Interrupts
//

void high_isr(void);

#pragma code can_int_service = 0x08
void can_int_service(void)
  {
  _asm goto high_isr _endasm
  }

#pragma code
#pragma	interrupt high_isr
void high_isr(void)
  {
  // High priority CAN interrupt
    if (RXB0CONbits.RXFUL) can_poll0();
    if (RXB1CONbits.RXFUL) can_poll1();
    PIR3=0;     // Clear Interrupt flags
  }

////////////////////////////////////////////////////////////////////////
// can_initialise()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to initialise itself.
//
void can_initialise(void)
{
    char *p;

    car_type[0] = 'R'; // Car is type RT - Renault Twizy
    car_type[1] = 'T';
    car_type[2] = 0;

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

    // Filter3 = unused
    RXF3SIDL = 0b00000000;
    RXF3SIDH = 0b00000000;

    // Filter4 = unused
    RXF4SIDL = 0b00000000;
    RXF4SIDH = 0b00000000;

    // Filter5 = unused
    RXF5SIDL = 0b00000000;
    RXF5SIDH = 0b00000000;


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

    RCONbits.IPEN = 1; // Enable Interrupt Priority
    PIE3bits.RXB0IE = 1; // CAN Receive Buffer 0 Interrupt Enable bit
    PIE3bits.RXB1IE = 1; // CAN Receive Buffer 1 Interrupt Enable bit
    IPR3 = 0b00000011; // high priority interrupts for Buffers 0 and 1

    p = par_get(PARAM_MILESKM);
    can_mileskm = *p;
    //can_lastspeedmsg[0] = 0;
    //can_lastspeedrpt = 0;
}


////////////////////////////////////////////////////////////////////////
// can_poll()
// This function is an entry point from the main() program loop, and
// gives the CAN framework an opportunity to poll for data.
//

// Poll buffer 0:
//  Filter 0 = ID 155 = SOC
//  Filter 1 = ID 599 = Range
void can_poll0(void)
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

}



// Poll buffer 1: unused
void can_poll1(void)
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
                    car_odometer = ( can_odometer * 100 - 50 ) >> 4;
                }

                // RANGE: ... bad: firmware needs value in miles
                // = we loose precision ... change?
                // 1 mile = 1.609344 kilometers
                // nearest approximation: mi = (km * 10 - 5) / 16
                // also we need to check for charging, as the Twizy
                // does not update range during charging
                if( ((car_doors1 & 0x04) == 0) && (can_databuffer[5] != 0xff) )
                {
                    can_range = can_databuffer[5];
                    // Twizy only tells us estrange, but STAT? uses idealrange anyway
                    // and Twizy idealrange varies widely, needs parameter => todo
                    car_estrange = ( (unsigned int) can_range * 10 - 5 ) >> 4;
                    car_idealrange = car_estrange;
                }

                // SPEED:
                new_speed = ((unsigned int) can_databuffer[6] << 8) + can_databuffer[7];
                if( new_speed != 0xffff )
                {
                    can_speed = new_speed;

                    if( can_mileskm == 'M' )
                        car_speed = ( can_speed / 100 * 10 - 5 ) >> 4; // miles/hour
                    else
                        car_speed = can_speed / 100; // km/hour
                }

                break;
                
        }

    }

}



////////////////////////////////////////////////////////////////////////
// can_state_ticker1()
// State Model: Per-second ticker
// This function is called approximately once per second, and gives
// the state a timeslice for activity.
//
void can_state_ticker1(void)
{
    if (car_stale_ambient>0) car_stale_ambient--;
    if (car_stale_temps>0)   car_stale_temps--;
    if (car_stale_gps>0)     car_stale_gps--;
    if (car_stale_tpms>0)    car_stale_tpms--;


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
        // *** CHARGING ***

        // Calculate range during charging:
        // scale last known range from can_soc_min to can_soc
        if( can_soc > 0 && can_soc_min > 0 )
        {
            car_estrange = ( (unsigned long)
                    ( (((float) can_range) / can_soc_min) * can_soc )
                    * 10 - 5 ) >> 4;
            car_idealrange = car_estrange;
        }

        // If charging has previously been interrupted...
        if( car_chargestate == 21 )
        {
            // ...send charge alert:
            net_req_notification( NET_NOTIFY_CHARGE );
        }

        // If we've not been charging before...
        if( car_chargestate != 1 )
        {
            // ...enter state 1=charging:
            car_chargestate = 1;

            // reset SOC max:
            can_soc_max = can_soc;

            // Send charge stat:
            net_req_notification( NET_NOTIFY_STAT );
        }

    }
    else
    {
        // *** NOT CHARGING ***

        // Check if we've been charging before:
        if( car_chargestate == 1 )
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

}


////////////////////////////////////////////////////////////////////////
// can_state_ticker60()
// State Model: Per-minute ticker
// This function is called approximately once per minute (since state
// was first entered), and gives the state a timeslice for activity.
//
void can_state_ticker60(void)
{
    int minSOC;

    // Check CAN status:
    if( can_status == 0 )
        return; // no valid CAN data yet

    // check minSOC
    minSOC = sys_features[FEATURE_MINSOC];
    if( (can_minSOCnotified == 0) && (car_SOC < minSOC) )
    {
        net_req_notification(NET_NOTIFY_STAT);
        can_minSOCnotified = 1;
    }
    else if( (can_minSOCnotified == 1) && (car_SOC > minSOC + 2) )
    {
        // reset the alert sent flag when SOC is 2% point higher than threshold
        can_minSOCnotified = 0;
    }
}

////////////////////////////////////////////////////////////////////////
// can_state_ticker300()
// State Model: Per-5-minute ticker
// This function is called approximately once per five minutes (since
// state was first entered), and gives the state a timeslice for activity.
//
void can_state_ticker300(void)
  {
  }

////////////////////////////////////////////////////////////////////////
// can_state_ticker600()
// State Model: Per-10-minute ticker
// This function is called approximately once per ten minutes (since
// state was first entered), and gives the state a timeslice for activity.
//
void can_state_ticker600(void)
  {
  }

////////////////////////////////////////////////////////////////////////
// can_ticker()
// This function is an entry point from the main() program loop, and
// gives the CAN framework a ticker call approximately once per second.
//
void can_ticker(void)
  {
  // This ticker is called once every second
  can_granular_tick++;
  can_state_ticker1();
  if ((can_granular_tick % 60)==0)
    can_state_ticker60();
  if ((can_granular_tick % 300)==0)
    can_state_ticker300();
  if ((can_granular_tick % 600)==0)
    {
    can_state_ticker600();
    can_granular_tick -= 600;
    }
  }

////////////////////////////////////////////////////////////////////////
// can_ticker10th()
// This function is an entry point from the main() program loop, and
// gives the CAN framework a ticker call approximately ten times per
// second.
//
void can_ticker10th(void)
  {
  }

void can_idlepoll(void)
  {
  }

void can_tx_wakeup(void)
  {
  }

void can_tx_wakeuptemps(void)
  {
  }

void can_tx_setchargemode(unsigned char mode)
  {
  }

void can_tx_setchargecurrent(unsigned char current)
  {
  }

void can_tx_startstopcharge(unsigned char start)
  {
  }

void can_tx_lockunlockcar(unsigned char mode, char *pin)
  {
  }

void can_tx_timermode(unsigned char mode, unsigned int starttime)
  {
  }

void can_tx_homelink(unsigned char button)
  {
  }
