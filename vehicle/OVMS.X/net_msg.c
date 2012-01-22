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
#include "crypt_base64.h"
#include "crypt_md5.h"
#include "crypt_hmac.h"
#include "crypt_rc4.h"

// NET_MSG data
#define TOKEN_SIZE 22
#pragma udata
char net_msg_notify = 0;
char net_msg_notifyenvironment = 0;
char net_msg_serverok = 0;
char net_msg_sendpending = 0;
char token[23] = {0};
char ptoken[23] = {0};
char ptokenmade = 0;
char digest[MD5_SIZE];
char pdigest[MD5_SIZE];
char net_msg_scratchpad[100];

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

  p = par_get(PARAM_NETPASS1);
  hmac_md5(token, TOKEN_SIZE, p, strlen(p), digest);

  net_puts_rom("MP-C 0 ");
  net_puts_ram(token);
  net_puts_rom(" ");
  base64encodesend(digest, MD5_SIZE);
  net_puts_rom(" ");
  p = par_get(PARAM_MYID);
  net_puts_ram(p);
  net_puts_rom("\r\n");
  }

void net_msg_stat(void)
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
    default:
      strcatpgm2ram(net_scratchpad,(char const rom far*)",");
    }
  if (*p == 'M') // Kmh or Miles
    sprintf(net_msg_scratchpad, (rom far char*)"%u,", car_idealrange);
  else
    sprintf(net_msg_scratchpad, (rom far char*)"%u,", (((car_idealrange << 4)+5)/10));
  strcat(net_scratchpad,net_msg_scratchpad);
  if (*p == 'M') // Kmh or Miles
    sprintf(net_msg_scratchpad, (rom far char*)"%u", car_estrange);
  else
    sprintf(net_msg_scratchpad, (rom far char*)"%u", (((car_estrange << 4)+5)/10));
  strcat(net_scratchpad,net_msg_scratchpad);
  net_msg_encode_puts();
  }

void net_msg_gps(void)
  {
  strcpypgm2ram(net_scratchpad,(char const rom far*)"MP-0 L");
  format_latlon(car_latitude,net_msg_scratchpad);
  strcat(net_scratchpad,net_msg_scratchpad);
  strcatpgm2ram(net_scratchpad,(char const rom far*)",");
  format_latlon(car_longitude,net_msg_scratchpad);
  strcat(net_scratchpad,net_msg_scratchpad);
  net_msg_encode_puts();
  }

void net_msg_tpms(void)
  {
  char k;
  long p;
  int b,a;

  if ((car_tpms_t[0]==0)&&(car_tpms_t[1]==0)&&
      (car_tpms_t[2]==0)&&(car_tpms_t[3]==0))
    return; // No TPMS, no report

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
  net_scratchpad[strlen(net_scratchpad)-1] = 0; // Remove trailing ','
  net_msg_encode_puts();
  }

void net_msg_firmware(void)
  {
  // Send firmware version and GSM signal level
  strcpypgm2ram(net_scratchpad,(char const rom far*)"MP-0 F");
  sprintf(net_msg_scratchpad, (rom far char*)"1.1.9-exp1,%s,%d",
    car_vin, net_sq);
  strcat(net_scratchpad,net_msg_scratchpad);
  net_msg_encode_puts();
  }

void net_msg_environment(void)
  {
  unsigned long park;

  if (car_parktime == 0)
    park = 0;
  else
    park = car_time - car_parktime;

  strcpypgm2ram(net_scratchpad,(char const rom far*)"MP-0 D");
  sprintf(net_msg_scratchpad, (rom far char*)"%d,%d,%d,%d,%d,%d,%d,%lu,%d,%lu",
          car_doors1, car_doors2, car_lockstate,
          car_tpem, car_tmotor, car_tbattery,
          car_trip, car_odometer, car_speed, park);
  strcat(net_scratchpad,net_msg_scratchpad);
  net_msg_encode_puts();
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
  p = par_get(PARAM_NETPASS1);
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
    p = par_get(PARAM_REGPASS);
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

  if (net_msg_serverok == 0)
    {
    if (memcmppgm2ram(msg, (char const rom far*)"MP-S 0 ", 7) == 0)
      {
      net_msg_server_welcome(msg+7);
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
          net_msg_stat();
          net_msg_gps();
          net_msg_tpms();
          net_msg_firmware();
          net_msg_environment();
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
      sprintf(net_scratchpad, (rom far char*)"MP-0 c%d,2",net_msg_cmd_code);
      net_msg_encode_puts();
      break;
    case 3: // Request parameter list (params unused)
      for (k=0;k<PARAM_MAX;k++)
        {
        p = par_get(k);
        if (k==PARAM_NETPASS1) *p=0; // Don't show netpass1
        sprintf(net_scratchpad, (rom far char*)"MP-0 c3,0,%d,%d,%s",
                k,PARAM_MAX,p);
        net_msg_encode_puts();
        }
      break;
    case 4: // Set parameter (params: param number, value)
      sprintf(net_scratchpad, (rom far char*)"MP-0 c%d,2",net_msg_cmd_code);
      net_msg_encode_puts();
      break;
    case 5: // Reboot (params unused)
      sprintf(net_scratchpad, (rom far char*)"MP-0 c%d,2",net_msg_cmd_code);
      net_msg_encode_puts();
      break;
    case 10: // Set charge mode (params: 0=standard, 1=storage,3=range,4=performance)
      sprintf(net_scratchpad, (rom far char*)"MP-0 c%d,2",net_msg_cmd_code);
      net_msg_encode_puts();
      break;
    case 11: // Start charge (params unused)
      sprintf(net_scratchpad, (rom far char*)"MP-0 c%d,2",net_msg_cmd_code);
      net_msg_encode_puts();
      break;
    case 12: // Stop charge (params unused)
      sprintf(net_scratchpad, (rom far char*)"MP-0 c%d,2",net_msg_cmd_code);
      net_msg_encode_puts();
      break;
    case 20: // Lock car (params pin)
      sprintf(net_scratchpad, (rom far char*)"MP-0 c%d,2",net_msg_cmd_code);
      net_msg_encode_puts();
      break;
    case 21: // Activate valet mode (params pin)
      sprintf(net_scratchpad, (rom far char*)"MP-0 c%d,2",net_msg_cmd_code);
      net_msg_encode_puts();
      break;
    case 22: // Unlock car (params pin)
      sprintf(net_scratchpad, (rom far char*)"MP-0 c%d,2",net_msg_cmd_code);
      net_msg_encode_puts();
      break;
    case 23: // Deactivate valet mode (params pin)
      sprintf(net_scratchpad, (rom far char*)"MP-0 c%d,2",net_msg_cmd_code);
      net_msg_encode_puts();
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
  SMS[70]=0; // Hacky limit on the max size of an SMS forwarded
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
