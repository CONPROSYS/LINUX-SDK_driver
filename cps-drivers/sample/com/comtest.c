/*
 * Linux COM Driver Sample Program -- Data Diagnose
 * CONTEC Co.,Ltd.
 *
 * source file : comtest.c 
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/serial.h>
#include <asm/ioctls.h>
#include <stdlib.h>
#include <string.h>


#define BAUDRATE B38400
#define SERIAL_DATA_LENGTH 5000

main()
{
	char TestStr[SERIAL_DATA_LENGTH];
	int A_Fd, B_Fd;
	int A_Res, B_Res;
	char A_Dev[50], B_Dev[50];
	char A_Buf[SERIAL_DATA_LENGTH], B_Buf[SERIAL_DATA_LENGTH];
	struct termios A_Newtio, B_Newtio;
	int Leng;

	system( "clear" );
	printf( "***************** Com Diagnose Program *********************** \n\n" );

	printf( "\nInput the  One  Port Device Name : " );
	fgets( A_Dev, 50, stdin );
	A_Dev[strlen( A_Dev )-1] = 0;

	printf( "\nInput the Other Port Device Name : " );
	fgets( B_Dev, 50, stdin );
	B_Dev[strlen( B_Dev )-1] = 0;

	printf( "\nInput the Test Characters Which You want  : " );	
	fgets( TestStr, SERIAL_DATA_LENGTH, stdin );

	printf( "\n" );

	/*** Open <Port A> and <Port B> ***/
	A_Fd = open( A_Dev, O_RDWR | O_NOCTTY );			// Open device A
	B_Fd = open( B_Dev, O_RDWR | O_NOCTTY );			// Open device B

	if ( A_Fd < 0 || B_Fd  < 0 ) { printf( "Open fd Error ! Exit !\n " ); exit( -1 ); }

	/*** Set attribute <Port A> ***/
	bzero( &A_Newtio, sizeof( A_Newtio ) );				// Clear A's new attribute 
	A_Newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL| CREAD;	// Assign A's new attribute
	A_Newtio.c_iflag = IGNPAR | ICRNL;
	A_Newtio.c_oflag = 0;
	A_Newtio.c_lflag = ICANON;
	tcflush( A_Fd, TCIOFLUSH );							// Flush A's data
	tcsetattr( A_Fd, TCSANOW , &A_Newtio );				// Set A's new attribute

	/*** Set attribute <Port B> ***/
	bzero( &B_Newtio, sizeof( B_Newtio ) );				// Clear B's new attribute
	B_Newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;	// Assign B's new attribute
	B_Newtio.c_iflag = IGNPAR | ICRNL;
	B_Newtio.c_oflag = 0;
	B_Newtio.c_lflag = ICANON;
	tcflush( B_Fd, TCIOFLUSH );							// Flush B's data
	tcsetattr( B_Fd, TCSANOW, &B_Newtio );				// Set B's new attribute 

	/*** Communication Test <Port A> -> <Port B> ***/
	Leng = strlen( TestStr );
	if ( ( A_Res = write( A_Fd, TestStr, Leng ) ) == -1 )	// Write data to A device
	{
		printf( "A Write Return Error \n" );
		exit( -1 );
	}
	if ( ( B_Res = read( B_Fd, B_Buf, SERIAL_DATA_LENGTH ) ) == -1 )		// Read data from B device
	{
		printf( "B Read Return Error \n" );
		exit( -1 );
	}
	B_Buf[B_Res] = 0;
	printf( "%s Receive Test Characters Form %s \n%s ", B_Dev, A_Dev, B_Buf );
	if ( strcmp( TestStr, B_Buf ) == 0 )
	{
		printf( "Transmit result is Right ,OK ! \n" );
	}
	else
	{
		printf( "Transmit result is Wrong ,NG! \n" );
	}

	/*** Communication Test <Port A> -> <Port B> ***/
	Leng = strlen( B_Buf );
	if ( ( B_Res = write( B_Fd, B_Buf, Leng ) ) == -1 )		// Write data to B device
	{
		printf( "B Write Return Error \n" );
		exit( -1 );
	}
	if ( ( A_Res = read(A_Fd, A_Buf, SERIAL_DATA_LENGTH) ) == -1 )			// Read data from A device
	{
		printf( "A Read Return Error \n" );
		exit( -1 );
	}
	A_Buf[A_Res] = 0;
	printf( "%s Receive Test Characters Form %s \n%s ", A_Dev, B_Dev, A_Buf );
	if ( strcmp( B_Buf, A_Buf) == 0 )
	{
		printf( "Transmit result is Right ,OK ! \n" );
	}
	else
	{
		printf( "Transmit result is Wrong ,NG! \n" );
	}

	/*** Close <Port A> and <Port B> ***/
	close( A_Fd );								// Close A device
	close( B_Fd );								// Close B device
}
