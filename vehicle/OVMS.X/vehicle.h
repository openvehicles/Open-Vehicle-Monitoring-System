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

#ifndef __OVMS_VEHICLE_H
#define __OVMS_VEHICLE_H

extern unsigned int   can_granular_tick;         // An internal ticker used to generate 1min, 5min, etc, calls
extern unsigned char  can_datalength;            // The number of valid bytes in the can_databuffer
extern unsigned char  can_databuffer[8];
extern unsigned char  can_minSOCnotified;        // minSOC notified flag
extern unsigned char  can_mileskm;               // Miles of Kilometers
extern unsigned char* can_capabilities;          // Vehicle capabilities

// These are the hook functions. The convention is that if a function is not
// NULL then it is called at the appropriate time. If it returns TRUE then
// it is considered to have completed the operation and the default action
// should not be run at all. If it returns FALSE then it is an indication
// that the default action should run as well.
//
extern rom BOOL (*vehicle_fn_init)(void);
extern rom BOOL (*vehicle_fn_poll0)(void);
extern rom BOOL (*vehicle_fn_poll1)(void);
extern rom BOOL (*vehicle_fn_ticker1)(void);
extern rom BOOL (*vehicle_fn_ticker60)(void);
extern rom BOOL (*vehicle_fn_ticker300)(void);
extern rom BOOL (*vehicle_fn_ticker600)(void);
extern rom BOOL (*vehicle_fn_ticker)(void);
extern rom BOOL (*vehicle_fn_ticker10th)(void);
extern rom BOOL (*vehicle_fn_idlepoll)(void);
extern rom BOOL (*vehicle_fn_commandhandler)(BOOL msgmode, int code, char* msg);
extern rom BOOL (*vehicle_fn_smshandler)(BOOL premsg, char *caller, char *command, char *arguments);
extern rom BOOL (*vehicle_fn_smsextensions)(char *caller, char *command, char *arguments);

void vehicle_initialise(void);

void vehicle_ticker(void);
void vehicle_ticker10th(void);
void vehicle_idlepoll(void);

#endif // #ifndef __OVMS_VEHICLE_H
