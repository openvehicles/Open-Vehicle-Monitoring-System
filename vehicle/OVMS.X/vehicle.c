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
#ifdef OVMS_ACCMODULE
#include "acc.h"
#endif

#pragma udata VEHICLE
unsigned int  can_granular_tick;             // An internal ticker used to generate 1min, 5min, etc, calls
unsigned int  can_id;                        // ID of can message
unsigned char can_filter;                    // CAN filter
unsigned char can_datalength;                // The number of valid bytes in the can_databuffer
unsigned char can_databuffer[8];             // A buffer to store the current CAN message
unsigned char can_minSOCnotified;            // minSOC notified flag
unsigned char can_mileskm;                   // Miles of Kilometers

rom unsigned char* vehicle_version;          // Vehicle module version
rom unsigned char* can_capabilities;         // Vehicle capabilities

rom BOOL (*vehicle_fn_init)(void);
rom BOOL (*vehicle_fn_poll0)(void);
rom BOOL (*vehicle_fn_poll1)(void);
rom BOOL (*vehicle_fn_ticker1)(void);
rom BOOL (*vehicle_fn_ticker10)(void);
rom BOOL (*vehicle_fn_ticker60)(void);
rom BOOL (*vehicle_fn_ticker300)(void);
rom BOOL (*vehicle_fn_ticker600)(void);
rom BOOL (*vehicle_fn_ticker)(void);
rom BOOL (*vehicle_fn_ticker10th)(void);
rom BOOL (*vehicle_fn_idlepoll)(void);
rom BOOL (*vehicle_fn_commandhandler)(BOOL msgmode, int code, char* msg);
rom BOOL (*vehicle_fn_smshandler)(BOOL premsg, char *caller, char *command, char *arguments);
rom BOOL (*vehicle_fn_smsextensions)(char *caller, char *command, char *arguments);
rom int  (*vehicle_fn_minutestocharge)(unsigned char chgmod, int wAvail, int imStart, int imTarget, int pctTarget, int cac100, signed char degAmbient, int *pimExpect);

#ifdef OVMS_POLLER

////////////////////////////////////////////////////////////////////////
// vehicle_poller
// This is an advanced vehicle poller

unsigned char vehicle_poll_state;        // Current poll state
rom vehicle_pid_t* vehicle_poll_plist;   // Head of poll list
rom vehicle_pid_t* vehicle_poll_plcur;   // Current position in poll list
unsigned int vehicle_poll_ticker;        // Polling ticker
unsigned int vehicle_poll_moduleid_low;  // Expected moduleid low mark
unsigned int vehicle_poll_moduleid_high; // Expected moduleid high mark
unsigned char vehicle_poll_type;         // Expected type
unsigned int vehicle_poll_pid;           // Expected pid
unsigned char vehicle_poll_busactive;    // Indicates recent activity on the passive bus
unsigned int vehicle_poll_ml_remain;     // Bytes remaining for vehicle poll
unsigned int vehicle_poll_ml_offset;     // Offset of vehicle poll data
unsigned int vehicle_poll_ml_frame;      // Frame number for vehicle poll

rom BOOL (*vehicle_fn_pollpid)(void);

#endif //#ifdef OVMS_POLLER

#pragma udata

#ifdef OVMS_POLLER

void vehicle_poll_setpidlist(rom vehicle_pid_t *plist)
  {
  vehicle_poll_plist = plist;
  }

void vehicle_poll_setstate(unsigned char state)
  {
  if ((state >= 0)&&(state < VEHICLE_POLL_NSTATES)&&(state != vehicle_poll_state))
    {
    vehicle_poll_state = state;
    vehicle_poll_ticker = 0;
    vehicle_poll_plcur = NULL;
    }
  }

void vehicle_poll_poller(void)
  {
  if (vehicle_poll_plcur == NULL)
    {
    vehicle_poll_plcur = vehicle_poll_plist;
    }

  while (vehicle_poll_plcur->moduleid != 0)
    {
    if ((vehicle_poll_plcur->polltime[vehicle_poll_state] > 0)&&
        ((vehicle_poll_ticker % vehicle_poll_plcur->polltime[vehicle_poll_state] ) == 0))
      {
      // We need to poll this one...
      while (TXB0CONbits.TXREQ) {} // Loop until TX is done
      vehicle_poll_type = vehicle_poll_plcur->type;
      vehicle_poll_pid = vehicle_poll_plcur->pid;
      if (vehicle_poll_plcur->rmoduleid != 0)
        {
        // send to <moduleid>, listen to response from <rmoduleid>:
        vehicle_poll_moduleid_low = vehicle_poll_plcur->rmoduleid;
        vehicle_poll_moduleid_high = vehicle_poll_plcur->rmoduleid;
        TXB0CON = 0;
        TXB0SIDL = (vehicle_poll_plcur->moduleid & 0x07)<<5;
        TXB0SIDH = (vehicle_poll_plcur->moduleid >>3);
        }
      else
        {
        // broadcast: send to 0x7df, listen to all responses:
        vehicle_poll_moduleid_low = 0x7e8;
        vehicle_poll_moduleid_high = 0x7ef;
        TXB0CON = 0;
        TXB0SIDL = 0b11100000;  // 0x07df
        TXB0SIDH = 0b11111011;
        }
      switch (vehicle_poll_plcur->type)
        {
        case VEHICLE_POLL_TYPE_OBDIICURRENT:
        case VEHICLE_POLL_TYPE_OBDIIFREEZE:
        case VEHICLE_POLL_TYPE_OBDIISESSION:
          // 8 bit PID request for single frame response:
          TXB0D0 = 0x02;
          TXB0D1 = vehicle_poll_type;
          TXB0D2 = vehicle_poll_pid;
          TXB0D3 = 0x00;
          TXB0D4 = 0x00;
          TXB0D5 = 0x00;
          TXB0D6 = 0x00;
          TXB0D7 = 0x00;
          TXB0DLC = 0b00001000; // data length (8)
          TXB0CON = 0b00001000; // mark for transmission
          break;
        case VEHICLE_POLL_TYPE_OBDIIVEHICLE:
        case VEHICLE_POLL_TYPE_OBDIIGROUP:
          // 8 bit PID request for multi frame response:
          vehicle_poll_ml_remain = 0;
          TXB0D0 = 0x02;
          TXB0D1 = vehicle_poll_type;
          TXB0D2 = vehicle_poll_pid;
          TXB0D3 = 0x00;
          TXB0D4 = 0x00;
          TXB0D5 = 0x00;
          TXB0D6 = 0x00;
          TXB0D7 = 0x00;
          TXB0DLC = 0b00001000; // data length (8)
          TXB0CON = 0b00001000; // mark for transmission
          break;
        case VEHICLE_POLL_TYPE_OBDIIEXTENDED:
          // 16 bit PID request:
          TXB0D0 = 0x03;
          TXB0D1 = VEHICLE_POLL_TYPE_OBDIIEXTENDED;    // Get extended PID
          TXB0D2 = vehicle_poll_pid >> 8;
          TXB0D3 = vehicle_poll_pid & 0xff;
          TXB0D4 = 0x00;
          TXB0D5 = 0x00;
          TXB0D6 = 0x00;
          TXB0D7 = 0x00;
          TXB0DLC = 0b00001000; // data length (8)
          TXB0CON = 0b00001000; // mark for transmission
          break;
        }
      vehicle_poll_plcur++;
      return;
      }
    vehicle_poll_plcur++;
    }

  vehicle_poll_plcur = vehicle_poll_plist;
  vehicle_poll_ticker++;
  if (vehicle_poll_ticker > 3600) vehicle_poll_ticker -= 3600;
  }

BOOL vehicle_poll_poll0(void)
  {
  unsigned char k;

  switch (vehicle_poll_type)
    {
    case VEHICLE_POLL_TYPE_OBDIICURRENT:
    case VEHICLE_POLL_TYPE_OBDIIFREEZE:
    case VEHICLE_POLL_TYPE_OBDIISESSION:
      // 8 bit PID single frame response:
      if ((can_databuffer[1] == 0x40+vehicle_poll_type)&&
          (can_databuffer[2] == vehicle_poll_pid))
        return TRUE; // Call vehicle poller
      break;
    case VEHICLE_POLL_TYPE_OBDIIVEHICLE:
    case VEHICLE_POLL_TYPE_OBDIIGROUP:
      // 8 bit PID multiple frame response:
      if (((can_databuffer[0]>>4) == 0x1)&&
          (can_databuffer[2] == 0x40+vehicle_poll_type)&&
          (can_databuffer[3] == vehicle_poll_pid))
        {
        // First frame; send flow control frame:
        while (TXB0CONbits.TXREQ) {} // Loop until TX is done
        TXB0CON = 0;
        TXB0SIDL = ((can_id-8) & 0x07)<<5;
        TXB0SIDH = ((can_id-8)>>3);
        TXB0D0 = 0x30; // flow control frame type
        TXB0D1 = 0x00; // request all frames available
        TXB0D2 = 0x32; // with 50ms send interval
        TXB0D3 = 0x00;
        TXB0D4 = 0x00;
        TXB0D5 = 0x00;
        TXB0D6 = 0x00;
        TXB0D7 = 0x00;
        TXB0DLC = 0b00001000; // data length (8)
        TXB0CON = 0b00001000; // mark for transmission
        
        // prepare frame processing, first frame contains first 3 bytes:
        vehicle_poll_ml_remain = (((unsigned int)(can_databuffer[0]&0xf0))<<8)+can_databuffer[1] - 3;
        vehicle_poll_ml_offset = 3;
        vehicle_poll_ml_frame = 0;
        can_datalength = 3;
        can_databuffer[0] = can_databuffer[5];
        can_databuffer[1] = can_databuffer[6];
        can_databuffer[2] = can_databuffer[7];
        return TRUE; // Call vehicle poller
        }
      else if (((can_databuffer[0]>>4)==0x2)&&(vehicle_poll_ml_remain>0))
        {
        // Consecutive frame (1 control + 7 data bytes)
        for (k=0;k<7;k++) { can_databuffer[k] = can_databuffer[k+1]; }
        if (vehicle_poll_ml_remain>7)
          {
          vehicle_poll_ml_remain -= 7;
          vehicle_poll_ml_offset += 7;
          can_datalength = 7;
          }
        else
          {
          can_datalength = vehicle_poll_ml_remain;
          vehicle_poll_ml_offset += vehicle_poll_ml_remain;
          vehicle_poll_ml_remain = 0;
          }
        vehicle_poll_ml_frame++;
        return TRUE;
        }
      break;
    case VEHICLE_POLL_TYPE_OBDIIEXTENDED:
      // 16 bit PID response:
      if ((can_databuffer[1] == 0x62)&&
          ((can_databuffer[3]+(((unsigned int) can_databuffer[2]) << 8)) == vehicle_poll_pid))
        return TRUE; // Call vehicle poller
      break;
    }

  return FALSE; // Don't call vehicle poller
  }

#endif //#ifdef OVMS_POLLER

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
// ISR optimization, see http://www.xargs.com/pic/c18-isr-optim.pdf
#pragma tmpdata high_isr_tmpdata
#pragma	interrupt high_isr nosave=section(".tmpdata")
void high_isr(void)
  {
  // High priority CAN interrupt
  if ((RXB0CONbits.RXFUL)&&(vehicle_fn_poll0 != NULL))
    {
    can_id = ((unsigned int)RXB0SIDL >>5)
           + ((unsigned int)RXB0SIDH <<3);
    can_filter = RXB0CON & 0x01;
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
#ifdef OVMS_POLLER
    if (vehicle_poll_plist != NULL)
      {
      if ((can_id >= vehicle_poll_moduleid_low)&&
          (can_id <= vehicle_poll_moduleid_high))
        {
        if (vehicle_poll_poll0())
          {
          vehicle_fn_poll0();
          }
        }
      }
    else
      {
      vehicle_fn_poll0();
      }
#else // #ifdef OVMS_POLLER
    vehicle_fn_poll0();
#endif //#ifdef OVMS_POLLER
    }
  if ((RXB1CONbits.RXFUL)&&(vehicle_fn_poll1 != NULL))
    {
#ifdef OVMS_POLLER
    vehicle_poll_busactive = 60; // Reset countdown timer for passive bus activity
#endif //#ifdef OVMS_POLLER
    can_id = ((unsigned int)RXB1SIDL >>5)
           + ((unsigned int)RXB1SIDH <<3);
    can_filter = RXB1CON & 0x07;
    can_datalength = RXB1DLC & 0x0F; // number of received bytes
    can_databuffer[0] = RXB1D0;
    can_databuffer[1] = RXB1D1;
    can_databuffer[2] = RXB1D2;
    can_databuffer[3] = RXB1D3;
    can_databuffer[4] = RXB1D4;
    can_databuffer[5] = RXB1D5;
    can_databuffer[6] = RXB1D6;
    can_databuffer[7] = RXB1D7;
    RXB1CONbits.RXFUL = 0;        // All bytes read, Clear flag
    vehicle_fn_poll1();
    }
  PIR3=0;     // Clear Interrupt flags
  }
#pragma tmpdata

////////////////////////////////////////////////////////////////////////
// vehicle_initialise()
// This function is an entry point from the main() program loop, and
// gives the vehicle framework an opportunity to initialise itself.
//
void vehicle_initialise(void)
  {
  char *p;

  can_granular_tick = 0;
  can_minSOCnotified = 0;
  can_capabilities = NULL;

#ifdef OVMS_POLLER
  vehicle_poll_state = 0;
  vehicle_poll_plist = NULL;
  vehicle_poll_plcur = NULL;
  vehicle_poll_ticker = 0;
  vehicle_poll_moduleid_low = 0;
  vehicle_poll_moduleid_high = 0;
  vehicle_poll_type = 0;
  vehicle_poll_pid = 0;
  vehicle_poll_busactive = 0;
  vehicle_fn_pollpid = NULL;
#endif //#ifdef OVMS_POLLER

  vehicle_version = NULL;
  vehicle_fn_init = NULL;
  vehicle_fn_poll0 = NULL;
  vehicle_fn_poll1 = NULL;
  vehicle_fn_ticker1 = NULL;
  vehicle_fn_ticker10 = NULL;
  vehicle_fn_ticker60 = NULL;
  vehicle_fn_ticker300 = NULL;
  vehicle_fn_ticker600 = NULL;
  vehicle_fn_ticker = NULL;
  vehicle_fn_ticker10th = NULL;
  vehicle_fn_idlepoll = NULL;
  vehicle_fn_commandhandler = NULL;
  vehicle_fn_smshandler = NULL;
  vehicle_fn_smsextensions = NULL;
  vehicle_fn_minutestocharge = NULL;

  // Clear the internal GPS flag, unless specifically requested by the module
  net_fnbits &= ~(NET_FN_INTERNALGPS);

  p = par_get(PARAM_MILESKM);
  can_mileskm = *p;

  p = par_get(PARAM_VEHICLETYPE);
  if (p == NULL)
    {
    car_type[0] = 0; // Car is undefined
    car_type[1] = 0;
    car_type[2] = 0;
    car_type[3] = 0;
    car_type[4] = 0;
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
#ifdef OVMS_CAR_THINKCITY
  else if (memcmppgm2ram(p, (char const rom far*)"TC", 2) == 0)
    {
    void vehicle_thinkcity_initialise(void);
    vehicle_thinkcity_initialise();
    }
#endif
#ifdef OVMS_CAR_NISSANLEAF
  else if (memcmppgm2ram(p, (char const rom far*)"NL", 2) == 0)
    {
    void vehicle_nissanleaf_initialise(void);
    vehicle_nissanleaf_initialise();
    }
#endif
#ifdef OVMS_CAR_TAZZARI
  else if (memcmppgm2ram(p, (char const rom far*)"TZ", 2) == 0)
    {
    void vehicle_tazzari_initialise(void);
    vehicle_tazzari_initialise();
    }
#endif
#ifdef OVMS_CAR_MITSUBISHI
  else if (memcmppgm2ram(p, (char const rom far*)"MI", 2) == 0)
    {
    void vehicle_mitsubishi_initialise(void);
    vehicle_mitsubishi_initialise();
    }
#endif
#ifdef OVMS_CAR_TRACK
  else if (memcmppgm2ram(p, (char const rom far*)"XX", 2) == 0)
    {
    void vehicle_track_initialise(void);
    vehicle_track_initialise();
    }
#endif
#ifdef OVMS_CAR_KYBURZ
  else if (memcmppgm2ram(p, (char const rom far*)"KD", 2) == 0)
    {
    void vehicle_kyburz_initialise(void);
    vehicle_kyburz_initialise();
    }
#endif
#ifdef OVMS_CAR_KIASOUL
  else if (memcmppgm2ram(p, (char const rom far*)"KS", 2) == 0)
    {
    void vehicle_kiasoul_initialise(void);
    vehicle_kiasoul_initialise();
    }
#endif
#ifdef OVMS_CAR_NONE
  else
    {
    void vehicle_none_initialise(void);
    vehicle_none_initialise();
    }
#endif

  if ((net_fnbits & NET_FN_CARTIME)>0)
    {
    car_time = 1;
    }

  RCONbits.IPEN = 1; // Enable Interrupt Priority
  PIE3bits.RXB1IE = 1; // CAN Receive Buffer 1 Interrupt Enable bit
  PIE3bits.RXB0IE = 1; // CAN Receive Buffer 0 Interrupt Enable bit
  IPR3 = 0b00000011; // high priority interrupts for Buffers 0 and 1
  }

////////////////////////////////////////////////////////////////////////
// Vehicle Public Hooks
//

void vehicle_ticker(void)
  {
  // This ticker is called once every second
  can_granular_tick++;

#ifdef OVMS_POLLER
  if ((vehicle_poll_busactive>0)&&(vehicle_poll_plist != NULL))
    {
    vehicle_poll_poller();
    vehicle_poll_busactive--; // Count down...
    }
#endif //#ifdef OVMS_POLLER

  // The one-second work...
  if (car_stale_ambient>0) car_stale_ambient--;
  if (car_stale_temps>0)   car_stale_temps--;
  if (car_stale_gps>0)     car_stale_gps--;
  if (car_stale_tpms>0)    car_stale_tpms--;

  /***************************************************************************
   * Resolve CAN lockups:
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

  // And give the vehicle module a chance...
  if (vehicle_fn_ticker1 != NULL)
    {
    vehicle_fn_ticker1();
    }

  if (((can_granular_tick % 10)==0)&&(vehicle_fn_ticker10 != NULL))
    {
    vehicle_fn_ticker10();
    }

  // Check chargelimits
  if (CAR_IS_CHARGING)
    {
    if (((car_chargelimit_rangelimit>0)&&(car_idealrange>=car_chargelimit_rangelimit))||
        ((car_chargelimit_soclimit>0)&&(car_SOC>=car_chargelimit_soclimit)))
      {
      // Charge has hit the limit, so can be stopped
#ifdef OVMS_ACCMODULE
      acc_handle_msg(FALSE, 12, NULL);
#endif
      vehicle_fn_commandhandler(FALSE, 12, NULL); // Stop charge
      }
    }

  // minSOC alerts
  if ((can_granular_tick % 60)==0)
    {
    int minSOC;

    // check minSOC
    minSOC = sys_features[FEATURE_MINSOC];
    if ((!(can_minSOCnotified & CAN_MINSOC_ALERT_MAIN)) && (car_SOC < minSOC))
      {
      net_req_notification(NET_NOTIFY_CHARGE);
      can_minSOCnotified |= CAN_MINSOC_ALERT_MAIN;
      }
    else if ((can_minSOCnotified & CAN_MINSOC_ALERT_MAIN) && (car_SOC > minSOC + 2))
      {
      // reset the alert sent flag when SOC is 2% point higher than threshold
      can_minSOCnotified &= ~CAN_MINSOC_ALERT_MAIN;
      }
    if (vehicle_fn_ticker60 != NULL) vehicle_fn_ticker60();
    }

  if (((can_granular_tick % 300)==0)&&(vehicle_fn_ticker300 != NULL))
    {
    vehicle_fn_ticker300();
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
