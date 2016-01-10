/*******************************************************************************
 * 
 * OVMS -- Open Vehicles Monitoring System
 *  https://www.openvehicles.com/
 *  https://github.com/openvehicles
 * 
 * cfgconv: Twizy SEVCON configuration profile converter
 * 	(convert from base64 to text commands or key=value tags format and vice versa)
 *
 * Usage:
 *  a) convert base64 to commands: ./cfgconv base64data > profile.txt
 *  b) convert commands to base64: ./cfgconv < profile.txt
 *  c) convert base64 to tags: ./cfgconv -t base64data > profile.inf
 *  d) convert tags to base64: ./cfgconv -t < profile.inf
 *
 * Build:
 *  gcc -o cfgconv cfgconv.c
 *
 * Version:		1.1 (2016/01/07)
 * Author:		Michael Balzer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>


typedef unsigned char BYTE;
typedef unsigned char UINT8;
typedef signed char INT8;
typedef unsigned int UINT;
typedef unsigned int WORD;


/*******************************************************************************
 * base64 encode & decode
 *
 * (copied from vehicle/OVMS.X/crypt_base64.c)
 *
 */

/*********************************************************************
MODULE NAME:    b64.c

ORIGIN:         http://base64.sourceforge.net/b64.c

AUTHOR:         Bob Trower 08/04/01

PROJECT:        Crypt Data Packaging

COPYRIGHT:      Copyright (c) Trantor Standard Systems Inc., 2001

NOTE:           This source code may be used as you wish, subject to
                the MIT license.  See the LICENCE section below.

LICENCE:        Copyright (c) 2001 Bob Trower, Trantor Standard Systems Inc.

                Permission is hereby granted, free of charge, to any person
                obtaining a copy of this software and associated
                documentation files (the "Software"), to deal in the
                Software without restriction, including without limitation
                the rights to use, copy, modify, merge, publish, distribute,
                sublicense, and/or sell copies of the Software, and to
                permit persons to whom the Software is furnished to do so,
                subject to the following conditions:

                The above copyright notice and this permission notice shall
                be included in all copies or substantial portions of the
                Software.

                THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
                KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
                WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
                PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
                OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
                OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
                OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
                SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

VERSION HISTORY:
                Bob Trower 08/04/01 -- Create Version 0.00.00B
/*********************************************************************/

// Translation Table as described in RFC1113
const unsigned char cb64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Translation Table to decode (created by author)
unsigned char cd64[]="|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";

unsigned char in[4], out[4];


void encodeblock( unsigned char in[3], unsigned char out[4], int len )
  {
  out[0] = cb64[ in[0] >> 2 ];
  out[1] = cb64[ ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) ];
  out[2] = (unsigned char) (len > 1 ? cb64[ ((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6) ] : '=');
  out[3] = (unsigned char) (len > 2 ? cb64[ in[2] & 0x3f ] : '=');
  }

char *base64encode(BYTE *inputData, WORD inputLen, BYTE *outputData)
  {
  int len = 0;
  int k;
  for (k=0;k<inputLen;k++)
    {
    in[len++] = inputData[k];
    if (len==3)
      {
      // Block is full
      encodeblock(in, out, 3);
      for (len=0;len<4;len++) *outputData++ = out[len];
      len = 0;
      }
    }
  if (len>0)
    {
    for (k=len;k<3;k++) in[k]=0;
    encodeblock(in, out, len);
    for (len=0;len<4;len++) *outputData++ = out[len];
    }
  *outputData = 0;
  return outputData;
  }


void decodeblock( unsigned char in[4], unsigned char out[3] )
  {
  out[ 0 ] = (unsigned char ) (in[0] << 2 | in[1] >> 4);
  out[ 1 ] = (unsigned char ) (in[1] << 4 | in[2] >> 2);
  out[ 2 ] = (unsigned char ) (((in[2] << 6) & 0xc0) | in[3]);
  }

int base64decode(BYTE *inputData, BYTE *outputData)
  {
  BYTE c = 1;
  unsigned char v;
  int i, len;
  int written = 0;

  while( c != 0 )
    {
    for( len = 0, i = 0; (i < 4) && (c != 0); i++ )
      {
      v = 0;
      while( (c != 0) && (v == 0) )
        {
		c = (*inputData) ? *inputData++ : 0;
        v = (unsigned char) ((c < 43 || c > 122) ? 0 : cd64[ c - 43 ]);
        if( v )
          {
          v = (unsigned char) ((v == '$') ? 0 : v - 61);
          }
        }
      if( c != 0 )
        {
        len++;
        if( v )
          {
          in[ i ] = (unsigned char) (v - 1);
          }
        }
      else
        {
        in[i] = 0;
        }
      }
    if( len )
      {
      decodeblock( in, out );
      for( i = 0; i < len - 1; i++ )
        {
        *outputData++ = out[i];
        written++;
        }
      }
    }
  *outputData = 0;
  return written;
  }

  
  

/*******************************************************************************
 *
 * Twizy SEVCON configuration
 * 
 * (copied from vehicle/OVMS.X/vehicle_twizy.c)
 *
 */

// SEVCON macro configuration profile:
// 1 kept in RAM (working set)
// 3 stored in binary EEPROM param slots PARAM_PROFILE1 /2 /3
struct twizy_cfg_profile {
  UINT8       checksum;
  UINT8       speed, warn;
  UINT8       torque, power_low, power_high;
  UINT8       drive, neutral, brake;
  struct tsmap {
    UINT8       spd1, spd2, spd3, spd4;
    UINT8       prc1, prc2, prc3, prc4;
  }           tsmap[3]; // 0=D 1=N 2=B
  UINT8       ramp_start, ramp_accel, ramp_decel, ramp_neutral, ramp_brake;
  UINT8       smooth;
  UINT8       brakelight_on, brakelight_off;
  // V3.2.1 additions:
  UINT8       ramplimit_accel, ramplimit_decel;
  // V3.4.0 additions:
  UINT8       autorecup_minprc;
  // V3.6.0 additions:
  UINT8       autorecup_ref;
  UINT8       autodrive_minprc;
  UINT8       autodrive_ref;
  // V3.7.0 additions:
  UINT8       current;
} twizy_cfg_profile;


// EEPROM memory usage info:
// sizeof twizy_cfg_profile = 24 + 8x3 = 48 byte
// Maximum size = 2 x PARAM_MAX_LENGTH = 64 byte

// Macros for converting profile values:
// shift values by 1 (-1.. => 0..) to map param range to UINT8
#define cfgparam(NAME)  (((int)(twizy_cfg_profile.NAME))-1)
#define cfgvalue(VAL)   ((UINT8)((VAL)+1))


char *tagname[] = {
	"checksum",
	
	"speed",
	"warn",
	
	"torque",
	"power_low",
	"power_high",
	
	"drive",
	"neutral",
	"brake",
	
	"tsmap.D.spd1",
	"tsmap.D.spd2",
	"tsmap.D.spd3",
	"tsmap.D.spd4",
	"tsmap.D.prc1",
	"tsmap.D.prc2",
	"tsmap.D.prc3",
	"tsmap.D.prc4",
	
	"tsmap.N.spd1",
	"tsmap.N.spd2",
	"tsmap.N.spd3",
	"tsmap.N.spd4",
	"tsmap.N.prc1",
	"tsmap.N.prc2",
	"tsmap.N.prc3",
	"tsmap.N.prc4",
	
	"tsmap.B.spd1",
	"tsmap.B.spd2",
	"tsmap.B.spd3",
	"tsmap.B.spd4",
	"tsmap.B.prc1",
	"tsmap.B.prc2",
	"tsmap.B.prc3",
	"tsmap.B.prc4",
	
	"ramp_start",
	"ramp_accel",
	"ramp_decel",
	"ramp_neutral",
	"ramp_brake",
	"smooth",
	
	"brakelight_on",
	"brakelight_off",
	
	"ramplimit_accel",
	"ramplimit_decel",
	
	"autorecup_minprc",
	"autorecup_ref",
	"autodrive_minprc",
	"autodrive_ref",
	
	"current"
};


// vehicle_twizy_cfg_calc_checksum: get checksum for twizy_cfg_profile
//
// Note: for extendability of struct twizy_cfg_profile, 0-Bytes will
//  not affect the checksum, so new fields can simply be added at the end
//  without losing version compatibility.
//  (0-bytes translate to value -1 = default)
BYTE vehicle_twizy_cfg_calc_checksum(BYTE *profile)
{
  UINT checksum;
  BYTE i;

  checksum = 0x0101; // version tag

  for (i=1; i<sizeof(twizy_cfg_profile); i++)
    checksum += profile[i];

  if ((checksum & 0x0ff) == 0)
    checksum >>= 8;

  return (checksum & 0x0ff);
}



/*******************************************************************************
 *
 * Conversion
 * 
 */

// profile2commands:
// 	convert twizy_cfg_profile to CFG text command syntax
void profile2commands(void) {
	
	int i;
	char map[3] = { 'D', 'N', 'B' };
	
	printf("SPEED %d %d\n",
		cfgparam(speed), cfgparam(warn));
	
	printf("POWER %d %d %d %d\n",
		cfgparam(torque), cfgparam(power_low), cfgparam(power_high), cfgparam(current));
	
	for (i = 0; i < 3; i++) {
		printf("TSMAP %c %d@%d %d@%d %d@%d %d@%d\n", map[i],
			cfgparam(tsmap[i].prc1), cfgparam(tsmap[i].spd1),
			cfgparam(tsmap[i].prc2), cfgparam(tsmap[i].spd2),
			cfgparam(tsmap[i].prc3), cfgparam(tsmap[i].spd3),
			cfgparam(tsmap[i].prc4), cfgparam(tsmap[i].spd4));
	}
	
	printf("BRAKELIGHT %d %d\n",
		cfgparam(brakelight_on), cfgparam(brakelight_off));
	
	printf("DRIVE %d %d %d\n",
		cfgparam(drive), cfgparam(autodrive_ref), cfgparam(autodrive_minprc));
	
	printf("RECUP %d %d %d %d\n",
		cfgparam(neutral), cfgparam(brake), cfgparam(autorecup_ref), cfgparam(autorecup_minprc));
	
	printf("RAMPS %d %d %d %d %d\n",
		cfgparam(ramp_start), cfgparam(ramp_accel), cfgparam(ramp_decel),
		cfgparam(ramp_neutral), cfgparam(ramp_brake));
	
	printf("RAMPL %d %d\n",
		cfgparam(ramplimit_accel), cfgparam(ramplimit_decel));
	
	printf("SMOOTH %d\n",
		cfgparam(smooth));
	
}


// commands2profile:
// 	read CFG text command syntax to twizy_cfg_profile
// 	ATT: no param range checks! (CFG SET / LOAD will do this)
void commands2profile(void) {
	
	char cmd[100];
	int arg[10], i;
	
	while (!feof(stdin) && scanf(" %s ", cmd)) {
		
		for (i = 0; i < 10; i++)
			arg[i] = -1;
		
		if (strcasecmp(cmd, "SPEED") == 0) {
			scanf(" %d %d ", &arg[0], &arg[1]);
			twizy_cfg_profile.speed = cfgvalue(arg[0]);
			twizy_cfg_profile.warn = cfgvalue(arg[1]);
		}
		
		else if (strcasecmp(cmd, "POWER") == 0) {
			scanf(" %d %d %d %d ", &arg[0], &arg[1], &arg[2], &arg[3]);
			twizy_cfg_profile.torque = cfgvalue(arg[0]);
			twizy_cfg_profile.power_low = cfgvalue(arg[1]);
			twizy_cfg_profile.power_high = cfgvalue(arg[2]);
			twizy_cfg_profile.current = cfgvalue(arg[3]);
		}
		
		else if (strcasecmp(cmd, "TSMAP") == 0) {
			char map;
			scanf(" %c %d@%d %d@%d %d@%d %d@%d ", &map,
				&arg[0], &arg[1], &arg[2], &arg[3],
				&arg[4], &arg[5], &arg[6], &arg[7]);
			i = (map=='D') ? 0 : (map=='N') ? 1 : 2;
			twizy_cfg_profile.tsmap[i].prc1 = cfgvalue(arg[0]);
			twizy_cfg_profile.tsmap[i].spd1 = cfgvalue(arg[1]);
			twizy_cfg_profile.tsmap[i].prc2 = cfgvalue(arg[2]);
			twizy_cfg_profile.tsmap[i].spd2 = cfgvalue(arg[3]);
			twizy_cfg_profile.tsmap[i].prc3 = cfgvalue(arg[4]);
			twizy_cfg_profile.tsmap[i].spd3 = cfgvalue(arg[5]);
			twizy_cfg_profile.tsmap[i].prc4 = cfgvalue(arg[6]);
			twizy_cfg_profile.tsmap[i].spd4 = cfgvalue(arg[7]);
		}
		
		else if (strcasecmp(cmd, "BRAKELIGHT") == 0) {
			scanf(" %d %d ", &arg[0], &arg[1]);
			twizy_cfg_profile.brakelight_on = cfgvalue(arg[0]);
			twizy_cfg_profile.brakelight_off = cfgvalue(arg[1]);
		}
		
		else if (strcasecmp(cmd, "DRIVE") == 0) {
			scanf(" %d %d %d ", &arg[0], &arg[1], &arg[2]);
			twizy_cfg_profile.drive = cfgvalue(arg[0]);
			twizy_cfg_profile.autodrive_ref = cfgvalue(arg[1]);
			twizy_cfg_profile.autodrive_minprc = cfgvalue(arg[2]);
		}
		
		else if (strcasecmp(cmd, "RECUP") == 0) {
			scanf(" %d %d %d %d ", &arg[0], &arg[1], &arg[2], &arg[3]);
			twizy_cfg_profile.neutral = cfgvalue(arg[0]);
			twizy_cfg_profile.brake = cfgvalue(arg[1]);
			twizy_cfg_profile.autorecup_ref = cfgvalue(arg[2]);
			twizy_cfg_profile.autorecup_minprc = cfgvalue(arg[3]);
		}
		
		else if (strcasecmp(cmd, "RAMPS") == 0) {
			scanf(" %d %d %d %d %d ", &arg[0], &arg[1], &arg[2], &arg[3], &arg[4]);
			twizy_cfg_profile.ramp_start = cfgvalue(arg[0]);
			twizy_cfg_profile.ramp_accel = cfgvalue(arg[1]);
			twizy_cfg_profile.ramp_decel = cfgvalue(arg[2]);
			twizy_cfg_profile.ramp_neutral = cfgvalue(arg[3]);
			twizy_cfg_profile.ramp_brake = cfgvalue(arg[4]);
		}
		
		else if (strcasecmp(cmd, "RAMPL") == 0) {
			scanf(" %d %d ", &arg[0], &arg[1]);
			twizy_cfg_profile.ramplimit_accel = cfgvalue(arg[0]);
			twizy_cfg_profile.ramplimit_decel = cfgvalue(arg[1]);
		}
		
		else if (strcasecmp(cmd, "SMOOTH") == 0) {
			scanf(" %d ", &arg[0]);
			twizy_cfg_profile.smooth = cfgvalue(arg[0]);
		}
		
	}
	
}


// profile2tags:
// 	convert twizy_cfg_profile to tag (key=value) format
void profile2tags(void) {
	
	BYTE *tagvalue = (BYTE *) &twizy_cfg_profile;
	int i;
	
	for (i = 1; i < sizeof(twizy_cfg_profile); i++ ) {
		printf("%s=%d\n", tagname[i], (int) tagvalue[i] - 1);
	}
}


// tags2profile:
// 	convert tag (key=value) format to twizy_cfg_profile
void tags2profile(void) {
	
	BYTE *tagvalue = (BYTE *) &twizy_cfg_profile;
	int i;
	char name[1000];
	int value;
	
	while (!feof(stdin) && scanf(" %[^=]=%d ", name, &value)) {
		for (i = 1; i < sizeof(twizy_cfg_profile); i++ ) {
			if (strcasecmp(tagname[i], name) == 0) {
				tagvalue[i] = cfgvalue(value);
				break;
			}
		}
	}
}



/*******************************************************************************
 *
 * MAIN
 * 
 */

#define FORMAT_COMMANDS		1
#define FORMAT_TAGS			2


int main(int argc, char *argv[])
{
	int format = FORMAT_COMMANDS;
	char *code = NULL;
	int i;
	
	// process command arguments:
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-' && argv[i][1] == 't')
			format = FORMAT_TAGS;
		else
			code = argv[i];
	}
	
	// clear profile:
	memset(&twizy_cfg_profile, 0, sizeof(twizy_cfg_profile));
	
	if (code) {
		// MODE: convert base64 to text
		
		// check length:
		int maxlen = (sizeof(twizy_cfg_profile) * 4 / 3 + 3) & ~3;
		if (strlen(argv[1]) > maxlen) {
			fprintf(stderr, "ERROR: invalid profile: too long\n");
			exit(1);
		}
		
		// decode base64 argument:
		base64decode(code, (BYTE *) &twizy_cfg_profile);
		
		// check integrity:
		if (twizy_cfg_profile.checksum != vehicle_twizy_cfg_calc_checksum((BYTE *) &twizy_cfg_profile)) {
			fprintf(stderr, "ERROR: invalid profile: checksum error\n");
			exit(1);
		}
		
		// convert profile into command/tag syntax:
		if (format == FORMAT_TAGS)
			profile2tags();
		else
			profile2commands();
	}
	
	else {
		// MODE: convert text to base64
		
		// parse commands/tags:
		if (format == FORMAT_TAGS)
			tags2profile();
		else
			commands2profile();
		
		// calculate checksum:
		twizy_cfg_profile.checksum = vehicle_twizy_cfg_calc_checksum((BYTE *) &twizy_cfg_profile);
		
		// encode & print:
		BYTE base64[1000];
		base64encode((BYTE *) &twizy_cfg_profile, sizeof(twizy_cfg_profile), base64);
		printf("%s\n", base64);
	}
	
	exit(0);
}
