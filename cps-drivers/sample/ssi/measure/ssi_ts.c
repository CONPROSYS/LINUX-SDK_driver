/*
-- ssi_ts.c (ssi task-script version)
-- Copyright (c) 2016 Syunsuke Okamoto
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
#include <math.h>
#include "libcpsssi.h"

int main(int argc, char *argv[]){

	short Id, ch;
	long d0=0;
	unsigned char devName[32];
	int cnt, wire,isReadCalib;
	unsigned long wStatus;
	double w3Val, w4Val, gVal;


	if( argc > 1 ){
		strcpy(devName, argv[1]);
	}else{
		printf(" ssi ( task script version ) \n");
		printf(" ssi_ts <device> -ch<channel> -wire<wire> <isCalib>\n\n");
		printf(" [device] : device name (example cpsssi0) \n");
		printf(" [channel] : channel number   \n");
		printf(" [wire] : (0) => 3-wire (1) => 4-wire \n");
		printf(" [isCalib] : (0) => Calibration Off  (1) => Calibration ON \n");
		printf(" CalibWrite cpsssi0 -ch2 -wire1 0\n");		
		return ( 1 );
	}

	if( argc > 2 ){
		sscanf(argv[2], "-ch%hd", &ch );
	}else{
		ch = 0;
	}
	if( argc > 3 ){
		sscanf(argv[3], "-wire%d", &wire );
	}else{
		wire = 0;
	}
	if( argc > 4 ){
		sscanf(argv[4], "%d", &isReadCalib );
	}else{
		isReadCalib = 1;
	}

	/* デバイスをopenする */
	ContecCpsSsiInit( devName, &Id );

	ContecCpsSsiSetChannel( Id, ch , wire, SSI_CHANNEL_PT);

	if( isReadCalib ){
		for( cnt = 0; cnt < 4 ; cnt ++ ){
			/* Offset ROM Read -> FPGA Write */
			ContecCpsSsiReadCalibrationOffset( Id, cnt, &w3Val, &w4Val );
			ContecCpsSsiSetCalibrationOffset( Id, cnt, SSI_CHANNEL_3WIRE, w3Val );
			ContecCpsSsiSetCalibrationOffset( Id, cnt, SSI_CHANNEL_4WIRE, w4Val );
		}
		/* Gain ROM Read -> FPGA Write */
		ContecCpsSsiReadCalibrationGain( Id, &gVal );
		ContecCpsSsiSetCalibrationGain( Id, gVal );
	}

	ContecCpsSsiStart( Id, ch );

	do{
		ContecCpsIsConversionStartBusyStatus( Id, &wStatus );
	}while( !(wStatus & 0x40) );

	ContecCpsSsiGetData( Id, ch, &d0 );
	
	printf("%ld", d0 );

	/* デバイスを閉じる*/
	ContecCpsSsiExit(Id);
	return 0;
}

