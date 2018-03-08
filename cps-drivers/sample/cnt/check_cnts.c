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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include "libcpscnt.h"

int main(int argc, char *argv[]){
	short Id = 0;
	short ChNo = 0;
	unsigned char devName[32]={0};
	unsigned short isPreset = 0;

	unsigned long CntDat = 0;

	if( argc < 4 ){
		printf(" --- %s Ver.1.0.1--- \n", argv[0]);
		printf(" check_cnts [device Number] [Channel] [Preset]\n");
		return 0;
	}

	sprintf(devName,"cpscnt%s", argv[1]); 	
	sscanf(argv[2], "%hd", &ChNo );
	sscanf(argv[3], "%hd", &isPreset );

	/* デバイスをopenする */
	ContecCpsCntInit(devName, &Id);

	/* デジタルフィルタ 最大 1 usec */
	ContecCpsCntSetDigitalFilter(Id, ChNo, 0x3 );  

	/* オペレーション・モード設定  (2相、同期クリア、１逓倍*/
	//ContecCpsCntSetOperationMode(Id, ChNo, CNT_MODE_2PHASE, CNT_MUL_X1, CNT_CLR_SYNC );
	
	/* 方向 アップカウント */
	ContecCpsCntSetCountDirection( Id, ChNo, CNT_DIR_UP );

	/* Z相　毎回有効 */
	//ContecCpsCntSetZMode( Id, ChNo, CNT_ZPHASE_EVERY_TIME );

	/* カウントスタート */
	ContecCpsCntStartCount( Id, &ChNo, 1 );
	
	if( isPreset ){
		/* プリセット  0 */
		ContecCpsCntPreset( Id, &ChNo, 1, &CntDat );
	}
	else {
		sleep( 4 );
		/* リードカウント */
		ContecCpsCntReadCount( Id, &ChNo, 1, &CntDat );
	}

	/*カウントストップ */
	ContecCpsCntStopCount( Id, &ChNo, 1 );

	if( !isPreset ){
		/* 出力 */
		printf("%ld", CntDat);
	}

	/* デバイスを閉じる*/
	ContecCpsCntExit(Id);
	return 0;
}

