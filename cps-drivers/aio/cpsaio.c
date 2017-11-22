/*
 *  Driver for CPS-AIO analog I/O
 *
 *  Copyright (C) 2015 syunsuke okamoto <okamoto@contec.jp>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <asm/current.h>
#include <asm/uaccess.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <linux/device.h>
#include <linux/delay.h>

#include <linux/slab.h>

#include "../include/cps_common_io.h"
#include "../include/cps.h"
#include "../include/cps_ids.h"
#include "../include/cps_extfunc.h"

#define DRV_VERSION	"1.0.0"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CONTEC CONPROSYS Analog I/O driver");
MODULE_AUTHOR("syunsuke okamoto");
MODULE_VERSION(DRV_VERSION);
#include "../include/cpsaio.h"

#define CPSAIO_DRIVER_NAME "cpsaio"

typedef struct __cpsaio_driver_file{
	rwlock_t lock;
	unsigned ref;

	unsigned int node;
	unsigned long localAddr;
	unsigned char __iomem *baseAddr;
	CPSAIO_DEV_DATA data;

}CPSAIO_DRV_FILE,*PCPSAIO_DRV_FILE;

#if 0
#define DEBUG_CPSAIO_COMMAND(fmt...)	printk(fmt)
#else
#define DEBUG_CPSAIO_COMMAND(fmt...)	do { } while (0)
#endif

#if 0
#define DEBUG_CPSAIO_OPEN(fmt...)	printk(fmt)
#else
#define DEBUG_CPSAIO_OPEN(fmt...)	do { } while (0)
#endif

#if 0
#define DEBUG_CPSAIO_EEPROM(fmt...)	printk(fmt)
#else
#define DEBUG_CPSAIO_EEPROM(fmt...)	do { } while (0)
#endif

#if 1
#define DEBUG_CPSAIO_READFIFO(fmt...)	printk(fmt)
#else
#define DEBUG_CPSAIO_READFIFO(fmt...)	do { } while (0)
#endif


static int cpsaio_max_devs;			/*device count */
static int cpsaio_major = 0;
static int cpsaio_minor = 0; 

static struct cdev cpsaio_cdev;
static struct class *cpsaio_class = NULL;

static dev_t cpsaio_dev;

static const CPSAIO_DEV_DATA cps_aio_data[] = {
	{
		.Name = "CPS-AI-1608LI",
		.ProductNumber = CPS_DEVICE_AI_1608LI ,
		.Ability = CPS_AIO_ABILITY_ECU | CPS_AIO_ABILITY_AI | CPS_AIO_ABILITY_MEM,
		.ai = {
			.Resolution = 16,
			.Channel = 8,
		},
		.ao = {
			.Resolution = 0,
			.Channel = 0,
		},
/*
		.aiResolution = 16,
		.ai.Channel = 8,
		.aoResolution = 0,
		.ao.Channel = 0,
*/
	},
	{
		.Name = "CPS-AO-1604LI",
		.ProductNumber = CPS_DEVICE_AO_1604LI,
		.Ability = CPS_AIO_ABILITY_ECU | CPS_AIO_ABILITY_AO | CPS_AIO_ABILITY_MEM,
		.ai = {
			.Resolution = 0,
			.Channel = 0,
		},
		.ao = {
			.Resolution = 16,
			.Channel = 4,
		},
/*
		.aiResolution = 0,
		.ai.Channel = 0,
		.aoResolution = 16,
		.ao.Channel = 4,
*/
	},
	{
		.Name = "",
		.ProductNumber = -1,
		.Ability = -1,
		.ai = {
			.Resolution = -1,
			.Channel = -1,
		},
		.ao = {
			.Resolution = -1,
			.Channel = -1,
		},
/*
		.aiResolution = -1,
		.ai.Channel = -1,
		.aoResolution = -1,
		.ao.Channel = -1,
*/
	},
};

#include "cpsaio_devdata.h"

// DAC161S055 0-FF not enough values.(CPS-AO-1604LI only)
static unsigned short calc_DAC161S055( unsigned short dat ){
	double dblDat;
	unsigned short tmpDat;

	dblDat = (double)( dat * 0.996094 + 256.0);
	tmpDat = ( unsigned short ) dblDat;

	//round ( up )
	if( (double)( dblDat - tmpDat ) > 0.5 ){
		tmpDat += 1;
	}

	return tmpDat;

}

/**
	@function __cpsaio_find_analog_device
	@param node : device node
	@return true : product id, false : -1
**/ 
int __cpsaio_find_analog_device( int node )
{
	int cnt;
	int product_id;

	product_id = contec_mcs341_device_productid_get( node );
	cnt = 0;
	do{
		if( cps_aio_data[cnt].ProductNumber == -1 ) return -1;
		if( cps_aio_data[cnt].ProductNumber == product_id )	break;
		cnt++;
	}while( 1 );
	return product_id;
}


/**
	@function cpsaio_read_ai_data
	@param BaseAddr : base address
	@param val : value
	@return true : 0
**/ 
static long cpsaio_read_ai_data( unsigned long BaseAddr, unsigned short *val )
{
	cps_common_inpw( (unsigned long)( BaseAddr + OFFSET_AIDATA_CPS_AIO ) , val );
	DEBUG_CPSAIO_COMMAND(KERN_INFO"[%lx]=%x\n",(unsigned long)( BaseAddr + OFFSET_AIDATA_CPS_AIO ), *val );
	return 0;
}

/**
	@function cpsaio_read_ao_data
	@param BaseAddr : base address
	@param val : value
	@return true : 0
**/ 
static long cpsaio_write_ao_data( unsigned long BaseAddr, unsigned short val )
{
	DEBUG_CPSAIO_COMMAND(KERN_INFO"[%lx]=%x\n",(unsigned long)( BaseAddr + OFFSET_AODATA_CPS_AIO ), val );
	cps_common_outw( (unsigned long)( BaseAddr + OFFSET_AODATA_CPS_AIO ) , val );
	return 0;
}

/**
	@function cpsaio_read_ai_status
	@param BaseAddr : base address
	@param wStatus : status
	@return true : 0
**/ 
static long cpsaio_read_ai_status( unsigned long BaseAddr, unsigned short *wStatus )
{
	cps_common_inpw( (unsigned long)( BaseAddr + OFFSET_AISTATUS_CPS_AIO ) , wStatus );
	DEBUG_CPSAIO_COMMAND(KERN_INFO"[%lx]=%x\n",(unsigned long)( BaseAddr + OFFSET_AISTATUS_CPS_AIO ), *wStatus );
	return 0;
}

/**
	@function cpsaio_read_ao_status
	@param BaseAddr : base address
	@param wStatus : status
	@return true : 0
**/ 
static long cpsaio_read_ao_status( unsigned long BaseAddr, unsigned short *wStatus )
{
	cps_common_inpw( (unsigned long)( BaseAddr + OFFSET_AOSTATUS_CPS_AIO ) , wStatus );
	DEBUG_CPSAIO_COMMAND(KERN_INFO"[%lx]=%x\n",(unsigned long)( BaseAddr + OFFSET_AOSTATUS_CPS_AIO ), *wStatus );
	return 0;
}

/**
	@function cpsaio_read_mem_status
	@param BaseAddr : base address
	@param wStatus : status
	@return true : 0
**/ 
static long cpsaio_read_mem_status( unsigned long BaseAddr, unsigned short *wStatus )
{
	cps_common_inpw( (unsigned long)( BaseAddr + OFFSET_MEMSTATUS_CPS_AIO ) , wStatus );
	DEBUG_CPSAIO_COMMAND(KERN_INFO"[%lx]=%x\n",(unsigned long)( BaseAddr + OFFSET_MEMSTATUS_CPS_AIO ), *wStatus );
	return 0;
}


/**
	@function cpsaio_command
	@param BaseAddr : base address
	@param isReadWrite : write, read, or call flag
	@param isEcu	: ecu or command flag
	@param size : size 1 ,2 , or 4.
	@param wCommand : command address
	@param vData : data
	@return true : 0
**/ 
static long cpsaio_command( unsigned long BaseAddr, unsigned char isReadWrite , unsigned int isEcu, unsigned int size, unsigned short wCommand , void *vData )
{
	unsigned long com_addr, dat_addr_l, dat_addr_u;
	unsigned short data_l, data_u; 
	unsigned short *tmpWordData;
	unsigned long *tmpDWordData;

//	if( (wCommand & CPS_AIO_COMMAND_MASK) == CPS_AIO_COMMAND_BASE_ECU ){
	if( isEcu ){
		com_addr = (unsigned long)(BaseAddr + OFFSET_ECU_COMMAND_LOWER_CPS_AIO);
		dat_addr_l = (unsigned long)(BaseAddr + OFFSET_ECU_DATA_LOWER_CPS_AIO);
		dat_addr_u = (unsigned long)(BaseAddr + OFFSET_ECU_DATA_UPPER_CPS_AIO);
	}else{
		com_addr = (unsigned long)(BaseAddr + OFFSET_EXT_COMMAND_LOWER_CPS_AIO);
		dat_addr_l = (unsigned long)(BaseAddr + OFFSET_EXT_DATA_LOWER_CPS_AIO);
		dat_addr_u = (unsigned long)(BaseAddr + OFFSET_EXT_DATA_UPPER_CPS_AIO);
	}


	/* command */
	cps_common_outw( com_addr , wCommand );
	DEBUG_CPSAIO_COMMAND(KERN_INFO"[%lx]=%x\n",com_addr, wCommand );
	/* data */
	switch( isReadWrite ){
	case CPS_AIO_COMMAND_READ :
		DEBUG_CPSAIO_COMMAND(KERN_INFO"<READ>\n");
		cps_common_inpw(  dat_addr_l , &data_l  );
		if( size == 4 ) cps_common_inpw(  dat_addr_u , &data_u  );
		else data_u = 0;



		switch( size ){
		case 2:
			tmpWordData = (unsigned short *) vData;
			*tmpWordData = data_l;
			DEBUG_CPSAIO_COMMAND(KERN_INFO"[%lx]=%04x\n",dat_addr_l, data_l);
			break;
		case 4:
			tmpDWordData = (unsigned long *) vData;
			*tmpDWordData = (unsigned long ) ( data_u << 16 | data_l );
			DEBUG_CPSAIO_COMMAND(KERN_INFO"[%lx]=%04x\n",dat_addr_l, data_l);
			DEBUG_CPSAIO_COMMAND(KERN_INFO"[%lx]=%04x\n", dat_addr_u, data_u);
			break;
	}	
	break;	
	case CPS_AIO_COMMAND_WRITE :
		DEBUG_CPSAIO_COMMAND(KERN_INFO"<WRITE>\n");
		switch( size ){
		case 2:
			tmpWordData = (unsigned short *) vData;
			data_l = *tmpWordData;
			data_u = 0;
			break;
		case 4:
			tmpDWordData = (unsigned long *) vData;
			data_l = (*tmpDWordData & 0x0000FFFF );
			data_u = ( (*tmpDWordData & 0xFFFF0000 )>> 16) ;
			break;
		}

		cps_common_outw( dat_addr_l , data_l );
		DEBUG_CPSAIO_COMMAND(KERN_INFO"[%lx]=%04x\n",dat_addr_l, data_l);

		if( size == 4 ){
			cps_common_outw( dat_addr_u , data_u );
			DEBUG_CPSAIO_COMMAND(KERN_INFO"[%lx]=%04x\n", dat_addr_u, data_u);
		}
		break;
	}
	return 0;
}

#define CPSAIO_COMMAND_ECU_INIT( addr ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_CALL, CPS_AIO_ADDRESS_MODE_ECU, 0, CPS_AIO_COMMAND_ECU_INIT, NULL )
#define CPSAIO_COMMAND_ECU_SETINPUTSIGNAL( addr, data ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_WRITE, CPS_AIO_ADDRESS_MODE_ECU, 4, CPS_AIO_COMMAND_ECU_SETINPUTSIGNAL, data )
#define CPSAIO_COMMAND_ECU_OUTPULSE0( addr )	\
	cpsaio_command( addr, CPS_AIO_COMMAND_CALL, CPS_AIO_ADDRESS_MODE_ECU, 0, CPS_AIO_COMMAND_ECU_GENERAL_PULSE_COM0, NULL )
#define CPSAIO_COMMAND_ECU_AI_GET_INTERRUPT_FLAG( addr, data ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_READ, CPS_AIO_ADDRESS_MODE_ECU, 2, CPS_AIO_COMMAND_ECU_AI_INTERRUPT_FLAG, data )
#define CPSAIO_COMMAND_ECU_AI_SET_INTERRUPT_FLAG( addr, data ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_WRITE, CPS_AIO_ADDRESS_MODE_ECU, 2, CPS_AIO_COMMAND_ECU_AI_INTERRUPT_FLAG, data )
#define CPSAIO_COMMAND_ECU_AO_GET_INTERRUPT_FLAG( addr, data ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_READ, CPS_AIO_ADDRESS_MODE_ECU, 2, CPS_AIO_COMMAND_ECU_AO_INTERRUPT_FLAG, data )
#define CPSAIO_COMMAND_ECU_AO_SET_INTERRUPT_FLAG( addr, data ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_WRITE, CPS_AIO_ADDRESS_MODE_ECU, 2, CPS_AIO_COMMAND_ECU_AO_INTERRUPT_FLAG, data )


#define CPSAIO_COMMAND_AI_SINGLEMULTI( addr, data ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_WRITE, CPS_AIO_ADDRESS_MODE_COMMAND, 2, CPS_AIO_COMMAND_AI_SET_EXCHANGE, data )
#define CPSAIO_COMMAND_AI_SETCHANNEL( addr, data ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_WRITE, CPS_AIO_ADDRESS_MODE_COMMAND, 2, CPS_AIO_COMMAND_AI_SET_CHANNELS, data )
#define CPSAIO_COMMAND_AI_INIT( addr ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_CALL, CPS_AIO_ADDRESS_MODE_COMMAND, 0, CPS_AIO_COMMAND_AI_INIT, NULL )
#define CPSAIO_COMMAND_AI_OPEN( addr ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_CALL, CPS_AIO_ADDRESS_MODE_COMMAND, 0, CPS_AIO_COMMAND_AI_GATE_OPEN, NULL )
#define CPSAIO_COMMAND_AI_STOP( addr ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_CALL, CPS_AIO_ADDRESS_MODE_COMMAND, 0, CPS_AIO_COMMAND_AI_FORCE_STOP, NULL )
#define CPSAIO_COMMAND_AI_SETCLOCK( addr, data ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_WRITE, CPS_AIO_ADDRESS_MODE_COMMAND, 4, CPS_AIO_COMMAND_AI_SET_INTERNAL_CLOCK, data )
#define CPSAIO_COMMAND_AI_SETSAMPNUM( addr, data ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_WRITE, CPS_AIO_ADDRESS_MODE_COMMAND, 4, CPS_AIO_COMMAND_AI_SET_BEFORE_TRIGGER_SAMPLING_NUMBER, data )
#define CPSAIO_COMMAND_AI_SET_CALIBRATION( addr, data ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_WRITE, CPS_AIO_ADDRESS_MODE_COMMAND, 4, CPS_AIO_COMMAND_AI_SET_CALIBRATION_TERMS, data )
#define CPSAIO_COMMAND_AI_GET_CALIBRATION( addr, data ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_READ, CPS_AIO_ADDRESS_MODE_COMMAND, 4, CPS_AIO_COMMAND_AI_SET_CALIBRATION_TERMS, data )

#define CPSAIO_COMMAND_AO_SINGLEMULTI( addr, data ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_WRITE, CPS_AIO_ADDRESS_MODE_COMMAND, 2, CPS_AIO_COMMAND_AO_SET_EXCHANGE, data )
#define CPSAIO_COMMAND_AO_SETCHANNEL( addr, data ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_WRITE, CPS_AIO_ADDRESS_MODE_COMMAND, 2, CPS_AIO_COMMAND_AO_SET_CHANNELS, data )
#define CPSAIO_COMMAND_AO_INIT( addr ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_CALL, CPS_AIO_ADDRESS_MODE_COMMAND, 0, CPS_AIO_COMMAND_AO_INIT, NULL )
#define CPSAIO_COMMAND_AO_OPEN( addr ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_CALL, CPS_AIO_ADDRESS_MODE_COMMAND, 0, CPS_AIO_COMMAND_AO_GATE_OPEN, NULL )
#define CPSAIO_COMMAND_AO_STOP( addr ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_CALL, CPS_AIO_ADDRESS_MODE_COMMAND, 0, CPS_AIO_COMMAND_AO_FORCE_STOP, NULL )
#define CPSAIO_COMMAND_AO_SETCLOCK( addr, data ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_WRITE, CPS_AIO_ADDRESS_MODE_COMMAND, 4, CPS_AIO_COMMAND_AO_SET_INTERNAL_CLOCK, data )
#define CPSAIO_COMMAND_AO_SETSAMPNUM( addr, data ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_WRITE, CPS_AIO_ADDRESS_MODE_COMMAND, 4, CPS_AIO_COMMAND_AO_SET_BEFORE_TRIGGER_SAMPLING_NUMBER, data )
#define CPSAIO_COMMAND_AO_SET_CALIBRATION( addr, data ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_WRITE, CPS_AIO_ADDRESS_MODE_COMMAND, 4, CPS_AIO_COMMAND_AO_SET_CALIBRATION_TERMS, data )
#define CPSAIO_COMMAND_AO_GET_CALIBRATION( addr, data ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_READ, CPS_AIO_ADDRESS_MODE_COMMAND, 4, CPS_AIO_COMMAND_AO_SET_CALIBRATION_TERMS, data )


#define CPSAIO_COMMAND_MEM_INIT( addr ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_CALL, CPS_AIO_ADDRESS_MODE_COMMAND, 0, CPS_AIO_COMMAND_MEM_INIT, NULL )

/**
	@function cpsaio_read_eeprom
	@param dev : device number
	@param cate : device category ( AI or AO )
	@param num : number 
	@param val : value
	@return true : 0
**/ 
unsigned long cpsaio_read_eeprom( unsigned int dev, unsigned int cate, unsigned int num, unsigned short *val )
{

	contec_mcs341_device_extension_value_get(dev, cate,  num );
	contec_mcs341_device_extension_value_get(dev, cate,  num );
	*val = contec_mcs341_device_extension_value_get(dev, cate,  num );

	return 0;
}

/**
	@function cpsaio_write_eeprom
	@param dev : device number
	@param cate : device category ( AI or AO )
	@param num : number 
	@param val : value
	@return true : 0
**/ 
unsigned long cpssio_write_eeprom(unsigned int dev, unsigned int cate, unsigned int num, unsigned short val )
{
	unsigned short chkVal;

	unsigned int count = 0;

	do{
		contec_mcs341_device_extension_value_set(dev , cate, num, val);
		cpsaio_read_eeprom( dev, cate, num, &chkVal );
//		chkVal = contec_mcs341_device_extension_value_get(dev ,cate,  num );
		contec_cps_micro_delay_sleep( 1 );
		if( count > 5 ){
			return 1;
		}
		else count++;

		DEBUG_CPSAIO_EEPROM(KERN_INFO" %d, %x :: %x \n", dev, val , chkVal );  

	}while( chkVal != val );
	return 0;
}

/**
	@function cpsaio_clear_eeprom
	@param dev : device number
	@return true : 0
**/ 
void cpsaio_clear_eeprom( unsigned int dev )
{
	contec_mcs341_device_extension_value_all_clear(dev , 0 );
}

/**
	@function cpsaio_clear_fpga_extension_reg
	@param dev : device number
	@return true : 0
**/ 
void cpsaio_clear_fpga_extension_reg(unsigned int dev, unsigned int cate, unsigned int num )
{
	unsigned short val = 0;
	unsigned int cnt;
	
	/* fpga-reg zero overwrite */
	for( cnt = 0; cnt < num; cnt ++ ){
		contec_mcs341_device_extension_value_set(dev , cate, cnt, val);
	}

}



/***** Interrupt function *******************************/
static const int AM335X_IRQ_NMI=7;

irqreturn_t cpsaio_isr_func(int irq, void *dev_instance){

	unsigned short wStatus;

	PCPSAIO_DRV_FILE dev =(PCPSAIO_DRV_FILE) dev_instance;
	
	if( !dev ) return IRQ_NONE;

	if( contec_mcs341_device_IsCategory( dev->node , CPS_CATEGORY_AIO ) ){ 
	
	}
	else return IRQ_NONE;
	

	if(printk_ratelimit()){
		printk("cpsaio Device Number:%d IRQ interrupt !\n",( dev->node ) );
	}

	return IRQ_HANDLED;
}


/***** file operation functions *******************************/

long cpsaio_ioctl_ecu(PCPSAIO_DRV_FILE dev, unsigned int cmd, unsigned long arg )
{
	unsigned short valw = 0;
	unsigned long valdw = 0;
	struct cpsaio_ioctl_arg ioc;

	switch( cmd ){
/* ECU */
		case IOCTL_CPSAIO_SETECU_SIGNAL:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					write_lock(&dev->lock);
					valdw = (unsigned long) ioc.val;
					CPSAIO_COMMAND_ECU_SETINPUTSIGNAL( (unsigned long)dev->baseAddr , &valdw );
					write_unlock(&dev->lock);

					break;

		case IOCTL_CPSAIO_SET_OUTPULSE0:
					CPSAIO_COMMAND_ECU_OUTPULSE0( (unsigned long)dev->baseAddr);
					break;

		case IOCTL_CPSAIO_GET_INTERRUPT_FLAG_AI:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					CPSAIO_COMMAND_ECU_AI_GET_INTERRUPT_FLAG((unsigned long)dev->baseAddr , &valw );
					ioc.val = (unsigned long) valw;
					read_unlock(&dev->lock);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSAIO_GET_INTERRUPT_FLAG_AO:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					CPSAIO_COMMAND_ECU_AO_GET_INTERRUPT_FLAG((unsigned long)dev->baseAddr , &valw );
					ioc.val = (unsigned long) valw;
					read_unlock(&dev->lock);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;
		}
		return 0;
}

long cpsaio_ioctl_ai(PCPSAIO_DRV_FILE dev, unsigned int cmd, unsigned long arg )
{
	unsigned short valw = 0;
	unsigned long valdw = 0;
	struct cpsaio_ioctl_arg ioc;

	switch( cmd ){
/* Analog Input */
		case IOCTL_CPSAIO_INDATA:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					cpsaio_read_ai_data((unsigned long)dev->baseAddr , &valw );
					ioc.val = (unsigned long) valw;
					read_unlock(&dev->lock);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;
/*
		case IOCTL_CPSAIO_GETSAMPLINGDATA_AI:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &datBuf, (int __user *)arg, sizeof(datBuf) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					for( cnt = 0; cnt < datBuf.num; cnt ++ ){
						
						datBuf.val[cnt] = (unsigned long)valw;
					}
					read_unlock(&dev->lock);

					if( copy_to_user( (int __user *)arg, &datBuf, sizeof(datBuf) ) ){
						return -EFAULT;
					}
					break;
*/
		case IOCTL_CPSAIO_INSTATUS:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					cpsaio_read_ai_status((unsigned long)dev->baseAddr , &valw );
					ioc.val = (unsigned long) valw;
					read_unlock(&dev->lock);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSAIO_START_AI:
					CPSAIO_COMMAND_AI_OPEN( (unsigned long)dev->baseAddr );
					break;
		case IOCTL_CPSAIO_STOP_AI:
					CPSAIO_COMMAND_AI_STOP( (unsigned long)dev->baseAddr );
					break;

		case IOCTL_CPSAIO_SETCHANNEL_AI:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					write_lock(&dev->lock);
					valw = (unsigned short) ioc.val;
					CPSAIO_COMMAND_AI_SETCHANNEL( (unsigned long)dev->baseAddr , &valw );
					write_unlock(&dev->lock);

					break;

		case IOCTL_CPSAIO_SETTRANSFER_MODE_AI:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					write_lock(&dev->lock);
					valw = (unsigned short) ioc.val;
					CPSAIO_COMMAND_AI_SINGLEMULTI( (unsigned long)dev->baseAddr , &valw );
					write_unlock(&dev->lock);

					break;

		case IOCTL_CPSAIO_SET_CLOCK_AI:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					write_lock(&dev->lock);
					valdw = (unsigned long) ioc.val;
					CPSAIO_COMMAND_AI_SETCLOCK( (unsigned long)dev->baseAddr, &valdw );
					write_unlock(&dev->lock);

					break;
		case IOCTL_CPSAIO_SET_SAMPNUM_AI:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					write_lock(&dev->lock);
					valdw = (unsigned long) ioc.val;
					CPSAIO_COMMAND_AI_SETSAMPNUM( (unsigned long)dev->baseAddr, &valdw );
					write_unlock(&dev->lock);

					break;

		case IOCTL_CPSAIO_SET_CALIBRATION_AI:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					write_lock(&dev->lock);
					valdw = (unsigned long) ioc.val;
					CPSAIO_COMMAND_AI_SET_CALIBRATION( (unsigned long)dev->baseAddr, &valdw );
					write_unlock(&dev->lock);

					do{
						udelay( 1 );
						cpsaio_read_ai_status( (unsigned long)dev->baseAddr, &valw );
					}while( valw & CPS_AIO_AI_STATUS_CALIBRATION_BUSY );

					break;

		case IOCTL_CPSAIO_GET_CALIBRATION_AI:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					CPSAIO_COMMAND_AI_GET_CALIBRATION( (unsigned long)dev->baseAddr, &valdw );
					ioc.val = valdw;
					read_unlock(&dev->lock);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;
		}

		return 0;
}

long cpsaio_ioctl_ao(PCPSAIO_DRV_FILE dev, unsigned int cmd, unsigned long arg )
{
	unsigned short valw = 0;
	unsigned long valdw = 0;
	struct cpsaio_ioctl_arg ioc;

	switch( cmd ){

/* Analog Output */
		case IOCTL_CPSAIO_OUTDATA:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					write_lock(&dev->lock);
					valw = (unsigned short) ioc.val;

					if( dev->data.ProductNumber == CPS_DEVICE_AO_1604LI )
						valw = calc_DAC161S055( valw );

					cpsaio_write_ao_data( (unsigned long)dev->baseAddr , valw );
					write_unlock(&dev->lock);

					break;

		case IOCTL_CPSAIO_OUTSTATUS:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					cpsaio_read_ao_status( (unsigned long)dev->baseAddr , &valw );
					ioc.val = (unsigned int) valw;
					read_unlock(&dev->lock);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSAIO_SETCHANNEL_AO:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					write_lock(&dev->lock);
					valw = (unsigned short) ioc.val;
					CPSAIO_COMMAND_AO_SETCHANNEL( (unsigned long)dev->baseAddr , &valw );
					write_unlock(&dev->lock);

					break;

		case IOCTL_CPSAIO_SET_CLOCK_AO:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					write_lock(&dev->lock);
					valdw = (unsigned long) ioc.val;
					CPSAIO_COMMAND_AO_SETCLOCK( (unsigned long)dev->baseAddr, &valdw );
					write_unlock(&dev->lock);

					break;

		case IOCTL_CPSAIO_SETTRANSFER_MODE_AO:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					write_lock(&dev->lock);
					valw = (unsigned short) ioc.val;
					CPSAIO_COMMAND_AO_SINGLEMULTI( (unsigned long)dev->baseAddr , &valw );
					write_unlock(&dev->lock);

					break;
		case IOCTL_CPSAIO_START_AO:
					CPSAIO_COMMAND_AO_OPEN( (unsigned long)dev->baseAddr );
					break;
		case IOCTL_CPSAIO_STOP_AO:
					CPSAIO_COMMAND_AO_STOP( (unsigned long)dev->baseAddr );
					break;
		case IOCTL_CPSAIO_SET_SAMPNUM_AO:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					write_lock(&dev->lock);
					valdw = (unsigned long) ioc.val;
					CPSAIO_COMMAND_AO_SETSAMPNUM( (unsigned long)dev->baseAddr, &valdw );
					write_unlock(&dev->lock);

					break;
	
		case IOCTL_CPSAIO_SET_CALIBRATION_AO:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					write_lock(&dev->lock);
					valdw = (unsigned long) ioc.val;
					CPSAIO_COMMAND_AO_SET_CALIBRATION( (unsigned long)dev->baseAddr, &valdw );
					write_unlock(&dev->lock);

					do{
						udelay( 1 );
						cpsaio_read_ao_status( (unsigned long)dev->baseAddr, &valw );
					}while( valw & CPS_AIO_AO_STATUS_CALIBRATION_BUSY );

					break;

		case IOCTL_CPSAIO_GET_CALIBRATION_AO:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					CPSAIO_COMMAND_AO_GET_CALIBRATION( (unsigned long)dev->baseAddr, &valdw );
					ioc.val = valdw;
					read_unlock(&dev->lock);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		}
		return 0;
}

static long cpsaio_ioctl( struct file *filp, unsigned int cmd, unsigned long arg )
{
	PCPSAIO_DRV_FILE dev = filp->private_data;
	unsigned short valw = 0;
	unsigned long valdw = 0;
	struct cpsaio_ioctl_arg ioc;
	struct cpsaio_direct_arg d_ioc;
	struct cpsaio_direct_command_arg dc_ioc;
	unsigned int num = 0;
	long lRet;
	
	unsigned int abi ;

	memset( &ioc, 0 , sizeof(ioc) );
	memset( &d_ioc, 0, sizeof(d_ioc) );
	memset( &dc_ioc, 0, sizeof(dc_ioc) );

	write_lock(&dev->lock);
	abi = dev->data.Ability;
	write_unlock(&dev->lock);

/* Hardware Ability IOCTL's */

	if( abi & CPS_AIO_ABILITY_ECU ){
		lRet = cpsaio_ioctl_ecu(dev, cmd, arg );
		if( lRet != 0 ) return lRet;
	}

	if( abi & CPS_AIO_ABILITY_AI ){
		lRet = cpsaio_ioctl_ai(dev, cmd, arg );
		if( lRet != 0 ) return lRet;
	}

	if( abi & CPS_AIO_ABILITY_AO ){
		lRet = cpsaio_ioctl_ao(dev, cmd, arg );
		if( lRet != 0 ) return lRet;
	}

	switch( cmd ){

/* Analog Common */
		case IOCTL_CPSAIO_INIT:
					CPSAIO_COMMAND_ECU_INIT( (unsigned long)dev->baseAddr );
					if( dev->data.ai.Channel > 0 )
						CPSAIO_COMMAND_AI_INIT( (unsigned long)dev->baseAddr );
					if( dev->data.ao.Channel > 0 )
						CPSAIO_COMMAND_AO_INIT( (unsigned long)dev->baseAddr );
					CPSAIO_COMMAND_MEM_INIT( (unsigned long)dev->baseAddr );
					break;

		case IOCTL_CPSAIO_EXIT:
					if( dev->data.ai.Channel > 0 )
						CPSAIO_COMMAND_AI_STOP( (unsigned long)dev->baseAddr );
					if( dev->data.ao.Channel > 0 )
						CPSAIO_COMMAND_AO_STOP( (unsigned long)dev->baseAddr );
					break;

		case IOCTL_CPSAIO_GETRESOLUTION:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					switch( ioc.inout ){
					case CPS_AIO_INOUT_AI :
						ioc.val = dev->data.ai.Resolution	;
						break;
					case CPS_AIO_INOUT_AO:
						ioc.val = dev->data.ao.Resolution	;
						break;
					}
					read_unlock(&dev->lock);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;			

		case IOCTL_CPSAIO_GETMAXCHANNEL:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					switch( ioc.inout ){
					case CPS_AIO_INOUT_AI :				
						ioc.val = dev->data.ai.Channel	;
						break;
					case CPS_AIO_INOUT_AO:
						ioc.val = dev->data.ao.Channel	;
						break;
					}
					read_unlock(&dev->lock);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;	

	
/****  EEPROM I/O Command *****/ 
		case IOCTL_CPSAIO_WRITE_EEPROM_AI:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}

					write_lock(&dev->lock);
					valw = (unsigned short) ioc.val;

					switch( dev->data.ProductNumber ){
					case CPS_DEVICE_AI_1608LI: num = 0;break;
					default :
						write_unlock(&dev->lock);
						return -EFAULT;
					}

					cpssio_write_eeprom( dev->node , CPS_DEVICE_COMMON_ROM_WRITE_PAGE_AI, num,  valw );
					write_unlock(&dev->lock);

					DEBUG_CPSAIO_EEPROM(KERN_INFO"EEPROM-WRITE:[%lx]=%x\n",(unsigned long)( dev->baseAddr ), valw );
					break;

		case IOCTL_CPSAIO_READ_EEPROM_AI:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					switch( dev->data.ProductNumber ){
					case CPS_DEVICE_AI_1608LI: num = 0;break;
					default :
						read_unlock(&dev->lock);
						return -EFAULT;
					}
					cpsaio_read_eeprom( dev->node ,CPS_DEVICE_COMMON_ROM_WRITE_PAGE_AI, num, &valw );
					ioc.val = (unsigned long) valw;
					read_unlock(&dev->lock);

					DEBUG_CPSAIO_EEPROM(KERN_INFO"EEPROM-READ:[%lx]=%x\n",(unsigned long)( dev->baseAddr ), valw );
				
					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}

					break;

		case IOCTL_CPSAIO_WRITE_EEPROM_AO:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}

					write_lock(&dev->lock);
					
					valw = (unsigned short) ioc.val;

					switch( dev->data.ProductNumber ){
					case CPS_DEVICE_AO_1604LI: num = (unsigned short) ioc.ch;break;
					default :
						write_unlock(&dev->lock);
						return -EFAULT;
					}

					cpssio_write_eeprom( dev->node , CPS_DEVICE_COMMON_ROM_WRITE_PAGE_AO, num,  valw );
					write_unlock(&dev->lock);

					DEBUG_CPSAIO_EEPROM(KERN_INFO"EEPROM-WRITE:[%lx]=%x\n",(unsigned long)( dev->baseAddr ), valw );
					break;

		case IOCTL_CPSAIO_READ_EEPROM_AO:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					switch( dev->data.ProductNumber ){
					case CPS_DEVICE_AO_1604LI: num = (unsigned short) ioc.ch;break;
					default :
						read_unlock(&dev->lock);
						return -EFAULT;
					}
					cpsaio_read_eeprom( dev->node ,CPS_DEVICE_COMMON_ROM_WRITE_PAGE_AO, num, &valw );
					ioc.val = (unsigned long) valw;
					read_unlock(&dev->lock);

					DEBUG_CPSAIO_EEPROM(KERN_INFO"EEPROM-READ:[%lx]=%x\n",(unsigned long)( dev->baseAddr ), valw );
				
					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}

					break;

		case IOCTL_CPSAIO_CLEAR_EEPROM:

					read_lock(&dev->lock);

					switch( dev->data.ProductNumber ){
					case CPS_DEVICE_AI_1608LI: num = 1;break;
					case CPS_DEVICE_AO_1604LI: num = dev->data.ao.Channel;break;
					}

					if( abi & CPS_AIO_ABILITY_AI )
						cpsaio_clear_fpga_extension_reg(dev->node ,CPS_DEVICE_COMMON_ROM_WRITE_PAGE_AI, num );
					if( abi & CPS_AIO_ABILITY_AO )
						cpsaio_clear_fpga_extension_reg(dev->node ,CPS_DEVICE_COMMON_ROM_WRITE_PAGE_AO, num );

					cpsaio_clear_eeprom( dev->node );
					read_unlock(&dev->lock);

					DEBUG_CPSAIO_EEPROM(KERN_INFO"EEPROM-CLEAR\n");
					break;

/****  Direct I/O Command *****/ 
		case IOCTL_CPSAIO_DIRECT_OUTPUT:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &d_ioc, (int __user *)arg, sizeof(d_ioc) ) ){
						return -EFAULT;
					}
					write_lock(&dev->lock);
					valw = (unsigned short) d_ioc.val;
					DEBUG_CPSAIO_COMMAND(KERN_INFO"DIRECT OUTPUT: [%lx]=%x\n",(unsigned long)( dev->baseAddr + d_ioc.addr ), valw );
					cps_common_outw( (unsigned long)( dev->baseAddr + d_ioc.addr ) , valw );
					write_unlock(&dev->lock);

					break;

		case IOCTL_CPSAIO_DIRECT_INPUT:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &d_ioc, (int __user *)arg, sizeof(d_ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					
					cps_common_inpw( (unsigned long)( dev->baseAddr + d_ioc.addr ) , &valw );
					DEBUG_CPSAIO_COMMAND(KERN_INFO"DIRECT INPUT:[%lx]=%x\n",(unsigned long)( dev->baseAddr + d_ioc.addr ), valw );
					d_ioc.val = (unsigned long) valw;

					read_unlock(&dev->lock);

					if( copy_to_user( (int __user *)arg, &d_ioc, sizeof(d_ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSAIO_DIRECT_COMMAND_OUTPUT:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &dc_ioc, (int __user *)arg, sizeof(dc_ioc) ) ){
						return -EFAULT;
					}
					write_lock(&dev->lock);
					if( dc_ioc.size == 2 ){
						valw = (unsigned short) dc_ioc.val;
						cpsaio_command( (unsigned long)( dev->baseAddr ) ,CPS_AIO_COMMAND_WRITE, dc_ioc.isEcu, dc_ioc.size, dc_ioc.addr, &valw );
					}else if( dc_ioc.size == 4 ){
						valdw = dc_ioc.val;
						cpsaio_command( (unsigned long)( dev->baseAddr ) ,CPS_AIO_COMMAND_WRITE, dc_ioc.isEcu, dc_ioc.size, dc_ioc.addr, &valdw );
					}
					write_unlock(&dev->lock);

					break;

		case IOCTL_CPSAIO_DIRECT_COMMAND_INPUT:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &dc_ioc, (int __user *)arg, sizeof(dc_ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					if( dc_ioc.size == 2 ){
						cpsaio_command( (unsigned long)( dev->baseAddr ) ,CPS_AIO_COMMAND_READ, dc_ioc.isEcu, dc_ioc.size, dc_ioc.addr, &valw );
						d_ioc.val = (unsigned long) valw;
					}else if( dc_ioc.size == 4 ){
						cpsaio_command( (unsigned long)( dev->baseAddr ) ,CPS_AIO_COMMAND_READ, dc_ioc.isEcu, dc_ioc.size, dc_ioc.addr, &valdw );
						d_ioc.val = valdw;
					}					
					read_unlock(&dev->lock);

					if( copy_to_user( (int __user *)arg, &dc_ioc, sizeof(dc_ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSAIO_DIRECT_COMMAND_CALL:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &dc_ioc, (int __user *)arg, sizeof(dc_ioc) ) ){
						return -EFAULT;
					}
					write_lock(&dev->lock);
					cpsaio_command( (unsigned long)( dev->baseAddr ) ,CPS_AIO_COMMAND_CALL, dc_ioc.isEcu, 0, dc_ioc.addr, NULL );				
					write_unlock(&dev->lock);

					if( copy_to_user( (int __user *)arg, &dc_ioc, sizeof(dc_ioc) ) ){
						return -EFAULT;
					}
					break;

	}
	return 0;
}


static ssize_t cpsaio_get_sampling_data_ai(struct file *filp, char __user *buf, size_t count, loff_t *f_pos )
{

	PCPSAIO_DRV_FILE dev = filp->private_data;
	unsigned short valw = 0;
	unsigned char valb = 0;
	unsigned short cnt = 0,cnt2 = 0;
	int retval = 0;
	unsigned short wStatus = 0;
	unsigned long timout;

	if( dev->data.Ability & CPS_AIO_ABILITY_AI ){
		for( cnt = 0; cnt < count; cnt += 2 )
		{
			timout = 0;
			do{
				contec_cps_micro_delay_sleep( 1 );
				cpsaio_read_mem_status( (unsigned long) dev->baseAddr, &wStatus );
				if( timout > 1000000 ){
					DEBUG_CPSAIO_READFIFO(KERN_INFO"mem timeout \n");
					retval = -EFAULT;
					goto out;
				}else	timout++;
			}while( !(wStatus & CPU_AIO_MEMSTATUS_DRE) );

			cpsaio_read_ai_data((unsigned long)dev->baseAddr , &valw );
			DEBUG_CPSAIO_READFIFO(KERN_INFO"%d:[%x]\n", cnt/2, valw );
			//short to char buffer copy  
			for( cnt2 = 0;cnt2 < 2; cnt2 ++ ){
				valb = (valw & (0xFF << ( 8 * cnt2 ) ) ) >> ( 8 * cnt2 );
				if( copy_to_user( &buf[cnt + cnt2], &valb, 1 ) ){
					DEBUG_CPSAIO_READFIFO(KERN_INFO"no copy to user \n");
					retval = -EFAULT;
					goto out;
				}
			}
		}
		retval = count;
	}
out:
	return retval;

}


static ssize_t cpsaio_set_sampling_data_ao(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos )
{

	PCPSAIO_DRV_FILE dev = filp->private_data;
	unsigned short valw = 0;
	unsigned short cnt = 0;
	int retval = 0;

	if( dev->data.Ability & CPS_AIO_ABILITY_AO ){
		for( cnt = 0; cnt < count; cnt ++ )
		{
			if( copy_to_user(&valw, &buf[cnt], 2 ) ){
				retval = -EFAULT;
				goto out;
			}
			cpsaio_write_ao_data((unsigned long)dev->baseAddr , valw );

		}

		retval = count;
	}

out:
	return retval;

}

static int cpsaio_open(struct inode *inode, struct file *filp )
{
	int ret;
	PCPSAIO_DRV_FILE dev;
	int cnt;
	unsigned char __iomem *allocMem;
	unsigned short product_id;

	inode->i_private = inode; /* omajinai */

	DEBUG_CPSAIO_OPEN(KERN_INFO"node %d\n",iminor( inode ) );

	if (	filp->private_data != (PCPSAIO_DRV_FILE)NULL ){
		dev =  (PCPSAIO_DRV_FILE)filp->private_data;
		if( dev->ref ){
			dev->ref++;
			return 0;
		}
	}

	filp->private_data = (PCPSAIO_DRV_FILE)kmalloc( sizeof(CPSAIO_DRV_FILE) , GFP_KERNEL );
	
	dev = (PCPSAIO_DRV_FILE)filp->private_data;

	dev->node = iminor( inode );
	
	dev->localAddr = 0x08000010 + (dev->node + 1) * 0x100;

	allocMem = cps_common_mem_alloc( dev->localAddr, 0xF0, "cps-aio", CPS_COMMON_MEM_REGION );

	if( allocMem != NULL ){
		dev->baseAddr = allocMem;
	}else{
		return -EBUSY;
	}

	product_id = contec_mcs341_device_productid_get( dev->node );
	cnt = 0;
	do{
		if( cps_aio_data[cnt].ProductNumber == -1 ) break;
		if( cps_aio_data[cnt].ProductNumber == product_id ){
			dev->data = cps_aio_data[cnt];
			break;
		}
		cnt++;
	}while( 1 );

	ret = request_irq(AM335X_IRQ_NMI, cpsaio_isr_func, IRQF_SHARED, "cps-aio-intr", dev);

	if( ret ){
		printk(" request_irq failed.(%x) \n",ret);
	}

	dev->ref = 1;

	return 0;
}

static int cpsaio_close(struct inode * inode, struct file *filp ){

	PCPSAIO_DRV_FILE dev;

	if (	filp->private_data != (PCPSAIO_DRV_FILE)NULL ){
		dev =  (PCPSAIO_DRV_FILE)filp->private_data;
		dev->ref--;
		if( dev->ref == 0 ){

			free_irq(AM335X_IRQ_NMI, dev);

			cps_common_mem_release( dev->localAddr,
				0xF0,
				dev->baseAddr ,
				CPS_COMMON_MEM_REGION);
				
			kfree( dev );
			
			dev = NULL;
		}
	}
	return 0;

}



static struct file_operations cpsaio_fops = {
		.owner = THIS_MODULE,
		.open = cpsaio_open,
		.release = cpsaio_close,
		.read = cpsaio_get_sampling_data_ai,
		.write = cpsaio_set_sampling_data_ao,
		.unlocked_ioctl = cpsaio_ioctl,
};

static int cpsaio_init(void)
{

	dev_t dev = MKDEV(cpsaio_major , 0 );
	int ret = 0;
	int major;
	int cnt;

	struct device *devlp = NULL;

	// CPS-MCS341 Device Init
	contec_mcs341_controller_cpsDevicesInit();

	// Get Device Number
	cpsaio_max_devs = contec_mcs341_controller_getDeviceNum();

	printk(" cpsaio : driver init. devices %d\n", cpsaio_max_devs);

	ret = alloc_chrdev_region( &dev, 0, cpsaio_max_devs, CPSAIO_DRIVER_NAME );

	if( ret )	return ret;

	cpsaio_major = major = MAJOR(dev);

	cdev_init( &cpsaio_cdev, &cpsaio_fops );
	cpsaio_cdev.owner = THIS_MODULE;
	cpsaio_cdev.ops	= &cpsaio_fops;
	ret = cdev_add( &cpsaio_cdev, MKDEV(cpsaio_major, cpsaio_minor), cpsaio_max_devs );

	if( ret ){
		unregister_chrdev_region( dev, cpsaio_max_devs );
		return ret;
	}

	cpsaio_class = class_create(THIS_MODULE, CPSAIO_DRIVER_NAME );

	if( IS_ERR(cpsaio_class) ){
		cdev_del( &cpsaio_cdev );
		unregister_chrdev_region( dev, cpsaio_max_devs );
		return PTR_ERR(cpsaio_class);
	}

	for( cnt = cpsaio_minor; cnt < ( cpsaio_minor + cpsaio_max_devs ) ; cnt ++){
		if( contec_mcs341_device_IsCategory(cnt , CPS_CATEGORY_AIO ) ){
			if( __cpsaio_find_analog_device( cnt ) >= 0  ){
				cpsaio_dev = MKDEV( cpsaio_major, cnt );

				devlp = device_create(
					cpsaio_class, NULL, cpsaio_dev, NULL, CPSAIO_DRIVER_NAME"%d", cnt);

				if( IS_ERR(devlp) ){
					cdev_del( &cpsaio_cdev );
					unregister_chrdev_region( dev, cpsaio_max_devs );
					return PTR_ERR(devlp);
				}
			}
		}
	}

	return 0;
}

static void cpsaio_exit(void)
{

	dev_t dev = MKDEV(cpsaio_major , 0 );
	int cnt;

	for( cnt = cpsaio_minor; cnt < ( cpsaio_minor + cpsaio_max_devs ) ; cnt ++){
		if( contec_mcs341_device_IsCategory(cnt , CPS_CATEGORY_AIO ) ){
			if( __cpsaio_find_analog_device( cnt ) >= 0  ){
				cpsaio_dev = MKDEV( cpsaio_major , cnt );
				device_destroy( cpsaio_class, cpsaio_dev );
			}
		}
	}

	class_destroy(cpsaio_class);

	cdev_del( &cpsaio_cdev );

	unregister_chrdev_region( dev, cpsaio_max_devs );


	//free_irq( AM335X_IRQ_NMI, am335x_irq_dev_id );

	//unregister_chrdev( cpsaio_major, CPSAIO_DRIVER_NAME);

}

module_init(cpsaio_init);
module_exit(cpsaio_exit);
