/*
 * Linux COM Driver Sample Program -- Data Send
 * CONTEC Co.,Ltd.
 *
 * source file : comsend.c 
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>

#define BAUDRATE B38400
#define SERIAL_DATA_LENGTH 4097

main()
{
	int Send_Fd, Send_Res;
	char Send_Dev[50];
	char Send_Buf[SERIAL_DATA_LENGTH];
	struct termios Send_Newtio;
	int SendLen;

	system( "clear" ); 
	printf( "***************** Com Send Program *********************** \n\n" );
	printf( "\nInput the  Send Port Name : " );

	fgets( Send_Dev, 50, stdin );
	Send_Dev[strlen( Send_Dev )-1] = 0;

	Send_Fd = open( Send_Dev, O_RDWR | O_NOCTTY );			// Open device
	if ( Send_Fd < 0 ) { perror( Send_Dev ); exit( -1 ); }

	bzero( &Send_Newtio, sizeof( Send_Newtio ) );			// Clear new attribute
	Send_Newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;	// New attribute assigned
	Send_Newtio.c_iflag = IGNPAR | ICRNL;
	Send_Newtio.c_oflag = 0;
	Send_Newtio.c_lflag = ICANON;

	tcflush( Send_Fd, TCOFLUSH );							// Flush data writen

	if ( tcsetattr( Send_Fd, TCSANOW, &Send_Newtio ) != 0 )	// Set new attribute
	{
		printf( "tcsetattr Error! \n " );
		exit( -1 );
	}

	while( 1 )
	{
		printf( "Input Send Data:\n" );
		fgets( Send_Buf, SERIAL_DATA_LENGTH, stdin );
		if ( Send_Buf[0] == '\n' )
		{
			continue;
		}
		SendLen = strlen( Send_Buf );
		printf( "Now Sending '%s'+'CR', [%d] bytes ...\n", Send_Buf, SendLen );

		Send_Res = write( Send_Fd, Send_Buf, SendLen );		// Write data to device
		if ( Send_Res != -1 )
		{
			printf( "Sending is over. [%d] bytes.\n", SendLen );
		}
		else
		{
			perror( "write error!!" );
			exit( -1 );
		}
		if ( Send_Buf[0] == 'z' ) break;
	}
	close( Send_Fd );										// Close device
}
