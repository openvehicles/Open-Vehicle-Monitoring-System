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
 * Based upon:
 * http://base64.sourceforge.net/b64.c
 *
 ********************************************************************/
#ifndef __CRYPT_BASE64_H
#define __CRYPT_BASE64_H

#include <stdlib.h>
#include <GenericTypeDefs.h>

extern const rom unsigned char cb64[];

char *base64encode(BYTE *inputData, WORD inputLen, BYTE *outputData);
void base64encodesend(BYTE *inputData, WORD inputLen);
int base64decode(BYTE *inputData, BYTE *outputData);

#endif //#ifndef __CRYPT_BASE64_H