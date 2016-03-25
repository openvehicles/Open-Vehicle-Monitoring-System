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
#include "vehicle.h"
#include "net_msg.h"
#include "net_sms.h"
#include "crypt_base64.h"
#include "crypt_md5.h"
#include "crypt_hmac.h"
#include "crypt_rc4.h"
#include "utils.h"
#ifdef OVMS_LOGGINGMODULE
#include "logging.h"
#endif // #ifdef OVMS_LOGGINGMODULE
#ifdef OVMS_ACCMODULE
#include "acc.h"
#endif

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
WORD crc_capabilities = 0;

#pragma udata NETMSG_SP
char net_msg_scratchpad[NET_BUF_MAX];
#pragma udata

#pragma udata Q_CMD
int  net_msg_cmd_code = 0;
char* net_msg_cmd_msg = NULL;

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

rom char NET_MSG_CMDRESP[] = "MP-0 c";
rom char NET_MSG_CMDOK[] = ",0";
rom char NET_MSG_CMDINVALIDSYNTAX[] = ",1,Invalid syntax";
rom char NET_MSG_CMDNOCANWRITE[] = ",1,No write access to CAN";
rom char NET_MSG_CMDINVALIDRANGE[] = ",1,Parameter out of range";
#ifndef OVMS_NO_CHARGECONTROL
rom char NET_MSG_CMDNOCANCHARGE[] = ",1,Cannot charge (charge port closed)";
rom char NET_MSG_CMDNOCANSTOPCHARGE[] = ",1,Cannot stop charge (charge not in progress)";
#endif // OVMS_NO_CHARGECONTROL
rom char NET_MSG_CMDUNIMPLEMENTED[] = ",3";


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
  if (net_state == NET_STATE_DIAGMODE)
    {
    net_puts_rom("# ");
    }
  else
    {
    net_msg_sendpending = 1;
    delay100(5);
    net_puts_rom("AT+CIPSEND\r");
    delay100(10);
    }
  }

// Finish sending a net msg
void net_msg_send(void)
  {
  if (net_state == NET_STATE_DIAGMODE)
    {
    net_puts_rom("\r\n");
    }
  else
    {
    net_puts_rom("\x1a");
    }
  }

// Encode the message in net_scratchpad and start the send process
void net_msg_encode_puts(void)
  {
  int k;
  char code;

  if (net_state == NET_STATE_DIAGMODE)
    {
    net_puts_ram(net_scratchpad);
    }
  else
    {
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

      stp_rom(net_scratchpad,(char const rom far*)"MP-0 EM");
      net_scratchpad[7] = code;
      base64encode(net_msg_scratchpad,k,net_scratchpad+8);
      // The messdage is now in paranoid mode...
      }

    k=strlen(net_scratchpad);
    RC4_crypt(&tx_crypto1, &tx_crypto2, net_scratchpad, k);
    base64encodesend(net_scratchpad,k);
    }

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
  char *p, *s;

  p = par_get(PARAM_MILESKM);

  s = stp_i(net_scratchpad, "MP-0 S", car_SOC);
  s = stp_s(s, ",", p);
  s = stp_i(s, ",", car_linevoltage);
  s = stp_i(s, ",", car_chargecurrent);

  switch (car_chargestate)
  {
  case 0x01:
    s = stp_rom(s, ",charging");
    break;
  case 0x02:
    s = stp_rom(s, ",topoff");
    break;
  case 0x04:
    s = stp_rom(s, ",done");
    break;
  case 0x0d:
    s = stp_rom(s, ",prepare");
    break;
  case 0x0f:
    s = stp_rom(s, ",heating");
    break;
  default:
    s = stp_rom(s, ",stopped");
  }

  switch (car_chargemode)
  {
  case 0x00:
    s = stp_rom(s, ",standard");
    break;
  case 0x01:
    s = stp_rom(s, ",storage");
    break;
  case 0x03:
    s = stp_rom(s, ",range");
    break;
  case 0x04:
    s = stp_rom(s, ",performance");
    break;
  default:
    s = stp_rom(s, ",");
  }

  if (*p == 'M') // Kmh or Miles
  {
    s = stp_i(s, ",", car_idealrange);
    s = stp_i(s, ",", car_estrange);
  }
  else
  {
    s = stp_i(s, ",", KmFromMi(car_idealrange));
    s = stp_i(s, ",", KmFromMi(car_estrange));
  }

  s = stp_i(s, ",", car_chargelimit);
  s = stp_i(s, ",", car_chargeduration);
  s = stp_i(s, ",", car_charge_b4);
  s = stp_i(s, ",", car_chargekwh);
  s = stp_i(s, ",", car_chargesubstate);
  s = stp_i(s, ",", car_chargestate);
  s = stp_i(s, ",", car_chargemode);
  s = stp_i(s, ",", car_timermode);
  s = stp_i(s, ",", car_timerstart);
  s = stp_i(s, ",", car_stale_timer);
  s = stp_l2f(s, ",", (unsigned long)car_cac100, 2);
  s = stp_i(s, ",", car_chargefull_minsremaining);
  s = stp_i(s, ",",
            ((car_chargelimit_minsremaining_range >= 0)
          && (car_chargelimit_minsremaining_range < car_chargelimit_minsremaining_soc))
          ? car_chargelimit_minsremaining_range
          : car_chargelimit_minsremaining_soc); // ETR for first limit reached
  s = stp_i(s, ",", (*p == 'M')
          ? car_chargelimit_rangelimit
          : KmFromMi(car_chargelimit_rangelimit));
  s = stp_i(s, ",", car_chargelimit_soclimit);
  s = stp_i(s, ",", car_coolingdown);
  s = stp_i(s, ",", car_cooldown_tbattery);
  s = stp_i(s, ",", car_cooldown_timelimit);
  s = stp_i(s, ",", car_chargeestimate);
  s = stp_i(s, ",", car_chargelimit_minsremaining_range);
  s = stp_i(s, ",", car_chargelimit_minsremaining_soc);
  s = stp_i(s, ",", (*p == 'M')
          ? car_max_idealrange
          : KmFromMi(car_max_idealrange));

  return net_msg_encode_statputs(stat, &crc_stat);
}

char net_msgp_gps(char stat)
{
  char *s;

  s = stp_latlon(net_scratchpad, "MP-0 L", car_latitude);
  s = stp_latlon(s, ",", car_longitude);
  s = stp_i(s, ",", car_direction);
  s = stp_i(s, ",", car_altitude);
  s = stp_i(s, ",", car_gpslock);
  s = stp_i(s, ",", car_stale_gps);

  return net_msg_encode_statputs(stat, &crc_gps);
}

char net_msgp_tpms(char stat)
{
  char k, *s;
  long p;

#if 0
  if ((car_tpms_t[0] == 0) && (car_tpms_t[1] == 0) &&
          (car_tpms_t[2] == 0) && (car_tpms_t[3] == 0))
    return stat; // No TPMS, no report
  // ...new stat fn: No TMPS = one report with stale=-1
#endif

  s = stp_rom(net_scratchpad, "MP-0 W");
  for (k = 0; k < 4; k++)
  {
    if (car_tpms_t[k] > 0)
    {
      p = (long) ((float) car_tpms_p[k] / 0.2755);
      s = stp_l2f(s, NULL, p, 1);
      s = stp_i(s, ",", car_tpms_t[k] - 40);
      s = stp_rom(s, ",");
    }
    else
    {
      s = stp_rom(s, "0,0,");
    }
  }
  s = stp_i(s, NULL, car_stale_tpms);

  return net_msg_encode_statputs(stat, &crc_tpms);
}

char net_msgp_firmware(char stat)
{
  // Send firmware version and GSM signal level
  char *s;
  unsigned char hwv = 1;
#ifdef OVMS_HW_V2
  hwv = 2;
#endif

  s = stp_i(net_scratchpad, "MP-0 F", ovms_firmware[0]);
  s = stp_i(s, ".", ovms_firmware[1]);
  s = stp_i(s, ".", ovms_firmware[2]);
  s = stp_s(s, "/", par_get(PARAM_VEHICLETYPE));
  if (vehicle_version)
    s = stp_rom(s, vehicle_version);
  s = stp_i(s, "/V", hwv);
  s = stp_rs(s, "/", OVMS_BUILDCONFIG);
  s = stp_s(s, ",", car_vin);
  s = stp_i(s, ",", net_sq);
  s = stp_i(s, ",", sys_features[FEATURE_CANWRITE]);
  s = stp_s(s, ",", car_type);
  s = stp_s(s, ",", car_gsmcops);

  return net_msg_encode_statputs(stat, &crc_firmware);
}

char net_msgp_environment(char stat)
{
  char *s;
  unsigned long park;

  if (car_parktime == 0)
    park = 0;
  else
    park = car_time - car_parktime;

  s = stp_i(net_scratchpad, "MP-0 D", car_doors1);
  s = stp_i(s, ",", car_doors2);
  s = stp_i(s, ",", car_lockstate);
  s = stp_i(s, ",", car_tpem);
  s = stp_i(s, ",", car_tmotor);
  s = stp_i(s, ",", car_tbattery);
  s = stp_i(s, ",", (can_mileskm=='M') ? car_trip : KmFromMi(car_trip));
  s = stp_ul(s, ",", (can_mileskm=='M') ? car_odometer : KmFromMi(car_odometer));
  s = stp_i(s, ",", car_speed); // no conversion, stored in user unit
  s = stp_ul(s, ",", park);
  s = stp_i(s, ",", car_ambient_temp);
  s = stp_i(s, ",", car_doors3);
  s = stp_i(s, ",", car_stale_temps);
  s = stp_i(s, ",", car_stale_ambient);
  s = stp_l2f(s, ",", car_12vline, 1);
  s = stp_i(s, ",", car_doors4);
  s = stp_l2f(s, ",", car_12vline_ref, 1);
  s = stp_i(s, ",", car_doors5);
  s = stp_i(s, ",", car_tcharger);

  return net_msg_encode_statputs(stat, &crc_environment);
}

char net_msgp_capabilities(char stat)
{
  char *s;

  s = stp_rom(net_scratchpad, "MP-0 V");
  if ((can_capabilities != NULL) && (can_capabilities[0] != 0))
  {
    s = stp_rom(s, can_capabilities);
    s = stp_rom(s, ",");
  }
  s = stp_rom(s, "C1-6,C40-41,C49");

  return net_msg_encode_statputs(stat, &crc_capabilities);
}

char net_msgp_group(char stat, char groupnumber, char *groupname)
{
  char *s;

  s = stp_s(net_scratchpad, "MP-0 g", groupname);
  s = stp_i(s, ",", car_SOC);
  s = stp_i(s, ",", car_speed);
  s = stp_i(s, ",", car_direction);
  s = stp_i(s, ",", car_altitude);
  s = stp_i(s, ",", car_gpslock);
  s = stp_i(s, ",", car_stale_gps);
  s = stp_latlon(s, ",", car_latitude);
  s = stp_latlon(s, ",", car_longitude);

  if (groupnumber == 1)
    return net_msg_encode_statputs(stat, &crc_group1);
  else
    return net_msg_encode_statputs(stat, &crc_group2);
}

void net_msg_server_welcome(char *msg)
  {
  // The server has sent a welcome (token <space> base64digest)
  char *d,*p,*s;
  int k;
  unsigned char hwv = 1;

  #ifdef OVMS_HW_V2
  hwv = 2;
  #endif

  if( !msg ) return;
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
    stp_rom(net_scratchpad,(char const rom far*)"MP-0 ET");
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


#ifndef OVMS_NO_CRASHDEBUG
  /* DEBUG / QA stats: Send crash counter and last reason:
   *
   * MP-0 H*-OVM-DebugCrash,0,2592000
   *  ,<firmware_version>/<vehicle_type><vehicle_version>/V<hardware_version>
   *  ,<crashcnt>,<crashreason>,<checkpoint>
   */

  if (debug_crashreason & 0x80)
    {
    debug_crashreason &= ~0x80; // clear checkpoint hold bit

    s = stp_i(net_scratchpad, "MP-0 H*-OVM-DebugCrash,0,2592000,", ovms_firmware[0]);
    s = stp_i(s, ".", ovms_firmware[1]);
    s = stp_i(s, ".", ovms_firmware[2]);
    s = stp_s(s, "/", par_get(PARAM_VEHICLETYPE));
    if (vehicle_version)
       s = stp_rom(s, vehicle_version);
    s = stp_i(s, "/V", hwv);
    s = stp_i(s, ",", debug_crashcnt);
    s = stp_x(s, ",", debug_crashreason);
    s = stp_i(s, ",", debug_checkpoint);

    delay100(20);
    net_msg_start();
    net_msg_encode_puts();
    net_msg_send();
    }
#endif // OVMS_NO_CRASHDEBUG


#ifdef OVMS_LOGGINGMODULE
  logging_serverconnect();
#endif // #ifdef OVMS_LOGGINGMODULE
}

// Receive a NET msg from the OVMS server
void net_msg_in(char* msg)
  {
  int k;

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
  k = base64decode(msg,net_scratchpad);
  CHECKPOINT(0x41)
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
    msg += 2; // Now pointing to the code just before encrypted paranoid message
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

  CHECKPOINT(0x42)
  switch (*msg)
    {
    case 'A': // PING
      stp_rom(net_scratchpad,(char const rom far*)"MP-0 a");
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
    case 'h': // Historical data acknowledgement
#ifdef OVMS_LOGGINGMODULE
      logging_ack(atoi(msg+1));
#endif // #ifdef OVMS_LOGGINGMODULE
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

  for (d=msg;(*d != 0)&&(*d != ',');d++) ;
  if (*d == ',')
    *d++ = 0;
  // At this point, <msg> points to the command, and <d> to the first param (if any)
  net_msg_cmd_code = atoi(msg);
  net_msg_cmd_msg = d;
  }

BOOL net_msg_cmd_exec(void)
  {
  UINT8 k;
  char *p, *s;

  delay100(2);

  CHECKPOINT(0x43)

  switch (net_msg_cmd_code)
    {
    case 1: // Request feature list (params unused)
      for (k=0;k<FEATURES_MAX;k++)
        {
          s = stp_i(net_scratchpad, "MP-0 c1,0,", k);
          s = stp_i(s, ",", FEATURES_MAX);
          s = stp_i(s, ",", sys_features[k]);
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
          if (k == FEATURE_CANWRITE) vehicle_initialise();
          STP_OK(net_scratchpad, net_msg_cmd_code);
          }
        else
          {
          STP_INVALIDRANGE(net_scratchpad, net_msg_cmd_code);
          }
        }
      else
        {
        STP_INVALIDSYNTAX(net_scratchpad, net_msg_cmd_code);
        }
      net_msg_encode_puts();
      break;

    case 3: // Request parameter list (params unused)
      for (k=0;k<PARAM_MAX;k++)
        {
          p = par_get(k);
          if (k==PARAM_SERVERPASS) *p=0; // Don't show netpass1
          s = stp_i(net_scratchpad, "MP-0 c3,0,", k);
          s = stp_i(s, ",", PARAM_MAX);
          s = stp_s(s, ",", p);
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
        // Check validity of param key and value (no empty value allowed for auth params):
        if ( (k>=0) && (k<PARAM_FEATURE_S)
                && ((k>PARAM_MODULEPASS) || (*p!=0)) )
          {
          par_set(k, p);
          STP_OK(net_scratchpad, net_msg_cmd_code);
          if ((k==PARAM_MILESKM) || (k==PARAM_VEHICLETYPE)) vehicle_initialise();
#ifdef OVMS_ACCMODULE
          // Reset the ACC state it an ACC parameter is changed
          if ((k>=PARAM_ACC_S)&&(k<(PARAM_ACC_S+PARAM_ACC_COUNT)))
            acc_state_enter(ACC_STATE_FIRSTRUN);
#endif
          }
        else
          {
          STP_INVALIDRANGE(net_scratchpad, net_msg_cmd_code);
          }
        }
      else
        {
        STP_INVALIDSYNTAX(net_scratchpad, net_msg_cmd_code);
        }
      net_msg_encode_puts();
      break;

    case 5: // Reboot (params unused)
      STP_OK(net_scratchpad, net_msg_cmd_code);
      net_msg_encode_puts();
      net_state_enter(NET_STATE_HARDSTOP);
      break;

    case 6: // CHARGE ALERT (params unused)
      net_msg_alert();
      net_msg_encode_puts();
      break;

    case 7: // SMS command wrapper

      // copy command to net_msg_scratchpad:
      //    !!! ATT: net_msg_scratchpad MUST NOT be used
      //    !!! in command handler prior to parameter parsing!
      stp_ram(net_msg_scratchpad, net_msg_cmd_msg);
      
      // redirect command output into net_buf[]:
      net_msg_bufpos = net_buf;
      
      // process command:
      net_assert_caller(NULL); // set net_caller to PARAM_REGPHONE
      k = net_sms_in(net_caller, net_msg_scratchpad);
      
      // terminate output redirection:
      *net_msg_bufpos = 0;
      net_msg_bufpos = NULL;
      
      // create return string:
      s = stp_i(net_scratchpad, NET_MSG_CMDRESP, net_msg_cmd_code);
      s = stp_i(s, ",", 1-k); // 0=ok 1=error
      if (k)
        {
        *s++ = ',';
        for (p = net_buf; *p; p++)
          {
            if (*p == '\n')
              *s++ = '\r'; // translate LF to CR
            else if (*p == ',')
              *s++ = ';'; // translate , to ;
            else
              *s++ = *p;
          }
        *s = 0;
        }

      // send return string:
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
        STP_OK(net_scratchpad, net_msg_cmd_code);
        }
      else
        {
        net_msg_start();
        STP_INVALIDSYNTAX(net_scratchpad, net_msg_cmd_code);
        }
      net_msg_encode_puts();
      delay100(2);
      break;

    case 41: // Send MMI/USSD Codes (param: USSD_CODE)
      net_puts_rom("AT+CUSD=1,\"");
      net_puts_ram(net_msg_cmd_msg);
      net_puts_rom("\",15\r");
      // cmd reply #1 to acknowledge command:
      delay100(5);
      net_msg_start();
      STP_OK(net_scratchpad, net_msg_cmd_code);
      net_msg_encode_puts();
      delay100(2);
      // cmd reply #2 sent on USSD response, see net_msg_reply_ussd()
      break;
      
    case 49: // Send raw AT command (param: raw AT command)
      net_puts_ram(net_msg_cmd_msg);
      net_puts_rom("\r");
      delay100(5);
      net_msg_start();
      STP_OK(net_scratchpad, net_msg_cmd_code);
      net_msg_encode_puts();
      delay100(2);
      break;

  default:
      return FALSE;
    }

  return TRUE;
  }

void net_msg_cmd_do(void)
  {
  CHECKPOINT(0x44)
  delay100(2);

  // commands 40-49 are special AT commands, thus, disable net_msg here
  if ((net_msg_cmd_code < 40) || (net_msg_cmd_code > 49))
    net_msg_start();

#ifdef OVMS_ACCMODULE
   if (! acc_handle_msg(TRUE, net_msg_cmd_code, net_msg_cmd_msg))
   {
#endif

   // Execute cmd: ask car module to execute first:
   if ((vehicle_fn_commandhandler == NULL)||
       (! vehicle_fn_commandhandler(TRUE, net_msg_cmd_code,net_msg_cmd_msg)))
     {

     // Car module does not feel responsible, fall back to standard:
     if( !net_msg_cmd_exec() )
       {
       // No standard as well => return "unimplemented"
       STP_UNIMPLEMENTED(net_scratchpad, net_msg_cmd_code);
       net_msg_encode_puts();
       }
     }

#ifdef OVMS_ACCMODULE
   }
#endif

   // terminate IPSEND by Ctrl-Z (should this be disabled for commands 40-49 as well?)
   net_msg_send();

   // clear command
   net_msg_cmd_code = 0;
   net_msg_cmd_msg[0] = 0;
  }

void net_msg_forward_sms(char *caller, char *SMS)
  {
  //Server not ready, stop sending
  //TODO: store this message inside buffer, resend it when server is connected
  if ((net_msg_serverok == 0)||(net_msg_sendpending)>0)
    return;

  CHECKPOINT(0x45)

  delay100(2);
  net_msg_start();
  stp_rom(net_scratchpad,(char const rom far*)"MP-0 PA");
  strcatpgm2ram(net_scratchpad,(char const rom far*)"SMS FROM: ");
  strcat(net_scratchpad, caller); // max 19 chars
  strcatpgm2ram(net_scratchpad,(char const rom far*)" - MSG: ");
  // scratchpad max 199 chars, rom strings 25 + caller 19 = 44 chars
  // => max payload = 155 chars
  SMS[155]=0; // Hacky limit on the max size of an SMS forwarded
  strcat(net_scratchpad, SMS);
  net_msg_encode_puts();
  net_msg_send();
}

void net_msg_reply_ussd(char *buf, unsigned char buflen)
{
  // called from net_state_activity()
  // buf contains a "+CUSD:" USSD command result
  // parse and return as command reply:
  char *s, *t = NULL;

  // Server not ready? abort
  // TODO: store, resend when server is connected
  if ((!net_msg_serverok) || (!buf) || (!buflen))
    return;

  // isolate USSD reply text
  if (t = memchr((void *) buf, '"', buflen))
  {
    ++t;
    buflen -= (t - buf);
    buf = t; // start of USSD string
    while ((*t) && (*t != '"') && ((t - buf) < buflen))
    {
      if (*t == ',') // "escape" comma for MP-0
        *t = '.';
      t++;
    }
    *t = 0; // end of USSD string
  }

  // format reply:
  s = stp_i(net_scratchpad, "MP-0 c", CMD_SendUSSD);
  if (t)
    s = stp_s(s, ",0,", buf);
  else
    s = stp_rom(s, ",1,Invalid USSD result");

  // send reply:

  if (net_msg_sendpending > 0)
  {
    delay100(20); // HACK... should buffer & retry later... but RAM is precious
    s = NULL; // flag
  }

  net_msg_start();
  net_msg_encode_puts();
  net_msg_send();

  if (!s)
    delay100(20); // HACK: give modem additional time if there was sendpending>0
}

char *net_prep_stat(char *s)
{
  // convert distance values as needed
  unsigned int estrange = car_estrange;
  unsigned int idealrange = car_idealrange;
  unsigned long odometer = car_odometer;
  const rom char *unit = " mi";
  if (can_mileskm == 'K')
  {
    estrange = KmFromMi(estrange);
    idealrange = KmFromMi(idealrange);
    odometer = KmFromMi(odometer);
    unit = " km";
  }

  if (car_coolingdown>=0)
    {
    s = stp_i(s, "Cooldown: ", car_tbattery);
    s = stp_i(s, "C/",car_cooldown_tbattery);
    s = stp_i(s, "C (",car_coolingdown);
    s = stp_i(s, "cycles, ",car_cooldown_timelimit);
    s = stp_rom(s, "mins remain)");
    }

  if (car_doors1bits.ChargePort)
  {
    char fShowVA = TRUE;
    // Charge port door is open, we are charging
    switch (car_chargemode)
    {
    case 0x00:
      s = stp_rom(s, "Standard - "); // Charge Mode Standard
      break;
    case 0x01:
      s = stp_rom(s, "Storage - "); // Storage
      break;
    case 0x03:
      s = stp_rom(s, "Range - "); // Range
      break;
    case 0x04:
      s = stp_rom(s, "Performance - "); // Performance
    }
    switch (car_chargestate)
    {
    case 0x01:
      s = stp_rom(s, "Charging"); // Charge State Charging
      break;
    case 0x02:
      s = stp_rom(s, "Charging, Topping off"); // Topping off
      break;
    case 0x04:
      s = stp_rom(s, "Charging Done"); // Done
      fShowVA = FALSE;
      break;
    case 0x0d:
      s = stp_rom(s, "Preparing"); // Preparing
      break;
    case 0x0f:
      s = stp_rom(s, "Charging, Heating"); // Heating
      break;
    default:
      s = stp_rom(s, "Charging Stopped"); // Stopped
      fShowVA = FALSE;
      break;
    }
//    car_doors1bits.ChargePort = 0; // MJ Close ChargePort, will open next CAN Reading
    if (fShowVA)
    {
      s = stp_i(s, "\r ", car_linevoltage);
      s = stp_i(s, "V/", car_chargecurrent);
      s = stp_rom(s, "A");
      if (car_chargefull_minsremaining >= 0)
        {
        s = stp_i(s,"\r Full: ",car_chargefull_minsremaining);
        s = stp_rom(s," mins");
        }
      if (car_chargelimit_soclimit > 0)
        {
        s = stp_i(s, "\r ", car_chargelimit_soclimit);
        s = stp_i(s,"%: ",car_chargelimit_minsremaining_soc);
        s = stp_rom(s," mins");
        }
      if (car_chargelimit_rangelimit > 0)
        {
        s = stp_i(s, "\r ", (can_mileskm == 'K')?KmFromMi(car_chargelimit_rangelimit):car_chargelimit_rangelimit);
        s = stp_rom(s, unit);
        s = stp_i(s,": ",car_chargelimit_minsremaining_range);
        s = stp_rom(s," mins");
        }
    }
  }
  else
  {
    s = stp_rom(s, "Not charging");
  }

  s = stp_i(s, "\r SOC: ", car_SOC);
  s = stp_rom(s, "%");

  if (idealrange != 0)
    {
    s = stp_i(s, "\r Ideal Range: ", idealrange);
    s = stp_rom(s, unit);
    }
  if (estrange != 0)
    {
    s = stp_i(s, "\r Est. Range: ", estrange);
    s = stp_rom(s, unit);
    }
  if (odometer != 0)
    {
    s = stp_l2f_h(s, "\r ODO: ", odometer, 1);
    s = stp_rom(s, unit);
    }
  if (car_cac100 != 0)
    {
    s = stp_l2f_h(s, "\r CAC: ", (unsigned long)car_cac100, 2);
    }

  return s;
}

#ifndef OVMS_NO_CTP
char *net_prep_ctp(char *s, char *argument)
{
  int imStart = car_idealrange;
  int imTarget = 0;
  int pctTarget = 0;
  int cac100 = car_cac100;
  int volts = car_linevoltage;
  int amps = car_chargelimit;
  int degAmbient = car_ambient_temp;
  int chargemode = car_chargemode;
  int watts = 0;
  int imExpect;
  int minRemain;
  
  if (vehicle_fn_minutestocharge == NULL)
  {
    return stp_rom(s, "CTP not available");
  }
  
  // CTP 90s 150e 70% 16800w 160a 24d
  while (argument != NULL)
  {
    int cch = strlen(argument);
    strupr(argument); // Convert argument to upper case
    if (cch > 0)
      {
      int val;
      char chType = argument[cch-1];
      argument[cch-1] = 0;
      if (cch > 1)
        {
        val = atoi(argument);
        switch (chType)
          {
          case 'S':
            imStart = val;
            break;
          case 'E':
            imTarget = val;
           break;
          case '%':
            pctTarget = val;
            break;
          case 'W':
            watts = val;
            break;
          case 'V':
            volts = val;
            break;
          case 'A':
            amps = val;
            break;
          case 'C':
            cac100 = val*100;
            break;
          case 'D':
            degAmbient = val;
            break;
          }
        }
      else
        {
        switch (chType)
          {
          case 'S':
            chargemode = 0;
            break;
          case 'R':
            chargemode = 3;
            break;
          case 'P':
            chargemode = 4;
            break;
          }
        }
      argument[cch-1] = chType;
      }
    argument = net_sms_nextarg(argument);
  }
  
  if (volts > 0 && amps > 0)
    watts = volts * amps;

  if (watts < 1000)
    {
    s = stp_rom(s, "no power level specified");
    return s;
    }
  
  minRemain = vehicle_fn_minutestocharge(chargemode, watts, imStart, imTarget, pctTarget, cac100, degAmbient, &imExpect);

  s = stp_i(s, NULL, imStart);
  s = stp_i(s, " to ", imExpect);
  s = stp_rom(s, " ideal mi\r");
  if (minRemain >= 0)
    {
    s = stp_i(s, NULL, minRemain/60);
    s = stp_ulp(s, ":", minRemain % 60, 2, '0');
    }
  else if (minRemain == -3)
    {
    s = stp_rom(s, "target reached");
    }
  else
    {
    s = stp_i(s, "error: ", minRemain);
    }
  return s;
}
#endif //OVMS_NO_CTP

void net_msg_alert(void)
{
  char *s;

  delay100(2);

  s = stp_rom(net_scratchpad, "MP-0 PA");
  net_prep_stat(s);
}

#ifndef OVMS_NO_VEHICLE_ALERTS
void net_msg_alarm(void)
  {
  char *p;

  delay100(2);
  net_msg_start();
  stp_rom(net_scratchpad,(char const rom far*)"MP-0 PAVehicle alarm is sounding!");
  net_msg_encode_puts();
  net_msg_send();
  }

void net_msg_valettrunk(void)
  {
  char *p;

  delay100(2);
  net_msg_start();
  stp_rom(net_scratchpad,(char const rom far*)"MP-0 PATrunk has been opened (valet mode).");
  net_msg_encode_puts();
  net_msg_send();
  }
#endif //OVMS_NO_VEHICLE_ALERTS

void net_msg_socalert(void)
  {
  char *s;

  delay100(2);
  net_msg_start();
  s = stp_i(net_scratchpad, "MP-0 PAALERT!!! CRITICAL SOC LEVEL APPROACHED (", car_SOC); // 95%
  s = stp_rom(s, "% SOC)");
  net_msg_encode_puts();
  net_msg_send();
  }

void net_msg_12v_alert(void)
  {
  char *s;

  delay100(2);
  net_msg_start();
  if (can_minSOCnotified & CAN_MINSOC_ALERT_12V)
    s = stp_l2f(net_scratchpad, "MP-0 PAALERT!!! 12V BATTERY CRITICAL (", car_12vline, 1);
  else
    s = stp_l2f(net_scratchpad, "MP-0 PA12V BATTERY OK (", car_12vline, 1);
  s = stp_l2f(s, "V, ref=", car_12vline_ref, 1);
  s = stp_rom(s, "V)");
  net_msg_encode_puts();
  net_msg_send();
  }

void net_msg_erroralert(unsigned int errorcode, unsigned long errordata)
  {
  char *s;

  delay100(2);
  net_msg_start();
  s = stp_s(net_scratchpad, "MP-0 PE", car_type);
  s = stp_ul(s, ",", (unsigned long)errorcode);
  s = stp_ul(s, ",", (unsigned long)errordata);
  net_msg_encode_puts();
  net_msg_send();
  }
