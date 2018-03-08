/*
 * Linux COM Driver Sample Program -- Data Read
 * CONTEC Co.,Ltd.
 *
 * source file : comread.c 
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>


#define BAUDRATE B38400
#define SERIAL_DATA_LENGTH	4097
main()
{
	int Read_Fd, Read_Res;
	char Read_Dev[50];
	char Read_Buf[SERIAL_DATA_LENGTH];
	struct termios Read_Newtio;
	int ReadLen;

	system( "clear" );
	printf( "***************** Com Read Program *********************** \n\n" );
	printf( "\nInput the  Read  Port Name : " );
	fgets( Read_Dev, 50, stdin );
	Read_Dev[strlen( Read_Dev )-1] = 0;

	Read_Fd = open( Read_Dev, O_RDWR | O_NOCTTY );			// Open device
	if ( Read_Fd < 0 ) { perror( Read_Dev ); exit( -1 ); }

	bzero( &Read_Newtio,  sizeof( Read_Newtio ) );			// Clear new attribute
	Read_Newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;	// New attribute assigned
	Read_Newtio.c_iflag = IGNPAR | ICRNL;
	Read_Newtio.c_oflag = 0;
	Read_Newtio.c_lflag = ICANON;

	tcflush( Read_Fd, TCIFLUSH );							// Flush data received

	if ( tcsetattr( Read_Fd, TCSANOW, &Read_Newtio ) != 0 )	// Set new attribute
	{
		printf( "tcsetattr Error ! \n" );
		exit( -1 );
	}

	while ( 1 )
	{
		Read_Res = read( Read_Fd, Read_Buf, SERIAL_DATA_LENGTH );			// Read data from device

		if ( Read_Res == -1 )
		{
			printf( "Read ERROR ! \n" );
			exit( -1 ); 
		}
		Read_Buf[Read_Res] = 0;
		printf( ":%s:%d\n", Read_Buf, Read_Res );
		if ( Read_Buf[0] == 'z' ) break;
	}
	close( Read_Fd );										// Close device
}

