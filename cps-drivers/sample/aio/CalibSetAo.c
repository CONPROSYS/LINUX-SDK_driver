/* 
-- CalibSetAo.c
-- Copyright (c) 2015 Syunsuke Okamoto
--
-- This software is released under the MIT License.
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
-- 
-- The above copyright notice and this permission notice shall be included in
-- all copies or substantial portions of the Software.
-- 
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
-- THE SOFTWARE.
-- 
*/
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include "libcpsaio.h"

int main(int argc, char *argv[]){

	short Id;
	unsigned long ai_dat[8];
	unsigned char devName[32], selectName[8], rangeName[16];
	int cnt;
	double clk = 100.0; // 100 usec
	unsigned short val;
	unsigned char sel, range;
	unsigned int ch;

	if( argc > 5 ){

		strcpy(devName, argv[1]);
		strcpy(selectName, argv[2] );

		if( strcmp( selectName, "gain" ) == 0){  
			sel = CPSAIO_AO_CALIBRATION_SELECT_GAIN;
		}else if( strcmp( selectName, "offset" ) == 0){ 
			sel = CPSAIO_AO_CALIBRATION_SELECT_OFFSET;
		}else{
			printf("select error\n");
			return ( 1 );
		}
		strcpy(rangeName, argv[3] );
		if( strcmp( rangeName, "p20ma" ) == 0 ){
			range = CPSAIO_AO_CALIBRATION_RANGE_P20MA;
		}else{
			printf("range error\n");
			return ( 1 );
		}
		sscanf(argv[4], "%x", &ch );
		sscanf(argv[5], "%hx", &val );

	}else{
		printf(" Calib Set Ao \n");
		printf(" CalibSetAo <device> <select> <range> <value>\n\n");
		printf(" [device] : device name (example cpsaio0) \n");
		printf(" [select] : set \"offset\" or \"gain\" (example gain) \n");
		printf(" [range] :  set range. (example p20ma ) \n");
		printf(" [channel] : set channel value. (example 0) \n");
		printf(" [value]  : set hex value. This range can set from 00 to FF. (example C0A0 ) \n\n");
		printf(" CalibSetAi cpsaio0 gain p20ma 0 0E \n");
		return ( 1 );
	}

	/* デバイスをopenする */
	ContecCpsAioInit(devName, &Id);

	ContecCpsAioSetAoCalibrationData(Id, sel, ch, range, val );

	/* デバイスを閉じる*/
	ContecCpsAioExit(Id);
	return 0;
}

