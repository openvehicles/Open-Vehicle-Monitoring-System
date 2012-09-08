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
#include "led.h"

// DIAG data
#pragma udata

void diag_initialise(void)
  {
  led_set(OVMS_LED_GRN,NET_LED_ERRDIAGMODE);
  led_set(OVMS_LED_RED,NET_LED_ERRDIAGMODE);
  led_start();
  net_timeout_ticks = 0;
  net_timeout_goto = 0;
  net_puts_rom("\x1B[2J\x1B[01;01H\rOVMS DIAGNOSTICS MODE\r\n\n");
  }

void diag_ticker(void)
  {
  }

// These are the diagnostic command handlers

void diag_handle_help(char *command, char *arguments)
  {
  net_puts_rom("\nCOMMANDS: HELP, ?, RESET\r\n\n");
  }

void diag_handle_reset(char *command, char *arguments)
  {
  led_set(OVMS_LED_GRN,OVMS_LED_OFF);
  led_set(OVMS_LED_RED,OVMS_LED_OFF);
  led_start();
  net_puts_rom("\n\n\rLeaving Diagnostics Mode\r\n");
  net_state_enter(NET_STATE_FIRSTRUN);
  }

// This is the SMS command table
//
// We're a bit limited by PIC C syntax (in particular no function pointers
// as members of structures), so keep it simple and use two tables. The first
// is a list of command strings (left match, all upper case). The second are
// the command handler function pointers. The command string table array is
// terminated by an empty "" command.

rom char diag_cmdtable[][10] =
  { "HELP",
    "?",
    "RESET"
    "" };

rom void (*diag_hfntable[])(char *command, char *arguments) =
  {
  &diag_handle_help,
  &diag_handle_help,
  &diag_handle_reset
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

  if (*buf == 0) return; // Ignore empty commands

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
  // Just ignore any commands that don't match
  }
