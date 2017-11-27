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

#include <linux/slab.h>

#ifdef CONFIG_CONPROSYS_SDK
 #include "../include/cps_common_io.h"
 #include "../include/cps.h"
 #include "../include/cps_ids.h"
 #include "../include/cps_extfunc.h"

 #include "../include/cpsaio.h"

#else
 #include "../../include/cps_common_io.h"
 #include "../../include/cps.h"
 #include "../../include/cps_ids.h"
 #include "../../include/cps_extfunc.h"

 #include "../../include/cpsaio.h"

#endif
#define DRV_VERSION	"1.0.11"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CONTEC CONPROSYS Analog I/O driver");
MODULE_AUTHOR("syunsuke okamoto");
MODULE_VERSION(DRV_VERSION);


#define CPSAIO_DRIVER_NAME "cpsaio"

/**
	@struct cps_aio_data
	@~English
	@brief CPSAIO driver's File
	@~Japanese
	@brief CPSAIO ドライバファイル構造体
**/
typedef struct __cpsaio_driver_file{
	spinlock_t		lock; 				///< lock
	unsigned int ref;						///< reference count 

	unsigned int node;					///< Device Node
	unsigned long localAddr; 			///< local Address
	unsigned char __iomem *baseAddr;	///< Memory Address
	CPSAIO_DEV_DATA data;				///< Device Data

}CPSAIO_DRV_FILE,*PCPSAIO_DRV_FILE;

/*!
 @~English
 @name DebugPrint macro
 @~Japanese
 @name デバッグ用表示マクロ
*/
/// @{

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

#if 0
#define DEBUG_CPSAIO_READFIFO(fmt...)	printk(fmt)
#else
#define DEBUG_CPSAIO_READFIFO(fmt...)	do { } while (0)
#endif

#if 0
#define DEBUG_CPSAIO_IOCTL(fmt...)	printk(fmt)
#else
#define DEBUG_CPSAIO_IOCTL(fmt...)	do { } while (0)
#endif

#if 0
#define DEBUG_CPSAIO_INTERRUPT_CHECK(fmt...)	printk(fmt)
#else
#define DEBUG_CPSAIO_INTERRUPT_CHECK(fmt...)	do { } while (0)
#endif

/// @}

static int cpsaio_max_devs;			///< device count
static int cpsaio_major = 0;		///< major number
static int cpsaio_minor = 0; 		///< minor number

static struct cdev cpsaio_cdev;
static struct class *cpsaio_class = NULL;

static dev_t cpsaio_dev;

static	int *notFirstOpenFlg = NULL;		// Ver.1.0.8 segmentation fault暫定対策フラグ

//static struct device *cpsaio_devlp[CPS_DEVICE_MAX_NUM];

/**
	@struct cps_aio_data
	@~English
	@brief CPSAIO driver's Data
	@~Japanese
	@brief CPSAIO ドライバデータ構造体
**/
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
	},
	{
		.Name = "CPS-AI-1608ALI",
		.ProductNumber = CPS_DEVICE_AI_1608ALI ,
		.Ability = CPS_AIO_ABILITY_ECU | CPS_AIO_ABILITY_AI | CPS_AIO_ABILITY_MEM,
		.ai = {
			.Resolution = 16,
			.Channel = 8,
		},
		.ao = {
			.Resolution = 0,
			.Channel = 0,
		},
	},
	{
		.Name = "CPS-AO-1604VLI",
		.ProductNumber = CPS_DEVICE_AO_1604VLI,
		.Ability = CPS_AIO_ABILITY_ECU | CPS_AIO_ABILITY_AO | CPS_AIO_ABILITY_MEM,
		.ai = {
			.Resolution = 0,
			.Channel = 0,
		},
		.ao = {
			.Resolution = 16,
			.Channel = 4,
		},
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
	},
};

#include "cpsaio_devdata.h"

/**
  @~English
	@brief DAC161S055 0-FF not enough values.
	@param dat : Analog Output Data ( from 0 to 65535 )
	@return tmpDat : Analog Output Data ( from 256 to 65535 )
	@warning CPS-AO-1604LI only!
	@~Japanese
	@brief 0-65535の範囲を 256-65535に計算する関数。
	@attention DAC161S055 ばらつきがひどいため ０からFFまで値を使用しない.
	@param dat : アナログ出力値 ( 0 から 65535まで )
	@return tmpDat : アナログ出力値 ( 256 から 65535まで )
	@warning CPS-AO-1604LI専用
**/
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
	@~English
	@param node : device node
	@return true : product id, false : -1
	@~Japanese
	@param node : デバイスノード
	@return 成功 : プロダクトID , 失敗 : -1
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
  @~English
 	@brief This function writes analog input data.
	@param BaseAddr : base address
	@param val : value
	@return true : 0
	@~Japanese
	@brief AIのデータを取得する関数
	@param BaseAddr : ベースアドレス
	@param val : 値
	@return 成功 : 0
**/ 
static long cpsaio_read_ai_data( unsigned long BaseAddr, unsigned short *val )
{
	unsigned long ulAddr = (unsigned long)( BaseAddr + OFFSET_AIDATA_CPS_AIO );
	cps_common_inpw( ulAddr , val );
	DEBUG_CPSAIO_COMMAND(KERN_INFO"cpsaio_read_ai_data()[%lx]=%x\n",ulAddr, *val );
	return 0;
}

/**
	@~English
	@brief This function writes analog output data.
	@param BaseAddr : base address
	@param val : value
	@return true : 0
	@~Japanese
	@brief AOのデータを取得する関数
	@param BaseAddr : ベースアドレス
	@param val : 値
	@return 成功 : 0
**/ 
static long cpsaio_write_ao_data( unsigned long BaseAddr, unsigned short val )
{
	unsigned long ulAddr = (unsigned long)( BaseAddr + OFFSET_AODATA_CPS_AIO );
	DEBUG_CPSAIO_COMMAND(KERN_INFO"cpsaio_write_ao_data()[%lx]=%x\n",ulAddr, val );
	cps_common_outw( ulAddr, val );
	return 0;
}

/**
	@~English
	@brief This function reads analog input status.
	@param BaseAddr : base address
	@param wStatus : status
	@return true : 0
	@~Japanese
	@brief AIのステータスを取得する関数
	@param BaseAddr : ベースアドレス
	@param wStatus : ステータス
	@return 成功 : 0
**/ 
static long cpsaio_read_ai_status( unsigned long BaseAddr, unsigned short *wStatus )
{
	unsigned long ulAddr = (unsigned long)( BaseAddr + OFFSET_AISTATUS_CPS_AIO );
	cps_common_inpw( ulAddr , wStatus );
	DEBUG_CPSAIO_COMMAND(KERN_INFO"cpsaio_read_ai_status()[%lx]=%x\n",ulAddr, *wStatus );
	return 0;
}

/**
	@~English
	@brief This function reads analog output status.
	@param BaseAddr : base address
	@param wStatus : status
	@return true : 0
	@~Japanese
	@brief AOのステータスを取得する関数
	@param BaseAddr : ベースアドレス
	@param wStatus : ステータス
	@return 成功 : 0
**/ 
static long cpsaio_read_ao_status( unsigned long BaseAddr, unsigned short *wStatus )
{
	unsigned long ulAddr = (unsigned long)( BaseAddr + OFFSET_AOSTATUS_CPS_AIO );
	cps_common_inpw( ulAddr , wStatus );
	DEBUG_CPSAIO_COMMAND(KERN_INFO"cpsaio_read_ao_status()[%lx]=%x\n",ulAddr, *wStatus );
	return 0;
}

// Ver.1.0.10 Add.
/**
	@~English
	@brief This function reads interrupt status.
	@param BaseAddr : base address
	@param wIntStatus : interrupt status
	@return true : 0
	@~Japanese
	@brief 割り込みのステータスを取得する関数
	@param BaseAddr : ベースアドレス
	@param wIntStatus : 割込ステータス
	@return 成功 : 0
**/
static long cpsaio_read_interrupt_status( unsigned long BaseAddr, unsigned short *wIntStatus )
{
	unsigned long ulAddr = (unsigned long)( BaseAddr + OFFSET_ECU_INTERRUPT_CHECK_CPS_AIO );
	cps_common_inpw( ulAddr , wIntStatus );
	DEBUG_CPSAIO_COMMAND(KERN_INFO"cpsaio_read_interrupt_status()[%lx]=%x\n",ulAddr, *wStatus );
	return 0;
}

/**
	@~English
	@param BaseAddr : base address
	@param wStatus : status
	@return true : 0
	@~Japanese
	@brief メモリのステータスを取得する関数
	@param BaseAddr : ベースアドレス
	@param wStatus : ステータス
	@return 成功 : 0
**/ 
static long cpsaio_read_mem_status( unsigned long BaseAddr, unsigned short *wStatus )
{
	cps_common_inpw( (unsigned long)( BaseAddr + OFFSET_MEMSTATUS_CPS_AIO ) , wStatus );
	DEBUG_CPSAIO_COMMAND(KERN_INFO"cpsaio_read_mem_status()[%lx]=%x\n",(unsigned long)( BaseAddr + OFFSET_MEMSTATUS_CPS_AIO ), *wStatus );
	return 0;
}


/**
	@~English
	@brief AIO Command Function.
	@param BaseAddr : base address
	@param isReadWrite : write, read, or call flag
	@param isEcu	: ecu or command flag
	@param size : size 1 ,2 , or 4.
	@param wCommand : command address
	@param vData : data
	@return true : 0
	@~Japanese
	@brief AIO コマンド関数
	@param BaseAddr : ベースアドレス
	@param isReadWrite : 書き込みか読み込みか呼び出し、いずれかのフラグ
	@param isEcu	: ECUか　COMMANDかのフラグ
	@param size : サイズ ( 1 か 2 か 4 )
	@param wCommand : コマンドアドレス
	@param vData : データ
	@return 成功 : 0
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
	DEBUG_CPSAIO_COMMAND(KERN_INFO"cpsaio_command(): [%lx]=%x\n",com_addr, wCommand );
	cps_common_outw( com_addr , wCommand );
	//DEBUG_CPSAIO_COMMAND(KERN_INFO"[%lx]=%x\n",com_addr, wCommand );
	/* data */
	switch( isReadWrite ){
	case CPS_AIO_COMMAND_READ :
		DEBUG_CPSAIO_COMMAND(KERN_INFO"cpsaio_command(): <READ>\n");
		cps_common_inpw(  dat_addr_l , &data_l  );
		if( size == 4 ) cps_common_inpw(  dat_addr_u , &data_u  );
		else data_u = 0;



		switch( size ){
		case 2:
			tmpWordData = (unsigned short *) vData;
			*tmpWordData = data_l;
			DEBUG_CPSAIO_COMMAND(KERN_INFO"cpsaio_command(): [%lx]=%04x\n",dat_addr_l, data_l);
			break;
		case 4:
			tmpDWordData = (unsigned long *) vData;
			*tmpDWordData = (unsigned long ) ( data_u << 16 | data_l );
			DEBUG_CPSAIO_COMMAND(KERN_INFO"cpsaio_command(): [%lx]=%04x\n",dat_addr_l, data_l);
			DEBUG_CPSAIO_COMMAND(KERN_INFO"cpsaio_command(): [%lx]=%04x\n", dat_addr_u, data_u);
			break;
	}	
	break;	
	case CPS_AIO_COMMAND_WRITE :
		DEBUG_CPSAIO_COMMAND(KERN_INFO"cpsaio_command(): <WRITE>\n");
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

		// DataWrite UnLock
		cps_common_outw( BaseAddr + OFFSET_COMMAND_DATALOCK_AIO	 , CPS_AIO_DATA_UNLOCK );

		cps_common_outw( dat_addr_l , data_l );
		DEBUG_CPSAIO_COMMAND(KERN_INFO"cpsaio_command(): [%lx]=%04x\n",dat_addr_l, data_l);

		if( size == 4 ){
			cps_common_outw( dat_addr_u , data_u );
			DEBUG_CPSAIO_COMMAND(KERN_INFO"cpsaio_command(): [%lx]=%04x\n", dat_addr_u, data_u);
		}

		// DataWrite Lock
		cps_common_outw( BaseAddr + OFFSET_COMMAND_DATALOCK_AIO	 , CPS_AIO_DATA_LOCK );


		break;
	}
	return 0;
}

/*!
 @name CPSAIO-COMMAND
*/
/// @{

/*!
 @~English
 @name ECU COMMAND
 @~Japanese
 @name ECUコマンド
*/
/// @{
#define CPSAIO_COMMAND_ECU_INIT( addr ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_CALL, CPS_AIO_ADDRESS_MODE_ECU, 0, CPS_AIO_COMMAND_ECU_INIT, NULL )
#define CPSAIO_COMMAND_ECU_SETIRQMASK_ENABLE( addr ) \
		cpsaio_command( addr, CPS_AIO_COMMAND_CALL, CPS_AIO_ADDRESS_MODE_ECU, 0, CPS_AIO_COMMAND_ECU_SETIRQMASK_ENABLE, NULL )
#define CPSAIO_COMMAND_ECU_SETIRQMASK_DISABLE( addr ) \
		cpsaio_command( addr, CPS_AIO_COMMAND_CALL, CPS_AIO_ADDRESS_MODE_ECU, 0, CPS_AIO_COMMAND_ECU_SETIRQMASK_DISABLE, NULL )
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
#define CPSAIO_COMMAND_ECU_MEM_GET_INTERRUPT_FLAG( addr, data ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_READ, CPS_AIO_ADDRESS_MODE_ECU, 2, CPS_AIO_COMMAND_ECU_MEM_INTERRUPT_FLAG, data )
#define CPSAIO_COMMAND_ECU_MEM_SET_INTERRUPT_FLAG( addr, data ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_WRITE, CPS_AIO_ADDRESS_MODE_ECU, 2, CPS_AIO_COMMAND_ECU_MEM_INTERRUPT_FLAG, data )
/// @}
/*!
 @~English
 @name AI COMMAND
 @~Japanese
 @name AI用コマンド
*/
/// @{
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

/// @}
/*!
 @~English
 @name AO COMMAND
 @~Japanese
 @name AO用コマンド
*/
/// @{

/*!
	@~English
	@brief Analog output set one channel or many channel.
	@~Japanese
	@brief AO 単数チャネル取得(Signle)か 複数チャネル取得(Multi)の設定
*/
#define CPSAIO_COMMAND_AO_SINGLEMULTI( addr, data ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_WRITE, CPS_AIO_ADDRESS_MODE_COMMAND, 2, CPS_AIO_COMMAND_AO_SET_EXCHANGE, data )
/*!
	@~English
	@brief Analog output set channel number.
	@~Japanese
	@brief AO チャネル数の設定
*/
#define CPSAIO_COMMAND_AO_SETCHANNEL( addr, data ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_WRITE, CPS_AIO_ADDRESS_MODE_COMMAND, 2, CPS_AIO_COMMAND_AO_SET_CHANNELS, data )
/*!
	@~English
	@brief Analog output initialize.
	@~Japanese
	@brief AO 初期化
*/
#define CPSAIO_COMMAND_AO_INIT( addr ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_CALL, CPS_AIO_ADDRESS_MODE_COMMAND, 0, CPS_AIO_COMMAND_AO_INIT, NULL )
/*!
	@~English
	@brief Analog output open.
	@~Japanese
	@brief AO ゲートオープン
*/
#define CPSAIO_COMMAND_AO_OPEN( addr ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_CALL, CPS_AIO_ADDRESS_MODE_COMMAND, 0, CPS_AIO_COMMAND_AO_GATE_OPEN, NULL )
/*!
	@~English
	@brief Analog output stop.
	@~Japanese
	@brief AO 強制停止
*/
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

/// @}
/*!
 @~English
 @name Memory COMMAND
 @~Japanese
 @name メモリ用コマンド
*/
/// @{

/*!
	@~English
	@brief Memory Init Command
	@~Japanese
	@brief メモリ初期化コマンド
*/
#define CPSAIO_COMMAND_MEM_INIT( addr ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_CALL, CPS_AIO_ADDRESS_MODE_COMMAND, 0, CPS_AIO_COMMAND_MEM_INIT, NULL )

/*!
	@~English
	@brief Memory AI Clear Command
	@~Japanese
	@brief AI メモリクリアコマンド
*/
#define CPSAIO_COMMAND_AI_CLR( addr ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_CALL, CPS_AIO_ADDRESS_MODE_COMMAND, 0, CPS_AIO_COMMAND_MEM_AI_CLEAR, NULL )

/*!
	@~English
	@brief Memory AO Clear Command
	@~Japanese
	@brief AO　メモリクリアコマンド
*/
#define CPSAIO_COMMAND_AO_CLR( addr ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_CALL, CPS_AIO_ADDRESS_MODE_COMMAND, 0, CPS_AIO_COMMAND_MEM_AO_CLEAR, NULL )

/*!
	@~English
	@brief Memory AI FIFO Counter Command
	@~Japanese
	@brief AI FIFO カウンタコマンド
*/
#define CPSAIO_COMMAND_AI_FIFO_COUNTER( addr , data ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_READ, CPS_AIO_ADDRESS_MODE_COMMAND, 2, CPS_AIO_COMMAND_MEM_AI_FIFO_COUNTER, data )

/*!
	@~English
	@brief Memory AO FIFO Counter Command
	@~Japanese
	@brief AI FIFO カウンタ コマンド
*/
#define CPSAIO_COMMAND_AO_FIFO_COUNTER( addr , data ) \
	cpsaio_command( addr, CPS_AIO_COMMAND_READ, CPS_AIO_ADDRESS_MODE_COMMAND, 2, CPS_AIO_COMMAND_MEM_AO_FIFO_COUNTER, data )
/// @}

/// @}

/**
	@brief cpsaio_read_eeprom
	@param dev : device number
	@param cate : device category ( AI or AO )
	@param num : number 
	@param val : value
	@return Success : 0, Failed : 1
	@~Japanese
	@brief EEPROMの読み出し
	@param dev : デバイス番号(  1-31 )
	@param cate : カテゴリ番号 ( AI, AO など )
	@param num : ページ番号 ( 0 から 4 )
	@param val : 値
	@return 成功 : 0 , 失敗 : 1
**/ 
unsigned long cpsaio_read_eeprom( unsigned int dev, unsigned int cate, unsigned int num, unsigned short *val )
{

	contec_mcs341_device_extension_value_get(dev, cate,  num );
	contec_mcs341_device_extension_value_get(dev, cate,  num );
	*val = contec_mcs341_device_extension_value_get(dev, cate,  num );

	return 0;
}

/**
	@~English
	@brief cpsaio_write_eeprom
	@param dev : device number
	@param cate : device category ( AI or AO )
	@param num : number 
	@param val : value
	@return Success : 0 , Failed : 1
	@~Japanese
	@brief EEPROMの書き込み
	@param dev : デバイス番号(  1-31 )
	@param cate : カテゴリ番号 ( AI, AO など )
	@param num : ページ番号 ( 0 から 4 )
	@param val : 値
	@return 成功 : 0 , 失敗 : 1
**/ 
unsigned long cpsaio_write_eeprom(unsigned int dev, unsigned int cate, unsigned int num, unsigned short val )
{
	unsigned short chkVal;

	unsigned int count = 0;

	do{
		contec_mcs341_device_extension_value_set(dev , cate, num, val);
		cpsaio_read_eeprom( dev, cate, num, &chkVal );
//		chkVal = contec_mcs341_device_extension_value_get(dev ,cate,  num );
		contec_cps_micro_sleep( 1 );
		if( count > 5 ){
			return 1;
		}
		else count++;

		DEBUG_CPSAIO_EEPROM(KERN_INFO"cpsaio_write_eeprom() %d, %x :: %x \n", dev, val , chkVal );  

	}while( chkVal != val );
	return 0;
}

/**
	@~English
	@brief cpsaio_clear_eeprom
	@param dev : device number
	@~Japanese
	@brief EEPROMのクリア
	@param dev : デバイス番号(  1-31 )
**/ 
void cpsaio_clear_eeprom( unsigned int dev )
{
	contec_mcs341_device_extension_value_all_clear(dev , 0 );
}

/**
	@~English
	@brief cpsaio_clear_fpga_extension_reg
	@param dev : device number
	@param cate : category number
	@param num : page number ( 0 to 4 )
	@~Japanese
	@brief FPGA拡張レジスタのクリア
	@param dev : デバイス番号(  1-31 )
	@param cate : カテゴリ番号 ( AI, AO など )
	@param num : ページ番号 ( 0 から 4 )
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
/***** Device Name function ********/

/**
	@~English
	@brief This function get Device Name with Analog I/O device.
	@param node : device node
	@param devName : Device Name
	@return true : 0
	@~Japanese
	@brief アナログ機器のデバイス名を取得する関数
	@param node : ノード
	@param devName : デバイス名
	@return 成功 : 0
**/
static long cpsaio_get_aio_devname( int node, unsigned char devName[] )
{
	int product_id, cnt;
	product_id = contec_mcs341_device_productid_get(node);

	for(cnt = 0; cps_aio_data[cnt].ProductNumber != -1; cnt++){
		if( cps_aio_data[cnt].ProductNumber == product_id ){
			strcpy( devName, cps_aio_data[cnt].Name );
			return 0;
		}
	}
	return 1;
}



/***** Interrupt function *******************************/
static const int AM335X_IRQ_NMI=7;


/**
	@~English
	@brief cpsaio_isr_func
	@param irq : interrupt 
	@param dev_instance : device instance
	@return intreturn_t ( IRQ_HANDLED or IRQ_NONE )
	@~Japanese
	@brief cpsaio 割り込み処理
	@param irq : IRQ番号
	@param dev_instance : デバイス・インスタンス
	@return IRQ_HANDLED か, IRQ_NONE
**/ 
irqreturn_t cpsaio_isr_func(int irq, void *dev_instance){

	unsigned short wStatus = 0;
	unsigned short wData = 0;
	int handled = 0;
	PCPSAIO_DRV_FILE dev =(PCPSAIO_DRV_FILE) dev_instance;
	// Ver.1.0.10 Don't insert interrupt "xx callbacks suppressed" by IRQ_NONE.
	if( !dev ){
		DEBUG_CPSAIO_INTERRUPT_CHECK("This interrupt is not CONPROSYS AIO Device.");
		goto END_OF_INTERRUPT_CPSAIO;
	}

	// Not Aio Device
	if( !contec_mcs341_device_IsCategory( dev->node , CPS_CATEGORY_AIO ) ){
		DEBUG_CPSAIO_INTERRUPT_CHECK("This interrupt is not Category AIO Device.");
		goto END_OF_INTERRUPT_CPSAIO;
	}

	spin_lock(&dev->lock);

	// Ver.1.0.10 Check Interrupt Status
	cpsaio_read_interrupt_status((unsigned long)dev->baseAddr, &wStatus);


	if( !wStatus ){
		 goto END_OF_INTERRUPT_SPIN_UNLOCK_CPSAIO;
	}

	DEBUG_CPSAIO_INTERRUPT_CHECK("Interrupt Status : %x\n.",wStatus);

	// Ver 1.0.10 test interrupt.
	CPSAIO_COMMAND_ECU_SETIRQMASK_ENABLE( (unsigned long)dev->baseAddr );
	
	/* Ver 1.0.10 Test Interrupt Check */

	// AI
	if( wStatus & 0x01 ){
		CPSAIO_COMMAND_ECU_AI_GET_INTERRUPT_FLAG( (unsigned long)dev->baseAddr, &wData );

		DEBUG_CPSAIO_INTERRUPT_CHECK("AI Interrupt Flag : %x \n", wData );

		if( wData ){
			CPSAIO_COMMAND_ECU_AI_SET_INTERRUPT_FLAG( (unsigned long)dev->baseAddr, &wData );
			handled = 1;
		}
	}
	
	//AO
	if( wStatus & 0x02 ){
		CPSAIO_COMMAND_ECU_AO_GET_INTERRUPT_FLAG( (unsigned long)dev->baseAddr, &wData );

		DEBUG_CPSAIO_INTERRUPT_CHECK("AO Interrupt Flag : %x \n", wData );

		if( wData ){
			CPSAIO_COMMAND_ECU_AO_SET_INTERRUPT_FLAG( (unsigned long)dev->baseAddr, &wData );
			handled = 1;
		}
	}
	
	// MEM
	if( wStatus & 0x10 ){
		CPSAIO_COMMAND_ECU_MEM_GET_INTERRUPT_FLAG( (unsigned long)dev->baseAddr, &wData );

		DEBUG_CPSAIO_INTERRUPT_CHECK("MEM Interrupt Flag : %x \n", wData );

		if( wData ){
			CPSAIO_COMMAND_ECU_MEM_SET_INTERRUPT_FLAG( (unsigned long)dev->baseAddr, &wData );
			handled = 1;
		}
	}

	CPSAIO_COMMAND_ECU_SETIRQMASK_DISABLE( (unsigned long)dev->baseAddr );
	// End of test Interrupt Check Ver.1.0.10

END_OF_INTERRUPT_SPIN_UNLOCK_CPSAIO:
	spin_unlock(&dev->lock);

END_OF_INTERRUPT_CPSAIO:

	if(IRQ_RETVAL(handled) ){
		if(printk_ratelimit())
			printk("cpsaio Device Number:%d IRQ interrupt !\n",( dev->node ) );
	}

	return IRQ_RETVAL(handled);
}


/***** file operation functions *******************************/

/**
	@~English
	@brief cpsaio_ioctl_ecu (cpsaio_ioctl sub function.)
	@param dev : CPSAIO_DRV_FILE pointer
	@param cmd : iocontrol command
	@param arg : argument
	@return Success 0, Failed:otherwise 0. (see errno.h)	@return long (see errno.h)
	@~Japanese
	@brief ECU用 I/Oコントロール
	@param dev : CPSAIO_DRV_FILE構造体ポインタ
	@param cmd : I/O コントロールコマンド
	@param arg : 引数
	@return 成功:0 , 失敗:0以外 (errno.h参照)
**/ 
long cpsaio_ioctl_ecu(PCPSAIO_DRV_FILE dev, unsigned int cmd, unsigned long arg )
{
	unsigned short valw = 0;
	unsigned long valdw = 0;
	struct cpsaio_ioctl_arg ioc;
	unsigned long flags;

	switch( cmd ){
/* ECU */
		case IOCTL_CPSAIO_SETECU_SIGNAL:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					valdw = (unsigned long) ioc.val;
					CPSAIO_COMMAND_ECU_SETINPUTSIGNAL( (unsigned long)dev->baseAddr , &valdw );
					spin_unlock_irqrestore(&dev->lock, flags);

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
					spin_lock_irqsave(&dev->lock, flags);
					CPSAIO_COMMAND_ECU_AI_GET_INTERRUPT_FLAG((unsigned long)dev->baseAddr , &valw );
					ioc.val = (unsigned long) valw;

					spin_unlock_irqrestore(&dev->lock, flags);

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
					spin_lock_irqsave(&dev->lock, flags);
					CPSAIO_COMMAND_ECU_AO_GET_INTERRUPT_FLAG((unsigned long)dev->baseAddr , &valw );
					ioc.val = (unsigned long) valw;

					spin_unlock_irqrestore(&dev->lock, flags);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

/////////////////////////////////// Ver.1.0.6 added
		case IOCTL_CPSAIO_SET_INTERRUPT_FLAG_AI:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					valw = (unsigned short) ioc.val;
					CPSAIO_COMMAND_ECU_AI_SET_INTERRUPT_FLAG((unsigned long)dev->baseAddr , &valw );
					spin_unlock_irqrestore(&dev->lock, flags);
					break;

		case IOCTL_CPSAIO_SET_INTERRUPT_FLAG_AO:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					valw = (unsigned short) ioc.val;
					CPSAIO_COMMAND_ECU_AO_SET_INTERRUPT_FLAG((unsigned long)dev->baseAddr , &valw );
					spin_unlock_irqrestore(&dev->lock, flags);
					break;
/////////////////////////////////// Ver.1.0.6 added
		}
		return 0;
}


/**
	@~English
	@brief cpsaio_ioctl_ai　(cpsaio_ioctl sub function.)
	@param dev : CPSAIO_DRV_FILE pointer
	@param cmd : iocontrol command
	@param arg : argument
	@return Success 0, Failed:otherwise 0. (see errno.h)	@return long (see errno.h)
	@~Japanese
	@brief AI用 I/Oコントロール
	@param dev : CPSAIO_DRV_FILE構造体ポインタ
	@param cmd : I/O コントロールコマンド
	@param arg : 引数
	@return 成功:0 , 失敗:0以外 (errno.h参照)
**/ 
long cpsaio_ioctl_ai(PCPSAIO_DRV_FILE dev, unsigned int cmd, unsigned long arg )
{
	unsigned short valw = 0;
	unsigned long valdw = 0;
	struct cpsaio_ioctl_arg ioc;
	unsigned long flags;

	switch( cmd ){
/* Analog Input */
		case IOCTL_CPSAIO_INDATA:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					cpsaio_read_ai_data((unsigned long)dev->baseAddr , &valw );
					ioc.val = (unsigned long) valw;
					spin_unlock_irqrestore(&dev->lock, flags);

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
					spin_lock_irqsave(&dev->lock, flags);
					for( cnt = 0; cnt < datBuf.num; cnt ++ ){
						
						datBuf.val[cnt] = (unsigned long)valw;
					}
					spin_unlock_irqrestore(&dev->lock, flags);

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
					spin_lock_irqsave(&dev->lock, flags);
					cpsaio_read_ai_status((unsigned long)dev->baseAddr , &valw );
					ioc.val = (unsigned long) valw;
					spin_unlock_irqrestore(&dev->lock, flags);

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
					spin_lock_irqsave(&dev->lock, flags);
					valw = (unsigned short) ioc.val;
					CPSAIO_COMMAND_AI_SETCHANNEL( (unsigned long)dev->baseAddr , &valw );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSAIO_SETTRANSFER_MODE_AI:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					spin_lock_irqsave(&dev->lock, flags);
					valw = (unsigned short) ioc.val;
					CPSAIO_COMMAND_AI_SINGLEMULTI( (unsigned long)dev->baseAddr , &valw );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSAIO_SET_CLOCK_AI:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					valdw = (unsigned long) ioc.val;
					CPSAIO_COMMAND_AI_SETCLOCK( (unsigned long)dev->baseAddr, &valdw );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;
		case IOCTL_CPSAIO_SET_SAMPNUM_AI:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					valdw = (unsigned long) ioc.val;
					CPSAIO_COMMAND_AI_SETSAMPNUM( (unsigned long)dev->baseAddr, &valdw );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSAIO_SET_CALIBRATION_AI:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					valdw = (unsigned long) ioc.val;
					CPSAIO_COMMAND_AI_SET_CALIBRATION( (unsigned long)dev->baseAddr, &valdw );
					spin_unlock_irqrestore(&dev->lock, flags);

					do{
						contec_cps_micro_sleep( 1 );
						cpsaio_read_ai_status( (unsigned long)dev->baseAddr, &valw );
					}while( valw & CPS_AIO_AI_STATUS_CALIBRATION_BUSY );

					//ポテンショメータへの反映時間が BusyFlagがおちたあと500msecかかるため　Waitする
					contec_cps_micro_sleep( 500 * USEC_PER_MSEC );
					break;

		case IOCTL_CPSAIO_GET_CALIBRATION_AI:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					CPSAIO_COMMAND_AI_GET_CALIBRATION( (unsigned long)dev->baseAddr, &valdw );
					ioc.val = valdw;
					spin_unlock_irqrestore(&dev->lock, flags);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;
		}

		return 0;
}

/**
	@~English
	@brief ao ioctl function. (cpsaio_ioctl sub function.)
	@param dev : CPSAIO_DRV_FILE pointer
	@param cmd : iocontrol command
	@param arg : argument
	@return Success 0, Failed:otherwise 0. (see errno.h)
	@~Japanese
	@brief AO用 I/Oコントロール
	@param dev : CPSAIO_DRV_FILE構造体ポインタ
	@param cmd : I/O コントロールコマンド
	@param arg : 引数
	@return 成功:0 , 失敗:0以外 (errno.h参照)

**/ 
long cpsaio_ioctl_ao(PCPSAIO_DRV_FILE dev, unsigned int cmd, unsigned long arg )
{
	unsigned short valw = 0;
	unsigned long valdw = 0;
	struct cpsaio_ioctl_arg ioc;
	unsigned long flags;

	switch( cmd ){

/* Analog Output */
		case IOCTL_CPSAIO_OUTDATA:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					spin_lock_irqsave(&dev->lock, flags);
					valw = (unsigned short) ioc.val;

					if( dev->data.ProductNumber == CPS_DEVICE_AO_1604LI ||
							dev->data.ProductNumber == CPS_DEVICE_AO_1604VLI )
						valw = calc_DAC161S055( valw );

					cpsaio_write_ao_data( (unsigned long)dev->baseAddr , valw );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSAIO_OUTSTATUS:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					cpsaio_read_ao_status( (unsigned long)dev->baseAddr , &valw );
					ioc.val = (unsigned int) valw;
					spin_unlock_irqrestore(&dev->lock, flags);

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
					spin_lock_irqsave(&dev->lock, flags);
					valw = (unsigned short) ioc.val;
					CPSAIO_COMMAND_AO_SETCHANNEL( (unsigned long)dev->baseAddr , &valw );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSAIO_SET_CLOCK_AO:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					valdw = (unsigned long) ioc.val;
					CPSAIO_COMMAND_AO_SETCLOCK( (unsigned long)dev->baseAddr, &valdw );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSAIO_SETTRANSFER_MODE_AO:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					valw = (unsigned short) ioc.val;
					CPSAIO_COMMAND_AO_SINGLEMULTI( (unsigned long)dev->baseAddr , &valw );
					spin_unlock_irqrestore(&dev->lock, flags);

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
					spin_lock_irqsave(&dev->lock, flags);
					valdw = (unsigned long) ioc.val;
					CPSAIO_COMMAND_AO_SETSAMPNUM( (unsigned long)dev->baseAddr, &valdw );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;
	
		case IOCTL_CPSAIO_SET_CALIBRATION_AO:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					valdw = (unsigned long) ioc.val;
					CPSAIO_COMMAND_AO_SET_CALIBRATION( (unsigned long)dev->baseAddr, &valdw );
					spin_unlock_irqrestore(&dev->lock, flags);

					do{
						contec_cps_micro_sleep( 1 );
						cpsaio_read_ao_status( (unsigned long)dev->baseAddr, &valw );
					}while( valw & CPS_AIO_AO_STATUS_CALIBRATION_BUSY );

					//ポテンショメータへの反映時間が BusyFlagがおちたあと500msecかかるため　Waitする
					contec_cps_micro_sleep( 500 * USEC_PER_MSEC );

					break;

		case IOCTL_CPSAIO_GET_CALIBRATION_AO:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					CPSAIO_COMMAND_AO_GET_CALIBRATION( (unsigned long)dev->baseAddr, &valdw );
					ioc.val = valdw;
					spin_unlock_irqrestore(&dev->lock, flags);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		}
		return 0;
}

/**
	@~English
	@brief cpsaio_ioctl
	@param filp : struct file pointer
	@param cmd : iocontrol command
	@param arg : argument
	@return Success 0, Failed:otherwise 0. (see errno.h)
	@~Japanese
	@brief cpsaio_ioctl
	@param filp : file構造体ポインタ
	@param cmd : I/O コントロールコマンド
	@param arg : 引数
	@return 成功:0 , 失敗:0以外 (errno.h参照)
**/ 
static long cpsaio_ioctl( struct file *filp, unsigned int cmd, unsigned long arg )
{
	PCPSAIO_DRV_FILE dev = filp->private_data;
	unsigned short valw = 0;
	unsigned long valdw = 0;
	struct cpsaio_ioctl_arg ioc;
	struct cpsaio_ioctl_string_arg ioc_str; // Ver.1.0.9
	struct cpsaio_direct_arg d_ioc;
	struct cpsaio_direct_command_arg dc_ioc;
	unsigned int num = 0;
	long lRet;
	unsigned long flags;

	unsigned int abi ;

	memset( &ioc, 0 , sizeof(ioc) );
	memset( &ioc_str, 0, sizeof(ioc_str) );	// Ver.1.0.9
	memset( &d_ioc, 0, sizeof(d_ioc) );
	memset( &dc_ioc, 0, sizeof(dc_ioc) );

	if ( dev == (PCPSAIO_DRV_FILE)NULL ){
		DEBUG_CPSAIO_IOCTL(KERN_INFO"CPSAIO_DRV_FILE NULL POINTER.");
		return -EFAULT;
	}

	spin_lock_irqsave(&dev->lock, flags);
	abi = dev->data.Ability;
	spin_unlock_irqrestore(&dev->lock, flags);

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
					spin_lock_irqsave(&dev->lock, flags);
					switch( ioc.inout ){
					case CPS_AIO_INOUT_AI :
						ioc.val = dev->data.ai.Resolution	;
						break;
					case CPS_AIO_INOUT_AO:
						ioc.val = dev->data.ao.Resolution	;
						break;
					}
					spin_unlock_irqrestore(&dev->lock, flags);

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
					spin_lock_irqsave(&dev->lock, flags);
					switch( ioc.inout ){
					case CPS_AIO_INOUT_AI :				
						ioc.val = dev->data.ai.Channel	;
						break;
					case CPS_AIO_INOUT_AO:
						ioc.val = dev->data.ao.Channel	;
						break;
					}
					spin_unlock_irqrestore(&dev->lock, flags);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;	

/////// MEMORY I/O COMMAND ////////

		case IOCTL_CPSAIO_GETMEMSTATUS:

					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					cpsaio_read_mem_status( (unsigned long)dev->baseAddr , &valw );
					ioc.val = (unsigned int) valw;
					spin_unlock_irqrestore(&dev->lock, flags);

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

					spin_lock_irqsave(&dev->lock, flags);
					valw = (unsigned short) ioc.val;

					switch( dev->data.ProductNumber ){
					case CPS_DEVICE_AI_1608LI:
					case CPS_DEVICE_AI_1608ALI:num = 0;break;
					default :
						spin_unlock_irqrestore(&dev->lock, flags);
						return -EFAULT;
					}
					spin_unlock_irqrestore(&dev->lock, flags);
					cpsaio_write_eeprom( dev->node , CPS_DEVICE_COMMON_ROM_WRITE_PAGE_AI, num,  valw );
//					spin_unlock_irqrestore(&dev->lock, flags);

					DEBUG_CPSAIO_EEPROM(KERN_INFO"EEPROM-WRITE:[%lx]=%x\n",(unsigned long)( dev->baseAddr ), valw );
					break;

		case IOCTL_CPSAIO_READ_EEPROM_AI:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					switch( dev->data.ProductNumber ){
					case CPS_DEVICE_AI_1608LI:
					case CPS_DEVICE_AI_1608ALI: num = 0;break;
					default :
						spin_unlock_irqrestore(&dev->lock, flags);
						return -EFAULT;
					}
					spin_unlock_irqrestore(&dev->lock, flags);

					cpsaio_read_eeprom( dev->node ,CPS_DEVICE_COMMON_ROM_WRITE_PAGE_AI, num, &valw );

					spin_lock_irqsave(&dev->lock, flags);
					ioc.val = (unsigned long) valw;
					spin_unlock_irqrestore(&dev->lock, flags);

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

					spin_lock_irqsave(&dev->lock, flags);
					
					valw = (unsigned short) ioc.val;

					switch( dev->data.ProductNumber ){
					case CPS_DEVICE_AO_1604LI:
					case CPS_DEVICE_AO_1604VLI: num = (unsigned short) ioc.ch;break;
					default :
						spin_unlock_irqrestore(&dev->lock, flags);
						return -EFAULT;
					}
					spin_unlock_irqrestore(&dev->lock, flags);
					cpsaio_write_eeprom( dev->node , CPS_DEVICE_COMMON_ROM_WRITE_PAGE_AO, num,  valw );
					//spin_unlock_irqrestore(&dev->lock, flags);

					DEBUG_CPSAIO_EEPROM(KERN_INFO"EEPROM-WRITE:[%lx]=%x\n",(unsigned long)( dev->baseAddr ), valw );
					break;

		case IOCTL_CPSAIO_READ_EEPROM_AO:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					switch( dev->data.ProductNumber ){
					case CPS_DEVICE_AO_1604LI:
					case CPS_DEVICE_AO_1604VLI: num = (unsigned short) ioc.ch;break;
					default :
						spin_unlock_irqrestore(&dev->lock, flags);
						return -EFAULT;
					}
					spin_unlock_irqrestore(&dev->lock, flags);

					cpsaio_read_eeprom( dev->node ,CPS_DEVICE_COMMON_ROM_WRITE_PAGE_AO, num, &valw );

					spin_lock_irqsave(&dev->lock, flags);
					ioc.val = (unsigned long) valw;
					spin_unlock_irqrestore(&dev->lock, flags);

					DEBUG_CPSAIO_EEPROM(KERN_INFO"EEPROM-READ:[%lx]=%x\n",(unsigned long)( dev->baseAddr ), valw );
				
					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}

					break;

		case IOCTL_CPSAIO_CLEAR_EEPROM:

					spin_lock_irqsave(&dev->lock, flags);

					switch( dev->data.ProductNumber ){
					case CPS_DEVICE_AI_1608LI:
					case CPS_DEVICE_AI_1608ALI: num = 1;break;
					case CPS_DEVICE_AO_1604LI:
					case CPS_DEVICE_AO_1604VLI : num = dev->data.ao.Channel;break;
					}

					spin_unlock_irqrestore(&dev->lock, flags);

					if( abi & CPS_AIO_ABILITY_AI )
						cpsaio_clear_fpga_extension_reg(dev->node ,CPS_DEVICE_COMMON_ROM_WRITE_PAGE_AI, num );
					if( abi & CPS_AIO_ABILITY_AO )
						cpsaio_clear_fpga_extension_reg(dev->node ,CPS_DEVICE_COMMON_ROM_WRITE_PAGE_AO, num );
					cpsaio_clear_eeprom( dev->node );
//					spin_unlock_irqrestore(&dev->lock, flags);

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
					spin_lock_irqsave(&dev->lock, flags);
					valw = (unsigned short) d_ioc.val;
					DEBUG_CPSAIO_COMMAND(KERN_INFO"DIRECT OUTPUT: [%lx]=%x\n",(unsigned long)( dev->baseAddr + d_ioc.addr ), valw );
					cps_common_outw( (unsigned long)( dev->baseAddr + d_ioc.addr ) , valw );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSAIO_DIRECT_INPUT:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &d_ioc, (int __user *)arg, sizeof(d_ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					
					cps_common_inpw( (unsigned long)( dev->baseAddr + d_ioc.addr ) , &valw );
					DEBUG_CPSAIO_COMMAND(KERN_INFO"DIRECT INPUT:[%lx]=%x\n",(unsigned long)( dev->baseAddr + d_ioc.addr ), valw );
					d_ioc.val = (unsigned long) valw;

					spin_unlock_irqrestore(&dev->lock, flags);

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
					spin_lock_irqsave(&dev->lock, flags);
					if( dc_ioc.size == 2 ){
						valw = (unsigned short) dc_ioc.val;
						cpsaio_command( (unsigned long)( dev->baseAddr ) ,CPS_AIO_COMMAND_WRITE, dc_ioc.isEcu, dc_ioc.size, dc_ioc.addr, &valw );
					}else if( dc_ioc.size == 4 ){
						valdw = dc_ioc.val;
						cpsaio_command( (unsigned long)( dev->baseAddr ) ,CPS_AIO_COMMAND_WRITE, dc_ioc.isEcu, dc_ioc.size, dc_ioc.addr, &valdw );
					}
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSAIO_DIRECT_COMMAND_INPUT:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &dc_ioc, (int __user *)arg, sizeof(dc_ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					if( dc_ioc.size == 2 ){
						cpsaio_command( (unsigned long)( dev->baseAddr ) ,CPS_AIO_COMMAND_READ, dc_ioc.isEcu, dc_ioc.size, dc_ioc.addr, &valw );
						d_ioc.val = (unsigned long) valw;
					}else if( dc_ioc.size == 4 ){
						cpsaio_command( (unsigned long)( dev->baseAddr ) ,CPS_AIO_COMMAND_READ, dc_ioc.isEcu, dc_ioc.size, dc_ioc.addr, &valdw );
						d_ioc.val = valdw;
					}					
					spin_unlock_irqrestore(&dev->lock, flags);

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
					spin_lock_irqsave(&dev->lock, flags);
					cpsaio_command( (unsigned long)( dev->baseAddr ) ,CPS_AIO_COMMAND_CALL, dc_ioc.isEcu, 0, dc_ioc.addr, NULL );				
					spin_unlock_irqrestore(&dev->lock, flags);

					if( copy_to_user( (int __user *)arg, &dc_ioc, sizeof(dc_ioc) ) ){
						return -EFAULT;
					}
					break;
// Other ...
		case IOCTL_CPSAIO_GET_DRIVER_VERSION:
			// Ver.1.0.8 Add
			// Ver.1.0.9 Modify using from cpsaio_ioctl_arg to cpsaio_ioctl_string_arg
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
		case IOCTL_CPSAIO_GET_DEVICE_NAME:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
					return -EFAULT;
				}
				if( copy_from_user( &ioc_str, (int __user *)arg, sizeof(ioc_str) ) ){
					return -EFAULT;
				}
				spin_lock_irqsave(&dev->lock, flags);
				cpsaio_get_aio_devname( dev->node , ioc_str.str );
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
	@brief This function is called by read user function.
	@param filp : struct file pointer
	@param buf : buffer (user)
	@param count : size count
	@param f_pos : struct Loff_t pointer
	@return Success: read_finished size, Failed : Less then 0. (see errno.h)
	@~Japanese
	@brief この関数は read関数から呼び出されます。
	@param filp : ファイル構造体ポインタ
	@param buf : ユーザバッファ
	@param count : サイズカウント
	@param f_pos : Loff_t構造体ポインタ
	@return 成功: 読み込み完了データサイズ, 失敗 : 0未満. ( errno.h参照)
**/ 
static ssize_t cpsaio_get_sampling_data_ai(struct file *filp, char __user *buf, size_t count, loff_t *f_pos )
{

	PCPSAIO_DRV_FILE dev = filp->private_data;
	unsigned short valw = 0;
	unsigned char valb = 0;
	unsigned short cnt = 0,cnt2 = 0;
	int retval = 0;
	unsigned short wStatus = 0;
	unsigned long timout;

	if( dev == (PCPSAIO_DRV_FILE)NULL ) return -EFAULT;

	if( dev->data.Ability & CPS_AIO_ABILITY_AI ){
		for( cnt = 0; cnt < count; cnt += 2 )
		{
			timout = 0;
			do{
				//contec_cps_micro_sleep( 1 );
				cpsaio_read_mem_status( (unsigned long) dev->baseAddr, &wStatus );
				if( timout > 1000000 ){
					DEBUG_CPSAIO_READFIFO(KERN_INFO"cpsaio: mem timeout (status mem:%x", wStatus);

					cpsaio_read_ai_status((unsigned long) dev->baseAddr, &wStatus );
					DEBUG_CPSAIO_READFIFO(KERN_INFO",status ai:%x", wStatus);

					CPSAIO_COMMAND_ECU_MEM_GET_INTERRUPT_FLAG( (unsigned long)dev->baseAddr, &wStatus );
					DEBUG_CPSAIO_READFIFO(KERN_INFO",flag mem:%x", wStatus);

					CPSAIO_COMMAND_ECU_AI_GET_INTERRUPT_FLAG( (unsigned long)dev->baseAddr, &wStatus );
					DEBUG_CPSAIO_READFIFO(KERN_INFO",flag ai:%x", wStatus);

					CPSAIO_COMMAND_AI_FIFO_COUNTER( (unsigned long)dev->baseAddr, &wStatus );
					DEBUG_CPSAIO_READFIFO(KERN_INFO",fifo counter:%x", wStatus);
					
					CPSAIO_COMMAND_AI_CLR( (unsigned long) dev->baseAddr );	// test 

					retval = -EFAULT;
					goto out;
				}else	timout++;
			}while( !(wStatus & CPU_AIO_MEMSTATUS_DRE) );

			cpsaio_read_ai_data((unsigned long)dev->baseAddr , &valw );
			CPSAIO_COMMAND_AI_FIFO_COUNTER( (unsigned long)dev->baseAddr, &wStatus );
			DEBUG_CPSAIO_READFIFO(KERN_INFO"%d:[%x] fifo<%x> \n", cnt/2, valw, wStatus );
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
	valw = 0xFFFF;
	CPSAIO_COMMAND_ECU_AI_SET_INTERRUPT_FLAG( (unsigned long)dev->baseAddr, &valw ); 
	CPSAIO_COMMAND_ECU_MEM_SET_INTERRUPT_FLAG( (unsigned long)dev->baseAddr, &valw ); 
	return retval;

}

/**
	@~English
	@brief This function is called by write user function.
	@param filp : struct file pointer
	@param buf : buffer (user)
	@param count : size count
	@param f_pos : struct Loff_t pointer
	@return Success :read_finished size Failed : Less then 0. (see errno.h)
	@~Japanese
	@brief この関数は WRITE関数で呼び出されます。
	@param filp : ファイル構造体ポインタ
	@param buf : ユーザバッファ
	@param count : サイズカウント
	@param f_pos : Loff_t構造体ポインタ
	@return 成功：読み込み完了したサイズ 　失敗：0未満　 ( errno.hを参照 )
**/ 
static ssize_t cpsaio_set_sampling_data_ao(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos )
{

	PCPSAIO_DRV_FILE dev = filp->private_data;
	unsigned short valw = 0;
	unsigned short cnt = 0;
	int retval = 0;

	if( dev == (PCPSAIO_DRV_FILE)NULL ) return -EFAULT;

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
static int cpsaio_open(struct inode *inode, struct file *filp )
{
	int ret;
	PCPSAIO_DRV_FILE dev;
	int cnt;
	unsigned char __iomem *allocMem;
	unsigned short product_id;
	int iRet = 0;
	int nodeNo = 0; // Ver.1.0.8

	nodeNo = iminor( inode );// Ver.1.0.8

	DEBUG_CPSAIO_OPEN(KERN_INFO"node %d\n",iminor( inode ) );

//	if( cpsaio_devlp[iminor( inode )] == NULL )
//		return -EFAULT;
//	test = dev_get_drvdata(cpsaio_devlp[iminor( inode )]);
//	DEBUG_CPSAIO_OPEN(KERN_INFO"inode->private %x\n",(int)inode->i_private );
//	DEBUG_CPSAIO_OPEN(KERN_INFO"drv_get_devdata %x\n",*test );

	if (notFirstOpenFlg[nodeNo]) {		// Ver.1.0.8 初回オープンでなければ（segmentation fault暫定対策）
		if ( inode->i_private != (PCPSAIO_DRV_FILE)NULL ){

			DEBUG_CPSAIO_OPEN(KERN_INFO"inode->private %x\n",(int)inode->i_private );

			dev = (PCPSAIO_DRV_FILE)inode->i_private;
			filp->private_data =  (PCPSAIO_DRV_FILE)dev;

			if( dev->ref ){
				dev->ref++;
				return 0;
			}else{
				return -EFAULT;
			}
		}
	}

	DEBUG_CPSAIO_OPEN(KERN_INFO"inode->private %x\n",(int)inode->i_private );

	filp->private_data = (PCPSAIO_DRV_FILE)kzalloc( sizeof(CPSAIO_DRV_FILE) , GFP_KERNEL );
	if( filp->private_data == (PCPSAIO_DRV_FILE)NULL ){
		iRet = -ENOMEM;
		goto NOT_MEM_PRIVATE_DATA;
	}
	dev = (PCPSAIO_DRV_FILE)filp->private_data;
	inode->i_private = dev;

	dev->node = iminor( inode );
	
	dev->localAddr = 0x08000010 + (dev->node + 1) * 0x100;

	allocMem = cps_common_mem_alloc( dev->localAddr, 0xF0, "cps-aio", CPS_COMMON_MEM_REGION );

	if( allocMem != NULL ){
		dev->baseAddr = allocMem;
	}else{
		iRet = -ENOMEM;
		goto NOT_IOMEM_ALLOCATION;
	}

	product_id = contec_mcs341_device_productid_get( dev->node );
	cnt = 0;
	do{
		if( cps_aio_data[cnt].ProductNumber == -1 ) {
			iRet = -EFAULT;
			DEBUG_CPSAIO_OPEN(KERN_INFO"product_id:%x", product_id);
			goto NOT_FOUND_AIO_PRODUCT;
		}
		if( cps_aio_data[cnt].ProductNumber == product_id ){
			dev->data = cps_aio_data[cnt];
			break;
		}
		cnt++;
	}while( 1 );

	ret = request_irq(AM335X_IRQ_NMI, cpsaio_isr_func, IRQF_SHARED, "cps-aio-intr", dev);

	if( ret ){
		DEBUG_CPSAIO_OPEN(" request_irq failed.(%x) \n",ret);
	}

	// spin_lock initialize
	spin_lock_init( &dev->lock );

	dev->ref = 1;
	notFirstOpenFlg[nodeNo]++;		// Ver.1.0.8 segmentation fault暫定対策フラグインクリメント

	return 0;
NOT_FOUND_AIO_PRODUCT:
	cps_common_mem_release( dev->localAddr,
		0xF0,
		dev->baseAddr ,
		CPS_COMMON_MEM_REGION);

NOT_IOMEM_ALLOCATION:
	kfree( dev );

NOT_MEM_PRIVATE_DATA:
 
	inode->i_private = (PCPSAIO_DRV_FILE)NULL;
	filp->private_data = (PCPSAIO_DRV_FILE)NULL;

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
static int cpsaio_close(struct inode * inode, struct file *filp ){

	PCPSAIO_DRV_FILE dev;

	if ( inode->i_private != (PCPSAIO_DRV_FILE)NULL ){
		dev =  (PCPSAIO_DRV_FILE)inode->i_private;

		if( dev->ref > 0 ) dev->ref--;

		if( dev->ref == 0 ){

			free_irq(AM335X_IRQ_NMI, dev);

			cps_common_mem_release( dev->localAddr,
				0xF0,
				dev->baseAddr ,
				CPS_COMMON_MEM_REGION);
				
			kfree( dev );
			
			inode->i_private = (PCPSAIO_DRV_FILE)NULL;
		}
		filp->private_data = (PCPSAIO_DRV_FILE)NULL;
	}
	return 0;

}


/**
	@struct cpsaio_fops
	@brief CPSAIO file operations
**/
static struct file_operations cpsaio_fops = {
		.owner = THIS_MODULE,	///< owner's name
		.open = cpsaio_open,	///< open
		.release = cpsaio_close,	///< close
		.read = cpsaio_get_sampling_data_ai,	///< write
		.write = cpsaio_set_sampling_data_ao,	///< read
		.unlocked_ioctl = cpsaio_ioctl,	///< I/O Control
};

/**
	@~English
	@brief cpsaio init function.
	@return Success: 0, Failed: otherwise 0
	@~Japanese
	@brief cpsaio 初期化関数.
	@return 成功: 0, 失敗: 0以外
**/
static int cpsaio_init(void)
{

	dev_t dev = MKDEV(cpsaio_major , 0 );
	int ret = 0;
	int major;
	int cnt;
	struct device *devlp = NULL;
	int	aioNum = 0; // Ver.1.0.8

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
//		cpsaio_devlp[cnt] = (struct device *) NULL;
		if( contec_mcs341_device_IsCategory(cnt , CPS_CATEGORY_AIO ) ){
			if( __cpsaio_find_analog_device( cnt ) >= 0  ){
				cpsaio_dev = MKDEV( cpsaio_major, cnt );

//				cpsaio_devlp[cnt] = device_create(
				devlp = device_create(
					cpsaio_class, NULL, cpsaio_dev, NULL, CPSAIO_DRIVER_NAME"%d", cnt);

				if( IS_ERR(devlp) ){
					cdev_del( &cpsaio_cdev );
					unregister_chrdev_region( dev, cpsaio_max_devs );
					return PTR_ERR(devlp);
				}
				aioNum++;
				//dev_set_drvdata( cpsaio_devlp[cnt], &cnt );

			}
		}
	}

	// Ver.1.0.8
	if (aioNum) {
		notFirstOpenFlg = (int *)kzalloc( sizeof(int) * aioNum, GFP_KERNEL );	// segmentation fault暫定対策フラグメモリ確保
	}

	return 0;
}

/**
	@~English
	@brief cpsaio exit function.
	@~Japanese
	@brief cpsaio 終了関数.
**/
static void cpsaio_exit(void)
{

	dev_t dev = MKDEV(cpsaio_major , 0 );
	int cnt;

	kfree(notFirstOpenFlg);		// Ver.1.0.8 segmentation fault暫定対策フラグメモリ解放

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
