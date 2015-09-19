/*
;    Project:       Open Vehicle Monitor System
;    Date:          16 October 2011
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011  Michael Stegen / Stegen Electronics
;    (C) 2011  Mark Webb-Johnson
;    (C) 2011  Sonny Chen @ EPRO/DX
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

#include <string.h>
#include "ovms.h"
#include "crypt_base64.h"

// EEprom data
// The following data can be changed by sending SMS commands, and will survive a reboot
#pragma romdata eedata=0xf00000
#ifdef OVMS_QC
rom char EEparam[PARAM_MAX][PARAM_MAX_LENGTH]
  = {"NOPHONE","OVMS","K","IP","54.197.255.127","imobile.three.com.hk","","","QC","QCPASS"};
#else
rom char EEparam[PARAM_MAX][PARAM_MAX_LENGTH]
  = {"NOPHONE","OVMS","K","","","","","","",""};
#endif //#ifdef OVMS_QC
#pragma romdata

#pragma udata
char par_value[PARAM_MAX_LENGTH];

void par_initialise(void)
  {
  }

void par_read(unsigned char param)
  {
  int k;
  unsigned int eeaddress;

  // Read parameter from EEprom
  EECON1 = 0; // select EEprom memory not Flash
  eeaddress = (int)param;
  eeaddress = eeaddress*PARAM_MAX_LENGTH;
  EEADRH = eeaddress >> 8;
  EEADR = eeaddress & 0x00ff;
  for (k=0; k<PARAM_MAX_LENGTH;k++)
    {
    EECON1bits.RD = 1;
    par_value[k] = EEDATA;
    EEADR++;
    }
  }

void par_write(unsigned char param)
  {
  int k = 0;
  char savint;
  unsigned int eeaddress;

  // Protect PARAM_REGPHONE & PARAM_MODULEPASS against empty writes:
  if ((param <= PARAM_MODULEPASS) && (par_value[0] == 0))
      return;
  
  // Write parameter to EEprom
  eeaddress = (int)param;
  eeaddress = eeaddress*PARAM_MAX_LENGTH;
  EEADRH = eeaddress >> 8;
  EEADR = eeaddress & 0x00ff;
  for (k=0;k<PARAM_MAX_LENGTH;k++)
    {
    EEADR = (char)&EEparam[param][k];
    EECON1 = 0; //ensure CFGS=0 and EEPGD=0
    EECON1bits.WREN = 1; //enable write to EEPROM
    EEDATA = par_value[k]; // and data
    savint = INTCON; // Save interrupts state
    INTCONbits.GIE=0; // Disable interrupts
    EECON2 = 0x55; // required sequence #1
    EECON2 = 0xAA; // #2
    EECON1bits.WR = 1; // #3 = actual write
    INTCON = savint; // Restore interrupts
    while (EECON1bits.WR);
    EECON1bits.WREN = 0; // disable write to EEPROM
    }
  }

char* par_get(unsigned char param)
  {
  par_value[0] = 0;

  if (param >= PARAM_MAX) return par_value;

#ifdef OVMS_TWIZY_CFG
  // exclude binary params for CFG profiles:
  if (param >= 16 && param <= 21) return par_value;
#endif //OVMS_TWIZY_CFG

  par_read(param);

  return par_value;
  }

void par_set(unsigned char param, char* value)
  {
  if (param >= PARAM_MAX) return;

#ifdef OVMS_TWIZY_CFG
  // exclude binary params for CFG profiles:
  if (param >= 16 && param <= 21) return;
#endif //OVMS_TWIZY_CFG

  if (value)
    {
    strncpy(par_value,value,PARAM_MAX_LENGTH);
    par_value[PARAM_MAX_LENGTH-1]='\0';
    }
  else
      par_value[0] = 0;

  par_write(param);
  }

void par_getbase64(unsigned char param, void* dest, size_t length)
  {
  char *p = par_get(param);
  memset(dest,0,length);
  base64decode(p, dest);
  }

void par_setbase64(unsigned char param, void* source, size_t length)
  {
  base64encode(source, length, par_value);
  par_write(param);
  }

void par_getbin(unsigned char param, void *dest, size_t length)
{
  // clear dest:
  memset(dest, 0, length);

  // read param slots into dest:
  while (param < PARAM_MAX) {
    par_read(param);
    if (length > PARAM_MAX_LENGTH) {
      memcpy(dest, (const void *)par_value, PARAM_MAX_LENGTH);
      length -= PARAM_MAX_LENGTH;
      dest += PARAM_MAX_LENGTH;
      param += 1;
    } else {
      // last chunk:
      memcpy(dest, (const void *)par_value, length);
      break;
    }
  }
}

void par_setbin(unsigned char param, void *source, size_t length)
{
  // write source to param slots:
  while (param < PARAM_MAX) {
    if (length > PARAM_MAX_LENGTH) {
      memcpy(par_value, source, PARAM_MAX_LENGTH);
      par_write(param);
      length -= PARAM_MAX_LENGTH;
      source += PARAM_MAX_LENGTH;
      param += 1;
    } else {
      // last chunk:
      memset(par_value, 0, PARAM_MAX_LENGTH);
      memcpy(par_value, source, length);
      par_write(param);
      break;
    }
  }
}

