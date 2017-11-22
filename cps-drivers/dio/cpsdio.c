/*
 *  Driver for CPS-DIO-xxx digital I/O
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
#include <asm/signal.h>
#include <linux/device.h>

#include <linux/slab.h>

#include "../include/cps_common_io.h"
#include "../include/cps.h"
#include "../include/cps_ids.h"
#include "../include/cps_extfunc.h"

#define DRV_VERSION	"0.9.3"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CONTEC CONPROSYS Digital I/O driver");
MODULE_AUTHOR("syunsuke okamoto");
MODULE_VERSION(DRV_VERSION);

#include "../include/cpsdio.h"

#define CPSDIO_DRIVER_NAME "cpsdio"

typedef struct __cpsdio_driver_file{
	rwlock_t lock;
	struct task_struct *int_callback;
	unsigned ref;

	unsigned int node;
	unsigned long localAddr;
	unsigned char __iomem *baseAddr;
	CPSDIO_DEV_DATA data;

}CPSDIO_DRV_FILE,*PCPSDIO_DRV_FILE;

#if 0
#define DEBUG_CPSDIO_OPEN(fmt...)	printk(fmt)
#else
#define DEBUG_CPSDIO_OPEN(fmt...)	do { } while (0)
#endif

static int cpsdio_max_devs;			/*device count */
static int cpsdio_major = 0;
static int cpsdio_minor = 0; 

static struct cdev cpsdio_cdev;
static struct class *cpsdio_class = NULL;

static dev_t cpsdio_dev;

static const CPSDIO_DEV_DATA cps_dio_data[] = {
	{
		.Name = "CPS-DIO-0808L",
		.ProductNumber = CPS_DEVICE_DIO0808L ,
		.inPortNum = 8,
		.outPortNum = 8
	},
	{
		.Name = "CPS-DIO-0808BL",
		.ProductNumber = CPS_DEVICE_DIO0808BL,
		.inPortNum = 8,
		.outPortNum = 8
	},
	{
		.Name = "",
		.ProductNumber = -1,
		.inPortNum = -1,
		.outPortNum = -1
	},
};

#include "cpsdio_devdata.h"

static long cpsdio_read_digital_input( unsigned long BaseAddr, unsigned char *val )
{
	cps_common_inpb( (unsigned long)( BaseAddr + OFFSET_INPUT0_CPS_DIO_0808 ) , val );
	return 0;
}

static long cpsdio_read_digital_output_echo( unsigned long BaseAddr, unsigned char *val )
{
	cps_common_inpb( (unsigned long)( BaseAddr + OFFSET_OUTPUT_ECHO0_CPS_DIO_0808 ) , val );
	return 0;
}

static long cpsdio_write_digital_output( unsigned long BaseAddr, unsigned char val )
{
	cps_common_outb( (unsigned long)( BaseAddr + OFFSET_OUTPUT_ECHO0_CPS_DIO_0808 ) , val );
	return 0;
}

static long cpsdio_set_internal_battery( unsigned long BaseAddr, unsigned char isOn )
{
	cps_common_outb( (unsigned long)( BaseAddr + OFFSET_INTERNALBATTERY_CPS_DIO_0808 ) , isOn );
	return 0;
}

static long cpsdio_set_digital_filter( unsigned long BaseAddr, unsigned char filter )
{
	cps_common_outb( (unsigned long)( BaseAddr + OFFSET_DIGITALFILTER_CPS_DIO_0808 ) , filter );
	return 0;
}

static long cpsdio_get_digital_filter( unsigned long BaseAddr, unsigned char *filter )
{
	cps_common_inpb( (unsigned long)( BaseAddr + OFFSET_DIGITALFILTER_CPS_DIO_0808 ) , filter );
	return 0;
}

static long cpsdio_read_interrupt_status( unsigned long BaseAddr, unsigned short *wStatus )
{
	cps_common_inpw( (unsigned long)( BaseAddr + OFFSET_INTERRUPT_STATUS_CPS_DIO_0808 ) , (unsigned short*) wStatus );
	return 0;
}

static long cpsdio_write_interrupt_mask( unsigned long BaseAddr, unsigned short wStatus )
{
	cps_common_outw( (unsigned long)(BaseAddr + OFFSET_INTERRUPT_STATUS_CPS_DIO_0808 ) , wStatus );
	return 0;
}

static long cpsdio_set_interrupt_edge( unsigned long BaseAddr, unsigned short wStatus )
{
	cps_common_outw( (unsigned long)(BaseAddr + OFFSET_INTERRUPT_EGDE_CPS_DIO_0808 ) , wStatus );
	return 0;
}

static long cpsdio_get_interrupt_edge( unsigned long BaseAddr, unsigned short *wStatus )
{
	cps_common_inpw( (unsigned long)(BaseAddr + OFFSET_INTERRUPT_EGDE_CPS_DIO_0808 ) , wStatus );
	return 0;
}

#define CPSDIO_TYPE_INPUT 0
#define CPSDIO_TYPE_OUTPUT 1

static long cpsdio_get_dio_portnum( int node, unsigned char type, unsigned short *wNum )
{
	int product_id, cnt;
	product_id = contec_mcs341_device_productid_get( node );

	for(cnt = 0; cps_dio_data[cnt].ProductNumber != -1; cnt++){
		if( cps_dio_data[cnt].ProductNumber == product_id ){
			if( type == CPSDIO_TYPE_INPUT ){
				*wNum = cps_dio_data[cnt].inPortNum;
			}else if( type == CPSDIO_TYPE_OUTPUT ){
				*wNum = cps_dio_data[cnt].outPortNum;
			}
			return 0;
		}
	}
	return 1;
}

static long cpsdio_get_dio_devname( int node, unsigned char devName[] )
{
	int product_id, cnt;
	product_id = contec_mcs341_device_productid_get(node);

	for(cnt = 0; cps_dio_data[cnt].ProductNumber != -1; cnt++){
		if( cps_dio_data[cnt].ProductNumber == product_id ){
			strcpy( devName, cps_dio_data[cnt].Name );
			return 0;
		}
	}
	return 1;
}

/***** Interrupt function *******************************/
static const int AM335X_IRQ_NMI=7;

irqreturn_t cpsdio_isr_func(int irq, void *dev_instance){

	unsigned short wStatus;

	PCPSDIO_DRV_FILE dev =(PCPSDIO_DRV_FILE) dev_instance;
	
	if( !dev ) return IRQ_NONE;

	if( contec_mcs341_device_IsCategory( ( dev->node + 1 ) , CPS_CATEGORY_DIO ) ){ 
		cpsdio_read_interrupt_status( (unsigned long)dev->baseAddr, &wStatus);

		if( dev->int_callback != NULL && wStatus ){
			// sending user callback function ( kernel context to user context <used signal SIGUSR1 > )
			send_sig( SIGUSR2, dev->int_callback, 1 );
		}
	}
	else return IRQ_NONE;
	

	if(printk_ratelimit()){
		printk("cpsdio Device Number:%d IRQ interrupt !\n",( dev->node + 1 ) );
	}

	return IRQ_HANDLED;
}


/***** file operation functions *******************************/

static long cpsdio_ioctl( struct file *filp, unsigned int cmd, unsigned long arg )
{
	PCPSDIO_DRV_FILE dev = filp->private_data;
	unsigned char valb = 0;
	unsigned short valw = 0;

	struct cpsdio_ioctl_arg ioc;
	memset( &ioc, 0 , sizeof(ioc) );

	switch( cmd ){

		case IOCTL_CPSDIO_INP_PORT:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					cpsdio_read_digital_input((unsigned long)dev->baseAddr , &valb );
					ioc.val = (unsigned int) valb;
					read_unlock(&dev->lock);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;
		case IOCTL_CPSDIO_OUT_PORT:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					write_lock(&dev->lock);
					valb = (unsigned char) ioc.val;
					cpsdio_write_digital_output( (unsigned long)dev->baseAddr , valb );
					write_unlock(&dev->lock);

					break;
		case IOCTL_CPSDIO_OUT_PORT_ECHO:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					cpsdio_read_digital_output_echo((unsigned long)dev->baseAddr , &valb );
					ioc.val = (unsigned int) valb;
					read_unlock(&dev->lock);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSDIO_SET_INTERNAL_BAT:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					write_lock(&dev->lock);
					valb = (unsigned char) ioc.val;
					cpsdio_set_internal_battery( (unsigned long)dev->baseAddr , valb );
					write_unlock(&dev->lock);

					break;

		case IOCTL_CPSDIO_SET_FILTER:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					write_lock(&dev->lock);
					valb = (unsigned char) ioc.val;
					cpsdio_set_digital_filter( (unsigned long)dev->baseAddr , valb );
					write_unlock(&dev->lock);

					break;

		case IOCTL_CPSDIO_GET_FILTER:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					cpsdio_get_digital_filter( (unsigned long)dev->baseAddr , &valb );
					ioc.val = (unsigned int) valb;
					read_unlock(&dev->lock);
					
					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSDIO_SET_INT_MASK:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					write_lock(&dev->lock);
					valw = (unsigned short) ioc.val;
					cpsdio_write_interrupt_mask( (unsigned long)dev->baseAddr , valw );
					write_unlock(&dev->lock);

					break;

		case IOCTL_CPSDIO_GET_INT_STATUS:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					cpsdio_read_interrupt_status( (unsigned long)dev->baseAddr , &valw );
					ioc.val = (unsigned int) valw;
					read_unlock(&dev->lock);
					
					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;


		case IOCTL_CPSDIO_GET_INT_EGDE:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					cpsdio_get_interrupt_edge( (unsigned long)dev->baseAddr , &valw );
					ioc.val = (unsigned int) valw;
					read_unlock(&dev->lock);
					
					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSDIO_SET_INT_EGDE:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					write_lock(&dev->lock);
					valw = (unsigned short) ioc.val;
					cpsdio_set_interrupt_edge( (unsigned long)dev->baseAddr , valw );
					write_unlock(&dev->lock);

					break;

		case IOCTL_CPSDIO_GET_INP_PORTNUM:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					cpsdio_get_dio_portnum( dev->node ,CPSDIO_TYPE_INPUT, &valw );
					ioc.val = (unsigned int) valw;
					read_unlock(&dev->lock);
					
					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSDIO_GET_OUTP_PORTNUM:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					cpsdio_get_dio_portnum( dev->node ,CPSDIO_TYPE_OUTPUT, &valw );
					ioc.val = (unsigned int) valw;
					read_unlock(&dev->lock);
					
					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSDIO_GET_DEVICE_NAME:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					cpsdio_get_dio_devname( dev->node , ioc.str );
					read_unlock(&dev->lock);
					
					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

	}

	return 0;
}


static int cpsdio_open(struct inode *inode, struct file *filp )
{
	int ret;
	PCPSDIO_DRV_FILE dev;
	int cnt;
	unsigned char __iomem *allocMem;
	unsigned short product_id;

	inode->i_private = inode; /* omajinai */

	DEBUG_CPSDIO_OPEN(KERN_INFO"node %d\n",iminor( inode ) );

	if (	filp->private_data != (PCPSDIO_DRV_FILE)NULL ){
		dev =  (PCPSDIO_DRV_FILE)filp->private_data;
		if( dev->ref ){
			dev->ref++;
			return 0;
		}
	}

	filp->private_data = (PCPSDIO_DRV_FILE)kmalloc( sizeof(CPSDIO_DRV_FILE) , GFP_KERNEL );
	
	dev = (PCPSDIO_DRV_FILE)filp->private_data;

	dev->node = iminor( inode );
	
	dev->localAddr = 0x08000010 + (dev->node + 1) * 0x100;

	allocMem = cps_common_mem_alloc( dev->localAddr, 0xF0, "cps-dio", CPS_COMMON_MEM_REGION );

	if( allocMem != NULL ){
		dev->baseAddr = allocMem;
	}else{
		return -EBUSY;
	}

	product_id = contec_mcs341_device_productid_get( dev->node );
	cnt = 0;
	do{
		if( cps_dio_data[cnt].ProductNumber == -1 ) break;
		if( cps_dio_data[cnt].ProductNumber == product_id ){
			dev->data = cps_dio_data[cnt];
			break;
		}
		cnt++;
	}while( 1 );

	ret = request_irq(AM335X_IRQ_NMI, cpsdio_isr_func, IRQF_SHARED, "cps-dio-intr", dev);

	if( ret ){
		printk(" request_irq failed.(%x) \n",ret);
	}

	dev->ref = 1;

	return 0;
}

static int cpsdio_close(struct inode * inode, struct file *filp ){

	PCPSDIO_DRV_FILE dev;

	if (	filp->private_data != (PCPSDIO_DRV_FILE)NULL ){
		dev =  (PCPSDIO_DRV_FILE)filp->private_data;
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



static struct file_operations cpsdio_fops = {
		.owner = THIS_MODULE,
		.open = cpsdio_open,
		.release = cpsdio_close,
		.unlocked_ioctl = cpsdio_ioctl,
};

static int cpsdio_init(void)
{

	dev_t dev = MKDEV(cpsdio_major , 0 );
	int ret = 0;
	int major;
	int cnt;
	struct device *devlp = NULL;

	// CPS-MCS341 Device Init
	contec_mcs341_controller_cpsDevicesInit();

	// Get Device Number
	cpsdio_max_devs = contec_mcs341_controller_getDeviceNum();

	printk(" cpsdio : driver init. devices %d\n", cpsdio_max_devs);

	ret = alloc_chrdev_region( &dev, 0, cpsdio_max_devs, CPSDIO_DRIVER_NAME );

	if( ret )	return ret;

	cpsdio_major = major = MAJOR(dev);

	cdev_init( &cpsdio_cdev, &cpsdio_fops );
	cpsdio_cdev.owner = THIS_MODULE;
	cpsdio_cdev.ops	= &cpsdio_fops;
	ret = cdev_add( &cpsdio_cdev, MKDEV(cpsdio_major, cpsdio_minor), cpsdio_max_devs );

	if( ret ){
		unregister_chrdev_region( dev, cpsdio_max_devs );
		return ret;
	}

	cpsdio_class = class_create(THIS_MODULE, CPSDIO_DRIVER_NAME );

	if( IS_ERR(cpsdio_class) ){
		cdev_del( &cpsdio_cdev );
		unregister_chrdev_region( dev, cpsdio_max_devs );
		return PTR_ERR(cpsdio_class);
	}

	for( cnt = cpsdio_minor; cnt < ( cpsdio_minor + cpsdio_max_devs ) ; cnt ++){
		if( contec_mcs341_device_IsCategory(cnt , CPS_CATEGORY_DIO ) ){

			cpsdio_dev = MKDEV( cpsdio_major, cnt );

			devlp = device_create(
				cpsdio_class, NULL, cpsdio_dev, NULL, CPSDIO_DRIVER_NAME"%d", cnt);

			if( IS_ERR(devlp) ){
				cdev_del( &cpsdio_cdev );
				unregister_chrdev_region( dev, cpsdio_max_devs );
				return PTR_ERR(devlp);
			}
		}
	}



	return 0;

}

static void cpsdio_exit(void)
{

	dev_t dev = MKDEV(cpsdio_major , 0 );
	int cnt;

	for( cnt = cpsdio_minor; cnt < ( cpsdio_minor + cpsdio_max_devs ) ; cnt ++){
		if( contec_mcs341_device_IsCategory(cnt , CPS_CATEGORY_DIO ) ){
			cpsdio_dev = MKDEV( cpsdio_major , cnt );
			device_destroy( cpsdio_class, cpsdio_dev );
		}
	}

	class_destroy(cpsdio_class);

	cdev_del( &cpsdio_cdev );

	unregister_chrdev_region( dev, cpsdio_max_devs );


	//free_irq( AM335X_IRQ_NMI, am335x_irq_dev_id );

	//unregister_chrdev( cpsdio_major, CPSDIO_DRIVER_NAME);

}

module_init(cpsdio_init);
module_exit(cpsdio_exit);
