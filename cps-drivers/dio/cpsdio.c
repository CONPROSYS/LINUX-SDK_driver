/*
 *  Driver for CPS-DIO digital I/O
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

#ifdef CONFIG_CONPROSYS_SDK
 #include "../include/cps_common_io.h"
 #include "../include/cps.h"
 #include "../include/cps_ids.h"
 #include "../include/cps_extfunc.h"

 #include "../include/cpsdio.h"

#else
 #include "../../include/cps_common_io.h"
 #include "../../include/cps.h"
 #include "../../include/cps_ids.h"
 #include "../../include/cps_extfunc.h"

 #include "../../include/cpsdio.h"

#endif

#define DRV_VERSION	"1.0.4"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CONTEC CONPROSYS Digital I/O driver");
MODULE_AUTHOR("syunsuke okamoto");
MODULE_VERSION(DRV_VERSION);

#define CPSDIO_DRIVER_NAME "cpsdio"

/**
	@struct cps_dio_data
	@~English
	@brief CPSDIO driver's File
	@~Japanese
	@brief CPSDIO ドライバファイル構造体
**/
typedef struct __cpsdio_driver_file{
	spinlock_t		lock; 				///< lock
	struct task_struct *int_callback;
	unsigned ref;						///< reference count 

	unsigned int node;					///< Device Node
	unsigned long localAddr; 			///< local Address
	unsigned char __iomem *baseAddr;	///< Memory Address
	CPSDIO_DEV_DATA data;				///< Device Data

}CPSDIO_DRV_FILE,*PCPSDIO_DRV_FILE;

static	int *notFirstOpenFlg = NULL;		// Ver.1.0.0 segmentation fault暫定対策フラグ

/*!
 @~English
 @name DebugPrint macro
 @~Japanese
 @name デバッグ用表示マクロ
*/
/// @{
#if 0
#define DEBUG_CPSDIO_OPEN(fmt...)	printk(fmt)
#else
#define DEBUG_CPSDIO_OPEN(fmt...)	do { } while (0)
#endif

#if 0
#define DEBUG_CPSDIO_IOCTL(fmt...)	printk(fmt)
#else
#define DEBUG_CPSDIO_IOCTL(fmt...)	do { } while (0)
#endif

#if 0
#define DEBUG_CPSDIO_INTERRUPT_CHECK(fmt...)	printk(fmt)
#else
#define DEBUG_CPSDIO_INTERRUPT_CHECK(fmt...)	do { } while (0)
#endif

/// @}
///
static int cpsdio_max_devs;		///< device count
static int cpsdio_major = 0;	///< major number
static int cpsdio_minor = 0;	///< minor number

static struct cdev cpsdio_cdev;
static struct class *cpsdio_class = NULL;

static dev_t cpsdio_dev;

/**
	@struct cps_dio_data
	@~English
	@brief CPSDIO driver's Data
	@~Japanese
	@brief CPSDIO ドライバデータ構造体
**/
static const CPSDIO_DEV_DATA cps_dio_data[] = {
	{
		.Name = "CPS-DIO-0808L",
		.ProductNumber = CPS_DEVICE_DIO0808L ,
		.inPortNum = 8,
		.outPortNum = 8,
		.isInternalPower = 0
	},
	{
		.Name = "CPS-DIO-0808BL",
		.ProductNumber = CPS_DEVICE_DIO0808BL,
		.inPortNum = 8,
		.outPortNum = 8,
		.isInternalPower = 1,
	},
	{
		.Name = "CPS-DIO-0808RL",
		.ProductNumber = CPS_DEVICE_DIO0808RL,
		.inPortNum = 8,
		.outPortNum = 8,
		.isInternalPower = 0,
	},
	{
		.Name = "CPS-RRY-4PCC",
		.ProductNumber = CPS_DEVICE_RRY_4PCC,
		.inPortNum = 0,
		.outPortNum = 4,
		.isInternalPower = 0,
	},
	{
		.Name = "CPS-DIO-0808RBL",
		.ProductNumber = CPS_DEVICE_DIO0808RBL,
		.inPortNum = 8,
		.outPortNum = 8,
		.isInternalPower = 1,
	},
	{
		.Name = "CPS-DI-16L",
		.ProductNumber = CPS_DEVICE_DI16L ,
		.inPortNum = 16,
		.outPortNum = 0,
		.isInternalPower = 0
	},
	{
		.Name = "CPS-DI-16BL",
		.ProductNumber = CPS_DEVICE_DI16BL,
		.inPortNum = 16,
		.outPortNum = 0,
		.isInternalPower = 1,
	},
	{
		.Name = "CPS-DO-16L",
		.ProductNumber = CPS_DEVICE_DO16L ,
		.inPortNum = 0,
		.outPortNum = 16,
		.isInternalPower = 0
	},
	{
		.Name = "CPS-DO-16BL",
		.ProductNumber = CPS_DEVICE_DO16BL,
		.inPortNum = 0,
		.outPortNum = 16,
		.isInternalPower = 1,
	},
	{
		.Name = "CPS-DI-16RL",
		.ProductNumber = CPS_DEVICE_DI16RL ,
		.inPortNum = 16,
		.outPortNum = 0,
		.isInternalPower = 0
	},
	{
		.Name = "CPS-DI-16RBL",
		.ProductNumber = CPS_DEVICE_DI16RBL,
		.inPortNum = 16,
		.outPortNum = 0,
		.isInternalPower = 1,
	},
	{
		.Name = "CPS-DO-16RL",
		.ProductNumber = CPS_DEVICE_DO16RL ,
		.inPortNum = 0,
		.outPortNum = 16,
		.isInternalPower = 0
	},
	{
		.Name = "CPS-DO-16RBL",
		.ProductNumber = CPS_DEVICE_DO16RBL,
		.inPortNum = 0,
		.outPortNum = 16,
		.isInternalPower = 1,
	},
	{
		.Name = "",
		.ProductNumber = -1,
		.inPortNum = -1,
		.outPortNum = -1,
		.isInternalPower = -1,
	},
};

#include "cpsdio_devdata.h"

/**
	@~English
	@brief This function get Digital Input Data.
	@param BaseAddr : Base Address
	@param port : Port Number
	@param val : value
	@return true : 0
	@~Japanese
	@brief デジタル入力のデータを取得する関数
	@param BaseAddr : ベースアドレス
	@param port : ポート番号
	@param val : 値
	@return 成功 : 0
**/
static long cpsdio_read_digital_input( unsigned long BaseAddr, unsigned int port, unsigned char *val )
{
	cps_common_inpb( (unsigned long)( BaseAddr + port + OFFSET_INPUT0_CPS_DIO_0808 ) , val );
	return 0;
}

/**
	@~English
	@brief This function get Digital Output Echo Data.
	@param BaseAddr : Base Address
	@param port : Port Number
	@param val : value
	@return true : 0
	@~Japanese
	@brief デジタル出力のエコーバック・データを取得する関数
	@param BaseAddr : ベースアドレス
	@param port : ポート番号
	@param val : 値
	@return 成功 : 0
**/
static long cpsdio_read_digital_output_echo( unsigned long BaseAddr, unsigned int port, unsigned char *val )
{
	cps_common_inpb( (unsigned long)( BaseAddr + port + OFFSET_OUTPUT_ECHO0_CPS_DIO_0808 ) , val );
	return 0;
}

/**
	@~English
	@brief This function set Digital Output Data.
	@param BaseAddr : Base Address
	@param port : Port Number
	@param val : value
	@return true : 0
	@~Japanese
	@brief デジタル出力のデータを設定する関数
	@param BaseAddr : ベースアドレス
	@param port : ポート番号
	@param val : 値
	@return 成功 : 0
**/
static long cpsdio_write_digital_output( unsigned long BaseAddr, unsigned int port, unsigned char val )
{
	cps_common_outb( (unsigned long)( BaseAddr + port + OFFSET_OUTPUT_ECHO0_CPS_DIO_0808 ) , val );
	return 0;
}

/**
	@~English
	@brief This function set Power Supply.　（Internal or External).
	@param BaseAddr : Base Address
	@param isOn : select Power Supply ( 1 : Internal, 0 : External )
	@return true : 0
	@~Japanese
	@brief デジタル出力の内部電源か外部電源かを設定する関数
	@param BaseAddr : ベースアドレス
	@param isOn : 内部電源か外部電源か（1 : 内部電源 , 0: 外部電源 )
	@return 成功 : 0
**/
static long cpsdio_set_internal_power_supply( unsigned long BaseAddr, unsigned char isOn )
{
	cps_common_outb( (unsigned long)( BaseAddr + OFFSET_INTERNAL_POWER_SUPPLY_CPS_DIO_0808 ) , isOn );
	return 0;
}

/**
	@~English
	@brief This function get Power Supply.　（Internal or External).
	@param BaseAddr : Base Address
	@param isOn : select Power Supply ( 1 : Internal, 0 : External )
	@return true : 0
	@~Japanese
	@brief デジタル出力の内部電源か外部電源かを取得する関数
	@param BaseAddr : ベースアドレス
	@param isOn : 内部電源か外部電源か（1 : 内部電源 , 0: 外部電源 )
	@return 成功 : 0
**/
static long cpsdio_get_internal_power_supply( unsigned long BaseAddr, unsigned char *isOn )
{
	cps_common_inpb( (unsigned long)( BaseAddr + OFFSET_INTERNAL_POWER_SUPPLY_CPS_DIO_0808 ) , isOn );
	return 0;
}

/**
	@~English
	@brief This function set Digital Filter.
	@param BaseAddr : Base Address
	@param filter : digital filter value.
	@return true : 0
	@~Japanese
	@brief デジタル入力のデジタルフィルタを設定する関数
	@param BaseAddr : ベースアドレス
	@param filter : デジタルフィルタの設定値
	@return 成功 : 0
**/
static long cpsdio_set_digital_filter( unsigned long BaseAddr, unsigned char filter )
{
	cps_common_outb( (unsigned long)( BaseAddr + OFFSET_DIGITALFILTER_CPS_DIO_0808 ) , filter );
	return 0;
}

/**
	@~English
	@brief This function get Digital Filter.
	@param BaseAddr : Base Address
	@param filter : digital filter value.
	@return true : 0
	@~Japanese
	@brief デジタル入力のデジタルフィルタを取得する関数
	@param BaseAddr : ベースアドレス
	@param filter : デジタルフィルタの取得値
	@return 成功 : 0
**/
static long cpsdio_get_digital_filter( unsigned long BaseAddr, unsigned char *filter )
{
	cps_common_inpb( (unsigned long)( BaseAddr + OFFSET_DIGITALFILTER_CPS_DIO_0808 ) , filter );
	return 0;
}

/**
	@~English
	@brief This function get Interrupt Status.
	@param BaseAddr : Base Address
	@param wStatus : Ineterrupt Status.
	@return true : 0
	@~Japanese
	@brief デジタル入力の割り込みステータスを取得する関数
	@param BaseAddr : ベースアドレス
	@param wStatus : 割り込みステータス
	@return 成功 : 0
**/
static long cpsdio_read_interrupt_status( unsigned long BaseAddr, unsigned short *wStatus )
{
	cps_common_inpw( (unsigned long)( BaseAddr + OFFSET_INTERRUPT_STATUS_CPS_DIO_0808 ) , (unsigned short*) wStatus );
	return 0;
}

/**
	@~English
	@brief This function get Interrupt mask.
	@param BaseAddr : Base Address
	@param wStatus : Ineterrupt Mask Status.
	@return true : 0
	@~Japanese
	@brief デジタル入力の割り込みマスクを設定する関数
	@param BaseAddr : ベースアドレス
	@param wStatus : 割り込みマスク
	@return 成功 : 0
**/
static long cpsdio_write_interrupt_mask( unsigned long BaseAddr, unsigned short wStatus )
{
	cps_common_outw( (unsigned long)(BaseAddr + OFFSET_INTERRUPT_STATUS_CPS_DIO_0808 ) , wStatus );
	return 0;
}

/**
	@~English
	@brief This function get Interrupt edge.
	@param BaseAddr : Base Address
	@param wStatus : Ineterrupt Edge Status.
	@return true : 0
	@~Japanese
	@brief デジタル入力の割り込みエッジを設定する関数
	@param BaseAddr : ベースアドレス
	@param wStatus : 割り込みエッジ
	@return 成功 : 0
**/
static long cpsdio_set_interrupt_edge( unsigned long BaseAddr, unsigned short wStatus )
{
	cps_common_outw( (unsigned long)(BaseAddr + OFFSET_INTERRUPT_EGDE_CPS_DIO_0808 ) , wStatus );
	return 0;
}

/**
	@~English
	@brief This function get Interrupt edge.
	@param BaseAddr : Base Address
	@param wStatus : Ineterrupt Edge Status.
	@return true : 0
	@~Japanese
	@brief デジタル入力の割り込みエッジを取得する関数
	@param BaseAddr : ベースアドレス
	@param wStatus : 割り込みエッジ
	@return 成功 : 0
**/
static long cpsdio_get_interrupt_edge( unsigned long BaseAddr, unsigned short *wStatus )
{
	cps_common_inpw( (unsigned long)(BaseAddr + OFFSET_INTERRUPT_EGDE_CPS_DIO_0808 ) , wStatus );
	return 0;
}

#define CPSDIO_TYPE_INPUT 0	///< 入力
#define CPSDIO_TYPE_OUTPUT 1	///< 出力

/**
	@~English
	@brief This function get Port Number with Digital I/O device.
	@param node : device node
	@param type : CPSDIO_TYPE_INPUT : input, CPS_TYPE_OUTPUT : output
	@param wNum : port number
	@return true : 0
	@~Japanese
	@brief デジタル機器のポート数を取得する関数
	@param node : ノード
	@param type : CPSDIO_TYPE_INPUT : 入力, CPS_TYPE_OUTPUT : 出力
	@param wNum : ポート数
	@return 成功 : 0
**/
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

/**
	@~English
	@brief This function get Device Name with Digital I/O device.
	@param node : device node
	@param devName : Device Name
	@return true : 0
	@~Japanese
	@brief デジタル機器のデバイス名を取得する関数
	@param node : ノード
	@param devName : デバイス名
	@return 成功 : 0
**/
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

/**
	@~English
	@brief cpsdio_isr_func
	@param irq : interrupt
	@param dev_instance : device instance
	@return intreturn ( IRQ_HANDLED or IRQ_NONE )
	@~Japanese
	@brief cpsdio 割り込み処理
	@param irq : IRQ番号
	@param dev_instance : デバイス・インスタンス
	@return IRQ_HANDLED か, IRQ_NONE
**/
irqreturn_t cpsdio_isr_func(int irq, void *dev_instance){

	unsigned short wStatus;
	int handled = 0;

	PCPSDIO_DRV_FILE dev =(PCPSDIO_DRV_FILE) dev_instance;
	
	// Ver.0.9.5 Don't insert interrupt "xx callbacks suppressed" by IRQ_NONE.
	if( !dev ){
		DEBUG_CPSDIO_INTERRUPT_CHECK(KERN_INFO"This interrupt is not CONPROSYS DIO Device.");
		goto END_OF_INTERRUPT_CPSDIO;
	}

	DEBUG_CPSDIO_INTERRUPT_CHECK(KERN_INFO"--- isr_func: isDio=%u\n", contec_mcs341_device_IsCategory( dev->node , CPS_CATEGORY_DIO ) );

	if( !contec_mcs341_device_IsCategory( dev->node , CPS_CATEGORY_DIO ) ){
		DEBUG_CPSDIO_INTERRUPT_CHECK(KERN_INFO"--- IRQ_NONE\n");
		goto END_OF_INTERRUPT_CPSDIO;
	}

	spin_lock(&dev->lock);

	cpsdio_read_interrupt_status( (unsigned long)dev->baseAddr, &wStatus);

	if( !wStatus ){
		DEBUG_CPSDIO_INTERRUPT_CHECK(KERN_INFO"--- wStatus = 0 IRQ_NONE\n");
		goto END_OF_INTERRUPT_SPIN_UNLOCK_CPSDIO;
	}

	handled = 1;

	if( dev->int_callback != NULL ){
			// sending user callback function ( kernel context to user context <used signal SIGUSR1 > )
			DEBUG_CPSDIO_INTERRUPT_CHECK(KERN_INFO"--- send_sig (1): wStatus=%02X\n", 
							wStatus);
			send_sig( SIGUSR2, dev->int_callback, 1 );
			DEBUG_CPSDIO_INTERRUPT_CHECK(KERN_INFO"--- send_sig (2): wStatus=%02X\n",
							wStatus);
	}

END_OF_INTERRUPT_SPIN_UNLOCK_CPSDIO:
	spin_unlock(&dev->lock);

END_OF_INTERRUPT_CPSDIO:

	if( IRQ_RETVAL(handled) ){
		if(printk_ratelimit())
			printk("cpsdio Device Number:%d IRQ interrupt !\n",( dev->node ) );
	}

	return IRQ_RETVAL(handled);
}


/***** file operation functions *******************************/

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
static long cpsdio_ioctl( struct file *filp, unsigned int cmd, unsigned long arg )
{
	PCPSDIO_DRV_FILE dev = filp->private_data;
	unsigned char valb = 0;
	unsigned short valw = 0;
	unsigned long flags;

	struct cpsdio_ioctl_arg ioc;
	struct cpsdio_ioctl_string_arg ioc_str; // Ver.1.0.2
	memset( &ioc, 0 , sizeof(ioc) );
	memset( &ioc_str, 0 , sizeof(ioc_str) );// Ver.1.0.2

	if ( dev == (PCPSDIO_DRV_FILE)NULL ){
		DEBUG_CPSDIO_IOCTL(KERN_INFO"CPSDIO_DRV_FILE NULL POINTER.");
		return -EFAULT;
	}
	switch( cmd ){

		case IOCTL_CPSDIO_INP_PORT:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					cpsdio_read_digital_input((unsigned long)dev->baseAddr , ioc.port, &valb );
					ioc.val = (unsigned int) valb;
					spin_unlock_irqrestore(&dev->lock, flags);

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
					spin_lock_irqsave(&dev->lock, flags);
					valb = (unsigned char) ioc.val;
					cpsdio_write_digital_output( (unsigned long)dev->baseAddr , ioc.port, valb );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;
		case IOCTL_CPSDIO_OUT_PORT_ECHO:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					cpsdio_read_digital_output_echo((unsigned long)dev->baseAddr , ioc.port, &valb );
					ioc.val = (unsigned int) valb;
					spin_unlock_irqrestore(&dev->lock, flags);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSDIO_SET_INTERNAL_POW:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					spin_lock_irqsave(&dev->lock, flags);
					valb = (unsigned char) ioc.val;
					cpsdio_set_internal_power_supply( (unsigned long)dev->baseAddr , valb );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSDIO_GET_INTERNAL_POW:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					cpsdio_get_internal_power_supply( (unsigned long)dev->baseAddr , &valb );
					ioc.val = (unsigned int) valb;
					spin_unlock_irqrestore(&dev->lock, flags);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSDIO_SET_FILTER:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					spin_lock_irqsave(&dev->lock, flags);
					valb = (unsigned char) ioc.val;
					cpsdio_set_digital_filter( (unsigned long)dev->baseAddr , valb );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSDIO_GET_FILTER:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					cpsdio_get_digital_filter( (unsigned long)dev->baseAddr , &valb );
					ioc.val = (unsigned int) valb;
					spin_unlock_irqrestore(&dev->lock, flags);
					
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
					spin_lock_irqsave(&dev->lock, flags);
					valw = (unsigned short) ioc.val;
					DEBUG_CPSDIO_INTERRUPT_CHECK(KERN_INFO"--- SET_INT_MASK: val=%02X\n", valw);
					cpsdio_write_interrupt_mask( (unsigned long)dev->baseAddr , valw );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSDIO_GET_INT_STATUS:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					cpsdio_read_interrupt_status( (unsigned long)dev->baseAddr , &valw );
					ioc.val = (unsigned int) valw;
					spin_unlock_irqrestore(&dev->lock, flags);
					
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
					spin_lock_irqsave(&dev->lock, flags);
					cpsdio_get_interrupt_edge( (unsigned long)dev->baseAddr , &valw );
					ioc.val = (unsigned int) valw;
					spin_unlock_irqrestore(&dev->lock, flags);
					
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
					spin_lock_irqsave(&dev->lock, flags);
					valw = (unsigned short) ioc.val;
					DEBUG_CPSDIO_INTERRUPT_CHECK(KERN_INFO"--- SET_INT_EGDE: val=%02X\n", valw);
					cpsdio_set_interrupt_edge( (unsigned long)dev->baseAddr , valw );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSDIO_GET_INP_PORTNUM:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					cpsdio_get_dio_portnum( dev->node ,CPSDIO_TYPE_INPUT, &valw );
					ioc.val = (unsigned int) valw;
					spin_unlock_irqrestore(&dev->lock, flags);
					
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
					spin_lock_irqsave(&dev->lock, flags);
					cpsdio_get_dio_portnum( dev->node ,CPSDIO_TYPE_OUTPUT, &valw );
					ioc.val = (unsigned int) valw;
					spin_unlock_irqrestore(&dev->lock, flags);
					
					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSDIO_GET_DEVICE_NAME:
			// Ver.1.0.2 Modify using from cpsdio_ioctl_arg to cpsdio_ioctl_string_arg

					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc_str, (int __user *)arg, sizeof(ioc_str) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					cpsdio_get_dio_devname( dev->node , ioc_str.str );
					spin_unlock_irqrestore(&dev->lock, flags);
					
					if( copy_to_user( (int __user *)arg, &ioc_str, sizeof(ioc_str) ) ){
						return -EFAULT;
					}
					break;

///////////////////////// Ver.1.0.0 test interrupt
		case IOCTL_CPSDIO_SET_CALLBACK_PROCESS:
					{
						if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
							return -EFAULT;
						}
						if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
							return -EFAULT;
						}

						spin_lock_irqsave(&dev->lock, flags);

///////////////////////// debug 
//						dev->int_callback = find_task_by_vpid(ioc.val); // ( Caution : Change find_task_by_vpid function, EXPORT_SYMBOL_GPL. (See kernel/pid.c) )
						dev->int_callback = get_pid_task( find_get_pid(ioc.val), PIDTYPE_PID );
///////////////////////// debug

///////////////////////// debug
						DEBUG_CPSDIO_INTERRUPT_CHECK(KERN_INFO"--- SET_CALLBACK_PROCESS(get_pid_task): val=%08X, int_callback=%08lX\n", 
							ioc.val,
							(unsigned long)dev->int_callback);
///////////////////////// debug

						spin_unlock_irqrestore(&dev->lock, flags);
					
						if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
							return -EFAULT;
						}
					}
					break;
///////////////////////// debug
		case IOCTL_CPSDIO_GET_DRIVER_VERSION:
			// Ver.1.0.2 Add
			if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
				return -EFAULT;
			}
			if( copy_from_user( &ioc_str, (int __user *)arg, sizeof(ioc_str) ) ){
				return -EFAULT;
			}
			spin_lock_irqsave(&dev->lock, flags);
			strcpy(ioc_str.str, DRV_VERSION);
			spin_unlock_irqrestore(&dev->lock, flags);

			if( copy_to_user( (int __user *)arg, &ioc_str, sizeof(ioc_str) ) ){
				return -EFAULT;
			}
			break;
	}

	return 0;
}

/**
	@~English
	@brief This function is called by open user function.
	@param filp : struct file pointer
	@param inode : node parameter
	@return success: 0 , failed: otherwise 0
 	@~Japanese
	@brief この関数はOPEN関数で呼び出されます。
	@param filp : ファイル構造体ポインタ
	@param inode : ノード構造体ポインタ
	@return 成功: 0 , 失敗: 0以外
	@note filp->private_dataは プロセスに１個, inode->i_privateは nodeに1個
**/
static int cpsdio_open(struct inode *inode, struct file *filp )
{
	int ret;
	PCPSDIO_DRV_FILE dev;
	int cnt;
	unsigned char __iomem *allocMem;
	unsigned short product_id;
	int iRet = 0;
	int nodeNo = 0;// Ver.1.0.0 

	nodeNo = iminor( inode );// Ver.1.0.0 

	DEBUG_CPSDIO_OPEN(KERN_INFO"node %d\n",iminor( inode ) );

	if (notFirstOpenFlg[nodeNo]) {		// Ver.1.0.0 初回オープンでなければ（segmentation fault暫定対策）
		if ( inode->i_private != (PCPSDIO_DRV_FILE)NULL ){

			DEBUG_CPSDIO_OPEN(KERN_INFO"inode->private %x\n",(int)inode->i_private );

			dev = (PCPSDIO_DRV_FILE)inode->i_private;
			filp->private_data =  (PCPSDIO_DRV_FILE)dev;

			if( dev->ref ){
				dev->ref++;
				return 0;
			}else{
				return -EFAULT;
			}
		}
	}

	DEBUG_CPSDIO_OPEN(KERN_INFO"inode->private %x\n",(int)inode->i_private );

	filp->private_data = (PCPSDIO_DRV_FILE)kzalloc( sizeof(CPSDIO_DRV_FILE) , GFP_KERNEL );
	if( filp->private_data == (PCPSDIO_DRV_FILE)NULL ){
		iRet = -ENOMEM;
		goto NOT_MEM_PRIVATE_DATA;
	}
	dev = (PCPSDIO_DRV_FILE)filp->private_data;
	inode->i_private = dev;
	dev->int_callback = NULL;

	dev->node = iminor( inode );
	
	dev->localAddr = 0x08000010 + (dev->node + 1) * 0x100;

	allocMem = cps_common_mem_alloc( dev->localAddr, 0xF0, "cps-dio", CPS_COMMON_MEM_REGION );

	if( allocMem != NULL ){
		dev->baseAddr = allocMem;
	}else{
		iRet = -ENOMEM;
		goto NOT_IOMEM_ALLOCATION;
	}

	product_id = contec_mcs341_device_productid_get( dev->node );
	cnt = 0;
	do{
		if( cps_dio_data[cnt].ProductNumber == -1 ) {
			iRet = -EFAULT;
			DEBUG_CPSDIO_OPEN(KERN_INFO"product_id:%x", product_id);
			goto NOT_FOUND_DIO_PRODUCT;
		}
		if( cps_dio_data[cnt].ProductNumber == product_id ){
			dev->data = cps_dio_data[cnt];
			break;
		}
		cnt++;
	}while( 1 );

	ret = request_irq(AM335X_IRQ_NMI, cpsdio_isr_func, IRQF_SHARED, "cps-dio-intr", dev);

	if( ret ){
		DEBUG_CPSDIO_OPEN(" request_irq failed.(%x) \n",ret);
	}

	// spin_lock initialize
	spin_lock_init( &dev->lock );

	dev->ref = 1;
	notFirstOpenFlg[nodeNo]++;		// Ver.1.0.0 segmentation fault暫定対策フラグインクリメント

	return 0;
NOT_FOUND_DIO_PRODUCT:
	cps_common_mem_release( dev->localAddr,
		0xF0,
		dev->baseAddr ,
		CPS_COMMON_MEM_REGION);

NOT_IOMEM_ALLOCATION:
	kfree( dev );

NOT_MEM_PRIVATE_DATA:
 
	inode->i_private = (PCPSDIO_DRV_FILE)NULL;
	filp->private_data = (PCPSDIO_DRV_FILE)NULL;

	return iRet;
}

/**
	@~English
	@brief This function is called by close user function.
	@param filp : struct file pointer
	@param inode : node parameter
	@return success: 0 , failed: otherwise 0
 	@~Japanese
	@brief この関数はCLOSE関数で呼び出されます。
	@param filp : ファイル構造体ポインタ
	@param inode : ノード構造体ポインタ
	@return 成功: 0 , 失敗: 0以外
	@note filp->private_dataは プロセスに１個, inode->i_privateは nodeに1個
**/
static int cpsdio_close(struct inode * inode, struct file *filp ){

	PCPSDIO_DRV_FILE dev;

	if ( inode->i_private != (PCPSDIO_DRV_FILE)NULL ){
		dev = (PCPSDIO_DRV_FILE)inode->i_private;

		if( dev->ref > 0 ) dev->ref--;

		if( dev->ref == 0 ){

			free_irq(AM335X_IRQ_NMI, dev);

			cps_common_mem_release( dev->localAddr,
				0xF0,
				dev->baseAddr ,
				CPS_COMMON_MEM_REGION);
				
			kfree( dev );
			
			inode->i_private = (PCPSDIO_DRV_FILE)NULL;
		}
		filp->private_data = (PCPSDIO_DRV_FILE)NULL;
	}
	return 0;

}

/**
	@struct cpsaio_fops
	@brief CPSAIO file operations
**/
static struct file_operations cpsdio_fops = {
		.owner = THIS_MODULE,///< owner's name
		.open = cpsdio_open,///< open
		.release = cpsdio_close,///< close
		.unlocked_ioctl = cpsdio_ioctl,///< I/O Control
};

/**
	@~English
	@brief cpsdio init function.
	@return Success: 0, Failed: otherwise 0
	@~Japanese
	@brief cpsdio 初期化関数.
	@return 成功: 0, 失敗: 0以外
**/
static int cpsdio_init(void)
{

	dev_t dev = MKDEV(cpsdio_major , 0 );
	int ret = 0;
	int major;
	int cnt;
	struct device *devlp = NULL;
	int	dioNum = 0; // Ver.1.0.0 

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
			dioNum++;// Ver.1.0.0 
		}
	}

	// Ver.1.0.0 
	if (dioNum) {
		notFirstOpenFlg = (int *)kzalloc( sizeof(int) * dioNum, GFP_KERNEL );	// segmentation fault暫定対策フラグメモリ確保
	}

	return 0;
}

/**
	@~English
	@brief cpsdio exit function.
	@~Japanese
	@brief cpsdio 終了関数.
**/
static void cpsdio_exit(void)
{

	dev_t dev = MKDEV(cpsdio_major , 0 );
	int cnt;

	kfree(notFirstOpenFlg);		// Ver.1.0.0 segmentation fault暫定対策フラグメモリ解放

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
