/* 
-- ai.c
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
	unsigned long ai_dat[8]={0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000};
	double ai_dat_ex[8]={0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0};
	unsigned char devName[32]="";
	int cnt;
	int iRet;
	int isMulti = 0;
	int isEx = 0;	
	int ch;

	unsigned char g = 0, o = 0;
	int isNotCalibRead = 0;

	double clk = 100.0; // 100 usec
	
	if( argc > 1 ){
		strcat(devName, argv[1]);
	}else{
		strcat(devName, "cpsaio0");
	}

	if( argc > 2 ){
		/*
		if(strcmp( argv[2], "single" ) == 0 ){

		}
		*/
		if( strcmp( argv[2], "singleEx" ) == 0 ){
			isMulti = 0;
			isEx = 1;
		}else if( strcmp( argv[2], "multi") == 0 ){
			isMulti = 1;
			isEx = 0;
		}else if( strcmp( argv[2], "multiEx") == 0 ){
			isMulti = 1;
			isEx = 1;
		}
	}

	/* 計測チャネル数選択 */
	if( argc > 3 ){
		sscanf( argv[3], "%d", &ch );
	}else{
		ch = 8;
	}

	if( argc > 4 ){
		sscanf(argv[4], "%d", &isNotCalibRead );
	}

	/* デバイスをopenする */
	ContecCpsAioInit(devName, &Id);

	if( !isNotCalibRead ){
		ContecCpsAioReadAiCalibrationData( Id, 0, &g, &o );
		ContecCpsAioSetAiCalibrationData( Id, CPSAIO_AI_CALIBRATION_SELECT_OFFSET, 0, CPSAIO_AI_CALIBRATION_RANGE_PM10, o );
		ContecCpsAioSetAiCalibrationData( Id, CPSAIO_AI_CALIBRATION_SELECT_GAIN, 0, CPSAIO_AI_CALIBRATION_RANGE_PM10, g );
	}

	ContecCpsAioSetAiSamplingClock(Id, clk);

	if( isMulti == 0 ){
		/* SingleAi or SingleAiEx sample */
		//for( cnt = 0; cnt < ch;cnt ++ ){
			if( isEx == 0 ){
				/* SingleAi */
				iRet = ContecCpsAioSingleAi(Id, ch, &ai_dat[ch] );
				printf("AI [%d] = %lx\n",ch,ai_dat[ch]);
				if( iRet ){
					printf("Error: %d\n", iRet );
				}
			}else{
				/* SingleAiEx */
				iRet = ContecCpsAioSingleAiEx(Id, ch, &ai_dat_ex[ch] );
				printf("AI [%d] = %lf\n",ch,ai_dat_ex[ch]);
				if( iRet ){
					printf("Error: %d\n", iRet );
				}
			}
		//}
	}else{
		/* MultiAi or MultiAiEx sample */
		if( isEx == 0 ){
			/* MultiAi */
			iRet = ContecCpsAioMultiAi(Id, ch, &ai_dat[0] );
			if( iRet ){
				printf("Error: %d\n", iRet );
			}
			for( cnt = 0;cnt < ch; cnt ++){
				printf("AI [%d] = %lx\n",cnt,ai_dat[cnt]);
			}
		}else{
			/* MultiAiEx */
			iRet = ContecCpsAioMultiAiEx(Id, ch, &ai_dat_ex[0] );
			if( iRet ){
				printf("Error: %d\n", iRet );
			}
			for( cnt = 0; cnt < ch ;cnt ++){
				printf("AI [%d] = %lf\n",cnt,ai_dat_ex[cnt]);
			}
		}
	}
	/* デバイスを閉じる*/
	ContecCpsAioExit(Id);
	return 0;
}

