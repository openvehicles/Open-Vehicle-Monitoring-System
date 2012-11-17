/*
;    Project:       Open Vehicle Monitor System
;    Date:          6 May 2012
;
;    Changes:
;    1.0  Initial release
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
#include "ovms.h"
#include "params.h"

#pragma udata
unsigned int  can_granular_tick = 0;         // An internal ticker used to generate 1min, 5min, etc, calls
unsigned char can_datalength;                // The number of valid bytes in the can_databuffer
unsigned char can_databuffer[8];             // A buffer to store the current CAN message
unsigned char can_minSOCnotified = 0;        // minSOC notified flag
unsigned char can_mileskm = 'M';             // Miles of Kilometers

rom BOOL (*vehicle_fn_init)(void) = NULL;
rom BOOL (*vehicle_fn_poll0)(void) = NULL;
rom BOOL (*vehicle_fn_poll1)(void) = NULL;
rom BOOL (*vehicle_fn_ticker1)(void) = NULL;
rom BOOL (*vehicle_fn_ticker60)(void) = NULL;
rom BOOL (*vehicle_fn_ticker300)(void) = NULL;
rom BOOL (*vehicle_fn_ticker600)(void) = NULL;
rom BOOL (*vehicle_fn_ticker)(void) = NULL;
rom BOOL (*vehicle_fn_ticker10th)(void) = NULL;
rom BOOL (*vehicle_fn_idlepoll)(void) = NULL;
rom BOOL (*vehicle_fn_commandhandler)(BOOL msgmode, int code, char* msg);

////////////////////////////////////////////////////////////////////////
// vehicle_initialise()
// This function is an entry point from the main() program loop, and
// gives the vehicle framework an opportunity to initialise itself.
//
void vehicle_initialise(void)
  {
  char *p;

  vehicle_fn_init = NULL;
  vehicle_fn_poll0 = NULL;
  vehicle_fn_poll1 = NULL;
  vehicle_fn_ticker1 = NULL;
  vehicle_fn_ticker60 = NULL;
  vehicle_fn_ticker300 = NULL;
  vehicle_fn_ticker600 = NULL;
  vehicle_fn_ticker = NULL;
  vehicle_fn_ticker10th = NULL;
  vehicle_fn_idlepoll = NULL;
  vehicle_fn_commandhandler = NULL;
  
  car_type[0] = 0; // Car is undefined
  car_type[1] = 0;
  car_type[2] = 0;
  car_type[3] = 0;
  car_type[4] = 0;

  // Clear the internal GPS flag, unless specifically requested by the module
  net_fnbits &= ~(NET_FN_INTERNALGPS);

  p = par_get(PARAM_VEHICLETYPE);
  if (p == NULL)
    {
    }
#ifdef OVMS_CAR_TESLAROADSTER
  else if (memcmppgm2ram(p, (char const rom far*)"TR", 2) == 0)
    {
    void vehicle_teslaroadster_initialise(void);
    vehicle_teslaroadster_initialise();
    }
#endif
#ifdef OVMS_CAR_VOLTAMPERA
  else if (memcmppgm2ram(p, (char const rom far*)"VA", 2) == 0)
    {
    void vehicle_voltampera_initialise(void);
    vehicle_voltampera_initialise();
    }
#endif
#ifdef OVMS_CAR_RENAULTTWIZY
  else if (memcmppgm2ram(p, (char const rom far*)"RT", 2) == 0)
    {
    void vehicle_twizy_initialise(void);
    vehicle_twizy_initialise();
    }
#endif
#ifdef OVMS_CAR_OBDII
  else if (memcmppgm2ram(p, (char const rom far*)"O2", 2) == 0)
    {
    void vehicle_obdii_initialise(void);
    vehicle_obdii_initialise();
    }
#endif
#ifdef OVMS_CAR_NONE
  else
    {
    void vehicle_none_initialise(void);
    vehicle_none_initialise();
    }
#endif

  RCONbits.IPEN = 1; // Enable Interrupt Priority
  PIE3bits.RXB1IE = 1; // CAN Receive Buffer 1 Interrupt Enable bit
  PIE3bits.RXB0IE = 1; // CAN Receive Buffer 0 Interrupt Enable bit
  IPR3 = 0b00000011; // high priority interrupts for Buffers 0 and 1

  p = par_get(PARAM_MILESKM);
  can_mileskm = *p;
  }

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
  if ((RXB0CONbits.RXFUL)&&(vehicle_fn_poll0 != NULL)) vehicle_fn_poll0();
  if ((RXB1CONbits.RXFUL)&&(vehicle_fn_poll1 != NULL)) vehicle_fn_poll1();
  PIR3=0;     // Clear Interrupt flags
  }

////////////////////////////////////////////////////////////////////////
// Vehicle Public Hooks
//

void vehicle_ticker(void)
  {
  // This ticker is called once every second
  can_granular_tick++;
  
  // The one-second work...
  if (car_stale_ambient>0) car_stale_ambient--;
  if (car_stale_temps>0)   car_stale_temps--;
  if (car_stale_gps>0)     car_stale_gps--;
  if (car_stale_tpms>0)    car_stale_tpms--;

  // And give the vehicle module a chance...
  if (vehicle_fn_ticker1 != NULL)
    {
    if (vehicle_fn_ticker1()) return;
    }

  if ((can_granular_tick % 60)==0)
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
    if (vehicle_fn_ticker60 != NULL) vehicle_fn_ticker60();
    }
  if ((can_granular_tick % 300)==0)
    {
    if (vehicle_fn_ticker300 != NULL) vehicle_fn_ticker300();
    }
  if ((can_granular_tick % 600)==0)
    {
    if (vehicle_fn_ticker600 != NULL) vehicle_fn_ticker600();
    can_granular_tick -= 600;
    }

  if (vehicle_fn_ticker != NULL) vehicle_fn_ticker();
  }

void vehicle_ticker10th(void)
  {
  if (vehicle_fn_ticker10th != NULL) vehicle_fn_ticker10th();
  }

void vehicle_idlepoll(void)
{
  if (vehicle_fn_idlepoll != NULL) vehicle_fn_idlepoll();
}
