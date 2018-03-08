/* 
 -- Copyright (c) 2016 Shinichiro Nagasato
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
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <sys/io.h>

#include "libcpsdio.h"

//	dio_test [-w,-r] [data]
//	[-r]:	Read SPI data
//	[-w]:	write SPI data

void para_err() {
	printf("dio_test -w address data [power supply] [device Name] \n");
	printf("dio_test -r address [power supply] [device name] \n");
}
int main(int argc, char **argv) {
	short Id;

//	unsigned char dat[] = { 0x01, 0x02, 0x04,0x08,0x10,0x20,0x40,0x80 };
//	unsigned char dat[] = { 0x00, 0x55, 0xAA };
//	unsigned char dat2;
	unsigned int snd_dat = 0;
	unsigned char rcv_dat = 0;
	unsigned int cnt = 0;
	unsigned int adr = 0;
	unsigned long data;
	int i = 0;

	unsigned char devName[32]="";
	unsigned int isPowerSupply;


	if (memcmp(argv[1], "-w", 2) == 0) {
		if (argc < 3) {
			para_err();
			return 0;
		}
		sscanf(argv[2], "%x", &adr);
		sscanf(argv[3], "%x", &snd_dat);

		// 電源制御 (BLのみ)
		if( argc > 4 ){
			sscanf( argv[4], "%d", &isPowerSupply );
		}else{
			isPowerSupply = DIO_POWERSUPPLY_INTERNAL;
		}		

		if( argc > 5 ){
			strcat(devName, argv[5]);
		}else{
			strcat(devName, "cpsdio0");
		}		

		/* デバイスをopenする */
		ContecCpsDioInit(devName, &Id);

#ifdef CONFIG_DIO_BL
			ContecCpsDioSetInternalPowerSupply( Id, (unsigned char)isPowerSupply );
#endif

		ContecCpsDioOutByte(Id, adr, snd_dat);
	} else if (memcmp(argv[1], "-r", 2) == 0) {
		if (argc < 2) {
			para_err();
			return 0;
		}
		sscanf(argv[2], "%x", &adr);

		// 電源制御 (BLのみ)
		if( argc > 3 ){
			sscanf( argv[3], "%d", &isPowerSupply );
		}else{
			isPowerSupply = DIO_POWERSUPPLY_INTERNAL;
		}		

		if( argc > 4 ){
			strcat(devName, argv[4]);
		}else{
			strcat(devName, "cpsdio0");
		}

		/* デバイスをopenする */
		ContecCpsDioInit(devName, &Id);

#ifdef CONFIG_DIO_BL
		ContecCpsDioSetInternalPowerSupply( Id, (unsigned char)isPowerSupply );
#endif

		ContecCpsDioInpByte(Id, adr, &rcv_dat);
//		printf("rcv=%x\n", rcv_dat);
		printf("rcv=%02x\n", rcv_dat);
	}

	/* デバイスを閉じる*/
	ContecCpsDioExit(Id);
	return 0;
}

