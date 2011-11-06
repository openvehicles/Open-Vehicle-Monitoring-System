//    Project:       Open Vehicle Monitor System
//    Date:          6 November 2011
//
//    Changes:
//    1.0  Initial release
//
//    (C) 2011  Sonny Chen
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

#define DATA_COUNT 7
unsigned char state, field;
unsigned char DummyData[DATA_COUNT * 8]
        = {
    0x80, 0x35, 0x80, 0, 0, 0, 0x68, 0, // state of charge/SOC%/IdealL/IdealH///EstL/EstH
    0x83, 0, 0, 0, 0xFF, 0x48, 0xCC, 0x09, // longitude (HK)
    0x84, 0, 0, 0, 0x9C, 0x65, 0x2C, 0x32, // latitube (HK)
    0x88, 0x46, 0, 0, 0, 0, 0, 0, //current
    0x89, 0, 0xDC, 0, 0, 0, 0, 0, //voltage
    0x95, 0x01, 0, 0, 0, 0x00, 0, 0, // charge state/mode
    0x96, 0x10, 0, 0, 0, 0, 0, 0 // charging/doors, charging yes/no
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

    BRGCON1 = 0; // SET BAUDRATE to 1 Mbps
    BRGCON2 = 0xD2;
    BRGCON3 = 0x02;

    CIOCON = 0b00100000; // CANTX pin will drive VDD when recessive
    //CANCON = 0b01100000; // Listen only mode, Receive bufer 0

    ECANCON = 0b00000000;
    CANCON = 0b00000000; // Normal mode

}

void IO_CanWrite() {
    TXB0CON = 0;
    TXB0SIDL = 0b00000000; // Setup Filter and Mask so that only CAN ID 0x100 will be accepted
    TXB0SIDH = 0b00100000; // Set Filter to 0x100

    field = 0;
    TXB0D0 = DummyData[(state * 8) + field++];
    TXB0D1 = DummyData[(state * 8) + field++];
    TXB0D2 = DummyData[(state * 8) + field++];
    TXB0D3 = DummyData[(state * 8) + field++];
    TXB0D4 = DummyData[(state * 8) + field++];
    TXB0D5 = DummyData[(state * 8) + field++];
    TXB0D6 = DummyData[(state * 8) + field++];
    TXB0D7 = DummyData[(state * 8) + field++];

    TXB0DLC = 0b00001000; // data length (8)
    TXB0CON = 0b00001000; // mark for transmission

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
    /**********************main Loop*******************************/
    while (1) {
        IO_CanWrite();
        if (state++ > DATA_COUNT) state = 0;

        delay_ms(100); // wait for sending message
        
        // pause and blink before sending next frame
        if (COMSTATbits.TXBO || COMSTATbits.TXBP) {
            // canbus not connected
            PORTCbits.RC4 = 0;
            delay_ms(1000);
        }
        else if (TXB0CONbits.TXREQ)
        {
            // previous message still in buffer
            PORTCbits.RC4 = 1;
            delay_ms(200);
        } else {
            PORTCbits.RC4 ^= 1;
            delay_ms(100);
            PORTCbits.RC4 ^= 1;
        }
    }

} //end main
