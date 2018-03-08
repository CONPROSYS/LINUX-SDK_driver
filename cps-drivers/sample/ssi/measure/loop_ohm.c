/*
-- loop_ohm.c 
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

#define PROGRAM_VERSION "1.0.0"

int main(int argc, char *argv[]){

	short Id;
	double data, data_m,temp_cmp;
	unsigned char devName[32];
	int cnt,cnt_num,cnt_m;
	int loop_num;
	int select_1, select_2;
	int wire[2]={SSI_CHANNEL_3WIRE, SSI_CHANNEL_4WIRE};
	double temp[3]={ -125.15, 0.0, 715.26 };
	char str_wire[2][7]={"3_wire", "4_wire"};
	char str_temp[3][8]={" 50 ohm","100 ohm", "350 ohm"};
	unsigned long status;

	int ch;

	double w3Val, w4Val, gVal;
	FILE *fp;

	int isReadCalib;

	int chk_wire;

	if( argc > 1 ){
		strcpy(devName, argv[1]);
	}else{
		printf(" ---loop measure (ohm) Version %s (PT100 only)---\n\n",PROGRAM_VERSION);
		printf(" loop_ohm <device node> <loop number> <select wire> <select sample-ohm> <Passed line>\n");
		printf(" device node : device node name. (example: cpsssi0 )\n");
		printf(" loop number : loop number (example 10)\n");
		printf(" select wire : (0):3-wire (1):4-wire \n");
		printf(" select sample-ohm : (0): 50 ohm (1): 100 ohm (2):350 ohm \n");
		printf(" passed line : measure data less than pass line. (example 0.1[digree-c] )\n");
		printf(" channel : 1 channel test mode.(select channel 0 to 3) (default -1:(All channel test) )\n");
		printf("\n loop_ohm cpsssi0 10 1 1 0.2");
		return;
	}
	if( argc > 2 ){
		sscanf(argv[2], "%d",&loop_num );
	}else{
		loop_num = 10;
	}
	if( argc > 3 ){
		sscanf(argv[3],"%d", &select_1);
	}else{
		select_1 = 0;
	}

	if( argc > 4 ){
		sscanf(argv[4], "%d", &select_2);
	}else{
		select_2 = 0;
	}

	if( argc > 5 ){
		sscanf(argv[5], "%lf", &temp_cmp );
	}else{
		temp_cmp = 0.1;
	}
	if( argc > 6){
		sscanf(argv[6], "%d", &ch );
	}else{
		ch = -1;
	}

	/* デバイスをopenする */
	ContecCpsSsiInit( devName, &Id );

	for( cnt = 0; cnt < 4 ; cnt ++ ){
		/* Offset ROM Read -> FPGA Write */
		ContecCpsSsiReadCalibrationOffset( Id, cnt, &w3Val, &w4Val );
		ContecCpsSsiSetCalibrationOffset( Id, cnt, SSI_CHANNEL_3WIRE, w3Val );
		ContecCpsSsiSetCalibrationOffset( Id, cnt, SSI_CHANNEL_4WIRE, w4Val );
	}
	/* Gain ROM Read -> FPGA Write */
	ContecCpsSsiReadCalibrationGain( Id, &gVal );
	ContecCpsSsiSetCalibrationGain( Id, gVal );

	fp = fopen("measure-data.csv","w");

	for( cnt_num = 0; cnt_num < loop_num ; cnt_num ++ ){

		for( cnt = 0; cnt < 4 ; cnt ++ ){

			if( ch != -1 && cnt != ch ) continue;

			ContecCpsSsiSetChannel( Id, cnt, wire[select_1], SSI_CHANNEL_PT );
			for( cnt_m = 0,data = 0 ;cnt_m< 1; cnt_m ++ ){
				ContecCpsSsiSingleTemperature(Id, cnt, &data_m );
				ContecCpsSsiGetStatus( Id, &status );
				if( !(status & ( 0x01 << ( cnt * 8 ) ) ) ){
					cnt_m -= 1;
					continue;
				}
				data += data_m;
			}
			data = data / 1.0;
			printf(" ch:%d %lf < %lf > , ", cnt, data, temp[select_2] );
			fprintf(fp,"%lf,", data );
			if( fabs( temp[select_2] -data ) <= temp_cmp ){
				printf("[PASS] \n");
			}else{
				if( (temp[select_2] - data) > temp_cmp ){
					printf("[Error:up over : %lf] ",temp[select_2] -data );
				}else{
					printf("[Error:down over : %lf] ",data - temp[select_2]);
				}
				printf("[NG] \n");
			}
		}
		fprintf(fp,"\n");
	}
	fclose(fp);

	/* デバイスを閉じる*/
	ContecCpsSsiExit(Id);
	return 0;
}

