/*********************************************************************
 *
 *   This file implements the library functionality of UART.
 *   It adds Transmit and receive functionality , circular buffer and 
 *	 interrupt functionality.
 *   
 *********************************************************************
 * FileName:        UARTIntC.h
 * Dependencies:    UARTIntC.def
 * Processor:       PIC18XXX
 * Compiler:		MCC18
 * Assembler:       MPASMWIN 03.20.07
 * Linker:          MPLINK 3.20
 * Company:         Microchip Technology, Inc.
 *
 * Software License Agreement
 *
 * The software supplied herewith by Microchip Technology Incorporated
 * (the "Company") is intended and supplied to you, the Company’s
 * customer, for use solely and exclusively with products manufactured
 * by the Company. 
 *
 * The software is owned by the Company and/or its supplier, and is 
 * protected under applicable copyright laws. All rights are reserved. 
 * Any use in violation of the foregoing restrictions may subject the 
 * user to criminal sanctions under applicable laws, as well as to 
 * civil liability for the breach of the terms and conditions of this 
 * license.
 
 * THIS SOFTWARE IS PROVIDED IN AN "AS IS" CONDITION. NO WARRANTIES, 
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED 
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT, 
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR 
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 *
 *
 *
 * Author               Date        Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Reddy TE			Sept. 19,2003		Original Release
 * 
 * Reddy TE			Sept. 24,2003		Changed return values of func-
 *										tions to unsigned char
 ********************************************************************/


#ifndef _UARTIntC_H
#define	_UARTIntC_H

#include "UARTIntC.def"


/* Constants found in .def file are given readable names*/
#define TX_BUFFER_SIZE UARTINTC_TX_BUFFER_SIZE
#define RX_BUFFER_SIZE UARTINTC_RX_BUFFER_SIZE

#ifdef UARTINTC_TXON
  #define TXON 1
#elif UARTINTC_TXOFF
  #define TXON 0
#endif

#ifdef UARTINTC_RXON
  #define RXON 1
#elif UARTINTC_RXOFF
  #define RXON 0
#endif

#define TXON_AND_RXON ((TXON) & (RXON))
#define	TXOFF_AND_RXON ((!(TXON)) & (RXON))


// User can change the following values and accordingly
// the ISR location in the main application.
//   Can they be MPAM module parameters?		
#define BRGH_VAL 1
#define	TX_PRIORITY_ON	0
#define	RX_PRIORITY_ON	0

// More error check to be done and the following
// code can be modified.
// If SPBRG is out of range, it won't let the 
// main application be compiled and linked.
#define SPBRG_V1  (UART_CLOCK_FREQ / UARTINTC_BAUDRATE)
#define SPBRG_V2  SPBRG_V1/16
#define SPBRG_VAL  (SPBRG_V2 - 1)
# if (SPBRG_VAL > 255)
  #error Calculated SPBRG value is out of range
#elif (SPBRG_VAL < 10)
  #error Calculated SPBRG value is out of range
#endif

struct status
{
	unsigned UARTIntTxBufferFull  :1;
	unsigned UARTIntTxBufferEmpty :1;
	unsigned UARTIntRxBufferFull  :1;
	unsigned UARTIntRxBufferEmpty :1;
	unsigned UARTIntRxOverFlow :1;
	unsigned UARTIntRxError:1;				
};

extern volatile struct status vUARTIntStatus;

//  variables representing status of transmission buffer and 
//  transmission buffer it self are declared below

#if TXON
extern volatile unsigned char vUARTIntTxBuffer[TX_BUFFER_SIZE];
extern volatile unsigned char vUARTIntTxBufDataCnt;
extern volatile unsigned char vUARTIntTxBufWrPtr;
extern volatile unsigned char vUARTIntTxBufRdPtr;
#endif

// variables referring the status of receive buffer.

#if	RXON
extern volatile unsigned char vUARTIntRxBuffer[RX_BUFFER_SIZE];
extern volatile unsigned char vUARTIntRxBufDataCnt;
extern volatile unsigned char vUARTIntRxBufWrPtr;
extern volatile unsigned char vUARTIntRxBufRdPtr;
#endif

//  functions offered by this module
#if	RXON
// function returns a character from receive buffer
unsigned char UARTIntGetChar(unsigned char*);

// function returns the number of characters in receive buffer
unsigned char UARTIntGetRxBufferDataSize(void);

#endif

#if	TXON
// function to put a character in Transmit buffer
unsigned char UARTIntPutChar(unsigned char);

// function returns size of the empty section of Transmit buffer
unsigned char UARTIntGetTxBufferEmptySpace(void);
#endif

// Initialisation of the module
void UARTIntInit(void);
// Interrupt service routine supplied by the module.This need to be 
// called from ISR of the main program.
void UARTIntISR(void);


// Other useful macros

#define mDisableUARTTxInt()				PIE1bits.TXIE = 0
#define mEnableUARTTxInt()				PIE1bits.TXIE = 1

#define mDisableUARTRxInt() 			PIE1bits.RCIE = 0
#define mEnableUARTRxInt()  			PIE1bits.RCIE = 1

#define mSetUARTRxIntHighPrior() \
										RCONbits.IPEN = 1;\
										IPR1bits.RCIP = 1										
#define mSetUARTRxIntLowPrior() 		IPR1bits.RCIP = 0	

#define mSetUARTTxIntHighPrior()\
										RCONbits.IPEN = 1;\
										IPR1bits.TXIP = 1
#define mSetUARTTxIntLowPrior()			IPR1bits.TXIP = 0


#define mSetUART_BRGHHigh()				TXSTAbits.BRGH = 1
#define mSetUART_BRGHLow()				TXSTAbits.BRGH = 0

#define mSetUART_SPBRG(iBRGValue)\
										RCSTAbits.SPEN = 0;\
										SPBRG = iBRGValue;\
										RCSTAbits.SPEN = 1																	

#define mSetUARTBaud(iBaudRate)\
		do{\
			\#define SPBRG_V11  (UART_CLOCK_FREQ / UARTINTC_BAUDRATE)\
			\#define SPBRG_V21  SPBRG_V11/16\
			\#define SPBRG_VAL  (SPBRG_V21 - 1)\
			\#if (SPBRG_VAL > 255)\
			  \#error Calculated SPBRG value is out of range\
			\#elif (SPBRG_VAL < 10)\
			  \#error Calculated SPBRG value is out of range\
			\#endif\
			RCSTAbits.SPEN = 0;\
			SPBRG = iBaudRate;\
			RCSTAbits.SPEN = 1;\
		}while(false)

#endif // #ifndef _UARTIntC_H