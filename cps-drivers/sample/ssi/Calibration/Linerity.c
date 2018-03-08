/*
-- Linerity.c 
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

	short Id;
	double data;
	unsigned char devName[32];
	int cnt, cnt_wire, cnt_temp,init_wire, init_temp;
	int jmpdat;
	int wire[2]={SSI_CHANNEL_3WIRE, SSI_CHANNEL_4WIRE};
	double temp[3]={ -125.15, 0.0, 715.26 };
	char str_wire[2][7]={"3_wire", "4_wire"};
	char str_temp[3][8]={" 50 ohm","100 ohm", "350 ohm"};
	double temp_cmp[2]={ 0.3, 0.1 };

	double w3Val, w4Val, gVal;

	int isReadCalib;

	if( argc > 1 ){
		strcpy(devName, argv[1]);
	}else{
		strcpy(devName, "cpsssi0");
	}
	if( argc > 2 ){
		sscanf(argv[2], "%d", &isReadCalib);
	}else{
		isReadCalib = 0;
	}
	if( argc > 3 ){
		sscanf(argv[3], "%d",&jmpdat );
		init_wire = jmpdat / 3;
		init_temp = jmpdat % 3;
	}else{
		init_wire = 0;
		init_temp = 0;
	}

	/* デバイスをopenする */
	ContecCpsSsiInit( devName, &Id );

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

	for( cnt_wire = init_wire;cnt_wire < 2; cnt_wire ++){
		for( cnt_temp = init_temp; cnt_temp < 3; cnt_temp ++ ){
			printf("%s %s tested. please %s register setting.\n", str_wire[cnt_wire], str_temp[cnt_temp], str_temp[cnt_temp] );
			getchar();
			for( cnt = 0; cnt < 4 ; cnt ++ ){
				ContecCpsSsiSetChannel( Id, cnt, wire[cnt_wire], SSI_CHANNEL_PT );
				ContecCpsSsiSingleTemperature(Id, cnt, &data );
				printf(" ch:%d %lf < %lf > , ", cnt, data, temp[cnt_temp] );
				if( fabs( temp[cnt_temp] - data ) <= temp_cmp[cnt_wire] ){
					printf("[PASS] \n");
				}else{
					printf("[NG] \n");
				}
			}
		}

	}

	/* デバイスを閉じる*/
	ContecCpsSsiExit(Id);
	return 0;
}

