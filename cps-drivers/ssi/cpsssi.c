/*
 *  Driver for CPS-SSI sensor input
 *
 *  Copyright (C) 2016 syunsuke okamoto <okamoto@contec.jp>
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

#include <linux/slab.h>
#include <linux/list.h>

#include "../include/cps_common_io.h"
#include "../include/cps.h"
#include "../include/cps_ids.h"
#include "../include/cps_extfunc.h"

#define DRV_VERSION	"1.0.0"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CONTEC CONPROSYS SenSor Input driver");
MODULE_AUTHOR("syunsuke okamoto");
MODULE_VERSION(DRV_VERSION);
#include "../include/cpsssi.h"

#define CPSSSI_DRIVER_NAME "cpsssi"

typedef struct __cpsssi_driver_file{
	rwlock_t lock;	//</ lock
	unsigned ref;	//</ reference count 

	unsigned int node;	//</ Device Node
	unsigned long localAddr; //</ local Address
	unsigned char __iomem *baseAddr;//</ Memory Address
	CPSSSI_DEV_DATA data;	//</ Device Data

}CPSSSI_DRV_FILE,*PCPSSSI_DRV_FILE;

typedef struct __cpsssi_xp_offset_software_data{
	unsigned int node;				//</ Device Node
	unsigned char ch;					//</ channel
	unsigned char w3off;			//</ 3 Wire Offset
	unsigned char w4off;			//</ 4 Wire Offset
	struct list_head	list;		//</	list of CPSSSI_XP_OFFSET_DATA
}CPSSSI_XP_OFFSET_DATA,*PCPSSSI_XP_OFFSET_DATA;

static LIST_HEAD(cpsssi_xp_head);

#if 0
#define DEBUG_CPSSSI_COMMAND(fmt...)	printk(fmt)
#else
#define DEBUG_CPSSSI_COMMAND(fmt...)	do { } while (0)
#endif

#if 0
#define DEBUG_CPSSSI_OPEN(fmt...)	printk(fmt)
#else
#define DEBUG_CPSSSI_OPEN(fmt...)	do { } while (0)
#endif

#if 0
#define DEBUG_CPSSSI_EEPROM(fmt...)	printk(fmt)
#else
#define DEBUG_CPSSSI_EEPROM(fmt...)	do { } while (0)
#endif

static int cpsssi_max_devs;			/*device count */
static int cpsssi_major = 0;
static int cpsssi_minor = 0; 

static struct cdev cpsssi_cdev;
static struct class *cpsssi_class = NULL;

static dev_t cpsssi_dev;

static const CPSSSI_DEV_DATA cps_ssi_data[] = {
	{
		.Name = "CPS-SSI-4P",
		.ProductNumber = CPS_DEVICE_SSI_4P ,
		.ssiChannel = 4,
	},
	{
		.Name = "",
		.ProductNumber = -1,
		.ssiChannel = -1,
	},
};

#include "cpsssi_devdata.h"
/**
	@function __cpsssi_find_sensor_device
	@param node : device node
	@return true : product id, false : -1
**/ 
int __cpsssi_find_sensor_device( int node )
{
	int cnt;
	int product_id;

	product_id = contec_mcs341_device_productid_get( node );
	cnt = 0;
	do{
		if( cps_ssi_data[cnt].ProductNumber == -1 ) return -1;
		if( cps_ssi_data[cnt].ProductNumber == product_id )	break;
		cnt++;
	}while( 1 );
	return product_id;
}

/**
	@function cpsssi_read_eeprom
	@param dev : device number
	@param cate : device category ( SSI )
	@param num : number 
	@param val : value
	@return true : 0
**/ 
unsigned long cpsssi_read_eeprom( unsigned int dev, unsigned int cate, unsigned int num, unsigned short *val )
{

	contec_mcs341_device_extension_value_get(dev, cate,  num );
	contec_mcs341_device_extension_value_get(dev, cate,  num );
	*val = contec_mcs341_device_extension_value_get(dev, cate,  num );

	return 0;
}

/**
	@function cpsssi_write_eeprom
	@param dev : device number
	@param cate : device category ( SSI )
	@param num : number 
	@param val : value
	@return true : 0
**/ 
unsigned long cpsssi_write_eeprom(unsigned int dev, unsigned int cate, unsigned int num, unsigned short val )
{
	unsigned short chkVal;

	unsigned int count = 0;

	do{
		contec_mcs341_device_extension_value_set(dev , cate, num, val);
		cpsssi_read_eeprom( dev, cate, num, &chkVal );
//		chkVal = contec_mcs341_device_extension_value_get(dev ,cate,  num );
		contec_cps_micro_delay_sleep( 1 );
		if( count > 5 ){
			return 1;
		}
		else count++;

		DEBUG_CPSSSI_EEPROM(KERN_INFO" %d, %x :: %x \n", dev, val , chkVal );  

	}while( chkVal != val );
	return 0;
}

/**
	@function cpsssi_clear_eeprom
	@param dev : device number
	@return true : 0
**/ 
void cpsssi_clear_eeprom( unsigned int dev )
{
	contec_mcs341_device_extension_value_all_clear(dev , 0 );
}

/**
	@function cpsssi_clear_fpga_extension_reg
	@param dev : device number
	@return true : 0
**/ 
void cpsssi_clear_fpga_extension_reg(unsigned int dev, unsigned int cate, unsigned int num )
{
	unsigned short val = 0;
	unsigned int cnt;
	
	/* fpga-reg zero overwrite */
	for( cnt = 0; cnt < num; cnt ++ ){
		contec_mcs341_device_extension_value_set(dev , cate, cnt, val);
	}

}



static long cpsssi_read_ssi_status( unsigned long BaseAddr, unsigned short *wStatus )
{
	cps_common_inpw( (unsigned long)( BaseAddr + OFFSET_INPUT_STATUS_CPS_SSI ) , wStatus );
	DEBUG_CPSSSI_COMMAND(KERN_INFO"[%lx]=%x\n",(unsigned long)( BaseAddr + OFFSET_INPUT_STATUS_CPS_SSI ), *wStatus );
	return 0;
}

static long cpsssi_command( unsigned long BaseAddr, unsigned char isReadWrite , unsigned size, unsigned short wCommand , void *vData )
{
	unsigned long com_addr, dat_addr_l, dat_addr_u;
	unsigned short data_l, data_u; 
	unsigned short *tmpWordData;
	unsigned long *tmpDWordData;


	com_addr = (unsigned long)(BaseAddr + OFFSET_EXT_COMMAND_LOWER_CPS_SSI);
	dat_addr_l = (unsigned long)(BaseAddr + OFFSET_EXT_DATA_LOWER_CPS_SSI);
	dat_addr_u = (unsigned long)(BaseAddr + OFFSET_EXT_DATA_UPPER_CPS_SSI);


	/* command */
	cps_common_outw( com_addr , wCommand );
	DEBUG_CPSSSI_COMMAND(KERN_INFO"[%lx]=%x\n",com_addr, wCommand );
	/* data */
	switch( isReadWrite ){
	case CPS_SSI_COMMAND_READ :
		DEBUG_CPSSSI_COMMAND(KERN_INFO"<READ>\n");
		cps_common_inpw(  dat_addr_l , &data_l  );
		if( size == 4 ) cps_common_inpw(  dat_addr_u , &data_u  );
		else data_u = 0;



		switch( size ){
		case 2:
			tmpWordData = (unsigned short *) vData;
			*tmpWordData = data_l;
			DEBUG_CPSSSI_COMMAND(KERN_INFO"[%lx]=%04x\n",dat_addr_l, data_l);
			break;
		case 4:
			tmpDWordData = (unsigned long *) vData;
			*tmpDWordData = (unsigned long ) ( data_u << 16 | data_l );
			DEBUG_CPSSSI_COMMAND(KERN_INFO"[%lx]=%04x\n",dat_addr_l, data_l);
			DEBUG_CPSSSI_COMMAND(KERN_INFO"[%lx]=%04x\n", dat_addr_u, data_u);
			break;
	}	
	break;	
	case CPS_SSI_COMMAND_WRITE :
		DEBUG_CPSSSI_COMMAND(KERN_INFO"<WRITE>\n");
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
		DEBUG_CPSSSI_COMMAND(KERN_INFO"[%lx]=%04x\n",dat_addr_l, data_l);

		if( size == 4 ){
			cps_common_outw( dat_addr_u , data_u );
			DEBUG_CPSSSI_COMMAND(KERN_INFO"[%lx]=%04x\n", dat_addr_u, data_u);
		}
		break;
	}
	return 0;
}

static long cpsssi_command_4p( unsigned long BaseAddr, unsigned char isReadWrite , unsigned short wCommand4p , unsigned short *vData )
{
	unsigned short val_rw = CPS_SSI_SSI_SET_RW_READ;
	unsigned short status;

	// Command Address Write
	cpsssi_command( BaseAddr, CPS_SSI_COMMAND_WRITE, 2, CPS_SSI_COMMAND_SSI_SET_ADDRESS, &wCommand4p );
	
	if( isReadWrite == CPS_SSI_4P_COMMAND_WRITE ){
		// Value Write
		cpsssi_command( BaseAddr, CPS_SSI_COMMAND_WRITE, 2, CPS_SSI_COMMAND_SSI_SET_DATA, vData );
		val_rw = CPS_SSI_SSI_SET_RW_WRITE;
	}

	// Read or Write Flag writes
	cpsssi_command( BaseAddr, CPS_SSI_COMMAND_WRITE, 2, CPS_SSI_COMMAND_SSI_SET_READ_OR_WRITE, &val_rw );

	// Busy Wait
	do{
		contec_cps_micro_delay_sleep( 1 );
		cpsssi_read_ssi_status( BaseAddr , &status );
	}while( status );


	if( isReadWrite == CPS_SSI_4P_COMMAND_READ ){
		//Value Read
		cpsssi_command( BaseAddr, CPS_SSI_COMMAND_READ, 2, CPS_SSI_COMMAND_SSI_SET_DATA, vData );				
	}	

	return 0;
}


#define CPSSSI_COMMAND_SSI_INIT( addr ) \
	cpsssi_command( addr, CPS_SSI_COMMAND_CALL, 0, CPS_SSI_COMMAND_SSI_INIT, NULL )

#define CPSSSI_COMMAND_4P_GET_STATUS( addr , val ) \
	cpsssi_command_4p( addr, CPS_SSI_4P_COMMAND_READ, \
		CPS_SSI_SSI_SET_ADDR_COMMAND_STATUS, val )


void cpsssi_command_4p_set_sence_resistance( unsigned long BaseAddr, unsigned long dwVal )
{
	unsigned short wVal;
	unsigned int i; 	

	for( i = 0; i < 4 ; i ++ ){
		wVal =  ( dwVal & (0xFF << (( 3 - i ) * 8) ) ) >> ( ( 3 - i ) * 8 );
		cpsssi_command_4p( BaseAddr, CPS_SSI_4P_COMMAND_WRITE,
			CPS_SSI_SSI_SET_ADDR_COMMAND_CHANNEL_SENCE_RESISTANCE( i ), &wVal );
	}
}

void cpsssi_command_4p_get_sence_resistance( unsigned long BaseAddr, unsigned long *dwVal )
{
	unsigned short wVal;
	unsigned int i; 	

	if( dwVal != NULL ){
		*dwVal = 0;
		for( i = 0; i < 4 ; i ++ ){
			cpsssi_command_4p( BaseAddr, CPS_SSI_4P_COMMAND_READ,
				CPS_SSI_SSI_SET_ADDR_COMMAND_CHANNEL_SENCE_RESISTANCE( i ), &wVal );
			*dwVal |= wVal << (( 3 - i ) * 8);
		}
	}
}

void cpsssi_command_4p_set_channel( unsigned long BaseAddr, unsigned int ch , unsigned long dwVal )
{
	unsigned short wVal = 0, wCom = 0;
	int i;
	
	switch ( ch ) {
	case 0 : wCom = CPS_SSI_SSI_SET_ADDR_COMMAND_CHANNEL0_DATA_ADDRESS; break;
	case 1 : wCom = CPS_SSI_SSI_SET_ADDR_COMMAND_CHANNEL1_DATA_ADDRESS; break;
	case 2 : wCom = CPS_SSI_SSI_SET_ADDR_COMMAND_CHANNEL2_DATA_ADDRESS; break;
	case 3 : wCom = CPS_SSI_SSI_SET_ADDR_COMMAND_CHANNEL3_DATA_ADDRESS; break;
	}
	for( i = 0; i < 4; i ++ ){
		wVal =  ( dwVal & (0xFF << (( 3 - i ) * 8) ) ) >> ( ( 3 - i ) * 8 );
		cpsssi_command_4p( BaseAddr, CPS_SSI_4P_COMMAND_WRITE,
			CPS_SSI_SSI_SET_ADDR_COMMAND_CHANNEL_DATA( i, wCom ), &wVal );
	}
}

void cpsssi_command_4p_get_channel( unsigned long BaseAddr, unsigned int ch , unsigned long *dwVal )
{
	unsigned short wVal = 0, wCom = 0;
	int i;
	
	switch ( ch ) {
	case 0 : wCom = CPS_SSI_SSI_SET_ADDR_COMMAND_CHANNEL0_DATA_ADDRESS; break;
	case 1 : wCom = CPS_SSI_SSI_SET_ADDR_COMMAND_CHANNEL1_DATA_ADDRESS; break;
	case 2 : wCom = CPS_SSI_SSI_SET_ADDR_COMMAND_CHANNEL2_DATA_ADDRESS; break;
	case 3 : wCom = CPS_SSI_SSI_SET_ADDR_COMMAND_CHANNEL3_DATA_ADDRESS; break;
	}
	if( dwVal != NULL ){
		*dwVal = 0;
		for( i = 0; i < 4; i ++ ){
//		wVal =  ( dwVal & (0xFF << (( 3 - i ) * 8) ) ) >> ( ( 3 - i ) * 8 );
			cpsssi_command_4p( BaseAddr, CPS_SSI_4P_COMMAND_READ,
				CPS_SSI_SSI_SET_ADDR_COMMAND_CHANNEL_DATA( i, wCom ), &wVal );
			*dwVal |= wVal << ( (3 - i) * 8 ) ;
		}
	}
}



void cpsssi_command_4p_set_start( unsigned long BaseAddr, unsigned int ch )
{
	unsigned short wVal = 0;
	unsigned short wStatus = 0;
	switch ( ch ) {
	case 0 : wVal = CPS_SSI_SSI_SET_ADDR_COMMAND_START_CHANNEL0; break;
	case 1 : wVal = CPS_SSI_SSI_SET_ADDR_COMMAND_START_CHANNEL1; break;
	case 2 : wVal = CPS_SSI_SSI_SET_ADDR_COMMAND_START_CHANNEL2; break;
	case 3 : wVal = CPS_SSI_SSI_SET_ADDR_COMMAND_START_CHANNEL3; break;
	}

	cpsssi_command_4p( BaseAddr, CPS_SSI_4P_COMMAND_WRITE,
			CPS_SSI_SSI_SET_ADDR_COMMAND_STATUS, &wVal );

	// Conversion Busy Wait
	do{
		CPSSSI_COMMAND_4P_GET_STATUS( BaseAddr , &wStatus );
	} while( !(wStatus & 0x40) );

}

void cpsssi_command_4p_get_temprature( unsigned long BaseAddr, unsigned int ch , unsigned long *dwVal )
{
	unsigned short wVal = 0, wCom = 0;
	unsigned int i; 	

	switch ( ch ) {
	case 0 : wCom = CPS_SSI_SSI_SET_ADDR_COMMAND_TEMP_CHANNEL0; break;
	case 1 : wCom = CPS_SSI_SSI_SET_ADDR_COMMAND_TEMP_CHANNEL1; break;
	case 2 : wCom = CPS_SSI_SSI_SET_ADDR_COMMAND_TEMP_CHANNEL2; break;
	case 3 : wCom = CPS_SSI_SSI_SET_ADDR_COMMAND_TEMP_CHANNEL3; break;
	}

	//contec_cps_micro_delay_sleep( 100 * 1000 );

	for( i = 0, *dwVal = 0; i < 4 ; i ++ ){
		cpsssi_command_4p( BaseAddr, CPS_SSI_4P_COMMAND_READ,
			CPS_SSI_SSI_SET_ADDR_COMMAND_TEMPERATURE( i , wCom ), &wVal );
			*dwVal |= ( ( wVal & 0xFF ) << (( 3 - i ) * 8 ) ); 
	}
	DEBUG_CPSSSI_COMMAND(KERN_INFO" val = %lx \n", *dwVal );
}

/***** allocate/free list_head *******************************/
void cpsssi_4p_allocate_offset_list( unsigned int node, unsigned int max_ch ){

	unsigned int cnt;

	for( cnt = 0;cnt < max_ch; cnt++ )
	{
		PCPSSSI_XP_OFFSET_DATA xp_off_data;

		xp_off_data = kzalloc( sizeof( CPSSSI_XP_OFFSET_DATA ), GFP_KERNEL );

		xp_off_data->node = node;
		xp_off_data->ch = cnt;
		xp_off_data->w3off = 0;
		xp_off_data->w4off = 0;
		INIT_LIST_HEAD( &xp_off_data->list );

		list_add_tail( &xp_off_data->list, &cpsssi_xp_head );
	}

}

void cpsssi_4p_free_offset_list_of_device( unsigned int node ){

	PCPSSSI_XP_OFFSET_DATA xp_off_data, next;

	list_for_each_entry_safe( xp_off_data, next, &cpsssi_xp_head, list ){
		if( xp_off_data->node == node || node == 0xFFFF ){
			list_del( &xp_off_data->list );
			kfree( xp_off_data );
		}
	}
}


void cpsssi_4p_set_channeldata_offset( unsigned int node, unsigned int ch ,CPSSSI_4P_CHANNEL_DATA pData[], unsigned char bVal ){

	PCPSSSI_XP_OFFSET_DATA xp_off_data, next;

	list_for_each_entry_safe( xp_off_data, next, &cpsssi_xp_head, list ){
		if( xp_off_data->node == node && xp_off_data->ch == ch ){
			switch( pData[ch].Current_Wire_Status ){
			case SSI_4P_CHANNEL_RTD_WIRE_3 :
				xp_off_data->w3off = bVal;
				break;
			case SSI_4P_CHANNEL_RTD_WIRE_4 :
			default:
				xp_off_data->w4off = bVal;
				break;
			}
			return;
		}
	}
}

void cpsssi_4p_get_channeldata_offset( unsigned int node, unsigned int ch ,CPSSSI_4P_CHANNEL_DATA pData[], unsigned char *bVal ){

	PCPSSSI_XP_OFFSET_DATA xp_off_data, next;

	list_for_each_entry_safe( xp_off_data, next, &cpsssi_xp_head, list ){
		if( xp_off_data->node == node && xp_off_data->ch == ch ){
			switch( pData[ch].Current_Wire_Status ){
			case SSI_4P_CHANNEL_RTD_WIRE_3 :
				*bVal = xp_off_data->w3off;
				break;
			case SSI_4P_CHANNEL_RTD_WIRE_4 :
			default:
				*bVal = xp_off_data->w4off;
				break;
			}
			return;
		}
	}
}

/***** Set/Get ChannelData structure function *******************************/

void cpsssi_4p_set_channeldata_basic( unsigned int ch , CPSSSI_4P_CHANNEL_DATA pData[], unsigned long dwVal ){

	pData[ch].Current_Wire_Status = SSI_4P_CHANNEL_GET_RTD_WIRE_MODE( dwVal );
	pData[ch].Current_Rtd_Standard = SSI_4P_CHANNEL_GET_RTD_STANDARD( dwVal );

}

void cpsssi_4p_set_channeldata_lastStatus( unsigned int ch ,CPSSSI_4P_CHANNEL_DATA pData[],  unsigned long dwVal ){

	pData[ch].lastStatus = ( (dwVal & 0xFF000000 ) >> 24 );

}


unsigned long cpsssi_4p_get_channeldata_lastStatus( unsigned int ch ,CPSSSI_4P_CHANNEL_DATA pData[] ){

	return pData[ch].lastStatus ;

}

/* Offset add or sub */
unsigned long cpsssi_4p_addsub_channeldata_offset( unsigned int node, unsigned int ch , CPSSSI_4P_CHANNEL_DATA pData[], unsigned long dwVal ){

	unsigned char offset;

	cpsssi_4p_get_channeldata_offset( node, ch , pData, &offset );

	if( offset & 0x80 )		 
		return ( dwVal - (unsigned long)( (0x80 - (offset & 0x7F)) << 5) );
	else
		return ( dwVal + (unsigned long)( (offset & 0x7F) << 5) );
}

/***** Interrupt function *******************************/
static const int AM335X_IRQ_NMI=7;

irqreturn_t cpsssi_isr_func(int irq, void *dev_instance){

	unsigned short wStatus;

	PCPSSSI_DRV_FILE dev =(PCPSSSI_DRV_FILE) dev_instance;
	
	if( !dev ) return IRQ_NONE;

	if( contec_mcs341_device_IsCategory( ( dev->node + 1 ) , CPS_CATEGORY_SSI ) ){ 
	
	}
	else return IRQ_NONE;
	

	if(printk_ratelimit()){
		printk("cpsssi Device Number:%d IRQ interrupt !\n",( dev->node + 1 ) );
	}

	return IRQ_HANDLED;
}


/***** file operation functions *******************************/

static long cpsssi_ioctl( struct file *filp, unsigned int cmd, unsigned long arg )
{
	PCPSSSI_DRV_FILE dev = filp->private_data;
	unsigned short valw = 0;
	unsigned long valdw = 0;
	unsigned int num = 0;
	unsigned char valb = 0;
	unsigned int cnt = 0;
	
	PCPSSSI_4P_CHANNEL_DATA pData = (PCPSSSI_4P_CHANNEL_DATA)dev->data.ChannelData;

	struct cpsssi_ioctl_arg ioc;
	memset( &ioc, 0 , sizeof(ioc) );

	//memcpy(pData, dev->data.ChannelData, sizeof(CPSSSI_4P_CHANNEL_DATA) * dev->data.ssiChannel);

	switch( cmd ){

/* SenSer Input Common */
		case IOCTL_CPSSSI_INIT:
					CPSSSI_COMMAND_SSI_INIT( (unsigned long)dev->baseAddr );
					break;

		case IOCTL_CPSSSI_EXIT:
					break;

/* SenSer Input */
		case IOCTL_CPSSSI_INDATA:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					cpsssi_command_4p_get_temprature((unsigned long)dev->baseAddr , ioc.ch, &valdw );
					cpsssi_4p_set_channeldata_lastStatus(ioc.ch, pData, valdw);
					ioc.val = cpsssi_4p_addsub_channeldata_offset(dev->node, ioc.ch, pData, valdw);
					read_unlock(&dev->lock);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSSSI_START:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					cpsssi_command_4p_set_start((unsigned long)dev->baseAddr , ioc.ch );
					read_unlock(&dev->lock);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;
				
		case IOCTL_CPSSSI_INSTATUS:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
//					CPSSSI_COMMAND_4P_GET_STATUS((unsigned long)dev->baseAddr , &valw );
//						ioc.val = (unsigned long) valw;
					for( cnt = 0, valdw = 0; cnt < 4 ; cnt ++ ){					
						valdw |= ( cpsssi_4p_get_channeldata_lastStatus(cnt , pData ) << (cnt * 8 ) );
						cpsssi_4p_set_channeldata_lastStatus(cnt , pData, 0x00 );
					}
					ioc.val = valdw;
					read_unlock(&dev->lock);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSSSI_STARTBUSYSTATUS:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					CPSSSI_COMMAND_4P_GET_STATUS((unsigned long)dev->baseAddr , &valw );
					ioc.val = (unsigned long) valw;
					read_unlock(&dev->lock);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSSSI_SET_SENCE_RESISTANCE:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					write_lock(&dev->lock);
					valdw = ioc.val;
					cpsssi_command_4p_set_sence_resistance( (unsigned long)dev->baseAddr , valdw );
					write_unlock(&dev->lock);

					break;

		case IOCTL_CPSSSI_GET_SENCE_RESISTANCE:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					cpsssi_command_4p_get_sence_resistance( (unsigned long)dev->baseAddr , &valdw );
					ioc.val = valdw;
					read_unlock(&dev->lock);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSSSI_SET_CHANNEL:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					write_lock(&dev->lock);
					valw = ioc.ch;
					valdw = ioc.val;
					cpsssi_4p_set_channeldata_basic( valw, pData, valdw );

					cpsssi_command_4p_set_channel( (unsigned long)dev->baseAddr, valw, valdw );
					write_unlock(&dev->lock);

					break;

		case IOCTL_CPSSSI_GET_CHANNEL:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					valw = (unsigned short) ioc.ch;
					cpsssi_command_4p_get_channel( (unsigned long)dev->baseAddr, valw, &valdw );
					ioc.val = valdw;
					read_unlock(&dev->lock);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}

					break;

/****  Software OFFSET SET/GET Command *****/ 
		case IOCTL_CPSSSI_SET_OFFSET:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					write_lock(&dev->lock);
					
					valw = (unsigned short) ioc.ch;
					valb = (unsigned char)ioc.val;
					cpsssi_4p_set_channeldata_offset( dev->node, valw, pData, valb );
					write_unlock(&dev->lock);

					break;

		case IOCTL_CPSSSI_GET_OFFSET:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					valw = (unsigned short) ioc.ch;
					cpsssi_4p_get_channeldata_offset( dev->node, valw, pData, &valb );
					ioc.val = (unsigned long) valb;
					read_unlock(&dev->lock);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}

					break;
/****  EEPROM I/O Command *****/ 
		case IOCTL_CPSSSI_WRITE_EEPROM_SSI:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}

					write_lock(&dev->lock);
					
					valw = (unsigned short) ioc.val;

					switch( dev->data.ProductNumber ){
					case CPS_DEVICE_SSI_4P: num = (unsigned short) ioc.ch;break;
					default :
						write_unlock(&dev->lock);
						return -EFAULT;
					}

					cpsssi_write_eeprom( dev->node , CPS_DEVICE_COMMON_ROM_WRITE_PAGE_SSI, num,  valw );
					write_unlock(&dev->lock);

					DEBUG_CPSSSI_EEPROM(KERN_INFO"EEPROM-WRITE:[%lx]=%x\n",(unsigned long)( dev->baseAddr ), valw );
					break;

		case IOCTL_CPSSSI_READ_EEPROM_SSI:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					switch( dev->data.ProductNumber ){
					case CPS_DEVICE_SSI_4P: num = (unsigned short) ioc.ch;break;
					default :
						read_unlock(&dev->lock);
						return -EFAULT;
					}
					cpsssi_read_eeprom( dev->node ,CPS_DEVICE_COMMON_ROM_WRITE_PAGE_SSI, num, &valw );
					ioc.val = (unsigned long) valw;
					read_unlock(&dev->lock);

					DEBUG_CPSSSI_EEPROM(KERN_INFO"EEPROM-READ:[%lx]=%x\n",(unsigned long)( dev->baseAddr ), valw );
				
					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}

					break;

		case IOCTL_CPSSSI_CLEAR_EEPROM:

					read_lock(&dev->lock);

					switch( dev->data.ProductNumber ){
					case CPS_DEVICE_SSI_4P: num = dev->data.ssiChannel;break;
					}

					cpsssi_clear_fpga_extension_reg(dev->node ,CPS_DEVICE_COMMON_ROM_WRITE_PAGE_SSI, num );

					cpsssi_clear_eeprom( dev->node );
					read_unlock(&dev->lock);

					DEBUG_CPSSSI_EEPROM(KERN_INFO"EEPROM-CLEAR\n");
					break;
	}

	return 0;
}


static int cpsssi_open(struct inode *inode, struct file *filp )
{
	int ret;
	PCPSSSI_DRV_FILE dev;
	int cnt;
	unsigned char __iomem *allocMem;
	unsigned short product_id;

	inode->i_private = inode; /* omajinai */

	DEBUG_CPSSSI_OPEN(KERN_INFO"node %d\n",iminor( inode ) );

	if (	filp->private_data != (PCPSSSI_DRV_FILE)NULL ){
		dev =  (PCPSSSI_DRV_FILE)filp->private_data;
		if( dev->ref ){
			dev->ref++;
			return 0;
		}
	}

	filp->private_data = (PCPSSSI_DRV_FILE)kmalloc( sizeof(CPSSSI_DRV_FILE) , GFP_KERNEL );
	
	dev = (PCPSSSI_DRV_FILE)filp->private_data;

	dev->node = iminor( inode );
	
	dev->localAddr = 0x08000010 + (dev->node + 1) * 0x100;

	allocMem = cps_common_mem_alloc( dev->localAddr, 0xF0, "cps-ssi", CPS_COMMON_MEM_REGION );

	if( allocMem != NULL ){
		dev->baseAddr = allocMem;
	}else{
		goto NOT_IOMEM_ALLOCATION;
	}

	product_id = contec_mcs341_device_productid_get( dev->node );
	cnt = 0;
	do{
		if( cps_ssi_data[cnt].ProductNumber == -1 ) goto NOT_FOUND_SSI_PRODUCT;
		if( cps_ssi_data[cnt].ProductNumber == product_id ){
			dev->data = cps_ssi_data[cnt];
			break;
		}
		cnt++;
	}while( 1 );


	//Extension(Channel Data) Allocation
	dev->data.ChannelData = 
		(PCPSSSI_4P_CHANNEL_DATA)kcalloc( dev->data.ssiChannel, sizeof(CPSSSI_4P_CHANNEL_DATA) , GFP_KERNEL );

	if( dev->data.ChannelData == NULL ){
		printk(KERN_INFO" channel data allocation failed.\n");
		goto NOT_FOUND_SSI_PRODUCT;
	}

//	memset( &dev->data.ChannelData, 0x00, sizeof(CPSSSI_4P_CHANNEL_DATA) * dev->data.ssiChannel );

	//IRQ Request
	ret = request_irq(AM335X_IRQ_NMI, cpsssi_isr_func, IRQF_SHARED, "cps-ssi-intr", dev);

	if( ret ){
		printk(" request_irq failed.(%x) \n",ret);
	}

	dev->ref = 1;

	return 0;

NOT_CHANNELDATA_ALLOCATION:
	kfree(dev->data.ChannelData);

NOT_FOUND_SSI_PRODUCT:
	cps_common_mem_release( dev->localAddr,
		0xF0,
		dev->baseAddr ,
		CPS_COMMON_MEM_REGION);

NOT_IOMEM_ALLOCATION:
	kfree( dev );

	return -EFAULT;
}

static int cpsssi_close(struct inode * inode, struct file *filp ){

	PCPSSSI_DRV_FILE dev;

	if (	filp->private_data != (PCPSSSI_DRV_FILE)NULL ){
		dev =  (PCPSSSI_DRV_FILE)filp->private_data;
		dev->ref--;
		if( dev->ref == 0 ){

			free_irq(AM335X_IRQ_NMI, dev);
			kfree(dev->data.ChannelData);

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



static struct file_operations cpsssi_fops = {
		.owner = THIS_MODULE,
		.open = cpsssi_open,
		.release = cpsssi_close,
		.unlocked_ioctl = cpsssi_ioctl,
};

static int cpsssi_init(void)
{

	dev_t dev = MKDEV(cpsssi_major , 0 );
	int ret = 0;
	int major;
	int cnt;
	unsigned short product_id;

	struct device *devlp = NULL;

	// CPS-MCS341 Device Init
	contec_mcs341_controller_cpsDevicesInit();

	// Get Device Number
	cpsssi_max_devs = contec_mcs341_controller_getDeviceNum();

	printk(" cpsssi : driver init. devices %d\n", cpsssi_max_devs);

	ret = alloc_chrdev_region( &dev, 0, cpsssi_max_devs, CPSSSI_DRIVER_NAME );

	if( ret )	return ret;

	cpsssi_major = major = MAJOR(dev);

	cdev_init( &cpsssi_cdev, &cpsssi_fops );
	cpsssi_cdev.owner = THIS_MODULE;
	cpsssi_cdev.ops	= &cpsssi_fops;
	ret = cdev_add( &cpsssi_cdev, MKDEV(cpsssi_major, cpsssi_minor), cpsssi_max_devs );

	if( ret ){
		unregister_chrdev_region( dev, cpsssi_max_devs );
		return ret;
	}

	cpsssi_class = class_create(THIS_MODULE, CPSSSI_DRIVER_NAME );

	if( IS_ERR(cpsssi_class) ){
		cdev_del( &cpsssi_cdev );
		unregister_chrdev_region( dev, cpsssi_max_devs );
		return PTR_ERR(cpsssi_class);
	}

	for( cnt = cpsssi_minor; cnt < ( cpsssi_minor + cpsssi_max_devs ) ; cnt ++){
		if( contec_mcs341_device_IsCategory(cnt , CPS_CATEGORY_SSI ) ){
			if( (product_id = __cpsssi_find_sensor_device( cnt ) ) >= 0  ){
				cpsssi_dev = MKDEV( cpsssi_major, cnt );

				devlp = device_create(
					cpsssi_class, NULL, cpsssi_dev, NULL, CPSSSI_DRIVER_NAME"%d", cnt);

				if( IS_ERR(devlp) ){
					cdev_del( &cpsssi_cdev );
					unregister_chrdev_region( dev, cpsssi_max_devs );
					return PTR_ERR(devlp);
				}

				if( product_id == CPS_DEVICE_SSI_4P )
				{
					cpsssi_4p_allocate_offset_list( cnt, 4 );
				}
			}
		}
	}

	return 0;

}

static void cpsssi_exit(void)
{

	dev_t dev = MKDEV(cpsssi_major , 0 );
	int cnt;
	unsigned short product_id;

	for( cnt = cpsssi_minor; cnt < ( cpsssi_minor + cpsssi_max_devs ) ; cnt ++){
		if( contec_mcs341_device_IsCategory(cnt , CPS_CATEGORY_SSI ) ){
			if( (product_id = __cpsssi_find_sensor_device( cnt ) ) >= 0  ){
				cpsssi_dev = MKDEV( cpsssi_major , cnt );
				device_destroy( cpsssi_class, cpsssi_dev );

				if( product_id == CPS_DEVICE_SSI_4P )
				{
					cpsssi_4p_free_offset_list_of_device( cnt );
				}
			}
		}
	}

	class_destroy(cpsssi_class);

	cdev_del( &cpsssi_cdev );

	unregister_chrdev_region( dev, cpsssi_max_devs );


	//free_irq( AM335X_IRQ_NMI, am335x_irq_dev_id );

	//unregister_chrdev( cpsssi_major, CPSSSI_DRIVER_NAME);

}

module_init(cpsssi_init);
module_exit(cpsssi_exit);
