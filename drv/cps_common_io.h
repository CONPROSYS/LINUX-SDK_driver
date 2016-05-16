/*
 *  Header for cps-common-io.
 *
 *  Copyright (C) 2015 Syunsuke Okamoto.<okamoto@contec.jp>
 *
*/

#ifndef CPS_COMMON_IO_H
#define CPS_COMMON_IO_H

#include <linux/gpio.h>
#include <linux/ioctl.h>
#define CPS_FPGA_ACCESS_WORD		0
#define CPS_FPGA_ACCESS_BYTE_HIGH	1
#define CPS_FPGA_ACCESS_BYTE_LOW	2
#define CPS_FPGA_ACCESS_RESERVED	3

#ifndef GPIO_TO_PIN
  #define GPIO_TO_PIN( bank, gpio )	( 32 * (bank) + (gpio) )
#endif

#define CPS_FPGA_BYTE_LOW	GPIO_TO_PIN( 1, 31 )
#define CPS_FPGA_BYTE_HIGH	GPIO_TO_PIN( 2, 5 )

int cps_fpga_init(void)
{
	int ret;

	/* clear gpio free */
	gpio_free(CPS_FPGA_BYTE_LOW);
	gpio_free(CPS_FPGA_BYTE_HIGH);

	ret = gpio_request(CPS_FPGA_BYTE_LOW, "cps_fpga_byte_low");
	if( ret )	return ret;
	ret = gpio_request(CPS_FPGA_BYTE_HIGH, "cps_fpga_byte_high");

	return ret;

}


int cps_fpga_access( int mode )
{
	int ret = 0;

	switch( mode ){
	case CPS_FPGA_ACCESS_WORD : 
			ret = gpio_direction_output( CPS_FPGA_BYTE_LOW, 0 );
			ret = gpio_direction_output( CPS_FPGA_BYTE_HIGH, 0 );
			break;
	case CPS_FPGA_ACCESS_BYTE_LOW:
			ret = gpio_direction_output( CPS_FPGA_BYTE_LOW, 1 );
			ret = gpio_direction_output( CPS_FPGA_BYTE_HIGH, 0 );
			break;
	case CPS_FPGA_ACCESS_BYTE_HIGH:
			ret = gpio_direction_output( CPS_FPGA_BYTE_LOW, 0 );
			ret = gpio_direction_output( CPS_FPGA_BYTE_HIGH, 1 );
			break;
	}

	return ret;

}


/********************** Input / Output ****************************/
void cps_common_inpb(unsigned long addr, unsigned char *data )
{

	if( addr & 0x01 ) {
		cps_fpga_access(CPS_FPGA_ACCESS_BYTE_HIGH); // cps_common_io.h
	}else{
		cps_fpga_access(CPS_FPGA_ACCESS_BYTE_LOW); // cps_common_io.h
	}

	*data = readb( (unsigned char*) addr);

	cps_fpga_access(CPS_FPGA_ACCESS_WORD); // cps_common_io.h

}

void cps_common_outb(unsigned long addr, unsigned char data )
{
	if( ( addr ) & 0x01 ) {
		cps_fpga_access(CPS_FPGA_ACCESS_BYTE_HIGH); // cps_common_io.h
	}else{
		cps_fpga_access(CPS_FPGA_ACCESS_BYTE_LOW); // cps_common_io.h
	}

	writeb(data, (unsigned char*)addr);

	cps_fpga_access(CPS_FPGA_ACCESS_WORD); // cps_common_io.h

}

void cps_common_inpw(unsigned long addr, unsigned short *data )
{

	cps_fpga_access(CPS_FPGA_ACCESS_WORD); // cps_common_io.h

	*data = readw( (unsigned char*)addr);

}

void cps_common_outw(unsigned long addr, unsigned short data )
{

	cps_fpga_access(CPS_FPGA_ACCESS_WORD); // cps_common_io.h

	writew(data, (unsigned char*)addr);

}

/***********************************************************************/
/** Memory Allocate and Release                              **/
/***********************************************************************/

#define CPS_COMMON_MEM_NONREGION	0
#define CPS_COMMON_MEM_REGION			1


static void __iomem *cps_common_mem_alloc( unsigned long baseMemory, unsigned int areaSize, char* drvName , unsigned int isRegion )
{
	int ret = 0;
	void __iomem *mappedAddress;
	if( isRegion ){
		if (!request_mem_region((resource_size_t)baseMemory, areaSize, drvName)) {
			ret = -EBUSY;
		}else{
			mappedAddress = ioremap_nocache((resource_size_t)baseMemory, areaSize );
			if ( !mappedAddress ) {
				release_mem_region(baseMemory, areaSize);
				ret = -ENOMEM;
			}
		}
	}
	else{
		mappedAddress = ioremap_nocache((resource_size_t)baseMemory, areaSize );
		if ( !mappedAddress ) ret = -ENOMEM;
	}

	return mappedAddress;
}

static int cps_common_mem_release( unsigned long baseMemory, unsigned int areaSize, unsigned char __iomem *mappedAddress ,unsigned int isRegion)
{
	int ret = 0;

	if (!mappedAddress ) {
		if ( isRegion ){
			release_mem_region(baseMemory, areaSize);
		}
		iounmap(mappedAddress);
	}

	return ret;
}

#endif

