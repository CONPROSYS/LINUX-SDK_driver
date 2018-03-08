/* 
-- CalibClear.c 
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

int main(int argc, char *argv[]){

	short Id;
	unsigned char devName[32];

	if( argc > 1 ){
		strcpy(devName, argv[1]);
	}else{
		printf(" Calib Clear \n");
		printf(" CalibClear <device> \n\n");
		printf(" [device] : device name (example cpsssi0) \n");
		printf(" CalibClear cpsssi0 \n");		
		return ( 1 );
	}

	/* デバイスをopenする */
	ContecCpsSsiInit(devName, &Id);

	ContecCpsSsiClearCalibrationData(Id, CPSSSI_CALIBRATION_CLEAR_ALL);

	/* デバイスを閉じる*/
	ContecCpsSsiExit(Id);
	return 0;
}

