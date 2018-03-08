/* 
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
#include <getopt.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <time.h>
#include <sys/time.h>
#include "libcpsdio.h"
#include <linux/spi/spidev.h>

int main(int argc, char *argv[]) {
	short Id;

//	unsigned char dat[] = { 0x01, 0x02, 0x04,0x08,0x10,0x20,0x40,0x80 };
	unsigned char devName[32]="";
	unsigned char dat[] = { 0x00, 0x55, 0xAA };
	unsigned char dat2;
	unsigned int cnt=0;
	unsigned int ch = 0;
	int i = 0;
	int pat_num;
	unsigned int isPowerSupply;

	pat_num = sizeof(dat) / sizeof(dat[0]);
 
	if( argc > 1 ){
		strcat(devName, argv[1]);
	}else{
		strcat(devName, "cpsdio0");
	}

	/* 計測チャネル数選択 */
	if( argc > 2 ){
		sscanf( argv[2], "%d", (unsigned int*)&ch );
	}else{
		ch = 0;
	}

	// 電源制御 (BLのみ)
	if( argc > 3 ){
		sscanf( argv[4], "%d", &isPowerSupply );
	}else{
		isPowerSupply = DIO_POWERSUPPLY_INTERNAL;
	}


	/* デバイスをopenする */
	ContecCpsDioInit(devName, &Id);

#ifdef CONFIG_DIO_BL
	ContecCpsDioSetInternalPowerSupply( Id, (unsigned char)isPowerSupply );
#endif

	while (1) {
		for (i = 0; i < pat_num; i++) {
			ContecCpsDioOutByte(Id, ch, dat[i]);

			usleep(100);

			ContecCpsDioInpByte(Id, ch, &dat2);
			cnt=cnt + 1;

			if (dat[i] != dat2) {
				printf(" In %x Out %x \n", dat2, dat[i]);
				ContecCpsDioExit(Id);
				return 1;
			}

			usleep(10000);
		}
	}
	/* デバイスを閉じる*/
	ContecCpsDioExit(Id);
	return 0;
}

