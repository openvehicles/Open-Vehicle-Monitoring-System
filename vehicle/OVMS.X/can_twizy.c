/*
;    Project:       Open Vehicle Monitor System
;    Date:          2 Nov 2012
;
;    Changes:
;    1.0  Initial release
;    1.1  Basic Twizy integration (Michael Balzer)
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
unsigned char can_lastspeedmsg[8];           // A buffer to store the last speed message
unsigned char can_lastspeedrpt;              // A mechanism to repeat the tx of last speed message
unsigned char k = 0;
unsigned char can_minSOCnotified = 0;        // minSOC notified flag
unsigned int  can_granular_tick = 0;         // An internal ticker used to generate 1min, 5min, etc, calls
unsigned char can_mileskm = 'M';             // Miles of Kilometers

// Additional Twizy state variables:
unsigned int can_soc = 0;                    // detailed SOC (1/100 %)
unsigned int can_soc_max = 0;                // max SOC reached during last charge
unsigned int can_range = 0;                  // range in km
unsigned int can_speed = 0;                  // current speed in 1/100 km/h
signed int can_power = 0;                    // current power in W, negative=charging

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

    // Mask0 = 0x7ff = use all ID bits
    RXM0SIDH = 0b11111111;
    RXM0SIDL = 0b11100000;

    // Filter0 = 0x155:
    RXF0SIDH = 0b00101010;
    RXF0SIDL = 0b10100000;

    // Filter1 = 0x599:
    RXF1SIDH = 0b10110011;
    RXF1SIDL = 0b00100000;


    // RX buffer1 uses Mask RXM1 and filters RXF2, RXF3, RXF4, RXF5
    RXB1CON = 0b00000000;

    // Mask1 = unused
    RXM1SIDH = 0b00000000;
    RXM1SIDL = 0b00000000;

    // Filter2 = unused
    RXF2SIDL = 0b00000000;
    RXF2SIDH = 0b00000000;

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
    BRGCON1 = 0x01;
    BRGCON2 = 0xD2;
    BRGCON3 = 0x02;

    // 500 kbps -- tool recommendation + multisampling:
    //BRGCON1 = 0x00;
    //BRGCON2 = 0xFA;
    //BRGCON3 = 0x07;


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
    PIE3bits.RXB1IE = 1; // CAN Receive Buffer 1 Interrupt Enable bit
    PIE3bits.RXB0IE = 1; // CAN Receive Buffer 0 Interrupt Enable bit
    IPR3 = 0b00000011; // high priority interrupts for Buffers 0 and 1

    p = par_get(PARAM_MILESKM);
    can_mileskm = *p;
    can_lastspeedmsg[0] = 0;
    can_lastspeedrpt = 0;
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


    if( CANfilter == 0 )
    {
        // Filter 0 = CAN ID 0x155

        // SOC:
        can_soc = ( ((unsigned int) can_databuffer[4] << 8) | can_databuffer[5] ) >> 2;
        car_SOC = can_soc / 100;

        // Remember maximum SOC for charging "done" distinction:
        if( can_soc > can_soc_max )
            can_soc_max = can_soc;

        // POWER:
        can_power = 2000 - (signed int) ( ((unsigned int) (can_databuffer[1] & 0x0f) << 8) | can_databuffer[2] );

    }
    else // CANfilter == 1
    {
        // Filter 1 = CAN ID 0x599

        // RANGE: ... bad: firmware needs value in miles
        // = we loose precision ... change?
        // 1 mile = 1.609344 kilometers
        if( can_databuffer[5] != 0xff )
        {
            can_range = can_databuffer[5];
            // Twizy only tells us estrange, but STAT? uses idealrange anyway
            // and Twizy idealrange varies widely, needs parameter => todo
            car_estrange = can_range * 1000 / 1609;
            car_idealrange = car_estrange;
        }

        // SPEED:
        can_speed = ((unsigned int) can_databuffer[6] << 8) | can_databuffer[7];
        if( can_speed == 0xffff )
            can_speed = 0;

        if( can_mileskm == 'M' )
            car_speed = can_speed * 1000 / 1609;    // speed in miles/hour
        else
            car_speed = can_speed;                  // speed in km/hour
        
    }


}



// Poll buffer 1: unused
void can_poll1(void)
{
    unsigned char CANfilter = RXB1CON & 0x07;
    //unsigned char CANsidh = RXB1SIDH;
    //unsigned char CANsidl = RXB1SIDL & 0b11100000;

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
     * car_chargestate: 1=charging, 2=top off, 4=done, 13=preparing to charge, 21-25=stopped charging
     * car_chargesubstate: unklar: 0x07 scheint was zu bedeuten, siehe net_msg.c
     * car_doors1: 0x04 = Ladeklappe offen, muss gesetzt sein für net_sms_stat()!
     *
     * Twizy: no direct charge state on CAN, so we need to derive it
     *  from speed and power (power jitters 0..-1)
     */
    if( can_speed == 0 && can_power < -1 )
    {
        // *** CHARGING ***

        // If charging has previously been interrupted...
        if( car_chargestate == 21 )
        {
            // ...send charge alert:
            net_req_notification( NET_NOTIFY_CHARGE );
        }

        // If we've not been charging before...
        if( car_chargestate != 1 )
        {
            // ...enter state 1=charging
            car_chargestate = 1;
            car_doors1 |= 0x04;
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
            // yes, check if we've reached 100% SOC:
            if( can_soc_max >= (100 * 100) )
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
        }

        // Check if we're discharging now:
        else if( car_doors1 & 0x04 && can_soc < can_soc_max )
        {
            // yes, so we virtually close our virtual charge port ;-)
            // this needs to happen here as net_sms_stat() needs the flag
            // plus some time to send the proper message
            car_doors1 &= ~0x04;
            // ...and assume "done" (default state)
            car_chargestate = 4;
        }

    }


#ifdef OVMS_DEBUG
    // CAN debug log via DIAG port SIM908 command echo:
    // ATT: could be a problem for running data transfers,
    //      should not be enabled for production/live firmware images!
    net_puts_rom( "ATE1\r" );
    sprintf( net_scratchpad,
            (rom far char*) "# ERR=%u SOC=%u RNG=%u SPD=%d PWR=%d CHG=%u\r\n",
            RXERRCNT, can_soc, can_range, can_speed, can_power, car_chargestate );
    net_puts_ram( net_scratchpad );
    sprintf( net_scratchpad,
            (rom far char*) "# FIX=%d LAT=%08lx LON=%08lx ALT=%d DIR=%d\r\n",
            car_gpslock, car_latitude, car_longitude, car_altitude, car_direction );
    net_puts_ram( net_scratchpad );
#endif

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

  // check minSOC
  minSOC = sys_features[FEATURE_MINSOC];
  if ((can_minSOCnotified == 0) && (car_SOC < minSOC))
    {
    net_req_notification(NET_NOTIFY_STAT);
    can_minSOCnotified = 1;
    }
  else if ((can_minSOCnotified == 1) && (car_SOC > minSOC + 2))
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
