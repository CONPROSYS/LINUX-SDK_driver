/*
 *  Sample gpmc_testd for iolib with CPS-MCS341.
 *
 *  Copyright (C) 2015 Syunsuke Okamoto.<okamoto@contec.jp>
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "cps-iolib.h"

#define DEVNAME "/dev/cps-iolib0"
#define SAMP_VER "1.0.3"

void para_err(){
	printf("*******************************************\n");
	printf(" gpmc_testd  cps-iolib version %s\n",SAMP_VER);
	printf("*******************************************\n");
	printf( "WORD access \n" );
	printf( " gpmc_testd -r address  \n" );
	printf( " gpmc_testd -w address data \n" );
	printf( "BYTE access \n" );
	printf( " gpmc_testd -r1 address  \n" );
	printf( " gpmc_testd -w1 address data \n" );
	printf( "TEST access \n" );
	printf( " gpmc_testd -t address \n" );
	printf( " gpmc_testd -t1 address \n" );
	printf( " gpmc_testd -t address data1 data2 \n");
	printf( " gpmc_testd -t1 address data1 data2 \n");
	printf( "LOOP access \n" );
	printf( " gpmc_testd -r1 -l address data \n" );
	printf( " gpmc_testd -r -l address \n" );
	printf( " gpmc_testd -w1 -l address data \n");
	printf( " gpmc_testd -w -l address data \n");
}

struct cpsio_ioctl_arg arg;
struct cpsio_ioctl_arg arg_r;

int main(int argc, char *argv[]){
	int fd;
	unsigned long from,num,port;
	unsigned int adr, dat, dat2;
	int w_flg=0;
	int b_flg=0;
	int l_flg=0;
	int i = 0;

	if( argc<3 ){
		para_err();
		exit(1);
	} 
	if( memcmp( argv[1], "-w", 2 )==0 ){
		if( argc < 4 ){
			para_err();
			exit(1);
		}
		if( memcmp( argv[1], "-w1", 3 )==0 )b_flg=1;

		if( memcmp( argv[2], "-l", 2 ) == 0 ){
			l_flg=1;
		}

		sscanf( argv[2 + l_flg], "%x", &adr );
		sscanf( argv[3 + l_flg], "%x", &dat );
		w_flg=1;
	}else if( memcmp( argv[1], "-r", 2 )==0){
		if( memcmp( argv[1], "-r1", 3 )==0 )b_flg=1;

		if( memcmp( argv[2], "-l", 2 ) == 0 ){
			l_flg=1;
			sscanf( argv[4], "%x", &dat );
		}
		sscanf( argv[2 + l_flg], "%x", &adr );

	}else if( memcmp( argv[1], "-t", 2 )==0){
		if( memcmp( argv[1], "-t1", 3 )==0 )b_flg=1;
		sscanf( argv[2], "%x", &adr );
		w_flg=2;
		if( argc > 4 ){
			sscanf( argv[3] , "%x", &dat );
			sscanf( argv[4] , "%x", &dat2 );
		}else{
			dat = 0x55;
			dat2 = 0xaa;
		}
	}else{
		para_err();
		exit(1);
	}

	/* デバイスをopenする */
	fd=open(DEVNAME, O_RDWR);
	if(fd<=0){
		perror(DEVNAME);
		exit(1);
	}

	if( w_flg==1 ){
		arg.addr	= adr;
		arg.val		= dat;
		
		if( b_flg ){
			printf( "write adr[0x%x] = [%x]\n", adr, dat & 0xff  );
			ioctl( fd, IOCTL_CPSIO_OUTBYTE, &arg);	
		}else{
			printf( "write adr[0x%x] = [%x]\n", adr, dat & 0xffff  );
			ioctl( fd, IOCTL_CPSIO_OUTWORD, &arg);	
		}
	}else if( w_flg==2 ){

		arg.addr	= adr;
		for(i=0;;i++){
		  if( i%2 == 0 )	arg.val = dat;
		  else			arg.val = dat2;
		  if( b_flg ){
			ioctl( fd, IOCTL_CPSIO_OUTBYTE, &arg);	
		  }else{
			ioctl( fd, IOCTL_CPSIO_OUTWORD, &arg);	
		  }
		  // read back
		  arg_r.addr	= adr;
		  arg_r.val=0;
		  if( b_flg ){
			ioctl( fd, IOCTL_CPSIO_INPBYTE, &arg_r);
		  }else{
			ioctl( fd, IOCTL_CPSIO_INPWORD, &arg_r);
		  }
		  if( arg.val != arg_r.val ){
		     printf( "compare ng(%d)[%x]<>[%x]\n", i, arg.val, arg_r.val );
		     break;
		  }
		}

	}else{
		i = 1;
		do{
			arg.addr = adr;
			if( b_flg ){
				ioctl( fd, IOCTL_CPSIO_INPBYTE, &arg);
				if( l_flg == 0 ) printf( "read adr[0x%x] = [%x]\n", arg.addr,arg.val ); 
			}else{
				ioctl( fd, IOCTL_CPSIO_INPWORD, &arg);
				if( l_flg == 0 ) printf( "read adr[0x%x] = [%x]\n", arg.addr, arg.val ); 
			}
			if( l_flg ){
				if( dat != arg.val ){
					printf( "compare ng(%d)[%x]<>[%x]\n", i, dat, arg.val );
					break;
				}
				i ++;
			}
	
		}	while( l_flg );
	}


	/* デバイスを閉じる*/
	close(fd);
	return 0;
}

