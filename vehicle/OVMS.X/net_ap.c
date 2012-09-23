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
#include "params.h"
#include "net.h"
#include "net_msg.h"
#include "led.h"

#define AP_NOTDONE        255
#define AP_DONTDOIT       254
#define AP_ALREADYDONE    253
#define AP_NOPROFILE      199
#define AP_HIGHESTPROFILE 199

#pragma udata
unsigned char ap_profile = AP_NOTDONE;

// Fields are ICCID-prefix, cellular APN, username, password and gsmlock
rom char ap_profiletable[][5][22] =
  { {"8923418","MOBILEDATA","","",""},                // Profile 0: geosim
    {"",       "WAP.CINGULAR","","",""},              // Profile 1: AT&T GoPhone
    {"",       "ISP.cingular","","",""},              // Profile 2: ATT Telemetry SIM for business
    {"",       "imobile.three.com.hk","","","3(2G)"}, // Profile 3: ThreeHK
    {"",       "internet.com","wapuser1","wap",""},   // Profile 4: Rogers "Pay as you go"
    {"",       "gcpre","","",""},                     // Profile 5: Guam Docomo Pacific GO!
    {"","","","",""}
  };

rom char AP_SERVER[] = "202.52.42.80";

void net_ap_init(void)
  {
  ap_profile = AP_NOTDONE;
  }

BOOL net_ap_running(void)
  {
  // We are only called after processing SIM phonebook and ICCID, so
  // if nothing matched the profile, we can 'assume' that AP is not active
  if (ap_profile == AP_NOTDONE) ap_profile = AP_ALREADYDONE;

  // Return TRUE if there is an activate AP profile
  return (ap_profile <= AP_HIGHESTPROFILE);
  }

BOOL net_ap_param(void)
  {
  // Return TRUE if there is an activate AP profile
  return (ap_profile <= AP_HIGHESTPROFILE);
  }

BOOL net_ap_par_get(char* target, int maxlen, unsigned char param)
  {
  if (ap_profile > AP_HIGHESTPROFILE) return NULL;
  if (ap_profile == AP_NOPROFILE) return NULL;

  if (param == PARAM_GPRSAPN)
    strncpypgm2ram(target,ap_profiletable[ap_profile][1],maxlen);
  else if (param == PARAM_GPRSUSER)
    strncpypgm2ram(target,ap_profiletable[ap_profile][2],maxlen);
  else if (param == PARAM_GPRSPASS)
    strncpypgm2ram(target,ap_profiletable[ap_profile][3],maxlen);
  else if (param == PARAM_GSMLOCK)
    {
    if (ap_profiletable[ap_profile][4][0] == 0)
      return FALSE; // Just fall through to whatever lock the user already has
    strncpypgm2ram(target,ap_profiletable[ap_profile][4],maxlen);
    }
  else if (param == PARAM_SERVERIP)
    strncpypgm2ram(target,AP_SERVER,maxlen);
  else
    return FALSE;

  target[maxlen-1] = 0;
  return TRUE;
  }

void net_ap_register(void)
  {
  led_set(OVMS_LED_GRN,OVMS_LED_OFF);
  led_set(OVMS_LED_RED,NET_LED_ERRAUTOPROV);
  net_puts_rom("AP-C 0 ");
  net_puts_ram(car_vin);
  net_puts_rom("\r\n");
  }

void net_ap_in(char* msg)
  {
  char *t,*d,*r;
  int k;

  if (ap_profile == AP_ALREADYDONE) return;

  t = strtokpgmram(msg," ");
  if (t==NULL) return;
  if (strcmppgm2ram(t,"AP-S")!=0)
    {
    ap_profile = AP_ALREADYDONE;
    net_state_enter(NET_STATE_SOFTRESET);
    return;
    }
  t = strtokpgmram(NULL," ");
  if (t==NULL) return;
  if (strcmppgm2ram(t,"0")!=0) return;
  t = strtokpgmram(NULL," ");
  if (t==NULL) return;
  d = strtokpgmram(NULL," ");
  if (d==NULL) return;
  r = strtokpgmram(NULL," ");
  if (r==NULL) return;

  // At this point, t is the token, d is the digest, and r is the record
  hmac_md5(t, strlen(t), net_iccid, strlen(net_iccid), digest);
  base64encode(digest, MD5_SIZE, net_scratchpad);

  if (strncmp(d,net_scratchpad,22)!=0)
    {
    // Server digest does not validate...
    ap_profile = AP_ALREADYDONE;
    net_state_enter(NET_STATE_SOFTRESET);
    return;
    }

  // Setup, and prime the rx crypto
  RC4_setup(&rx_crypto1, &rx_crypto2, digest, MD5_SIZE);
  for (k=0;k<1024;k++)
    {
    net_scratchpad[0] = 0;
    RC4_crypt(&rx_crypto1, &rx_crypto2, net_scratchpad, 1);
    }

  // Go ahead and decode the message...
  k = base64decode(r,net_scratchpad);
  RC4_crypt(&rx_crypto1, &rx_crypto2, net_scratchpad, k);

  // Now process the Auto Provision message...
  r = strtokpgmram(net_scratchpad," ");
  for (k=0;(r != NULL);k++,r = strtokpgmram(NULL," "))
    {
    d = par_get(k);
    if ((r[0]=='-')&&(r[1]==0))
      {
      if (*d != 0) par_set(k,(char*)"");
      }
    else
      {
      if (strcmp(d,r)!=0) par_set(k,r);
      }
    }

  led_set(OVMS_LED_GRN,OVMS_LED_OFF);
  led_set(OVMS_LED_RED,OVMS_LED_OFF);
  ap_profile = AP_ALREADYDONE;
  net_state_enter(NET_STATE_SOFTRESET);
  }

void net_ap_iccid(char *iccid)
  {
  // We have an ICCID for the SIM
  // Looking like 89xxxxxx...
  int k;

  if (ap_profile == AP_ALREADYDONE) return;

  for (k=0;(ap_profiletable[k][1][0] != 0);k++)
    {
    if (strncmpram2pgm((rom char*)ap_profiletable[k][0],iccid,strlenpgm(ap_profiletable[k][0])) == 0)
      {
      // We have matched the AP profile, based on ICCID...
      ap_profile = k;
      return;
      }
    }
  }

void net_ap_phonebook(char *pb)
  {
  // We have a phonebook entry line from the SIM card / modem
  // Looking like: +CPBF: 1,"0",129,"OVMS-AUTO"
  char *n,*p, *ep;

  if (ap_profile == AP_ALREADYDONE) return;

  n = strtokpgmram(pb,"\"");
  if (n==NULL) return;
  n = strtokpgmram(NULL,"\"");
  if (n==NULL) return;
  p = strtokpgmram(NULL,"\"");
  if (p==NULL) return;
  p = strtokpgmram(NULL,"\"");
  if (p==NULL) return;

  // At this point, *n is the second token (the number), and *p is
  // the fourth token (the parameter). Both are defined.
  if (strcmppgm2ram(p,"OVMS-AUTO")==0)
    {
    ap_profile = atoi(n);
    }
  else if (strcmppgm2ram(p,"OVMS-NOAUTO")==0)
    {
    ap_profile = AP_DONTDOIT;
    }
  else if ((p[0]=='O')&&(p[1]=='-'))
    {
    n = strtokpgmram(p+2,"-");
    if (n==NULL) return;
    p = strtokpgmram(NULL,"-");
    ep = par_get(atoi(n));
    if (p==NULL)
      {
      if (ep[0] != 0)
        par_set((unsigned char)atoi(n), (char*)"");
      }
    else
      {
      if (strcmp(ep,p) != 0)
        par_set((unsigned char)atoi(n), p);
      }
    }
  }
