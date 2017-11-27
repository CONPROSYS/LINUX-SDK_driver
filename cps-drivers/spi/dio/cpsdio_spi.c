/*
 *  Driver for CPS-DIO digital I/O for SPI
 *
 *  Copyright (C) 2016 shinichiro nagasato <shinichiro_nagasato@contec.jp>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Ver 0.0.1 : first make.
 * Ver 0.0.2 : digital filter added.
 * Ver.0.0.3 : 
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>

#include "cpsdio_spidevdata.h"
#include "cpsdio.h"
#include "cps_mc341.h"


/**
	@~English
	@param address : offset address
	@param type : byte access or word access
	@param value : value
	@~Japanese
	@brief CPS-MC341-DSXのSPI ICへのSPI送信用パケットを作成します。
	@param address : オフセットアドレス
	@param type : バイトアクセスか　ワードアクセス
	@param value : 値
**/
#define CPS_SET_MC341_DS1X_SPI_WRITE_PACKET( address, type, value ) \
CPS_SPI_SET_PACKET(	\
			PAGE_CPS_MC341_DSX_DIO,	\
			address, \
			CPS_SPI_ACCESS_WRITE, \
			type, \
			0,\
			value \
)

/**
	@~English
	@param address : offset address
	@param type : byte access or word access
	@~Japanese
	@brief CPS-MC341-DSXのSPI ICへのSPI受信用パケットを作成します。
	@param address : オフセットアドレス
	@param type : バイトアクセスか　ワードアクセス
**/
#define CPS_SET_MC341_DS1X_SPI_READ_PACKET( address, type ) \
		CPS_SPI_SET_PACKET(\
			PAGE_CPS_MC341_DSX_DIO,\
			address,\
			CPS_SPI_ACCESS_READ,\
			type,	\
			0,\
			(0x0000)	\
)


#define CPSDIOX_PACKET_SIZE	4
#define CPSDIO_SPI_CHANNELS	1
#define CPSDIO_SPI_MAJOR 0
#define CPSDIO_SPI_MINOR 0
#define CPSDIO_SPI_NUM_DEVS 1
#define CPSDIO_SPI_DRIVER_NAME	"cpsdio"
#define DRV_VERSION "0.0.4"
#define max_speed 6000000

/*	Define the name of the device and sysfs class name*/
static struct cdev cpsdio_spi_cdev;
static struct class *cpsdio_spi_class = NULL;

static int cpsdio_spi_major = 0;	///< SPI Major ID
static int cpsdio_spi_minor = 0;	///< SPI Minor ID

struct CPSDIO_SPI_drvdata {
	struct spi_device *spi;
	struct mutex lock;
};

static struct spi_board_info CPSDIO_SPI_info = {
		.modalias = "CPSDIO_SPI",
		.max_speed_hz = max_speed,
		.bus_num = 2,
		.chip_select = 0,
		.mode = SPI_MODE_2,
};

/*	To be able to specify the bus number and the CS in the parameter*/
static int spi_bus_num = 2;
static int spi_chip_select = 0;
module_param(spi_bus_num, int, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
module_param(spi_chip_select, int, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);

static int cpsdio_open(struct inode *inode, struct file *filp) {

	pr_info("Entered function: %s\n", __FUNCTION__);
	return 0;
}

/**
	@~English
	@brief SPI Write function with CONTEC FPGA
	@param spi : SPI structure
	@param address : offset address
	@param size : data size
	@param value : value
	@return true : 0, false : otherwise
	@~Japanese
	@brief CONTECのFPGAに対するSPI書き込み関数です。
	@param spi : SPI構造体
	@param address : オフセットアドレス
	@param size : サイズ
	@param value : 値
	@return 成功: 0 , 失敗: それ以外
**/
static long cpsdio_spi_write(struct spi_device *spi, int address, int size ,unsigned short value ) {
	int status;
	int access_type;
	CPS_SPI_FORMAT tx;

	// サイズ選択
	switch( size ){
	case 1 :
		access_type = CPS_SPI_ACCESS_TYPE_BYTE;
		break;
	case 2 :
		access_type = CPS_SPI_ACCESS_TYPE_WORD;
		break;
	default :
		pr_err("FAILURE: %s failed \n", __FUNCTION__);
		return -ENODEV;
	}

	tx.value = cpu_to_be32(
			CPS_SET_MC341_DS1X_SPI_WRITE_PACKET(
					address,
					access_type,
					value
				)
			);
//	pr_info("tx.value=%lx\n",tx.value);
//	pr_info("address %x value %lx, size %d", address, tx.value, sizeof(tx));

	status = spi_write(spi, (const void *)&(tx.array[0]), sizeof(tx));

	if (status < 0) {
		pr_err("FAILURE: %s failed \n", __FUNCTION__);
		return -ENODEV;
	}

	return 0;
}

/**
	@~English
	@brief SPI Read function with CONTEC FPGA
	@param spi : SPI structure
	@param address : offset address
	@param size : data size
	@param value : value
	@return true : 0, false : otherwise
	@~Japanese
	@brief CONTECのFPGAに対するSPI読み込み関数です。
	@param spi : SPI構造体
	@param address : オフセットアドレス
	@param size : サイズ
	@param value : 値
	@return 成功: 0 , 失敗: それ以外
**/
static long cpsdio_spi_read(struct spi_device *spi, int address, int size ,unsigned short *value ) {
	int status;
	int access_type;
	CPS_SPI_FORMAT rx;

	switch( size ){
	case 1 :
		access_type = CPS_SPI_ACCESS_TYPE_BYTE;
		break;
	case 2 :
		access_type = CPS_SPI_ACCESS_TYPE_WORD;
		break;
	default :
		pr_err("FAILURE: %s failed \n", __FUNCTION__);
		return -ENODEV;
	}
	rx.value = cpu_to_be32(
			CPS_SET_MC341_DS1X_SPI_READ_PACKET(
					address,
					access_type
			)
	);

//	pr_info("command %x %x, size %x", rx.COM_VAL.Frame.command[0], rx.COM_VAL.Frame.command[1], sizeof(rx.COM_VAL.Frame.command));
//	pr_info("value %x %x size %x", rx.COM_VAL.value[0], rx.COM_VAL.value[1], sizeof(rx.COM_VAL.value));

	status = spi_write_then_read(
			spi,
			&(rx.COM_VAL.Frame.command[0]),
			sizeof(rx.COM_VAL.Frame.command),
			&(rx.COM_VAL.value[0]),
			sizeof(rx.COM_VAL.value)
	);

	if (status < 0) {
		pr_err("FAILURE: %s failed \n", __FUNCTION__);
		return -ENODEV;
	}

	switch( size ){
	case 1:
		*value = rx.COM_VAL.value[1];
		break;
	case 2:
		*value = ( rx.COM_VAL.value[0] << 8 ) | rx.COM_VAL.value[1];
		break;
	}

	return 0;
}

/**
	@~English
	@brief This function set Digital Output Data.
	@param spi : SPI structure
	@param port : Port Number
	@param size : data size
	@param value : value
	@return true : 0, false : otherwise
	@~Japanese
	@brief デジタル出力のデータを設定する関数
	@param spi : SPI structure
	@param port : ポート番号
	@param size : データサイズ
	@param value : 値
	@return 成功: 0 , 失敗: それ以外
**/
static long cpsdio_write_output(struct spi_device *spi, int port, int size ,unsigned short value ) {

	int address = OFFSET_OUTPUT0_CPS_MC341_DSX + port;
	return cpsdio_spi_write(spi, address, size ,value );
}
EXPORT_SYMBOL_GPL(cpsdio_write_output);

/**
	@~English
	@brief This function get Digital Input Data.
	@param spi : SPI structure
	@param port : Port Number
	@param size : data size
	@param value : value
	@return true : 0, false : otherwise
	@~Japanese
	@brief デジタル入力のデータを取得する関数
	@param spi : SPI structure
	@param port : ポート番号
	@param size : データサイズ
	@param value : 値
	@return 成功: 0 , 失敗: それ以外
**/
static long cpsdio_read_input(struct spi_device *spi, int port, int size ,unsigned short *value ) {

	int address = OFFSET_INPUT0_CPS_MC341_DSX + port;
	return cpsdio_spi_read(spi, address, size ,value );
}

static int cpsdio_close(struct inode *inode, struct file *filp) {
	pr_info("Entered function: %s\n", __FUNCTION__);
	return 0;
}

/**
	@~English
	@brief This function get Digital Filter.
	@param spi : SPI structure
	@param value : digital filter value.
	@return true : 0, false : otherwise
	@~Japanese
	@brief デジタル入力のデジタルフィルタを取得する関数
	@param spi : SPI構造体
	@param value : デジタルフィルタの取得値
	@return 成功: 0 , 失敗: それ以外
**/
static long cpsdio_get_digital_filter( struct spi_device *spi, unsigned char *value )
{
	long lRet = 0;
	int address = OFFSET_DIGITALFILTER_CPS_MC341_DSX;
	unsigned short valw;

	lRet = cpsdio_spi_read(spi, address, 1 ,&valw );

	if( lRet ) return lRet;

	*value = valw;
	return 0;
}

/**
	@~English
	@brief This function set Digital Filter.
	@param spi : SPI structure
	@param value : digital filter value.
	@return true : 0, false : otherwise
	@~Japanese
	@brief デジタル入力のデジタルフィルタを設定する関数
	@param spi : SPI構造体
	@param value : デジタルフィルタの取得値
	@return 成功: 0 , 失敗: それ以外
**/
static long cpsdio_set_digital_filter( struct spi_device *spi, unsigned char value )
{
	int address = OFFSET_DIGITALFILTER_CPS_MC341_DSX;

	return cpsdio_spi_write(spi, address, 1 ,(unsigned short)value );
}

/**
	@~English
	@brief cpsdio_ioctl
	@param filp : struct file pointer
	@param cmd : iocontrol command
	@param arg : argument
	@return Success 0, Failed:otherwise 0. (see errno.h)
	@~Japanese
	@brief cpsdio_ioctl
	@param filp : file構造体ポインタ
	@param cmd : I/O コントロールコマンド
	@param arg : 引数
	@return 成功:0 , 失敗:0以外 (errno.h参照)
**/
static long cpsdio_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
	struct device *dev;
	struct spi_device *spi;
	struct spi_master *master;
	struct CPSDIO_SPI_drvdata *data;
	char str[128];

	long lRet = 0;
	unsigned char valb = 0;
	unsigned short valw = 0;

	struct cpsdio_ioctl_arg ioc;

	master = spi_busnum_to_master(CPSDIO_SPI_info.bus_num);
	snprintf(str, sizeof(str), "%s.%u", dev_name(&master->dev),
			CPSDIO_SPI_info.chip_select);

	dev = bus_find_device_by_name(&spi_bus_type, NULL, str);
	spi = to_spi_device(dev);
	data = (struct CPSDIO_SPI_drvdata *)spi_get_drvdata(spi);

	memset( &ioc, 0 , sizeof(ioc) );

	switch (cmd) {
		case IOCTL_CPSDIO_INP_PORT:
			if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
				return -EFAULT;
			}
			if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
				return -EFAULT;
			}
			mutex_lock( &data->lock );
			lRet = cpsdio_read_input(spi, ioc.port, 1, &valw);
			ioc.val = valw;
			mutex_unlock( &data->lock );

			if( lRet )	return lRet;

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
			mutex_lock( &data->lock );
			lRet = cpsdio_write_output(spi, ioc.port, 1, ioc.val);
			mutex_unlock( &data->lock );

			if( lRet ) return lRet;

			break;
		case IOCTL_CPSDIO_SET_FILTER:
			if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ) {
				return -EFAULT;
			}

			if (copy_from_user(&ioc, (int __user *) arg, sizeof(ioc))) {
				return -EFAULT;
			}
			mutex_lock(&data->lock);
			lRet = cpsdio_set_digital_filter(spi, (unsigned char)ioc.val);
			mutex_unlock(&data->lock);
			if( lRet ) return lRet;
			break;

		case IOCTL_CPSDIO_GET_FILTER:
			if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ) {
				return -EFAULT;
			}
			if (copy_from_user(&ioc, (int __user *) arg, sizeof(ioc))) {
				return -EFAULT;
			}
			mutex_lock(&data->lock);
			lRet = cpsdio_get_digital_filter(spi,&valb);
			ioc.val = valb;
			mutex_unlock(&data->lock);

			if( lRet ) return lRet;

			if (copy_to_user((int __user *) arg, &ioc, sizeof(ioc))) {
				return -EFAULT;
			}
			break;
	}
	return 0;
}

static struct file_operations cpsdio_spi_fops = { .owner = THIS_MODULE,
		.open = cpsdio_open,
		.release = cpsdio_close,
		.unlocked_ioctl = cpsdio_ioctl,
};

static int CPSDIO_SPI_register_dev(void) {
	dev_t dev = MKDEV(CPSDIO_SPI_MAJOR, CPSDIO_SPI_MINOR);
	int ret = 0;
	struct device *devlp = NULL;

/*	Registration of character device number*/
	ret = alloc_chrdev_region(&dev, 0, CPSDIO_SPI_NUM_DEVS,
			CPSDIO_SPI_DRIVER_NAME);
	if (ret < 0) {
		pr_err( "alloc_chrdev_region failed.\n");
		return ret;
	}
	cpsdio_spi_major = MAJOR(dev);
	cpsdio_spi_minor = MINOR(dev);

/*	Registration of character device and operation function*/
	cdev_init(&cpsdio_spi_cdev, &cpsdio_spi_fops);
	cpsdio_spi_cdev.owner = THIS_MODULE;
	cpsdio_spi_cdev.ops = &cpsdio_spi_fops;
	ret = cdev_add(&cpsdio_spi_cdev, dev, CPSDIO_SPI_NUM_DEVS);
	if (ret) {
		pr_err(" cpsdio : driver init. devices error \n");
		unregister_chrdev_region(dev, CPSDIO_SPI_NUM_DEVS);
		return ret;
	}

/*	Creating a sysfs (/ sys) class*/
	cpsdio_spi_class = class_create(THIS_MODULE, CPSDIO_SPI_DRIVER_NAME);
	if (IS_ERR(cpsdio_spi_class)) {
		pr_err(" cpsdio : driver init. devices error \n");
		cdev_del(&cpsdio_spi_cdev);
		unregister_chrdev_region(dev, CPSDIO_SPI_NUM_DEVS);
		return PTR_ERR(cpsdio_spi_class);
	}

/*	Registration to the sysfs*/
	devlp = device_create(cpsdio_spi_class, NULL, dev, NULL,
			CPSDIO_SPI_DRIVER_NAME"%d", 0);
	if (IS_ERR(devlp)) {
		pr_err(" cpsdio : driver init. devices error \n");
		cdev_del(&cpsdio_spi_cdev);
		unregister_chrdev_region(dev, CPSDIO_SPI_NUM_DEVS);
		return PTR_ERR(devlp);
	}
	return 0;
}

static int CPSDIO_SPI_probe(struct spi_device *spi) {
	struct CPSDIO_SPI_drvdata *data;

/*	Setting the SPI*/
	spi->max_speed_hz = CPSDIO_SPI_info.max_speed_hz;
	spi->mode = CPSDIO_SPI_info.mode;
	spi->bits_per_word = 8;
	if (spi_setup(spi)) {
		pr_err( "spi_setup returned error\n");
		return -ENODEV;
	}
	data = kzalloc(sizeof(struct CPSDIO_SPI_drvdata), GFP_KERNEL);
	if (data == NULL) {
		pr_err( "%s: no memory\n", __func__ );
		return -ENODEV;
	}
	data->spi = spi;
	mutex_init(&data->lock);

/*	Save CPSDIO_SPI_drvdata structure*/
	spi_set_drvdata(spi, data);

/*	Registration of the device node*/
	CPSDIO_SPI_register_dev();

	pr_info( "cpsdio_spi driver registered\n");

	return 0;
}

static int CPSDIO_SPI_remove(struct spi_device *spi) {
	struct CPSDIO_SPI_drvdata *data;
//	unsigned char tx[]={0x01, 0x2C, 0x00, 0x00};

	data = (struct CPSDIO_SPI_drvdata *) spi_get_drvdata(spi);

	/*	DIO LED initialization processing*/
	cpsdio_write_output(spi, 0, 2, 0x0000 );

	kfree(data);
	pr_info( "cpsdio_spi driver removed\n");

	return 0;
}

static struct spi_device_id CPSDIO_SPI_id[] = {
		{ "CPSDIO_SPI", 0 },
		{ },
};
MODULE_DEVICE_TABLE(spi, CPSDIO_SPI_id);

static struct spi_driver CPSDIO_SPI_driver = {
		.driver = {
				.name =	CPSDIO_SPI_DRIVER_NAME,
				.owner = THIS_MODULE,
		},
		.id_table =	CPSDIO_SPI_id,
		.probe = CPSDIO_SPI_probe,
		.remove = CPSDIO_SPI_remove,
};

static void spi_remove_device(struct spi_master *master, unsigned int cs) {
	struct device *dev;
	char str[128];

	snprintf(str, sizeof(str), "%s.%u", dev_name(&master->dev), cs);
/*	Find SPI device*/
	dev = bus_find_device_by_name(&spi_bus_type, NULL, str);

/*	Delete any SPI device*/
	if (dev) {
		pr_info( "Delete %s\n", str);
		device_del(dev);
	}
}

static int CPSDIO_SPI_init(void) {
	struct spi_master *master;
	struct spi_device *spi_device;

	spi_register_driver(&CPSDIO_SPI_driver);

	CPSDIO_SPI_info.bus_num = spi_bus_num;
	CPSDIO_SPI_info.chip_select = spi_chip_select;

	master = spi_busnum_to_master(CPSDIO_SPI_info.bus_num);
	if (!master) {
		pr_err( "spi_busnum_to_master returned NULL\n");
		goto init_err;
	}
/*	Remove because spidev 2.0 in the initial state is occupying*/
	spi_remove_device(master, CPSDIO_SPI_info.chip_select);
	spi_device = spi_new_device(master, &CPSDIO_SPI_info);
	if (!spi_device) {
		pr_err( "spi_new_device returned NULL\n" );
		goto init_err;
	}
	return 0;
init_err:
	spi_unregister_driver(&CPSDIO_SPI_driver);
	return -ENODEV;
}
module_init(CPSDIO_SPI_init);

static void CPSDIO_SPI_exit(void) {
	struct spi_master *master;
	dev_t dev_exit;

	master = spi_busnum_to_master(CPSDIO_SPI_info.bus_num);

/*	Delete SPI devices*/
	if (master) {
		spi_remove_device(master, CPSDIO_SPI_info.chip_select);
	} else {
		pr_err( "CPSDIO_SPI remove error\n");
	}
	spi_unregister_driver(&CPSDIO_SPI_driver);

/*	And deletion of the device, deletion of the class*/
	dev_exit = MKDEV(cpsdio_spi_major, cpsdio_spi_minor);
	device_destroy(cpsdio_spi_class, dev_exit);
	class_destroy(cpsdio_spi_class);

/*	Deleting a character device, the return of the character device number*/
	cdev_del(&cpsdio_spi_cdev);
	unregister_chrdev_region(dev_exit, CPSDIO_SPI_NUM_DEVS);
}
module_exit(CPSDIO_SPI_exit);

MODULE_AUTHOR("shinichiro nagasato");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CONTEC CONPROSYS Digital I/O driver for SPI");
MODULE_VERSION(DRV_VERSION);
