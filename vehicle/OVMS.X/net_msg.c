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
#include "net.h"
#include "can.h"
#include "net_msg.h"
#include "net_sms.h"
#include "crypt_base64.h"
#include "crypt_md5.h"
#include "crypt_hmac.h"
#include "crypt_rc4.h"
#include "utils.h"

// NET_MSG data
#define TOKEN_SIZE 22
#pragma udata
char net_msg_serverok = 0;
char net_msg_sendpending = 0;
char token[23] = {0};
char ptoken[23] = {0};
char ptokenmade = 0;
char digest[MD5_SIZE];
char pdigest[MD5_SIZE];
WORD crc_stat = 0;
WORD crc_gps = 0;
WORD crc_tpms = 0;
WORD crc_firmware = 0;
WORD crc_environment = 0;
WORD crc_group1 = 0;
WORD crc_group2 = 0;

#pragma udata NETMSG_SP
char net_msg_scratchpad[NET_BUF_MAX];
#pragma udata

#pragma udata Q_CMD
int  net_msg_cmd_code = 0;
char net_msg_cmd_msg[100];

#pragma udata TX_CRYPTO
RC4_CTX2 tx_crypto2;
#pragma udata RX_CRYPTO
RC4_CTX2 rx_crypto2;
#pragma udata PM_CRYPTO
RC4_CTX2 pm_crypto2;
#pragma udata
RC4_CTX1 tx_crypto1;
RC4_CTX1 rx_crypto1;
RC4_CTX1 pm_crypto1;

rom char NET_MSG_OK[] = "MP-0 c%d,0";
rom char NET_MSG_INVALIDSYNTAX[] = "MP-0 c%d,1,Invalid syntax";
rom char NET_MSG_NOCANWRITE[] = "MP-0 c%d,1,No write access to CAN";
rom char NET_MSG_INVALIDRANGE[] = "MP-0 c%d,1,Parameter out of range";
rom char NET_MSG_NOCANCHARGE[] = "MP-0 c%d,1,Cannot charge (charge port closed)";
rom char NET_MSG_NOCANSTOPCHARGE[] = "MP-0 c%d,1,Cannot stop charge (charge not in progress)";

void net_msg_init(void)
  {
  net_msg_cmd_code = 0;
  }

void net_msg_disconnected(void)
  {
  net_msg_serverok = 0;
  }

// Start to send a net msg
void net_msg_start(void)
  {
  net_msg_sendpending = 1;
  delay100(5);
  net_puts_rom("AT+CIPSEND\r");
  delay100(10);
  }

// Finish sending a net msg
void net_msg_send(void)
  {
  net_puts_rom("\x1a");
  }

// Encode the message in net_scratchpad and start the send process
void net_msg_encode_puts(void)
  {
  int k;
  char code;

  if ((ptokenmade==1)&&
      (net_scratchpad[5]!='E')&&
      (net_scratchpad[5]!='A')&&
      (net_scratchpad[5]!='a')&&
      (net_scratchpad[5]!='g')&&
      (net_scratchpad[5]!='P'))
    {
    // We must convert the message to a paranoid one...
    // The message in net_scratchpad is of the form MP-0 X...
    // Where X is the code and ... is the (optional) data
    // Let's rebuild it in the net_msg_scratchpad...
    code = net_scratchpad[5];
    strcpy(net_msg_scratchpad,net_scratchpad+6);

    // Paranoid encrypt the message part of the transaction
    RC4_setup(&pm_crypto1, &pm_crypto2, pdigest, MD5_SIZE);
    for (k=0;k<1024;k++)
      {
      net_scratchpad[0] = 0;
      RC4_crypt(&pm_crypto1, &pm_crypto2, net_scratchpad, 1);
      }
    k=strlen(net_msg_scratchpad);
    RC4_crypt(&pm_crypto1, &pm_crypto2, net_msg_scratchpad, k);

    strcpypgm2ram(net_scratchpad,(char const rom far*)"MP-0 EM");
    net_scratchpad[7] = code;
    base64encode(net_msg_scratchpad,k,net_scratchpad+8);
    // The messdage is now in paranoid mode...
    }

  k=strlen(net_scratchpad);
  RC4_crypt(&tx_crypto1, &tx_crypto2, net_scratchpad, k);
  base64encodesend(net_scratchpad,k);
  net_puts_rom("\r\n");
  }

// Register to the NET OVMS server
void net_msg_register(void)
  {
  char k;
  char *p;
  unsigned int sr;

  // Make a (semi-)random client token
  sr = TMR0L*256;
  sr += TMR0H;
  for (k=0;k<8;k++)
    {
    sr += can_databuffer[k];
    }
  srand(sr);
  for (k=0;k<TOKEN_SIZE;k++)
    {
    token[k] = cb64[rand()%64];
    }
  token[TOKEN_SIZE] = 0;

  p = par_get(PARAM_SERVERPASS);
  hmac_md5(token, TOKEN_SIZE, p, strlen(p), digest);

  net_puts_rom("MP-C 0 ");
  net_puts_ram(token);
  net_puts_rom(" ");
  base64encodesend(digest, MD5_SIZE);
  net_puts_rom(" ");
  p = par_get(PARAM_VEHICLEID);
  net_puts_ram(p);
  net_puts_rom("\r\n");
  }

// net_msgp_*
//
// The net_msgp_* function output message parts.
// They all operate in the same way and are controlled by a <stat> parameter.
// stat:
//   0= Always output the message, and assume net_msg_start() has already been done
//      Return 0
//   2= If message has changed, call net_msg_start(), output msg, return 1
//      else return 2
//   1= If message has changed, output msg
//      Always return 1
//
// A caller that has done net_msg_start() and always wants output is to just use stat=0
// A caller that wants only changed output does not call net_msg_start(), but instead
// sets stat=2 and calls the functions one after the other, setting stat with the result of the call
// At the end, if stat=1, call net_msg_send().

// <stat> guarded encode the message in net_scratchpad and start the send process
char net_msg_encode_statputs(char stat, WORD *oldcrc)
  {
  WORD newcrc = crc16(net_scratchpad, strlen(net_scratchpad));

  switch (stat)
    {
    case 0:
      // Always output
      net_msg_encode_puts();
      *oldcrc = newcrc;
      break;
    case 1:
      // Guarded output, but net_msg_start() has already been sent
      if (*oldcrc != newcrc)
        {
        net_msg_encode_puts();
        *oldcrc = newcrc;
        }
      break;
    case 2:
      // Guarded output, but net_msg_start() has not yet been sent
      if (*oldcrc != newcrc)
        {
        net_msg_start();
        net_msg_encode_puts();
        *oldcrc = newcrc;
        stat = 1;
        }
      break;
    }
  return stat;
  }

char net_msgp_stat(char stat)
  {
  char *p;

  strcpypgm2ram(net_scratchpad,(char const rom far*)"MP-0 S");
  p = par_get(PARAM_MILESKM);
  sprintf(net_msg_scratchpad,(rom far char*)"%d,%s,%d,%d,",car_SOC,p,car_linevoltage,car_chargecurrent);
  strcat(net_scratchpad,net_msg_scratchpad);
  switch (car_chargestate)
    {
    case 0x01:
      strcatpgm2ram(net_scratchpad,(char const rom far*)"charging,"); // Charge State Charging
      break;
    case 0x02:
      strcatpgm2ram(net_scratchpad,(char const rom far*)"topoff,"); // Topping off
      break;
    case 0x04:
      strcatpgm2ram(net_scratchpad,(char const rom far*)"done,"); // Done
      break;
    case 0x0d:
      strcatpgm2ram(net_scratchpad,(char const rom far*)"prepare,"); // Preparing
      break;
    case 0x0f:
      strcatpgm2ram(net_scratchpad,(char const rom far*)"heating,"); // Heating
      break;
    default:
      strcatpgm2ram(net_scratchpad,(char const rom far*)"stopped,"); // Stopped
    }
  switch (car_chargemode)
    {
    case 0x00:
      strcatpgm2ram(net_scratchpad,(char const rom far*)"standard,"); // Charge Mode Standard
      break;
    case 0x01:
      strcatpgm2ram(net_scratchpad,(char const rom far*)"storage,"); // Storage
      break;
    case 0x03:
      strcatpgm2ram(net_scratchpad,(char const rom far*)"range,"); // Range
      break;
    case 0x04:
      strcatpgm2ram(net_scratchpad,(char const rom far*)"performance,"); // Performance
      break;
    default:
      strcatpgm2ram(net_scratchpad,(char const rom far*)",");
    }
  if (*p == 'M') // Kmh or Miles
    sprintf(net_msg_scratchpad, (rom far char*)"%u,", car_idealrange);
  else
    sprintf(net_msg_scratchpad, (rom far char*)"%u,", (((car_idealrange << 4)+5)/10));
  strcat(net_scratchpad,net_msg_scratchpad);
  if (*p == 'M') // Kmh or Miles
    sprintf(net_msg_scratchpad, (rom far char*)"%u,", car_estrange);
  else
    sprintf(net_msg_scratchpad, (rom far char*)"%u,", (((car_estrange << 4)+5)/10));
  strcat(net_scratchpad,net_msg_scratchpad);
  sprintf(net_msg_scratchpad,(rom far char*)"%d,%u,%d,%d,%d,%d,%d,%d,%d,%d",
          car_chargelimit,car_chargeduration,
          car_charge_b4, car_chargekwh, car_chargesubstate,
          car_chargestate,car_chargemode,
          car_timermode,car_timerstart,car_stale_timer);
  strcat(net_scratchpad,net_msg_scratchpad);

  return net_msg_encode_statputs(stat, &crc_stat);
  }

char net_msgp_gps(char stat)
  {
  strcpypgm2ram(net_scratchpad,(char const rom far*)"MP-0 L");
  format_latlon(car_latitude,net_msg_scratchpad);
  strcat(net_scratchpad,net_msg_scratchpad);
  strcatpgm2ram(net_scratchpad,(char const rom far*)",");
  format_latlon(car_longitude,net_msg_scratchpad);
  strcat(net_scratchpad,net_msg_scratchpad);
  sprintf(net_msg_scratchpad, (rom far char*)",%d,%d,%d,%d",
          car_direction, car_altitude, car_gpslock,car_stale_gps);
  strcat(net_scratchpad,net_msg_scratchpad);

  return net_msg_encode_statputs(stat, &crc_gps);
  }

char net_msgp_tpms(char stat)
  {
  char k;
  long p;
  int b,a;

  if ((car_tpms_t[0]==0)&&(car_tpms_t[1]==0)&&
      (car_tpms_t[2]==0)&&(car_tpms_t[3]==0))
    return stat; // No TPMS, no report

  strcpypgm2ram(net_scratchpad,(char const rom far*)"MP-0 W");
  for (k=0;k<4;k++)
    {
    if (car_tpms_t[k]>0)
      {
      p = (long)((float)car_tpms_p[k]/0.2755);
      b = (p / 10);
      a = (p % 10);
      sprintf(net_msg_scratchpad, (rom far char*)"%d.%d,%d,",
              b,a,(int)(car_tpms_t[k]-40));
      strcat(net_scratchpad,net_msg_scratchpad);
      }
    else
      {
      strcatpgm2ram(net_scratchpad, (rom far char*)"0,0,");
      }
    }
  sprintf(net_msg_scratchpad, (rom far char*)"%d",car_stale_tpms);
  strcat(net_scratchpad,net_msg_scratchpad);

  return net_msg_encode_statputs(stat, &crc_tpms);
  }

char net_msgp_firmware(char stat)
  {
  // Send firmware version and GSM signal level
  unsigned char hwv = 1;
  #ifdef OVMS_HW_V2
  hwv = 2;
  #endif

  strcpypgm2ram(net_scratchpad,(char const rom far*)"MP-0 F");
  sprintf(net_msg_scratchpad, (rom far char*)"%d.%d.%d/V%d,%s,%d,%d,%s,%s",
    ovms_firmware[0],ovms_firmware[1],ovms_firmware[2],hwv,
    car_vin, net_sq, sys_features[FEATURE_CANWRITE],car_type,
    car_gsmcops);
  strcat(net_scratchpad,net_msg_scratchpad);

  return net_msg_encode_statputs(stat, &crc_firmware);
  }

char net_msgp_environment(char stat)
  {
  unsigned long park;

  if (car_parktime == 0)
    park = 0;
  else
    park = car_time - car_parktime;

  strcpypgm2ram(net_scratchpad,(char const rom far*)"MP-0 D");
  sprintf(net_msg_scratchpad, (rom far char*)"%d,%d,%d,%d,%d,%d,%d,%lu,%d,%lu,%d,%d,%d,%d,%d.%d,%d",
          car_doors1, car_doors2, car_lockstate,
          car_tpem, car_tmotor, car_tbattery,
          car_trip, car_odometer, car_speed, park,
          car_ambient_temp, car_doors3,
          car_stale_temps, car_stale_ambient,
          car_12vline/10,car_12vline%10,
          car_doors4);
  strcat(net_scratchpad,net_msg_scratchpad);

  return net_msg_encode_statputs(stat, &crc_environment);
  }

char net_msgp_group(char stat, char groupnumber, char *groupname)
  {
  strcpypgm2ram(net_scratchpad,(char const rom far*)"MP-0 g");
  sprintf(net_msg_scratchpad, (rom far char*)"%s,%d,%d,%d,%d,%d,%d,",
          groupname, car_SOC, car_speed,
          car_direction, car_altitude, car_gpslock, car_stale_gps);
  strcat(net_scratchpad,net_msg_scratchpad);

  format_latlon(car_latitude,net_msg_scratchpad);
  strcat(net_scratchpad,net_msg_scratchpad);
  strcatpgm2ram(net_scratchpad,(char const rom far*)",");
  format_latlon(car_longitude,net_msg_scratchpad);
  strcat(net_scratchpad,net_msg_scratchpad);

  if (groupnumber==1)
    return net_msg_encode_statputs(stat, &crc_group1);
  else
    return net_msg_encode_statputs(stat, &crc_group1);
  }

void net_msg_server_welcome(char *msg)
  {
  // The server has sent a welcome (token <space> base64digest)
  char *d,*p;
  int k;

  for (d=msg;(*d != 0)&&(*d != ' ');d++) ;
  if (*d != ' ') return;
  *d++ = 0;

  // At this point, <msg> is token, and <x> is base64digest
  // (both null-terminated)

  // Check for token-replay attack
  if (strcmp(token,msg)==0)
    return; // Server is using our token!

  // Validate server token
  p = par_get(PARAM_SERVERPASS);
  hmac_md5(msg, strlen(msg), p, strlen(p), digest);
  base64encode(digest, MD5_SIZE, net_scratchpad);
  if (strcmp(d,net_scratchpad)!=0)
    return; // Invalid server digest

  // Ok, at this point, our token is ok
  strcpy(net_scratchpad,msg);
  strcat(net_scratchpad,token);
  hmac_md5(net_scratchpad,strlen(net_scratchpad),p,strlen(p),digest);

  // Setup, and prime the rx and tx cryptos
  RC4_setup(&rx_crypto1, &rx_crypto2, digest, MD5_SIZE);
  for (k=0;k<1024;k++)
    {
    net_scratchpad[0] = 0;
    RC4_crypt(&rx_crypto1, &rx_crypto2, net_scratchpad, 1);
    }
  RC4_setup(&tx_crypto1, &tx_crypto2, digest, MD5_SIZE);
  for (k=0;k<1024;k++)
    {
    net_scratchpad[0] = 0;
    RC4_crypt(&tx_crypto1, &tx_crypto2, net_scratchpad, 1);
    }

  net_msg_serverok = 1;

  p = par_get(PARAM_PARANOID);
  if (*p == 'P')
    {
    // Paranoid mode initialisation
    if (ptokenmade==0)
      {
      // We need to make the ptoken
      for (k=0;k<TOKEN_SIZE;k++)
        {
        ptoken[k] = cb64[rand()%64];
        }
      ptoken[TOKEN_SIZE] = 0;
      }

    // To be truly paranoid, we must send the paranoid token to the server ;-)
    ptokenmade=0; // Leave it off for the MP-0 ET message
    strcpypgm2ram(net_scratchpad,(char const rom far*)"MP-0 ET");
    strcat(net_scratchpad,ptoken);
    net_msg_start();
    net_msg_encode_puts();
    net_msg_send();
    ptokenmade=1; // And enable paranoid mode from now on...

    // And calculate the pdigest for future use
    p = par_get(PARAM_MODULEPASS);
    hmac_md5(ptoken, strlen(ptoken), p, strlen(p), pdigest);
    }
  else
    {
    ptokenmade = 0; // This disables paranoid mode
    }
  }

// Receive a NET msg from the OVMS server
void net_msg_in(char* msg)
  {
  int k;
  char s;

  if (net_msg_serverok == 0)
    {
    if (memcmppgm2ram(msg, (char const rom far*)"MP-S 0 ", 7) == 0)
      {
      net_msg_server_welcome(msg+7);
      net_granular_tick = 3590; // Nasty hack to force a status transmission in 10 seconds
      }
    return; // otherwise ignore it
    }

  // Ok, we've got an encrypted message waiting for work.
  // The following is a nasty hack because base64decode doesn't like incoming
  // messages of length divisible by 4, and is really expecting a CRLF
  // terminated string, so we give it one...
  strcatpgm2ram(msg,(char const rom far*)"\r\n");
  k = base64decode(msg,net_scratchpad);
  RC4_crypt(&rx_crypto1, &rx_crypto2, net_scratchpad, k);
  if (memcmppgm2ram(net_scratchpad, (char const rom far*)"MP-0 ", 5) != 0)
    {
    net_state_enter(NET_STATE_DONETINIT);
    return;
    }
  msg = net_scratchpad+5;

  if ((*msg == 'E')&&(msg[1]=='M'))
    {
    // A paranoid-mode message from the server (or, more specifically, app)
    // The following is a nasty hack because base64decode doesn't like incoming
    // messages of length divisible by 4, and is really expecting a CRLF
    // terminated string, so we give it one...
    msg += 2; // Now pointing to the code just before encrypted paranoid message
    strcatpgm2ram(msg,(char const rom far*)"\r\n");
    k = base64decode(msg+1,net_msg_scratchpad+1);
    RC4_setup(&pm_crypto1, &pm_crypto2, pdigest, MD5_SIZE);
    for (k=0;k<1024;k++)
      {
      net_scratchpad[0] = 0;
      RC4_crypt(&pm_crypto1, &pm_crypto2, net_scratchpad, 1);
      }
    RC4_crypt(&pm_crypto1, &pm_crypto2, net_msg_scratchpad+1, k);
    net_msg_scratchpad[0] = *msg; // The code
    // The message is now out of paranoid mode...
    msg = net_msg_scratchpad;
    }

  switch (*msg)
    {
    case 'A': // PING
      strcpypgm2ram(net_scratchpad,(char const rom far*)"MP-0 a");
      if (net_msg_sendpending==0)
        {
        net_msg_start();
        net_msg_encode_puts();
        net_msg_send();
        }
      break;
    case 'Z': // PEER connection
      if (msg[1] != '0')
        {
        net_apps_connected = 1;
        if (net_msg_sendpending==0)
          {
          net_msg_start();
          net_msgp_stat(0);
          net_msgp_gps(0);
          net_msgp_tpms(0);
          net_msgp_firmware(0);
          net_msgp_environment(0);
          net_msg_send();
          }
        }
      else
        {
        net_apps_connected = 0;
        }
      break;
    case 'C': // COMMAND
      net_msg_cmd_in(msg+1);
      if (net_msg_sendpending==0) net_msg_cmd_do();
      break;
    }
  }

void net_msg_cmd_in(char* msg)
  {
  // We have received a command message (pointed to by <msg>)
  char *d;
  int k;

  for (d=msg;(*d != 0)&&(*d != ',');d++) ;
  if (*d == ',')
    *d++ = 0;
  // At this point, <msg> points to the command, and <d> to the first param (if any)
  net_msg_cmd_code = atoi(msg);
  strcpy(net_msg_cmd_msg,d);
  }

void net_msg_cmd_do(void)
  {
  int k;
  char *p;

  delay100(2);

  // commands 40-49 are special AT commands, thus, disable net_msg here
  if ((net_msg_cmd_code < 40) || (net_msg_cmd_code > 49)) 
    net_msg_start();

  switch (net_msg_cmd_code)
    {
    case 1: // Request feature list (params unused)
      for (k=0;k<FEATURES_MAX;k++)
        {
        sprintf(net_scratchpad, (rom far char*)"MP-0 c1,0,%d,%d,%d",
                k,FEATURES_MAX,sys_features[k]);
        net_msg_encode_puts();
        }
      break;
    case 2: // Set feature (params: feature number, value)
      for (p=net_msg_cmd_msg;(*p != 0)&&(*p != ',');p++) ;
      // check if a value exists and is separated by a comma
      if (*p == ',')
        {
        *p++ = 0;
        // At this point, <net_msg_cmd_msg> points to the command, and <p> to the param value
        k = atoi(net_msg_cmd_msg);
        if ((k>=0)&&(k<FEATURES_MAX))
          {
          sys_features[k] = atoi(p);
          if (k>=FEATURES_MAP_PARAM) // Top N features are persistent
            par_set(PARAM_FEATURE_S+(k-FEATURES_MAP_PARAM), p);
          if (k == FEATURE_CANWRITE) can_initialise();
          sprintf(net_scratchpad, (rom far char*)NET_MSG_OK,net_msg_cmd_code);
          }
        else
          {
          sprintf(net_scratchpad, (rom far char*)NET_MSG_INVALIDRANGE,net_msg_cmd_code);
          }
        }
      else
        {
        sprintf(net_scratchpad, (rom far char*)NET_MSG_INVALIDSYNTAX,net_msg_cmd_code);
        }
      net_msg_encode_puts();
      break;
    case 3: // Request parameter list (params unused)
      for (k=0;k<PARAM_MAX;k++)
        {
        p = par_get(k);
        if (k==PARAM_SERVERPASS) *p=0; // Don't show netpass1
        sprintf(net_scratchpad, (rom far char*)"MP-0 c3,0,%d,%d,%s",
                k,PARAM_MAX,p);
        net_msg_encode_puts();
        }
      break;
    case 4: // Set parameter (params: param number, value)
      for (p=net_msg_cmd_msg;(*p != 0)&&(*p != ',');p++) ;
      // check if a value exists and is separated by a comma
      if (*p == ',')
        {
        *p++ = 0;
        // At this point, <net_msg_cmd_msg> points to the command, and <p> to the param value
        k = atoi(net_msg_cmd_msg);
        if ((k>=0)&&(k<PARAM_FEATURE_S))
          {
          par_set(k, p);
          sprintf(net_scratchpad, (rom far char*)NET_MSG_OK,net_msg_cmd_code);
          if (k == PARAM_MILESKM) can_initialise();
          }
        else
          {
          sprintf(net_scratchpad, (rom far char*)NET_MSG_INVALIDRANGE,net_msg_cmd_code);
          }
        }
      else
        {
        sprintf(net_scratchpad, (rom far char*)NET_MSG_INVALIDSYNTAX,net_msg_cmd_code);
        }
      net_msg_encode_puts();
      break;
    case 5: // Reboot (params unused)
      sprintf(net_scratchpad, (rom far char*)NET_MSG_OK,net_msg_cmd_code);
      net_msg_encode_puts();
      net_state_enter(NET_STATE_HARDSTOP);
      break;
    case 10: // Set charge mode (params: 0=standard, 1=storage,3=range,4=performance)
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        sprintf(net_scratchpad, (rom far char*)NET_MSG_NOCANWRITE,net_msg_cmd_code);
        }
      else
        {
        can_tx_setchargemode(atoi(net_msg_cmd_msg));
        sprintf(net_scratchpad, (rom far char*)NET_MSG_OK,net_msg_cmd_code);
        }
      net_msg_encode_puts();
      break;
    case 11: // Start charge (params unused)
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        sprintf(net_scratchpad, (rom far char*)NET_MSG_NOCANWRITE,net_msg_cmd_code);
        }
      else
        {
        if ((car_doors1 & 0x04)&&(car_chargesubstate != 0x07))
          {
          can_tx_startstopcharge(1);
          net_notify_suppresscount = 0; // Enable notifications
          sprintf(net_scratchpad, (rom far char*)NET_MSG_OK,net_msg_cmd_code);
          }
        else
          {
          sprintf(net_scratchpad, (rom far char*)NET_MSG_NOCANCHARGE,net_msg_cmd_code);
          }
        }
      net_msg_encode_puts();
      break;
    case 12: // Stop charge (params unused)
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        sprintf(net_scratchpad, (rom far char*)NET_MSG_NOCANWRITE,net_msg_cmd_code);
        }
      else
        {
        if ((car_doors1 & 0x10))
          {
          can_tx_startstopcharge(0);
          net_notify_suppresscount = 30; // Suppress notifications for 30 seconds
          sprintf(net_scratchpad, (rom far char*)NET_MSG_OK,net_msg_cmd_code);
          }
        else
          {
          sprintf(net_scratchpad, (rom far char*)NET_MSG_NOCANSTOPCHARGE,net_msg_cmd_code);
          }
        }
      net_msg_encode_puts();
      break;
    case 15: // Set charge current (params: current in amps)
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        sprintf(net_scratchpad, (rom far char*)NET_MSG_NOCANWRITE,net_msg_cmd_code);
        }
      else
        {
        can_tx_setchargecurrent(atoi(net_msg_cmd_msg));
        sprintf(net_scratchpad, (rom far char*)NET_MSG_OK,net_msg_cmd_code);
        }
      net_msg_encode_puts();
      break;
    case 16: // Set charge mode and current (params: mode, current)
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        sprintf(net_scratchpad, (rom far char*)NET_MSG_NOCANWRITE,net_msg_cmd_code);
        }
      else
        {
        for (p=net_msg_cmd_msg;(*p != 0)&&(*p != ',');p++) ;
        // check if a value exists and is separated by a comma
        if (*p == ',')
          {
          *p++ = 0;
          // At this point, <net_msg_cmd_msg> points to the mode, and p to the current
          can_tx_setchargemode(atoi(net_msg_cmd_msg));
          can_tx_setchargecurrent(atoi(p));
          sprintf(net_scratchpad, (rom far char*)NET_MSG_OK,net_msg_cmd_code);
          }
        else
          {
          sprintf(net_scratchpad, (rom far char*)NET_MSG_INVALIDSYNTAX,net_msg_cmd_code);
          }
        }
      net_msg_encode_puts();
      break;
    case 17: // Set charge timer mode and start time
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        sprintf(net_scratchpad, (rom far char*)NET_MSG_NOCANWRITE,net_msg_cmd_code);
        }
      else
        {
        for (p=net_msg_cmd_msg;(*p != 0)&&(*p != ',');p++) ;
        // check if a value exists and is separated by a comma
        if (*p == ',')
          {
          *p++ = 0;
          // At this point, <net_msg_cmd_msg> points to the mode, and p to the time
          can_tx_timermode(atoi(net_msg_cmd_msg),atoi(p));
          sprintf(net_scratchpad, (rom far char*)NET_MSG_OK,net_msg_cmd_code);
          }
        else
          {
          sprintf(net_scratchpad, (rom far char*)NET_MSG_INVALIDSYNTAX,net_msg_cmd_code);
          }
        }
      net_msg_encode_puts();
      break;
    case 18: // Wakeup car
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        sprintf(net_scratchpad, (rom far char*)NET_MSG_NOCANWRITE,net_msg_cmd_code);
        }
      else
        {
        can_tx_wakeup();
        can_tx_wakeuptemps();
        sprintf(net_scratchpad, (rom far char*)NET_MSG_OK,net_msg_cmd_code);
        }
      net_msg_encode_puts();
      break;
    case 19: // Wakeup temperature subsystem
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        sprintf(net_scratchpad, (rom far char*)NET_MSG_NOCANWRITE,net_msg_cmd_code);
        }
      else
        {
        can_tx_wakeuptemps();
        sprintf(net_scratchpad, (rom far char*)NET_MSG_OK,net_msg_cmd_code);
        }
      net_msg_encode_puts();
      break;
    case 20: // Lock car (params pin)
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        sprintf(net_scratchpad, (rom far char*)NET_MSG_NOCANWRITE,net_msg_cmd_code);
        }
      else
        {
        can_tx_lockunlockcar(2, net_msg_cmd_msg);
        sprintf(net_scratchpad, (rom far char*)NET_MSG_OK,net_msg_cmd_code);
        }
      net_msg_encode_puts();
      delay100(2);
      net_msgp_environment(0);
      break;
    case 21: // Activate valet mode (params pin)
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        sprintf(net_scratchpad, (rom far char*)NET_MSG_NOCANWRITE,net_msg_cmd_code);
        }
      else
        {
        can_tx_lockunlockcar(0, net_msg_cmd_msg);
        sprintf(net_scratchpad, (rom far char*)NET_MSG_OK,net_msg_cmd_code);
        }
      net_msg_encode_puts();
      delay100(2);
      net_msgp_environment(0);
      break;
    case 22: // Unlock car (params pin)
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        sprintf(net_scratchpad, (rom far char*)NET_MSG_NOCANWRITE,net_msg_cmd_code);
        }
      else
        {
        can_tx_lockunlockcar(3, net_msg_cmd_msg);
        sprintf(net_scratchpad, (rom far char*)NET_MSG_OK,net_msg_cmd_code);
        }
      net_msg_encode_puts();
      delay100(2);
      net_msgp_environment(0);
      break;
    case 23: // Deactivate valet mode (params pin)
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        sprintf(net_scratchpad, (rom far char*)NET_MSG_NOCANWRITE,net_msg_cmd_code);
        }
      else
        {
        can_tx_lockunlockcar(1, net_msg_cmd_msg);
        sprintf(net_scratchpad, (rom far char*)NET_MSG_OK,net_msg_cmd_code);
        }
      net_msg_encode_puts();
      delay100(2);
      net_msgp_environment(0);
      break;
    case 24: // Home Link
      if (sys_features[FEATURE_CANWRITE]==0)
        {
        sprintf(net_scratchpad, (rom far char*)NET_MSG_NOCANWRITE,net_msg_cmd_code);
        }
      else
        {
        can_tx_homelink(atoi(net_msg_cmd_msg));
        sprintf(net_scratchpad, (rom far char*)NET_MSG_OK,net_msg_cmd_code);
        }
      net_msg_encode_puts();
      break;
    case 40: // Send SMS (params: phone number, SMS message)
      for (p=net_msg_cmd_msg;(*p != 0)&&(*p != ',');p++) ;
      // check if a value exists and is separated by a comma
      if (*p == ',')
        {
        *p++ = 0;
        // At this point, <net_msg_cmd_msg> points to the phone number, and <p> to the SMS message
        net_send_sms_start(net_msg_cmd_msg);
        net_puts_ram(p);
        net_puts_rom("\x1a");
        delay100(5);
        net_msg_start();
        sprintf(net_scratchpad, (rom far char*)NET_MSG_OK,net_msg_cmd_code);
        }
      else
        {
        net_msg_start();
        sprintf(net_scratchpad, (rom far char*)NET_MSG_INVALIDSYNTAX,net_msg_cmd_code);
        }
      net_msg_encode_puts();
      delay100(2);
      break;
    case 41: // Send MMI/USSD Codes (param: USSD_CODE)
      net_puts_rom("AT+CUSD=1,\"");
      net_puts_ram(net_msg_cmd_msg);
      net_puts_rom("\",15\r");
      delay100(5);
      net_msg_start();
      sprintf(net_scratchpad, (rom far char*)NET_MSG_OK,net_msg_cmd_code);
      net_msg_encode_puts();
      delay100(2);
      break;
    case 49: // Send raw AT command (param: raw AT command)
      net_puts_ram(net_msg_cmd_msg);
      net_puts_rom("\r");
      delay100(5);
      net_msg_start();
      sprintf(net_scratchpad, (rom far char*)NET_MSG_OK,net_msg_cmd_code);
      net_msg_encode_puts();
      delay100(2);
      break;
    default:
      sprintf(net_scratchpad, (rom far char*)"MP-0 c%d,3",net_msg_cmd_code);
      net_msg_encode_puts();
      break;
    }
  net_msg_send();
  net_msg_cmd_code = 0;
  net_msg_cmd_msg[0] = 0;
  }

void net_msg_forward_sms(char *caller, char *SMS)
  {
  //Server not ready, stop sending
  //TODO: store this message inside buffer, resend it when server is connected
  if ((net_msg_serverok == 0)||(net_msg_sendpending)>0)
    return;

  delay100(2);
  net_msg_start();
  strcpypgm2ram(net_scratchpad,(char const rom far*)"MP-0 PA");
  strcatpgm2ram(net_scratchpad,(char const rom far*)"SMS FROM: ");
  strcat(net_scratchpad, caller);
  strcatpgm2ram(net_scratchpad,(char const rom far*)" - MSG: ");
  SMS[170]=0; // Hacky limit on the max size of an SMS forwarded
  strcat(net_scratchpad, SMS);
  net_msg_encode_puts();
  net_msg_send();
  }

void net_msg_alert(void)
  {
  char *p;

  delay100(2);
  net_msg_start();
  strcpypgm2ram(net_scratchpad,(char const rom far*)"MP-0 PA");

  switch (car_chargemode)
    {
    case 0x00:
      strcatpgm2ram(net_scratchpad,(char const rom far *)"Standard - "); // Charge Mode Standard
      break;
    case 0x01:
      strcatpgm2ram(net_scratchpad,(char const rom far *)"Storage - "); // Storage
      break;
    case 0x03:
      strcatpgm2ram(net_scratchpad,(char const rom far *)"Range - "); // Range
      break;
    case 0x04:
      strcatpgm2ram(net_scratchpad,(char const rom far *)"Performance - "); // Performance
    }
  switch (car_chargestate)
    {
    case 0x01:
      strcatpgm2ram(net_scratchpad,(char const rom far *)"Charging"); // Charge State Charging
      break;
    case 0x02:
      strcatpgm2ram(net_scratchpad,(char const rom far *)"Charging, Topping off"); // Topping off
      break;
    case 0x04:
      strcatpgm2ram(net_scratchpad,(char const rom far *)"Charging Done"); // Done
      break;
    case 0x0d:
      strcatpgm2ram(net_scratchpad,(char const rom far *)"Preparing"); // Preparing
      break;
    case 0x0f:
      strcatpgm2ram(net_scratchpad,(char const rom far *)"Charging, Heating"); // Heating
      break;
    default:
      strcatpgm2ram(net_scratchpad,(char const rom far *)"Charging Stopped"); // Stopped
    }

  strcatpgm2ram(net_scratchpad,(char const rom far *)"\rIdeal Range: "); // Ideal Range
  p = par_get(PARAM_MILESKM);
  if (*p == 'M') // Kmh or Miles
    sprintf(net_msg_scratchpad, (rom far char*)"%u mi", car_idealrange); // Miles
  else
    sprintf(net_msg_scratchpad, (rom far char*)"%u Km", (((car_idealrange << 4)+5)/10)); // Kmh
  strcat((char*)net_scratchpad,net_msg_scratchpad);

  strcatpgm2ram(net_scratchpad,(char const rom far *)" SOC: ");
  sprintf(net_msg_scratchpad, (rom far char*)"%u%%", car_SOC); // 95%
  strcat(net_scratchpad,net_msg_scratchpad);
  net_msg_encode_puts();
  net_msg_send();
  }

void net_msg_alarm(void)
  {
  char *p;

  delay100(2);
  net_msg_start();
  strcpypgm2ram(net_scratchpad,(char const rom far*)"MP-0 PAVehicle alarm is sounding!");
  net_msg_encode_puts();
  net_msg_send();
  }

void net_msg_valettrunk(void)
  {
  char *p;

  delay100(2);
  net_msg_start();
  strcpypgm2ram(net_scratchpad,(char const rom far*)"MP-0 PATrunk has been opened (valet mode).");
  net_msg_encode_puts();
  net_msg_send();
  }

void net_msg_socalert(void)
  {
  char *p;

  sprintf(net_scratchpad, (rom far char*)"MP-0 PAALERT!!! CRITICAL SOC LEVEL APPROACHED (%u%% SOC)", car_SOC); // 95%
  net_msg_encode_puts();
  }
