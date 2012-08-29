//
//  crypto_base64.h
//  ovms

/*********************************************************************
 *
 *  base64conversion
 *
 *********************************************************************
 *
 * FileName:        base64conversion.c
 * Processor:       PIC18, PIC24F, PIC24H, dsPIC30, dsPIC33F
 * Complier:        Microchip C18 v3.03 or higher
 *                                      Microchip C30 v2.02 or higher
 *
 * Based upon:
 * http://users.fmrib.ox.ac.uk/~crodgers/netgear/base64-encode.c
 * http://users.fmrib.ox.ac.uk/~crodgers/netgear/base64-decode.c
 *
 * Based upon:
 * http://code.google.com/p/pic-tweeter/source/browse/branches/MCHPStack402/TCPIP+Demo+App/base64conversion.c
 *
 ********************************************************************/

#ifndef ovms_crypto_base64_h
#define ovms_crypto_base64_h

#include "crypto.h"

extern const unsigned char cb64[];
extern const unsigned char cd64[];

void encodeblock( unsigned char in[3], unsigned char out[4], int len );
void base64encode(uint8_t *inputData, int inputLen, uint8_t *outputData);

void decodeblock( unsigned char in[4], unsigned char out[3] );
int base64decode(uint8_t *inputData, uint8_t *outputData);

#endif
