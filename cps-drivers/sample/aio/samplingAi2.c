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
	unsigned long ai_dat[8000];
	double ai_dat_ex[8];
	unsigned char devName[32];
	int cnt, cnt_c;
	int iRet;
	int isMulti = 0;
	int isEx = 0;	
	int ch, num;
	FILE *fp, *fp2;
	int tmp_num = 0;

	double clk = 100.0; // 100 usec
//	double clk = 1000.0; // 1000 usec

	if( argc > 1 ){
		strcpy(devName, argv[1]);
	}else{
		strcpy(devName, "cpsaio0");
	}

	if( argc > 2 ){
		sscanf(argv[2], "%d", &num);
	}else{
		num = 8000;
	}

	/* 計測チャネル数選択 */
	if( argc > 3 ){
		sscanf( argv[3], "%d", &ch );
	}else{
		ch = 8;
	}


	fp = fopen("sampling_ai.dat","w");

	fp2 = fopen("sampling_ai.csv","w");

	/* デバイスをopenする */
	ContecCpsAioInit(devName, &Id);

	ContecCpsAioSetAiSamplingClock(Id, clk);

	

//	ContecCpsAioSetAiEventSamplingTimes( Id, num );

//	ContecCpsAioStartAi( Id );
	for( cnt = 0; 8000 * cnt < num; cnt ++ ){
		memset( &ai_dat[0], 0 ,8000 );
		if( num - 8000 * cnt > 8000 ) tmp_num = 8000;
		else tmp_num = num - 8000 * cnt;
		ContecCpsAioSetAiEventSamplingTimes( Id, tmp_num );
		
		ContecCpsAioSetAiChannels( Id, ch );

		ContecCpsAioStartAi( Id );
		
//		if( num - 8000 * (cnt+1) < 0 ){
//			ContecCpsAioGetAiSamplingData( Id, (num - 8000 * (cnt) ), ai_dat );
		if( tmp_num < 8000 ){
			ContecCpsAioGetAiSamplingData( Id, tmp_num, ai_dat );
			for( cnt_c = 0;cnt_c < tmp_num; cnt_c ++){
				fprintf(fp,"%lx\n",ai_dat[cnt_c]);
				fprintf(fp2,"%ld\n",ai_dat[cnt_c]);
			}
			break;
		}else{
			ContecCpsAioGetAiSamplingData( Id, 8000, ai_dat);
			for( cnt_c = 0; cnt_c < 8000; cnt_c ++ ){
				fprintf(fp,"%lx\n",ai_dat[cnt_c]);
				fprintf(fp2,"%ld\n",ai_dat[cnt_c]);
			}				 
		}
		ContecCpsAioStopAi( Id );
	}
//	ContecCpsAioStopAi( Id );


	/* デバイスを閉じる*/
	ContecCpsAioExit(Id);

	fclose(fp2);
	fclose(fp);
	
	return 0;
}

