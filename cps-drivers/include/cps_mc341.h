/*** cps_mc341.h ******************************/

/* SPI Data Format */

typedef union _cps_spi_command_frame{
	struct _cps_command_bit{
		unsigned char page:4;
		unsigned char address;
		unsigned char readwrite:1;
		unsigned char reserved:2;
		unsigned short value;
	}Bit;
	unsigned char command[2];
}CPS_SPI_COMMAND_FRAME, PCPS_SPI_COMMAND_FRAME;

typedef union __cps_spi_format{
	struct _cps_spi_command_value{
		CPS_SPI_COMMAND_FRAME Frame;
		unsigned char value[2];
	}COM_VAL;
	unsigned char array[4];
	unsigned long value;
}CPS_SPI_FORMAT, PCPS_SPI_FORMAT;

#define CPS_SPI_ACCESS_READ	(0x00)
#define CPS_SPI_ACCESS_WRITE	(0x01)

#define CPS_SPI_ACCESS_TYPE_BYTE	(0x00)
#define CPS_SPI_ACCESS_TYPE_WORD	(0x01)

#define CPS_SPI_SET_PACKET( page, address, readwrite, accesstype, reserved, value )	\
	( \
	( ( page & 0x0F ) << 28 ) | \
	( ( address & 0xFF ) << 20 ) | \
	( ( readwrite & 0x01 ) << 19 ) | \
	( ( accesstype & 0x01 ) << 18 ) | \
	( ( value & 0xFFFF ) ) \
	)

#define CPS_SPI_GET_PAGE_REGISTER ( packet ) \
	( (packet & 0xF0000000) >> 28 )

#define CPS_SPI_GET_ADDRESS_REGISTER ( packet ) \
	( (packet & 0x0FF00000) >> 20 )

#define CPS_SPI_GET_READWRITE_REGISTER ( packet ) \
	( (packet & 0x00080000) >> 19 )

#define CPS_SPI_GET_ACCESSTYPE_REGISTER ( packet ) \
	( (packet & 0x00040000) >> 18 )

#define CPS_SPI_GET_VALUE ( packet ) \
	( packet & 0x0000FFFF )

#include "cps_def.h"

/*********************************************************/
