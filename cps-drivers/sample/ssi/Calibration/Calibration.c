/* 
-- CalibWrite.c 
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
#include "libcpsssi.h"

int Calibration( short Id, unsigned int ch, unsigned int pattern , double ohm , int avg_num )
{

	int val;
	int max, min, area;
	double ohm_r, ohm_rr, ohm_area, temp_r,sence = 0.0;
	unsigned int cnt, cnt_avg;
	unsigned int ok = 0;
	unsigned int n;
	int n_pm = 0;
	val = 0;

	switch( pattern ){
	case 1 :
	case 3 : max = 127; min = -128; area = 0x80; ohm_area= 0.01; n=0; break;
	case 2 : max = 32767; min = -32768 ; area = 0x8000;ohm_area= 0.0005; ;n=9; break;
	}

	printf(" max = %d , min = %d \n", max, min );
	for( cnt = 0; cnt < area; cnt ++){
		printf("ch[%d] val=%d ", ch, val );
		switch ( pattern ){
		case 1:
			ContecCpsSsiSetCalibrationOffsetToUShort( Id, ch, SSI_CHANNEL_4WIRE, val );
			break;
		case 2:
			ContecCpsSsiSetCalibrationGainToUShort( Id, val );
			break;
		case 3:
			ContecCpsSsiSetCalibrationOffsetToUShort( Id, ch, SSI_CHANNEL_3WIRE, val );
			break;
		}	
		//ContecCpsSsiSingleTemperature( Id, ch, &temp_r );
		for( cnt_avg = 0, ohm_r = 0.0;cnt_avg < avg_num; cnt_avg ++ ){
			ContecCpsSsiSingleRegistance( Id, ch, &ohm_rr );
			ohm_r = ohm_rr + ohm_r;
		}
		ohm_r = ohm_r / avg_num;

		printf(" R=%lf\n", ohm_r );
		if( ohm_r >= (ohm - ohm_area) && ohm_r <= (ohm + ohm_area ) ){
			if( ok > 9 ) break;
			else ok++;
		}else if( ohm_r > (ohm + ohm_area  ) ){
			val -= (1 << n);
			if( n_pm > 0 && n > 0 ) n--;
			n_pm = -1;
		}else if( ohm_r < (ohm - ohm_area ) ){
			val += (1 << n);
			if( n_pm < 0 && n > 0 ) n--;
			n_pm = 1; 
		}
		if( val == min || val == max ){
			return max;
		}
	}

	if( pattern == 2 ){
		ContecCpsSsiGetSenceRegister(Id, &sence );
		printf(" RSence = %lf \n", sence );
	}

	return val;
}



int main(int argc, char *argv[]){

	short Id;
	unsigned char devName[32];

	unsigned char w3off[4], w4off[4];
	unsigned short gain;
	unsigned int cnt;
	unsigned int ch;
	unsigned int jmppat = 0;
	unsigned int gain_ch = 0;

	if( argc > 1 ){
		strcpy(devName, argv[1]);
		if( argc > 2 ){
			sscanf(argv[2],"%d",&gain_ch);
		}
		if( argc > 3 ){
			sscanf(argv[3],"%d",&jmppat );
		}
		
	}else{
		printf(" Calibration  \n");
		printf(" Calibration <device>\n\n");
		printf(" [device] : device name (example cpsssi0) \n");
		printf(" [gain_ch] : gain channel \n");
		printf(" Calibration cpsssi0 \n");		
		return ( 1 );
	}
	

	/* デバイスをopenする */
	ContecCpsSsiInit(devName, &Id);
	
	if( jmppat < 1 ){
		ContecCpsSsiClearCalibrationData( Id, CPSSSI_CALIBRATION_CLEAR_ALL );

		printf(" --- 4 wire offset --- \n");
		printf(" [ please set 50 ohm register. ]\n\n");
		getchar();

		// 4 Wire Offset

		for( cnt = 0;cnt < 4;cnt ++ ){
			ContecCpsSsiSetChannel( Id, cnt, SSI_CHANNEL_4WIRE, SSI_CHANNEL_PT );
			w4off[cnt] = Calibration( Id, cnt , 1, 50.0, 1);
		}	
	}

	if( jmppat < 2 ){
		printf(" --- 4 wire gain  channel : %d--- \n",gain_ch);
		printf(" [ please set 350 ohm register. ]\n\n");
		getchar();

		ContecCpsSsiSetChannel( Id, gain_ch, SSI_CHANNEL_4WIRE, SSI_CHANNEL_PT );

		// Gain
		gain = Calibration( Id, gain_ch , 2, 350.0, 5);
	}
	printf(" --- 3 wire offset --- \n");
	printf(" [ please set 50 ohm register. ]\n\n");
	getchar();
 
	// 3 Wire Offset

	for( cnt = 0;cnt < 4;cnt ++ ){
		ContecCpsSsiSetChannel( Id, cnt, SSI_CHANNEL_3WIRE, SSI_CHANNEL_PT );
		w3off[cnt] = Calibration( Id, cnt , 3, 50.0, 1);
	}


	/* ROM WRITE */
	ContecCpsSsiClearCalibrationData(Id, CPSSSI_CALIBRATION_CLEAR_ROM);
	// Gain
	ContecCpsSsiWriteCalibrationGainToUShort(Id, gain);
	
	// Offset
	for( cnt = 0; cnt < 4; cnt ++ ){
		ContecCpsSsiWriteCalibrationOffsetToUChar(Id, cnt, w3off[cnt], w4off[cnt] );
	}

	/* デバイスを閉じる*/
	ContecCpsSsiExit(Id);
	return 0;
}

