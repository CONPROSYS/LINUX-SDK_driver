/*
 *  Driver for cps-iolib with CPS-MCS341.
 * Version 1.0.0
 *
 *  I/O Control CPS-MCS341 Series (only) Driver by CONTEC .
 *
 *  Copyright (C) 2015 Syunsuke Okamoto.<okamoto@contec.jp>
 *
 * cps-iolib driver program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * cps-iolib driver program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
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

#include "cps_common_io.h"

MODULE_LICENSE("GPL");
MODULE_ALIAS("CONTEC I/O Driver for CPS-MCS341");
MODULE_AUTHOR("CONTEC");

#include "cps-iolib.h"

#define CPSIO_DRIVER_NAME "cps-iolib"

struct cpsio_data{
	rwlock_t lock;
	unsigned char val;
};

static int cpsio_max_devs = 1;			/*device count */
static int cpsio_major = 0;
static int cpsio_minor = 0; 

static struct cdev cpsio_cdev;
static struct class *cpsio_class = NULL;

static dev_t cpsio_dev;

static void __iomem *map_baseaddr ;


/***** file operation functions *******************************/

static long cpsio_ioctl( struct file *filp, unsigned int cmd, unsigned long arg )
{
	struct cpsio_data *dev = filp->private_data;
	unsigned char valb;
	unsigned short valw;

	struct cpsio_ioctl_arg ioc;
	memset( &ioc, 0 , sizeof(ioc) );

	switch( cmd ){

		case IOCTL_CPSIO_INPWORD:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					
					cps_common_inpw( (unsigned long)(map_baseaddr + ioc.addr), &valw );
					read_unlock(&dev->lock);

					ioc.val = valw;
					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;
		case IOCTL_CPSIO_OUTWORD:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if(!capable(CAP_SYS_ADMIN) ){
						return -EPERM;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					write_lock(&dev->lock);
					valw = ioc.val;
					cps_common_outw( (unsigned long)(map_baseaddr + ioc.addr), valw );
					write_unlock(&dev->lock);

					break;

		case IOCTL_CPSIO_INPBYTE:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					read_lock(&dev->lock);
					cps_common_inpb( (unsigned long)(map_baseaddr + ioc.addr), &valb );
					read_unlock(&dev->lock);

					ioc.val = (unsigned short) valb;
					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSIO_OUTBYTE:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if(!capable(CAP_SYS_ADMIN) ){
						return -EPERM;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					write_lock(&dev->lock);
					valb = (unsigned char)ioc.val;
					cps_common_outb( (unsigned long)(map_baseaddr + ioc.addr), valb );
					write_unlock(&dev->lock);

					break;
	}

	return 0;
}


static int cpsio_open(struct inode *inode, struct file *filp )
{
	void __iomem *allocaddr = NULL;
	inode->i_private = inode;
	filp->private_data = filp;

	/* I/O Mapping */
	allocaddr = cps_common_mem_alloc( 0x08000000, 0x2000, "", CPS_COMMON_MEM_NONREGION );

	if( !allocaddr ){
			printk(KERN_INFO "cpsio : MEMORY cannot allocation. [%lx]", (unsigned long)map_baseaddr );
			return -ENOMEM;
	}else{
		map_baseaddr = allocaddr;
	}

	return 0;
}

static int cpsio_close(struct inode * inode, struct file *file ){

	cps_common_mem_release( 0x08000000, 0x2000, map_baseaddr, CPS_COMMON_MEM_NONREGION );

	return 0;
}


static struct file_operations cpsio_fops = {
		.owner = THIS_MODULE,
		.open = cpsio_open,
		.release = cpsio_close,
		.unlocked_ioctl = cpsio_ioctl,
};

static int cpsio_init(void)
{

	dev_t dev = MKDEV(cpsio_major , 0 );
	int ret = 0;
	int major;
	int cnt;

	struct device *devlp = NULL;

	printk(KERN_INFO " cps-iolib : <INIT> \n");

	ret = alloc_chrdev_region( &dev, 0, cpsio_max_devs, CPSIO_DRIVER_NAME );

	if( ret ){
		printk(KERN_ERR " cps-iolib : <INIT> ERROR ALLOC CHARCTOR DEVICE \n");
		return ret;
	}
	cpsio_major = major = MAJOR(dev);

	cdev_init( &cpsio_cdev, &cpsio_fops );
	cpsio_cdev.owner = THIS_MODULE;
	cpsio_cdev.ops	= &cpsio_fops;
	ret = cdev_add( &cpsio_cdev, MKDEV(cpsio_major, cpsio_minor), 1 );

	if( ret ){
		unregister_chrdev_region( dev, cpsio_max_devs );
		printk(KERN_ERR " cps-iolib : <INIT> ERROR ADD CHARCTOR DEVICE \n");
		return ret;
	}

	cpsio_class = class_create(THIS_MODULE, CPSIO_DRIVER_NAME );

	if( IS_ERR(cpsio_class) ){
		cdev_del( &cpsio_cdev );
		unregister_chrdev_region( dev, cpsio_max_devs );
		printk(KERN_ERR " cps-iolib : <INIT> ERROR ADD CHARCTOR DEVICE \n");
		return PTR_ERR(cpsio_class);
	}

	ret = cps_fpga_init();

	if( ret ){
		cdev_del( &cpsio_cdev );
		unregister_chrdev_region( dev, cpsio_max_devs );
		printk(KERN_ERR " cps-iolib : <INIT> ERROR FPGA INIT \n");
		return ret;
	}	

	for( cnt = cpsio_minor; cnt < ( cpsio_minor + cpsio_max_devs ) ; cnt ++){
		cpsio_dev = MKDEV( cpsio_major, cnt );

		devlp = device_create(
			cpsio_class, NULL, cpsio_dev, NULL, CPSIO_DRIVER_NAME"%d", cnt);

		if( IS_ERR(devlp) ){
			cdev_del( &cpsio_cdev );
			unregister_chrdev_region( dev, cpsio_max_devs );
			printk(KERN_ERR " cps-iolib : <INIT> ERROR ADD CHARCTOR DEVICE \n");
			return PTR_ERR(devlp);
		}
	}

	return 0;

}

static void cpsio_exit(void)
{

	dev_t dev = MKDEV(cpsio_major , 0 );
	int cnt;

	for( cnt = cpsio_minor; cnt < ( cpsio_minor + cpsio_max_devs ) ; cnt ++){
		cpsio_dev = MKDEV( cpsio_major , cnt );
		device_destroy( cpsio_class, cpsio_dev );
	}

	class_destroy(cpsio_class);

	cdev_del( &cpsio_cdev );

	unregister_chrdev_region( dev, cpsio_max_devs );

	printk(KERN_INFO " cps-iolib : <EXIT> \n");

}

module_init(cpsio_init);
module_exit(cpsio_exit);
