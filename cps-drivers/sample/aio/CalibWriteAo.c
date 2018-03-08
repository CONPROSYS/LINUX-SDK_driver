/* 
-- CalibWriteAo.c 
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
	unsigned char devName[32];
	int cnt;
	double clk = 100.0; // 100 usec
	unsigned short val;
	unsigned int gain, offset;
	unsigned int ch;
	if( argc > 4 ){

		strcpy(devName, argv[1]);
		sscanf(argv[2], "%x", &ch);
		sscanf(argv[3], "%x", &gain );
		sscanf(argv[4], "%x", &offset );

	}else{
		printf(" Calib Write Ao \n");
		printf(" CalibWriteAo <device> <gain-value> <offset-value>\n\n");
		printf(" [device] : device name (example cpsaio0) \n");
		printf(" [gain-value] : set \"offset\" or \"gain\" (example gain) \n");
		printf(" [offset-value] :  set . (example pm10 ) \n");
		printf(" CalibWriteAo cpsaio0 0 0E 00 \n");		
		return ( 1 );
	}

	/* デバイスをopenする */
	ContecCpsAioInit(devName, &Id);

	ContecCpsAioWriteAoCalibrationData(Id, ch, (unsigned char)gain, (unsigned char)offset );

	/* デバイスを閉じる*/
	ContecCpsAioExit(Id);
	return 0;
}

