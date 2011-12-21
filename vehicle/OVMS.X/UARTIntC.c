/*********************************************************************
 *
 *   This file implements the functionality of UART with user defined 
 *	 circular buffer. It has the implementation of functions namely 
 *	 UARTIntPutChar, UARTIntGetChar and UARTIntISR . These functions 
 *	 are the core part of UART MPAM module. Additional functions will
 *   be added in future as more desired functionality is identified.
 *   
 *********************************************************************
 * FileName:        UARTIntC.C
 * Dependencies:    UARTIntC.h
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
 
#include "UARTIntC.h"

// target specific file will go here
#include <p18f2680.h>

// status flags of receive and transmit buffers
struct status vUARTIntStatus;
		
// variable definitions
#if TXON
unsigned char vUARTIntTxBuffer[TX_BUFFER_SIZE];
unsigned char vUARTIntTxBufDataCnt;
unsigned char vUARTIntTxBufWrPtr;
unsigned char vUARTIntTxBufRdPtr;
#endif

#if	RXON
unsigned char vUARTIntRxBuffer[RX_BUFFER_SIZE];
unsigned char vUARTIntRxBufDataCnt;
unsigned char vUARTIntRxBufWrPtr;
unsigned char vUARTIntRxBufRdPtr;
#endif

/*********************************************************************
 * Function:        	void UARTIntInit(void)
 * PreCondition:    	None
 * Input:           	None
 * Output:          	None
 * Side Effects:    	None
 * Stack Requirements:	1 level deep
 * Overview:        	This function initialises UART peripheral.This
 *						function need to be called before using 
 *						UARTIntPutChar and UARTIntGetChar functions to 
 *                      send and receive the characters.
 ********************************************************************/
void UARTIntInit(void)
{
	// Intialising the status variables and circular buffer 
	// variables .
	#if TXON
		vUARTIntStatus.UARTIntTxBufferFull = 0;
		vUARTIntStatus.UARTIntTxBufferEmpty = 1;	
		vUARTIntTxBufDataCnt = 0;
		vUARTIntTxBufWrPtr = 0;
		vUARTIntTxBufRdPtr = 0;
	#endif
	#if	RXON
		vUARTIntStatus.UARTIntRxBufferFull = 0;
		vUARTIntStatus.UARTIntRxBufferEmpty = 1;
		vUARTIntStatus.UARTIntRxError = 0;
		vUARTIntStatus.UARTIntRxOverFlow = 0;
		vUARTIntRxBufDataCnt = 0;
		vUARTIntRxBufWrPtr = 0;
		vUARTIntRxBufRdPtr = 0;	
	#endif	
	    
    /* Initialising BaudRate value */
              SPBRG = SPBRG_VAL;
              TXSTAbits.BRGH = BRGH_VAL;



	/* Initialising TX/RX port pins */
	#if	TXON
		TRISCbits.TRISC6 = 0;		
	#endif	
	#if RXON
		TRISCbits.TRISC7 = 1;
	#endif

	/* Setting priority */
	#if	TX_PRIORITY_ON
		RCONbits.IPEN = 1;
		IPR1bits.TXIP = 1;
	#else
		IPR1bits.TXIP = 0;
	#endif					
	#if	RX_PRIORITY_ON
		RCONbits.IPEN = 1;
		IPR1bits.RCIP = 1;
	#else
		IPR1bits.RCIP = 0;
	#endif 		

	/* Enabling Transmitter or Receiver */
	#if	TXON
		TXSTAbits.TXEN = 1;
	#endif	
	#if	RXON
		RCSTAbits.CREN = 1;
	#endif
	
	/* Enabling Serial Port	*/
	RCSTAbits.SPEN = 1;
	
	/* Enable the TX and RX. Interrupt */
	#if	TXON
		PIE1bits.TXIE = 1;
	#endif	
	#if	RXON
		PIE1bits.RCIE = 1;
	#endif

	/* Setting Global interrupt pins */
        #if ((TX_PRIORITY_ON)|(RX_PRIORITY_ON))
	INTCONbits.GIEH = 1;        // Enable Interrupt Priority
	INTCONbits.GIEL = 1;
	#else
	INTCONbits.GIE = 1;
	INTCONbits.PEIE = 1;
	#endif
}

/*********************************************************************
 * Function:        	unsigned char UARTIntPutChar(unsigned char)
 * PreCondition:    	UARTIntInit()function should have been called.
 * Input:           	unsigned char
 * Output:          	unsigned char
 *							  0 - single character is successfully 
 *								  added to transmit buffer.
 *							  1 - transmit buffer is full and the 
 *								  character could not be added to 
 *								  transmit buffer.
 *								  
 * Side Effects:    	None
 * Stack Requirements: 	1 level deep
 * Overview:        	This function puts the data in to transmit 
 *						buffer. Internal implementation wise , 
 *						it places the argument data in transmit buffer
 *						and updates the data count and write pointer 
 *						variables. 
 *
 ********************************************************************/
#if	TXON
unsigned char UARTIntPutChar(unsigned char chCharData)
{
	/* check if its full , if not add one */
	/* if not busy send data */
	
	if(vUARTIntStatus.UARTIntTxBufferFull)
		return 0;
   
    //critical code	, disable interrupts
	PIE1bits.TXIE = 0;	
	vUARTIntTxBuffer[vUARTIntTxBufWrPtr] = chCharData;
	vUARTIntStatus.UARTIntTxBufferEmpty = 0;
	vUARTIntTxBufDataCnt ++;
	if(vUARTIntTxBufDataCnt == TX_BUFFER_SIZE)
	vUARTIntStatus.UARTIntTxBufferFull = 1;
	vUARTIntTxBufWrPtr++;	
	if(vUARTIntTxBufWrPtr == TX_BUFFER_SIZE)
		vUARTIntTxBufWrPtr = 0;								
	PIE1bits.TXIE = 1;	
	
	return 1;
}

/*********************************************************************
 * Function:          unsigned char UARTIntGetTxBufferEmptySpace(void)
 * PreCondition:    	UARTIntInit()function should have been called.
 * Input:           	None
 * Output:          	unsigned char
 *								 0  - There is no empty space in 
 *								       transmit buffer.                  
 *							  number - the number of bytes of empty
 *								       space in transmit buffer. 
 *								                  
 *								  
 * Side Effects:    	None
 * Stack Requirements: 	1 level deep
 * Overview:        	This function returns the number of bytes
 *						of free space left out in transmit buffer at
 *						the calling time of this function. It helps  
 *						the user to further write data in to transmit
 *						buffer at once, rather than checking transmit
 *                      buffer is full or not with every addition of
 *                      data in to the transmit buffer.
 ********************************************************************/
unsigned char UARTIntGetTxBufferEmptySpace(void)
{
	if(vUARTIntTxBufDataCnt < TX_BUFFER_SIZE)
	  	return(TX_BUFFER_SIZE-vUARTIntTxBufDataCnt);
  	else
  		return 0;
}

#endif

/*********************************************************************
 * Function:        	unsigned char UARTIntGetChar(unsigned char*)
 * PreCondition:    	UARTIntInit()function should have been called.
 * Input:           	unsigned char*
 * Output:          	unsigned char
 *							  0 - receive buffer is empty and the 
 *								  character could not be read from
 *								  the receive buffer.
 *							  1 - single character is successfully 
 *							      read from receive buffer.
 * Side Effects:    	None
 * Stack Requirements: 	1 level deep
 * Overview:        	This function reads the data from the receive
 *						buffer. It places the data in to argument and
 *						updates the data count and read pointer 
 *						variables of receive buffer.
 *
 ********************************************************************/
#if	RXON
unsigned char UARTIntGetChar(unsigned char *chTemp)
{
	
	if( vUARTIntStatus.UARTIntRxBufferEmpty)
		return 0;

	//critical code, disabling interrupts here keeps the
	//access pointer values proper.
	PIE1bits.RCIE = 0;  
	vUARTIntStatus.UARTIntRxBufferFull = 0;
	*chTemp = vUARTIntRxBuffer[vUARTIntRxBufRdPtr];
	vUARTIntRxBufDataCnt--;
	if(vUARTIntRxBufDataCnt == 0 )
		vUARTIntStatus.UARTIntRxBufferEmpty = 1;
	vUARTIntRxBufRdPtr++;
	if(vUARTIntRxBufRdPtr == RX_BUFFER_SIZE)
		vUARTIntRxBufRdPtr = 0;
	PIE1bits.RCIE = 1;
	return 1;
}

/*********************************************************************
 * Function:        	unsigned char UARTIntGetRxBufferDataSize(void)
 * PreCondition:    	UARTIntInit()function should have been called.
 * Input:           	None
 * Output:          	unsigned char 
 *								        number - the number of bytes
 *								             of data in receive buffer. 
 * Side Effects:    	None
 * Stack Requirements: 	1 level deep
 * Overview:        	This function returns the number of bytes
 *						of data available in receive buffer at
 *						the calling time of this function. It helps  
 *						the user to read data from receive buffer
 *						at once, rather than checking receive buffer
 *                      is empty or not with every read of data from
 *                      receive buffer.
 ********************************************************************/
unsigned char UARTIntGetRxBufferDataSize(void)
{
	return vUARTIntRxBufDataCnt;				
}

#endif

/*********************************************************************
 * Function:        	void UARTIntISR(void)
 * PreCondition:    	UARTIntInit() function should have been called.
 * Input:           	None
 * Output:          	None 
 * Side Effects:    	None
 * Stack Requirements: 	2 level deep
 * Overview:        	This is the Interrupt service routine which is
 *						called in the user application's ISR portion.
 *						This function actually sends the data from
 *						transmit buffer to USART and updates the data
 *						count and read pointer variables of transmit 
 *						buffer. For the receive portion, it reads the
 *						data from USART and places the data in to 
 *						receive buffer (if no errors occured) and
 *						updates data count and write pointer variables
 *						of receive buffer. If the receive buffer is 
 *						full and it receives more data error flag is 
 *						set.If frame errors(FERR) occur	it sets the
 *						error flag. If over flow errors(OERR) occurs,
 *						it clears and sets the CREN bit, so that
 *						USART can receive further data.
 ********************************************************************/
void UARTIntISR(void)
{
	#if	RXON
		unsigned char chTemp;
	#endif
	#if TXON 
		if(PIR1bits.TXIF & PIE1bits.TXIE)
		{		
			if(!vUARTIntStatus.UARTIntTxBufferEmpty)
			{			
				TXREG = vUARTIntTxBuffer[vUARTIntTxBufRdPtr];									
				if(vUARTIntStatus.UARTIntTxBufferFull)
					vUARTIntStatus.UARTIntTxBufferFull = 0;				
				vUARTIntTxBufDataCnt--;
				if(vUARTIntTxBufDataCnt == 0)
				vUARTIntStatus.UARTIntTxBufferEmpty = 1;				
				vUARTIntTxBufRdPtr++;
				if(vUARTIntTxBufRdPtr == TX_BUFFER_SIZE)
					vUARTIntTxBufRdPtr = 0;				
			}
			else
			{   
				PIE1bits.TXIE = 0;				
			}
		}
	#endif	
	#if	TXON_AND_RXON
		else if( PIR1bits.RCIF & PIE1bits.RCIE)		
	#elif TXOFF_AND_RXON
		if( PIR1bits.RCIF & PIE1bits.RCIE)
	#endif
	    #if	RXON
			{	
				if(RCSTAbits.FERR)   /* FERR error condition */
				{ 
					chTemp = RCREG;
					vUARTIntStatus.UARTIntRxError = 1;				
				}
				else if (RCSTAbits.OERR) /* OERR error condition */
				{					
					RCSTAbits.CREN = 0;
					RCSTAbits.CREN = 1;
					chTemp = RCREG;							
					vUARTIntStatus.UARTIntRxError = 1;								
				}
				else if ( vUARTIntStatus.UARTIntRxBufferFull) 
				{ 
					chTemp = RCREG;
					vUARTIntStatus.UARTIntRxOverFlow = 1;
				}		 
				else if(!vUARTIntStatus.UARTIntRxBufferFull)
				{	
					vUARTIntStatus.UARTIntRxOverFlow = 0;								
					vUARTIntStatus.UARTIntRxBufferEmpty = 0;
					vUARTIntRxBuffer[vUARTIntRxBufWrPtr] = RCREG;
					vUARTIntRxBufDataCnt ++;
					if(vUARTIntRxBufDataCnt == RX_BUFFER_SIZE)
					 	vUARTIntStatus.UARTIntRxBufferFull = 1;
					vUARTIntRxBufWrPtr++;
					if(vUARTIntRxBufWrPtr == RX_BUFFER_SIZE)
						vUARTIntRxBufWrPtr = 0; 						
				}	
				
			}
  		#endif		
}
