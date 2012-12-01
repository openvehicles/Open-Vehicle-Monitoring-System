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

#include <stdio.h>
#include <usart.h>
#include <string.h>
#include <stdlib.h>
#include "ovms.h"
#include "diag.h"
#include "net.h"
#include "net_sms.h"
#include "net_msg.h"
#include "led.h"
#include "inputs.h"

// DIAG data
#pragma udata

#define DATA_COUNT 17u

char orig_canwrite = 0;
int canwrite_state = -1;
unsigned int CANIDMap[4]
        = { 0x100, 0x344, 0x400, 0x402 };

unsigned char DummyData[DATA_COUNT * 9]
        = {
0x00, 0x83, 0x03, 0x00, 0x00, 0x3F, 0x71, 0xD1, 0x09,   //      VDS GPS latitude (latitude 22.341710)
0x00, 0x84, 0x03, 0x00, 0x00, 0xCC, 0xB2, 0x2E, 0x32,   //      VDS GPS longitude (longitude 114.192875)
0x00, 0xA4, 0x53, 0x46, 0x5A, 0x52, 0x45, 0x38, 0x42,   //      VDS VIN1 (vin SFZRE8B)
0x00, 0xA5, 0x31, 0x35, 0x42, 0x33, 0x39, 0x39, 0x39,   //      VDS VIN2 (vin 15B3999)
0x00, 0xA6, 0x39, 0x39, 0x39, 0x00, 0x00, 0x00, 0x00,   //      VDS VIN3 (vin 999)
0x00, 0x80, 0x56, 0xA5, 0x00, 0x8C, 0x00, 0x90, 0x00,   //      VDS Range (SOC 86%) (ideal 165) (est 144)
0x00, 0x88, 0x00, 0x8C, 0x00, 0xFF, 0xFF, 0x46, 0x00,   //      VDS Charger settings (limit 70A) (current 0A) (duration 140mins)
0x00, 0x89, 0x1D, 0xFF, 0xFF, 0x7F, 0xFF, 0x00, 0x00,   //      VDS Charger interface (speed 29mph) (vline 65535V) (Iavailable 255A)
0x00, 0x95, 0x04, 0x07, 0x64, 0x57, 0x02, 0x0E, 0x1C,   //      VDS Charger v1.5 (done) (conn-pwr-cable) (standard)
0x00, 0x96, 0x88, 0x20, 0x02, 0xA1, 0x0C, 0x00, 0x00,   //      VDS Doors (l-door: closed) (r-door: closed) (chargeport: closed) (pilot: true) (charging: false) (bits 88)
0x00, 0x9C, 0x01, 0xC6, 0x01, 0x00, 0x00, 0x00, 0x00,   //      VDS Trip->VDS (trip 45.4miles)
0x00, 0x81, 0x00, 0x00, 0x00, 0x57, 0x46, 0xBA, 0x4E,   //      VDS Time/Date UTC (time Wed Nov  9 09:22:31 2011)
0x00, 0x97, 0x00, 0x88, 0x01, 0x5F, 0x97, 0x00, 0x00,   //      VDS Odometer (miles: 3875.1 km: 6236.4)
0x00, 0xA3, 0x26, 0x49, 0x00, 0x00, 0x00, 0x1B, 0x00,   //      VDS TEMPS (Tpem 38) (Tmotor 73) (Tbattery 27)
0x01, 0x54, 0x40, 0x53, 0x42, 0x72, 0x45, 0x72, 0x45,   //      TPMS (f-l 30psi,24C f-r 30psi,26C r-l 41psi,29C r-r 41psi,29C)
0x02, 0x02, 0xA9, 0x41, 0x80, 0xE6, 0x80, 0x55, 0x00,   //      DASHBOARD AMPS
0x03, 0xFA, 0x03, 0xC4, 0x5E, 0x97, 0x00, 0xC6, 0x01    //      402?? Odometer (miles: 3875.0 km: 6236.2) (trip 45.4miles)
};

void diag_initialise(void)
  {
  led_set(OVMS_LED_GRN,NET_LED_ERRDIAGMODE);
  led_set(OVMS_LED_RED,NET_LED_ERRDIAGMODE);
  led_start();
  net_timeout_ticks = 0;
  net_timeout_goto = 0;
  net_puts_rom("\x1B[2J\x1B[01;01H\r# OVMS DIAGNOSTICS MODE\r\n\n");
  orig_canwrite = sys_features[FEATURE_CANWRITE];
  canwrite_state = -1;
  }

void diag_ticker(void)
  {
  if (canwrite_state>=0)
    {
    // re-map DummyData's 8-bit field 0 into an 11-bit CAN ID
    unsigned int message_id = CANIDMap[DummyData[(canwrite_state * 9)]];
    unsigned char field;

    TXB0CON = 0;            // Reset TX buffer

    TXB0SIDL = (message_id & 0x7) << 5;  // Set CAN ID to 0x100
    TXB0SIDH = message_id >> 3;

    field = 1; // field 0 = CAN ID, data starts from field 1
    TXB0D0 = DummyData[(canwrite_state * 9) + field++];
    TXB0D1 = DummyData[(canwrite_state * 9) + field++];
    TXB0D2 = DummyData[(canwrite_state * 9) + field++];
    TXB0D3 = DummyData[(canwrite_state * 9) + field++];
    TXB0D4 = DummyData[(canwrite_state * 9) + field++];
    TXB0D5 = DummyData[(canwrite_state * 9) + field++];
    TXB0D6 = DummyData[(canwrite_state * 9) + field++];
    TXB0D7 = DummyData[(canwrite_state * 9) + field++];

    TXB0DLC = 0b00001000; // data length (8)
    TXB0CON |= 0b00001000; // mark for transmission

    canwrite_state = (canwrite_state+1)%DATA_COUNT;
    }
  }

// These are the diagnostic command handlers

void diag_handle_sms(char *command, char *arguments)
  {
  char *p = par_get(PARAM_REGPHONE);
  if (*p != 0)
    {
    strncpy(net_caller,p,NET_TEL_MAX);
    }
  else
    {
    strcpy(net_caller,(const char*)"OVMSDIAG");
    }
  net_puts_rom("\r\n");
  net_sms_in(net_caller,arguments,strlen(arguments));
  }

void diag_handle_msg(char *command, char *arguments)
{
    net_puts_rom("\r\n");
    net_msg_cmd_in(arguments);
    if( net_msg_cmd_code && (net_msg_sendpending==0) )
        net_msg_cmd_do();
}

void diag_handle_help(char *command, char *arguments)
  {
  net_puts_rom("\r\n");
  delay100(1);
  net_puts_rom("# COMMANDS: HELP ? DIAG RESET or S ...\r\n");
  net_puts_rom("# 'S' COMMANDS:\r\n  ");
  diag_handle_sms(command,command);
  net_puts_rom("# 'M' COMMANDS:\r\n  ");
  net_msgp_capabilities(0);
  }

void diag_handle_reset(char *command, char *arguments)
  {
  led_set(OVMS_LED_GRN,OVMS_LED_OFF);
  led_set(OVMS_LED_RED,OVMS_LED_OFF);
  led_start();
  net_puts_rom("\r\n");
  delay100(1);
  net_puts_rom("# Leaving Diagnostics Mode\r\n");
  delay100(10);
  reset_cpu();
}

void diag_handle_diag(char *command, char *arguments)
  {
  unsigned int x;
  unsigned char hwv = 1;
  char *p;

  #ifdef OVMS_HW_V2
  hwv = 2;
  #endif

  net_puts_rom("\r\n# DIAG\r\n\n");

  p = par_get(PARAM_VEHICLETYPE);
  if (vehicle_version)
      strcpypgm2ram(net_msg_scratchpad, vehicle_version);
  else
      net_msg_scratchpad[0] = 0;

  sprintf(net_scratchpad, (rom far char*)"# Firmware: %d.%d.%d/%s%s/V%d\r\n",
          ovms_firmware[0],ovms_firmware[1],ovms_firmware[2],
          p, net_msg_scratchpad, hwv);
  net_puts_ram(net_scratchpad);

  sprintf(net_scratchpad, (rom far char*)"#  SWITCH:   %d\r\n", inputs_gsmgprs());
  net_puts_ram(net_scratchpad);

  #ifdef OVMS_HW_V2
  x = inputs_voltage()*10;
  sprintf(net_scratchpad, (rom far char*)"#  12V Line: %d.%d V\r\n", x/10,x%10);
  net_puts_ram(net_scratchpad);
  #endif // #ifdef OVMS_HW_V2

  sprintf(net_scratchpad, (rom far char*)"#  Signal:   %d\r\n\n", net_sq);
  net_puts_ram(net_scratchpad);

  sprintf(net_scratchpad, (rom far char*)"#  VIN:      %s (%s)\r\n",
          car_vin, car_type);
  net_puts_ram(net_scratchpad);

  sprintf(net_scratchpad, (rom far char*)"#  SOC:      %d%% (%d ideal, %d est miles)\r\n",
          car_SOC, car_idealrange, car_estrange);
  net_puts_ram(net_scratchpad);

  if (canwrite_state>=0)
    {
    sprintf(net_scratchpad, (rom far char*)"#  Sim Tx: %d/%d\r\n",
            canwrite_state,DATA_COUNT);
    net_puts_ram(net_scratchpad);
    }

  net_puts_rom("\n");

  delay100(5);
  net_puts_rom("AT+COPS?\r");
  delay100(5);
  net_puts_rom("AT+CPIN?\r");
  delay100(5);
  net_puts_rom("AT+CSQ\r");
  }

void diag_handle_csq(char *command, char *arguments)
  {
  net_sq = atoi(arguments);
  }

void diag_handle_cantxstart(char *command, char *arguments)
  {
  // We're going to initialise ourselves as a CAN bus transmitter
  // and transmit a repeating sequence of messages. A simulator...
  orig_canwrite = sys_features[FEATURE_CANWRITE];
  sys_features[FEATURE_CANWRITE] = 1;
  vehicle_initialise();
  canwrite_state = 0;

  net_puts_rom("# Starting CAN sim\r\n");
  delay100(2);
  }

void diag_handle_cantxstop(char *command, char *arguments)
  {
  // Stop transmitting can bus messages
  sys_features[FEATURE_CANWRITE] = orig_canwrite;
  vehicle_initialise();
  canwrite_state = -1;

  net_puts_rom("# Stopped CAN sim\r\n");
  delay100(2);
  }

// This is the DIAG command table
//
// We're a bit limited by PIC C syntax (in particular no function pointers
// as members of structures), so keep it simple and use two tables. The first
// is a list of command strings (left match, all upper case). The second are
// the command handler function pointers. The command string table array is
// terminated by an empty "" command.

rom char diag_cmdtable[][27] =
  { "HELP",
    "?",
    "RESET",
    "DIAG",
    "+CSQ:",
    "CANTXSTART",
    "CANTXSTOP",
    "" };

rom void (*diag_hfntable[])(char *command, char *arguments) =
  {
  &diag_handle_help,
  &diag_handle_help,
  &diag_handle_reset,
  &diag_handle_diag,
  &diag_handle_csq,
  &diag_handle_cantxstart,
  &diag_handle_cantxstop
  };

// net_sms_in handles reception of an SMS message
//
// It tries to find a matching command handler based on the
// command tables.

void diag_activity(char *buf, unsigned char pos)
  {
  // The buf contains a DIAG command
  char *p;
  int k;

  if ((*buf == 0) || (*buf == '#'))
      return; // Ignore empty commands and comments/debug outputs

  // Convert DIAG command (first word) to upper-case
  for (p=buf; ((*p!=0)&&(*p!=' ')); p++)
  	if ((*p > 0x60) && (*p < 0x7b)) *p=*p-0x20;
  if (*p==' ') p++;

  // Command parsing...
  for (k=0; diag_cmdtable[k][0] != 0; k++)
    {
    if (memcmppgm2ram(buf, (char const rom far*)diag_cmdtable[k], strlenpgm((char const rom far*)diag_cmdtable[k])) == 0)
      {
      (*diag_hfntable[k])(buf, p);
      return;
      }
    }
  if ((buf[0]=='S')&&(buf[1]==' '))
    {
    // A 'S' command...
    diag_handle_sms(buf,buf+2);
    }
  else if ((buf[0]=='M')&&(buf[1]==' '))
    {
    // A 'M' command...
    diag_handle_msg(buf,buf+2);
    }

  // Just ignore any commands that don't match
  }
