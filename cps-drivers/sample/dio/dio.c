/* 
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
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include "libcpsdio.h"

int main(int argc, char *argv[]){
	short Id;
	unsigned char devName[32]="";
	unsigned char dat, dat2;
	unsigned int isPowerSupply;
	int ch;

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

	// データ設定
	if( argc > 3 ){
		sscanf( argv[3], "%x",(unsigned int*) &dat );
	}else{
		dat = 0x55;
	}

	// 電源制御 (BLのみ)
	if( argc > 4 ){
		sscanf( argv[4], "%d", &isPowerSupply );
	}else{
		isPowerSupply = DIO_POWERSUPPLY_INTERNAL;
	}

	/* デバイスをopenする */
	ContecCpsDioInit(devName, &Id);

#ifdef CONFIG_DIO_BL
	ContecCpsDioSetInternalPowerSupply( Id, (unsigned char)isPowerSupply );
#endif
	ContecCpsDioOutByte(Id, ch, dat );

	sleep( 1 );

	ContecCpsDioInpByte(Id, ch, &dat2 );

	if( dat == dat2 ) printf("[ch:%d] In = Out (value %x)\n", ch, dat );
	else	printf(" In %x Out %x \n", dat2, dat ); 

	/* デバイスを閉じる*/
	ContecCpsDioExit(Id);
	return 0;
}

