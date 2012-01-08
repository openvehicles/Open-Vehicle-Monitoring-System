//    Project:       Open Vehicle Monitor System
//    Date:          6 November 2011
//
//    Changes:
//    1.0  Initial release
//
//    (C) 2011 OVMS - OpenVehicles.com
//
// Based on information and analysis provided by Scott451, Michael Stegen,
// and others at the Tesla Motors Club forums, as well as personal analysis
// of the CAN bus on a 2011 Tesla Roadster.
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
//
// This script turns a regulator OVMS car module into a car simulator by transmitting
// dummy data over CAN bus. The purpose of the script is to ease debugging while
// working on the OVMS code away from a car.

#include "p18f2680.h"
//#include <stdio.h>

// Configuration settings

#pragma	config	FCMEN = ON,  IESO = ON // Fail-safe clock enable, two-speed startup enable
#pragma	config	PWRT = ON,  BOREN = OFF
#pragma config OSC = HS
#if defined(__DEBUG)
#pragma config MCLRE = ON  // DEBUG MODE FLAGS
#pragma	config DEBUG = ON
#else
#pragma config MCLRE = OFF
#pragma	config DEBUG = OFF
#endif
#pragma	config WDT = OFF//  WDTPS = 32768 //Watchdog Timer disabled during debugging
#pragma	config 	LPT1OSC = OFF, PBADEN = OFF
#pragma	config	XINST = OFF, BBSIZ = 1024, LVP = OFF, STVREN = ON
#pragma	config	CP0 = OFF,	CP1 = OFF,	CP2 = OFF,	CP3 = OFF
#pragma	config	CPB = OFF,	CPD = OFF
#pragma	config	WRT0 = OFF,	WRT1 = OFF,	WRT2 = OFF,	WRT3 = OFF
#pragma	config	WRTB = OFF,	WRTC = OFF,	WRTD = OFF
#pragma	config	EBTR0 = OFF, EBTR1 = OFF, EBTR2 = OFF, EBTR3 = OFF
#pragma	config	EBTRB = OFF


//***************************************************************
//Function Proto types
//**************************************************************
void Init_Micro(void);
void Init_CAN(void);
void IO_CanWrite(void);
void Delay1KTCYx(unsigned char);

#define DATA_COUNT 17u
unsigned char state, field;
unsigned int CANIDMap[4]
        = { 0x100, 0x344, 0x400, 0x402 };
unsigned char DummyData[DATA_COUNT * 9]
        = {
0x00, 0x83, 0x03, 0x00, 0x00, 0x3F, 0x71, 0xD1, 0x09,	//	VDS GPS latitude (latitude 22.341710)
0x00, 0x84, 0x03, 0x00, 0x00, 0xCC, 0xB2, 0x2E, 0x32,	//	VDS GPS longitude (longitude 114.192875)
0x00, 0xA4, 0x53, 0x46, 0x5A, 0x52, 0x45, 0x38, 0x42,	//	VDS VIN1 (vin SFZRE8B)
0x00, 0xA5, 0x31, 0x35, 0x42, 0x33, 0x39, 0x39, 0x39,	//	VDS VIN2 (vin 15B3999)
0x00, 0xA6, 0x39, 0x39, 0x39, 0x00, 0x00, 0x00, 0x00,	//	VDS VIN3 (vin 999)
0x00, 0x80, 0x56, 0xA5, 0x00, 0x8C, 0x00, 0x90, 0x00,	//	VDS Range (SOC 86%) (ideal 165) (est 144)
0x00, 0x88, 0x00, 0x8C, 0x00, 0xFF, 0xFF, 0x46, 0x00,	//	VDS Charger settings (limit 70A) (current 0A) (duration 140mins)
0x00, 0x89, 0x1D, 0xFF, 0xFF, 0x7F, 0xFF, 0x00, 0x00,	//	VDS Charger interface (speed 29mph) (vline 65535V) (Iavailable 255A)
0x00, 0x95, 0x04, 0x07, 0x64, 0x57, 0x02, 0x0E, 0x1C,	//	VDS Charger v1.5 (done) (conn-pwr-cable) (standard)
0x00, 0x96, 0x88, 0x20, 0x02, 0xA1, 0x0C, 0x00, 0x00,	//	VDS Doors (l-door: closed) (r-door: closed) (chargeport: closed) (pilot: true) (charging: false) (bits 88)
0x00, 0x9C, 0x01, 0xC6, 0x01, 0x00, 0x00, 0x00, 0x00,	//	VDS Trip->VDS (trip 45.4miles)
0x00, 0x81, 0x00, 0x00, 0x00, 0x57, 0x46, 0xBA, 0x4E,	//	VDS Time/Date UTC (time Wed Nov  9 09:22:31 2011)
0x00, 0x97, 0x00, 0x88, 0x01, 0x5F, 0x97, 0x00, 0x00,	//	VDS Odometer (miles: 3875.1 km: 6236.4)
0x00, 0xA3, 0x26, 0x49, 0x00, 0x00, 0x00, 0x1B, 0x00,	//	VDS TEMPS (Tpem 38) (Tmotor 73) (Tbattery 27)
0x01, 0x54, 0x40, 0x53, 0x42, 0x72, 0x45, 0x72, 0x45,	//	TPMS (f-l 30psi,24C f-r 30psi,26C r-l 41psi,29C r-r 41psi,29C)
0x02, 0x02, 0xA9, 0x41, 0x80, 0xE6, 0x80, 0x55, 0x00,	//	DASHBOARD AMPS
0x03, 0xFA, 0x03, 0xC4, 0x5E, 0x97, 0x00, 0xC6, 0x01	//	402?? Odometer (miles: 3875.0 km: 6236.2) (trip 45.4miles)

};

// Delay of 1ms calculation
// Cycles = (TimeDelay * Fosc) / 4

void delay_ms(long t) // delays t millisecs
{
    do {
        // Cycles = (1sec * 20MHz) / 4 / 1000ms
        // Cycles = 5,000
        Delay1KTCYx(5); // time will be a small bit over because of overhead, close % wise
    } while (--t);
}

/****************************************************************/
void Init_Micro() {
    // I/O configuration PORT A
    PORTA = 0x00; // Initialise port A
    ADCON1 = 0x0F; // Switch off A/D converter
    // I/O configuration PORT B
    TRISB = 0xFF;
    // I/O configuration PORT C
    TRISC = 0x80; // Port C RC0-6 output, RC7 input
    PORTC = 0x00;
}

/*****************Initialize CAN Module************************/
void Init_CAN() {
    // init
    CANCON = 0b10010000; // Initialize CAN
    while (!CANSTATbits.OPMODE2); // Wait for Configuration mode

    // We are now in Configuration Mode
    RXB0CON = 0b00000000; // Receive all valid messages

    RXF0SIDL = (0x100 & 0x7) << 5; // Setup Filter and Mask so that only CAN ID 0x100 will be accepted
    RXF0SIDH = 0x100 >> 3; // Set Filter to 0x100
    RXM0SIDL = 0b11100000;
    RXM0SIDH = 0b11111111; // Set Mask to 0x7ff

    RXF1SIDL = (0x344 & 0x7) << 5; // Setup Filter and Mask so that only CAN ID 0x344 will be accepted
    RXF1SIDH = 0x344 >> 3; // Set Filter to 0x344
    RXM1SIDL = 0b11100000;
    RXM1SIDH = 0b11111111; // Set Mask to 0x7ff

    RXF2SIDL = (0x400 & 0x7) << 5; // Setup Filter and Mask so that only CAN ID 0x400 will be accepted
    RXF2SIDH = 0x400 >> 3; // Set Filter to 0x400

    RXF3SIDL = (0x402 & 0x7) << 5; // Setup Filter and Mask so that only CAN ID 0x402 will be accepted
    RXF3SIDH = 0x402 >> 3; // Set Filter to 0x402

    BRGCON1 = 0; // SET BAUDRATE to 1 Mbps
    BRGCON2 = 0xD2;
    BRGCON3 = 0x02;

    CIOCON = 0b00100000; // CANTX pin will drive VDD when recessive
    //CANCON = 0b01100000; // Listen only mode, Receive bufer 0

    ECANCON = 0b00000000;
    CANCON = 0b00000000; // Normal mode

}

void IO_CanWrite() {
    // re-map DummyData's 8-bit field 0 into an 11-bit CAN ID
    unsigned int message_id = CANIDMap[DummyData[(state * 9)]];

    TXB0CON = 0;            // Reset TX buffer

    TXB0SIDL = (message_id & 0x7) << 5;  // Set CAN ID to 0x100
    TXB0SIDH = message_id >> 3;

    field = 1; // field 0 = CAN ID, data starts from field 1
    TXB0D0 = DummyData[(state * 9) + field++];
    TXB0D1 = DummyData[(state * 9) + field++];
    TXB0D2 = DummyData[(state * 9) + field++];
    TXB0D3 = DummyData[(state * 9) + field++];
    TXB0D4 = DummyData[(state * 9) + field++];
    TXB0D5 = DummyData[(state * 9) + field++];
    TXB0D6 = DummyData[(state * 9) + field++];
    TXB0D7 = DummyData[(state * 9) + field++];

    TXB0DLC = 0b00001000; // data length (8)
    TXB0CON |= 0b00001000; // mark for transmission

}

//************************EOF***********************************************



//===============================================================
//main Code
//===============================================================

void main() {
    PORTCbits.RC4 = 1;
    Init_Micro();
    Init_CAN();
    state = 0;
    PORTCbits.RC4 = 0;

    // send frame
    IO_CanWrite();

    /**********************main Loop*******************************/
    while (1) {
        if (RXB0CONbits.RXFUL)
        {
            //incoming CAN message

            //blink green LED
            PORTCbits.RC5 = 1;
            delay_ms(50);
            PORTCbits.RC5 = 0;

            // clear RX flag
            RXB0CONbits.RXFUL = 0;
        }

        // check bus status
        if (COMSTATbits.TXBO || COMSTATbits.TXBP || TXB0CONbits.TXERR) {
            // canbus off / passive (not connected)
            PORTCbits.RC4 = 0;
            //delay_ms(1000); // sleep for 1 second
        }
        else if (TXB0CONbits.TXREQ)
        {
            // previous message still in buffer
            PORTCbits.RC4 = 1;
            //delay_ms(200); // sleep
        } else {
            // canbus status normal, blink red LED
            PORTCbits.RC4 = 1;
            delay_ms(50);
            PORTCbits.RC4 = 0;

            if (++state >= DATA_COUNT) state = 0;
            // send frame
            IO_CanWrite();
        }

        delay_ms(50); // pause before sending next message
    }

} //end main
