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
	short Id;
	int cnt;
	short ChNo[2]={0,1};

	unsigned long CntDat[2];


	/* デバイスをopenする */
	ContecCpsCntInit("cpscnt0", &Id);

	ContecCpsCntStartCount( Id, &ChNo[0], 2 );

	sleep( 10 );
	
	ContecCpsCntReadCount( Id, &ChNo[0], 2, &CntDat[0] );

	ContecCpsCntStopCount( Id, &ChNo[0], 2);

	for( cnt = 0; cnt < 2; cnt ++ ){
		printf("%ld\n", CntDat[cnt]);
	}
	
	/* デバイスを閉じる*/
	ContecCpsCntExit(Id);
	return 0;
}

