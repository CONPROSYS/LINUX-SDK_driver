/* 
-- ao_ts.c  ao ( cps mc341 task script version ) 
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
#include "libcpsaio.h"

int main(int argc, char *argv[]){

	short Id;
	long ao_dat[4]={0x8000,0x8000,0x8000,0x8000};
	double ao_dat_ex[4]={0.0,0.0,0.0,0.0};

	unsigned char devName[32];
	int cnt;
	double clk=100.0;
	long lRet;
	int isMulti = 0;
	int isEx = 0;	
	int ch = 3;
	double dblVal;
	int iVal;

	unsigned char g = 0, o = 0;
	int isCalibRead = 0;

	if( argc > 1){
		if( strcmp(argv[1], "-h" ) == 0 || strcmp(argv[1], "--help") == 0 ){
			printf(" ao_ts [deviceName] [ch] [value] [iscalibread] [mode]\n");
			printf(" device : device Name ( cpsaio0 )\n");
			printf(" channel : channel number .(0 to 3)\n");
			printf(" value : value ( mode single or multi [from 0 to 65535], mode singleEx or MultiEx  [from 0.0 to 20.0] )\n");
			printf(" isCalibRead : Calibration Data Read on:1 off:0\n");
			printf(" mode : select single,singleEx,multi,multiEx\n\n");
		 	printf(" example ) ./ao_ts cpsaio0 2 3.00 1 singleEx \n");
			return 0;		
		}
		strcpy(devName, argv[1]);
	}else{
		strcpy(devName, "cpsaio0");
	}

	/* channel */
	if( argc > 2 ){
		sscanf(argv[2], "%d",&ch );
	}

	/* mode select */
	if( argc > 5 ){
		/*
		if(strcmp( argv[5], "single" ) == 0 ){

		}
		*/
		if( strcmp( argv[5], "singleEx" ) == 0 ){
			isMulti = 0;
			isEx = 1;
		}else if( strcmp( argv[5], "multi") == 0 ){
			isMulti = 1;
			isEx = 0;
		}else if( strcmp( argv[5], "multiEx") == 0 ){
			isMulti = 1;
			isEx = 1;
		}
	}


	/* 出力値変更 */
	if( argc > 3 ){
		if( isEx == 0 )
			sscanf( argv[3], "%d", &iVal );
		else
			sscanf( argv[3], "%lf", &dblVal);

		if( isMulti ){
			for( cnt = 0; cnt < ch;cnt ++ ){
				if( isEx == 0 )
					ao_dat[cnt] = iVal;
				else
					ao_dat_ex[cnt] = dblVal;
			}
		}else{
			if( isEx == 0 )
				ao_dat[ch] = iVal;
			else
				ao_dat_ex[ch] = dblVal;
		}
	}

	if( argc > 4 ){
		sscanf(argv[4], "%d", &isCalibRead );
	}else{
		isCalibRead = 1;
	}

	/* デバイスをopenする */
	lRet = ContecCpsAioInit(devName, &Id);

	if( lRet != 0 ){
		printf(" open error \n");
	}

	if( isCalibRead ){
		for( cnt = 0;cnt < 4; cnt ++ ){
			ContecCpsAioReadAoCalibrationData( Id, cnt, &g, &o );
			ContecCpsAioSetAoCalibrationData( Id, CPSAIO_AO_CALIBRATION_SELECT_OFFSET, cnt, CPSAIO_AO_CALIBRATION_RANGE_P20MA, o );
			ContecCpsAioSetAoCalibrationData( Id, CPSAIO_AO_CALIBRATION_SELECT_GAIN, cnt, CPSAIO_AO_CALIBRATION_RANGE_P20MA, g );
		}
	}

	ContecCpsAioSetAoSamplingClock( Id, clk ); 

	if( isMulti == 0 ){
		/* SingleAo or SingleAoEx Sample */
		if( isEx == 0 ){
			/* SingleAo */
			ContecCpsAioSingleAo(Id, ch, ao_dat[ch] );
			printf("%ld\n",ao_dat[ch]);
		}
		else{
			/* SingleAoEx */
			ContecCpsAioSingleAoEx(Id, ch, ao_dat_ex[ch] );
			printf("%lf\n",ao_dat_ex[ch]);
		}
	}else{
		/* MultiAo or MultiAoEx Sample */
		if( isEx == 0 )	/* MultiAo */
			ContecCpsAioMultiAo(Id, ch, &ao_dat[0] );
		else	/* MultiAoEx */
			ContecCpsAioMultiAoEx(Id, ch, &ao_dat_ex[0] );

		for( cnt = 0; cnt < ch + 1;cnt ++ ){
			if( isEx == 0 )
				printf("[%ld]\n",ao_dat[cnt]);
			else
				printf("%lf\n", ao_dat_ex[cnt]);
		}
	}

	/* デバイスを閉じる*/
	ContecCpsAioExit(Id);
	return 0;
}

