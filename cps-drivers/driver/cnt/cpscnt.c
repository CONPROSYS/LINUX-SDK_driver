/*
 *  Driver for CPS-CNT Counter I/O
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
#include <asm/signal.h>
#include <linux/device.h>

#include <linux/slab.h>

#ifdef CONFIG_CONPROSYS_SDK
 #include "../include/cps_common_io.h"
 #include "../include/cps.h"
 #include "../include/cps_ids.h"
 #include "../include/cps_extfunc.h"

 #include "../include/cpscnt.h"

#else
 #include "../../include/cps_common_io.h"
 #include "../../include/cps.h"
 #include "../../include/cps_ids.h"
 #include "../../include/cps_extfunc.h"

 #include "../../include/cpscnt.h"

#endif


#define DRV_VERSION	"0.9.6"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CONTEC CONPROSYS Counter I/O driver");
MODULE_AUTHOR("syunsuke okamoto");
MODULE_VERSION(DRV_VERSION);

#define CPSCNT_DRIVER_NAME "cpscnt"

/**
	@struct cps_cnt_data
	@~English
	@brief CPSCNT driver's File
	@~Japanese
	@brief CPSCNT ドライバファイル構造体
**/
typedef struct __cpscnt_driver_file{
	spinlock_t		lock; 				///< lock
	struct task_struct *int_callback;
	unsigned ref;						///< reference count 

	unsigned int node;					///< Device Node
	unsigned long localAddr; 			///< local Address
	unsigned char __iomem *baseAddr;	///< Memory Address
	CPSCNT_DEV_DATA data;				///< Device Data

}CPSCNT_DRV_FILE,*PCPSCNT_DRV_FILE;


#define CONTEC_CPSCNT_MAX_CHANNELS 2

typedef union __cpscnt_mode{
	struct _contec_cps_cnt_mode_bit{
		unsigned char sel0:1;
		unsigned char sel1:1;
		unsigned char sel2:1;
		unsigned char dir:1;
		unsigned char ub:1;
		unsigned char zsel:1;
		unsigned char sel:1;
		unsigned char reset:1;
	}Bit;
	unsigned char value;
}CPSCNT_MODE, *PCPSCNT_MODE;

typedef struct __cpscnt_channel_data
{
	unsigned long preset;		///< Preset Data
	unsigned long compare;		///< Compare Register 1
	CPSCNT_MODE mode;				///< Mode Register ( unsigned char )
	unsigned char zPhase;		///< Z Phase
	unsigned char filter;		///< digital filter
	unsigned char reserved;	///< 4byte 境界防止用
}CPSCNT_CHANNEL_DATA, *PCPSCNT_CHANNEL_DATA;

typedef struct __cpscnt_32xxI_software_data{
	unsigned int node;				///< Device Node
	unsigned int max_ch;			///< Max Channel Number
	PCPSCNT_CHANNEL_DATA data;	///< channel Data
	unsigned long timer;				///< Timer
	unsigned long pulsewidth;			///< Pulse Width
	unsigned char latch;				///< Latch
	unsigned char int_mask;			///< Interrupt Mask
	unsigned char sence;				///< Sence
	unsigned char input_sig;			///< Input Signal
	struct list_head	list;		///<	list of CPSCNT_32XXI_DATA
}CPSCNT_32XXI_DATA,*PCPSCNT_32XXI_DATA;

static LIST_HEAD(cpscnt_32xxI_head);	//software data register

static	int *notFirstOpenFlg = NULL;		// Ver.0.9.3 segmentation fault暫定対策フラグ

/*!
 @~English
 @name DebugPrint macro
 @~Japanese
 @name デバッグ用表示マクロ
*/
/// @{
#if 0
#define DEBUG_CPSCNT_OPEN(fmt...)	printk(fmt)
#else
#define DEBUG_CPSCNT_OPEN(fmt...)	do { } while (0)
#endif

#if 0
#define DEBUG_CPSCNT_COMMAND(fmt...)	printk(fmt)
#else
#define DEBUG_CPSCNT_COMMAND(fmt...)	do { } while (0)
#endif


#if 0
#define DEBUG_CPSCNT_DEVICE_LIST(fmt...)	printk(fmt)
#else
#define DEBUG_CPSCNT_DEVICE_LIST(fmt...)	do { } while (0)
#endif

#if 0
#define DEBUG_CPSCNT_START_COUNT(fmt...)	printk(fmt)
#else
#define DEBUG_CPSCNT_START_COUNT(fmt...)	do { } while (0)
#endif

#if 0
#define DEBUG_CPSCNT_IOCTL(fmt...)	printk(fmt)
#else
#define DEBUG_CPSCNT_IOCTL(fmt...)	do { } while (0)
#endif

#if 0
#define DEBUG_CPSCNT_INTERRUPT_CHECK(fmt...)	printk(fmt)
#else
#define DEBUG_CPSCNT_INTERRUPT_CHECK(fmt...)	do { } while (0)
#endif

/// @}
///
static int cpscnt_max_devs;		///< device count
static int cpscnt_major = 0;	///< major number
static int cpscnt_minor = 0;	///< minor number

static struct cdev cpscnt_cdev;
static struct class *cpscnt_class = NULL;

static dev_t cpscnt_dev;

static unsigned int reset_flag_always_on = 0;
module_param(reset_flag_always_on, uint, 0644 );

/**
	@struct cps_cnt_data
	@~English
	@brief CPSCNT driver's Data
	@~Japanese
	@brief CPSCNT ドライバデータ構造体
**/
static const CPSCNT_DEV_DATA cps_cnt_data[] = {
	{
		.Name = "CPS-CNT-3202I",
		.ProductNumber = CPS_DEVICE_CNT_3202I ,
		.channelNum = 2
	},
	{
		.Name = "",
		.ProductNumber = -1,
		.channelNum = -1,
	},
};

#include "cpscnt_devdata.h"

/***** allocate/free list_head *******************************/
/**
	@~English
	@brief CPS-CNT-32XXI allocate offset list.
	@param node : device node
	@param max_ch : maximum channel
	@return true : 0
	@~Japanese
	@brief CPS-CNT-3202Iのデバイスデータリストを生成する関数
	@param node : デバイスノード
	@param max_ch : 最大チャネル
	@return 成功 : 0
**/
void cpscnt_32xxI_allocate_list( unsigned int node, unsigned int max_ch ){

	unsigned int cnt;

	PCPSCNT_32XXI_DATA cnt_32xxi_data;

	cnt_32xxi_data = kzalloc( sizeof( CPSCNT_32XXI_DATA ), GFP_KERNEL );

	if( cnt_32xxi_data == (PCPSCNT_32XXI_DATA) NULL ){
		DEBUG_CPSCNT_DEVICE_LIST(KERN_INFO"list allocate error");
		return;
	}

	// Initialize Paramater
	cnt_32xxi_data->node = node;
	cnt_32xxi_data->max_ch = max_ch;
	cnt_32xxi_data->latch = 0;
	cnt_32xxi_data->int_mask = 0xff;
	cnt_32xxi_data->sence = 0xff;
	cnt_32xxi_data->pulsewidth = 0;
	cnt_32xxi_data->timer = 0;
	cnt_32xxi_data->data = ( PCPSCNT_CHANNEL_DATA ) kzalloc( sizeof( CPSCNT_CHANNEL_DATA ) * max_ch, GFP_KERNEL );

	if( cnt_32xxi_data == (PCPSCNT_32XXI_DATA) NULL ){
		DEBUG_CPSCNT_DEVICE_LIST(KERN_INFO"list channeldata allocate error");
		kfree( cnt_32xxi_data );
		return;
	}

	// Initialize channel Parameter
	for( cnt = 0; cnt < max_ch; cnt ++ ){
		PCPSCNT_CHANNEL_DATA init_data = &cnt_32xxi_data->data[cnt];
		init_data->compare = 0;
		init_data->filter = 0;
		init_data->preset = 0;
		init_data->zPhase = 0x0;//0x02;	// CNT_ZPHASE_NOT_USE
		if(reset_flag_always_on){
			init_data->mode.value = 0x80;
		}else{
			init_data->mode.value = 0x0;
		}
	}

	INIT_LIST_HEAD( &cnt_32xxi_data->list );

	list_add_tail( &cnt_32xxi_data->list, &cpscnt_32xxI_head );

}

/**
	@~English
	@brief CPS-CNT-3202I free software data list.
	@param node : device node
	@return true : 0
	@~Japanese
	@brief CPS-CNT-3202Iのソフトウェアデータリストを削除する関数
	@param node : デバイスノード
	@return 成功 : 0
**/
void cpscnt_32xxI_free_list_of_device( unsigned int node ){

	PCPSCNT_32XXI_DATA cnt_32xxi_data, next;
	unsigned int cnt;

	list_for_each_entry_safe( cnt_32xxi_data, next, &cpscnt_32xxI_head, list ){
		if( cnt_32xxi_data->node == node || node == 0xFFFF ){
			list_del( &cnt_32xxi_data->list );

			for(cnt = 0; cnt < cnt_32xxi_data->max_ch; cnt ++ )
				kfree( cnt_32xxi_data->data );

			kfree( cnt_32xxi_data );
		}
	}
}

/**
	@~English
	@brief CPS-CNT-3202I get software data list.
	@param node : device node
	@return true : structure CPSCNT_32XXI_DATA pointer, false : NULL pointer
	@~Japanese
	@brief CPS-CNT-3202Iのソフトウェアデータリストを取得する関数
	@param node : デバイスノード
	@return 成功 : CPSCNT_32XXI_DATA 構造体ポインタ , 失敗: NULL
**/
PCPSCNT_32XXI_DATA cpscnt_32xxI_get_list_of_device( unsigned int node ){

	PCPSCNT_32XXI_DATA cnt_32xxi_data, next;

	list_for_each_entry_safe( cnt_32xxi_data, next, &cpscnt_32xxI_head, list ){
		if( cnt_32xxi_data->node == node ){
			DEBUG_CPSCNT_DEVICE_LIST(KERN_INFO"get list data : addr: %lx \n",(unsigned long)cnt_32xxi_data );
			return cnt_32xxi_data;
		}
	}
	DEBUG_CPSCNT_DEVICE_LIST(KERN_INFO"Not found list data.");
	return (PCPSCNT_32XXI_DATA)NULL;
}


//debug

/*!
 @name CPSCNT SET/GET list functions
*/
/// @{

/**
	@~English
	@brief CPS-CNT-3202I set operation mode by channel.
	@param ch : channel
	@param pData : data list .
	@param valb : value
	@return true : 0
	@~Japanese
	@brief CPS-CNT-3202Iのオペレーション・モードを設定する関数
	@param ch : チャネル
	@param pData : チャネル・データ
	@param valb : モード
	@return 成功 : 0
**/
void cpscnt_32xxi_set_op_mode( unsigned int ch ,PCPSCNT_32XXI_DATA pData, unsigned char valb )
{
/////////////////// Ver.0.9.2
	pData->data[ch].mode.Bit.sel0 = ( valb & 0x01 )  ? 1  : 0;
	pData->data[ch].mode.Bit.sel1 = ( valb & 0x02 )  ? 1  : 0;
	pData->data[ch].mode.Bit.sel2 = ( valb & 0x04 )  ? 1  : 0;
	pData->data[ch].mode.Bit.ub   = ( valb & 0x10 )  ? 1  : 0;
/////////////////// Ver.0.9.2
}

/**
	@~English
	@brief CPS-CNT-3202I set operation mode by channel.
	@param ch : channel
	@param pData : data list .
	@param valb : value
	@return true : 0
	@~Japanese
	@brief CPS-CNT-3202Iのオペレーション・モードを取得する関数
	@param ch : チャネル
	@param pData : チャネル・データ
	@param valb : モード
	@return 成功 : 0
**/
void cpscnt_32xxi_get_op_mode( unsigned int ch ,PCPSCNT_32XXI_DATA pData, unsigned char *valb )
{
	*valb = ( pData->data[ch].mode.value & 0x17 );
}

/**
	@~English
	@brief CPS-CNT-3202I set operation mode by channel.
	@param ch : channel
	@param pData : data list .
	@param valb : value
	@return true : 0
	@~Japanese
	@brief CPS-CNT-3202Iのオペレーション・モードを設定する関数
	@param ch : チャネル
	@param pData : チャネル・データ
	@param valb : モードリセット
	@return 成功 : 0
**/
void cpscnt_32xxi_set_reset( unsigned int ch ,PCPSCNT_32XXI_DATA pData, unsigned char valb )
{
/////////////////// Ver.0.9.2
	pData->data[ch].mode.Bit.reset = (valb & 0x01)  ? 1  : 0;
/////////////////// Ver.0.9.2
}

/**
	@~English
	@brief CPS-CNT-3202I set operation mode by channel.
	@param ch : channel
	@param pData : data list .
	@param valb : value
	@return true : 0
	@~Japanese
	@brief CPS-CNT-3202Iのオペレーション・モードを取得する関数
	@param ch : チャネル
	@param pData : チャネル・データ
	@param valb : モード・リセット
	@return 成功 : 0
**/
void cpscnt_32xxi_get_reset( unsigned int ch ,PCPSCNT_32XXI_DATA pData, unsigned char *valb )
{
/////////////////// Ver.0.9.2
	*valb = pData->data[ch].mode.Bit.reset  ? 1  : 0;
/////////////////// Ver.0.9.2
}


/**
	@~English
	@brief CPS-CNT-3202I set z logic by channel.
	@param ch : channel
	@param pData : data list .
	@param valb : value
	@return true : 0
	@~Japanese
	@brief CPS-CNT-3202IのZ相の論理を設定する関数
	@param ch : チャネル
	@param pData : チャネル・データ
	@param valb : Z相論理
	@return 成功 : 0
**/
void cpscnt_32xxi_set_z_logic( unsigned int ch ,PCPSCNT_32XXI_DATA pData, unsigned char valb )
{
/////////////////// Ver.0.9.2
	pData->data[ch].mode.Bit.zsel = ( valb & 0x01 )  ? 1  : 0;
/////////////////// Ver.0.9.2
}

/**
	@~English
	@brief CPS-CNT-3202I set z logic by channel.
	@param ch : channel
	@param pData : data list .
	@param valb : value
	@return true : 0
	@~Japanese
	@brief CPS-CNT-3202IのZ相の論理を取得する関数
	@param ch : チャネル
	@param pData : チャネル・データ
	@param valb : Z相論理
	@return 成功 : 0
**/
void cpscnt_32xxi_get_z_logic( unsigned int ch ,PCPSCNT_32XXI_DATA pData, unsigned char *valb )
{
/////////////////// Ver.0.9.2
	*valb = ( pData->data[ch].mode.Bit.zsel )  ? 1  : 0;
/////////////////// Ver.0.9.2
}

/**
	@~English
	@brief CPS-CNT-3202I set z logic by channel.
	@param ch : channel
	@param pData : data list .
	@param valb : value
	@return true : 0
	@~Japanese
	@brief CPS-CNT-3202Iの入力選択を設定する関数
	@param ch : チャネル
	@param pData : チャネル・データ
	@param valb : 入力選択
	@return 成功 : 0
**/
void cpscnt_32xxi_set_selectsignal( unsigned int ch ,PCPSCNT_32XXI_DATA pData, unsigned char valb )
{
	pData->input_sig = ( (valb &  0x01) << ch );
}

/**
	@~English
	@brief CPS-CNT-3202I set z logic by channel.
	@param ch : channel
	@param pData : data list .
	@param valb : value
	@return true : 0
	@~Japanese
	@brief CPS-CNT-3202Iの選択入力を取得する関数
	@param ch : チャネル
	@param pData : チャネル・データ
	@param valb : 入力選択
	@return 成功 : 0
**/
void cpscnt_32xxi_get_selectsignal( unsigned int ch ,PCPSCNT_32XXI_DATA pData, unsigned char *valb )
{
	*valb = ( pData->input_sig & ( 0x01 << ch ) ) >> ch;
}

/**
	@~English
	@brief CPS-CNT-3202I set count direction by channel.
	@param ch : channel
	@param pData : data list .
	@param valb : value
	@return true : 0
	@~Japanese
	@brief CPS-CNT-3202Iのカウント方向を設定する関数
	@param ch : チャネル
	@param pData : チャネル・データ
	@param valb : カウント方向
	@return 成功 : 0
**/
void cpscnt_32xxi_set_count_direction( unsigned int ch ,PCPSCNT_32XXI_DATA pData, unsigned char valb )
{
/////////////////// Ver.0.9.2
	pData->data[ch].mode.Bit.dir = ( valb & 0x01 )  ? 1  : 0;
/////////////////// Ver.0.9.2
}

/**
	@~English
	@brief CPS-CNT-3202I set count direction by channel.
	@param ch : channel
	@param pData : data list .
	@param valb : value
	@return true : 0
	@~Japanese
	@brief CPS-CNT-3202Iのカウント方向を取得する関数
	@param ch : チャネル
	@param pData : チャネル・データ
	@param valb : カウント方向
	@return 成功 : 0
**/
void cpscnt_32xxi_get_count_direction( unsigned int ch ,PCPSCNT_32XXI_DATA pData, unsigned char *valb )
{
/////////////////// Ver.0.9.2
	*valb = ( pData->data[ch].mode.Bit.dir )  ? 1  : 0;
/////////////////// Ver.0.9.2
}

/**
	@~English
	@brief CPS-CNT-3202I set z mode by channel.
	@param ch : channel
	@param pData : data list .
	@param valb : value
	@return true : 0
	@~Japanese
	@brief CPS-CNT-3202IのZモードを設定する関数
	@param ch : チャネル
	@param pData : チャネル・データ
	@param valb : Zモード
	@return 成功 : 0
**/
void cpscnt_32xxi_set_z_mode( unsigned int ch ,PCPSCNT_32XXI_DATA pData, unsigned char valb )
{
	pData->data[ch].zPhase = ( valb & 0x03 ) << 1;

}

/**
	@~English
	@brief CPS-CNT-3202I get z mode by channel.
	@param ch : channel
	@param pData : data list .
	@param valb : value
	@return true : 0
	@~Japanese
	@brief CPS-CNT-3202IのZモードを取得する関数
	@param ch : チャネル
	@param pData : チャネル・データ
	@param valb : Zモード
	@return 成功 : 0
**/
void cpscnt_32xxi_get_z_mode( unsigned int ch ,PCPSCNT_32XXI_DATA pData, unsigned char *valb )
{
	*valb = ( (pData->data[ch].zPhase >> 1) & 0x03 );
}

/**
	@~English
	@brief CPS-CNT-3202I set digital filter by channel.
	@param ch : channel
	@param pData : data list .
	@param valb : value
	@return true : 0
	@~Japanese
	@brief CPS-CNT-3202Iのデジタルフィルタを設定する関数
	@param ch : チャネル
	@param pData : チャネル・データ
	@param valb : デジタルフィルタ
	@return 成功 : 0
**/
void cpscnt_32xxi_set_digital_filter( unsigned int ch ,PCPSCNT_32XXI_DATA pData, unsigned char valb )
{
	pData->data[ch].filter = valb;
}

/**
	@~English
	@brief CPS-CNT-3202I get digital filter by channel.
	@param ch : channel
	@param pData : data list .
	@param valb : value
	@return true : 0
	@~Japanese
	@brief CPS-CNT-3202Iのデジタルフィルタを取得する関数
	@param ch : チャネル
	@param pData : チャネル・データ
	@param valb : デジタルフィルタ
	@return 成功 : 0
**/
void cpscnt_32xxi_get_digital_filter( unsigned int ch ,PCPSCNT_32XXI_DATA pData, unsigned char *valb )
{
	*valb = pData->data[ch].filter;
}

/**
	@~English
	@brief CPS-CNT-3202I set pulse width.
	@param pData : data list .
	@param valb : value
	@return true : 0
	@~Japanese
	@brief CPS-CNT-3202Iのワンショット・パルス幅を設定する関数
	@param pData : チャネル・データ
	@param valb : ワンショット・パルス幅
	@return 成功 : 0
**/
void cpscnt_32xxi_set_pulse_width( PCPSCNT_32XXI_DATA pData, unsigned char valb )
{
	pData->pulsewidth = valb;
}

/**
	@~English
	@brief CPS-CNT-3202I get pulse width.
	@param pData : data list .
	@param valb : value
	@return true : 0
	@~Japanese
	@brief CPS-CNT-3202Iのワンショット・パルス幅を取得する関数
	@param pData : チャネル・データ
	@param valb : ワンショット・パルス幅
	@return 成功 : 0
**/
void cpscnt_32xxi_get_pulse_width( PCPSCNT_32XXI_DATA pData, unsigned char *valb )
{
	*valb = pData->pulsewidth;
}


/**
	@~English
	@brief CPS-CNT-3202I set pulse width.
	@param pData : data list .
	@param valb : value
	@return true : 0
	@~Japanese
	@brief CPS-CNT-3202Iのワンショット・パルス幅を設定する関数
	@param pData : チャネル・データ
	@param valb : カウントラッチ
	@return 成功 : 0
**/
void cpscnt_32xxi_set_count_latch( PCPSCNT_32XXI_DATA pData, unsigned char valb )
{
	pData->latch = valb ;
}

/**
	@~English
	@brief CPS-CNT-3202I get pulse width.
	@param pData : data list .
	@param valb : value
	@return true : 0
	@~Japanese
	@brief CPS-CNT-3202Iのワンショット・パルス幅を取得する関数
	@param pData : チャネル・データ
	@param valb : カウントラッチ
	@return 成功 : 0
**/
void cpscnt_32xxi_get_count_latch( PCPSCNT_32XXI_DATA pData, unsigned char *valb )
{
	*valb = pData->latch;
}


///}@

/**
	@~English
	@brief CNT unsigned long to unsigned char.
	@param data : unsigned long Data
	@param retDat : unsigned char Array
	@par This function is internal function.
	@~Japanese
	@brief CNT 変換関数( unsigned long から unsigned char配列 )
	@param data : unsigned long型 データ
	@param retDat : unsigned char型 データ配列
	@par この間数は内部関数です。
**/ 
void __cpscnt_ulong2uchar(unsigned long data, unsigned char retDat[] )
{
	unsigned int cnt;

	for( cnt = 0; cnt < 4; cnt ++ )
		retDat[cnt] = (unsigned char ) ( ( data >> (cnt * 8) ) & 0xFF );
}


/**
	@~English
	@brief CNT unsigned char to unsigned long.
	@param data : unsigned long Data
	@param retDat : unsigned char Array
	@par This function is internal function.
	@~Japanese
	@brief CNT 変換関数( unsigned char配列 から unsigned long )
	@param data : unsigned char型 データ配列
	@param retDat : unsigned long型 データ
	@par この間数は内部関数です。
**/ 
void __cpscnt_uchar2ulong(unsigned char data[], unsigned long *retDat )
{
	unsigned int cnt;

	for( cnt = 0, *retDat = 0; cnt < 4; cnt ++ )
		*retDat |= ( data[cnt] << (cnt * 8) );
}

/**
	@~English
	@brief CNT Command Function.
	@param BaseAddr : base address
	@param isReadWrite : write, read, or call flag
	@param size : size 1 or 4.
	@param wCommand : command address
	@param vData : data
	@return true : 0
	@~Japanese
	@brief CNT コマンド関数
	@param BaseAddr : ベースアドレス
	@param isReadWrite : 書き込みか読み込みか呼び出し、いずれかのフラグ
	@param size : サイズ ( 1 か  4 )
	@param wCommand : コマンドアドレス
	@param vData : データ
	@return 成功 : 0
**/ 
static long cpscnt_command( unsigned long BaseAddr, unsigned char isReadWrite , unsigned int size, unsigned short wCommand , void* vData )
{
	unsigned char dat[4] = {0};
	unsigned int cnt;
	unsigned long *valdw;
	unsigned char *valb;

	/* command */
	cps_common_outb( BaseAddr + OFFSET_CNT_ADDRESS_COMMAND , wCommand );
	DEBUG_CPSCNT_COMMAND(KERN_INFO"Com:%x hex \n", wCommand );
	/* data */
	switch( isReadWrite ){
	case CPS_CNT_COMMAND_READ :
		DEBUG_CPSCNT_COMMAND(KERN_INFO"<READ>\n");
	
		for( cnt = 0; cnt < size; cnt ++ ){
			cps_common_inpb( BaseAddr + OFFSET_CNT_ADDRESS_DATA  , &dat[cnt] );
			DEBUG_CPSCNT_COMMAND(KERN_INFO"   +%d: %x hex", cnt, dat[cnt]);
		}

		switch( size )
		{
		case 4 :
			valdw = (unsigned long *) vData;
			__cpscnt_uchar2ulong( dat, valdw );
			break;
		case 1 :
			valb = (unsigned char *) vData;
			*valb = (unsigned char)dat[0];
			break;
		default :
			return 0;		
		}

		break;
	
	case CPS_CNT_COMMAND_WRITE :
		DEBUG_CPSCNT_COMMAND(KERN_INFO"<WRITE>\n");

		switch( size )
		{
		case 4 :
			valdw = (unsigned long *) vData;
			__cpscnt_ulong2uchar( *valdw, dat );
			break;
		case 1 :
			valb = (unsigned char *) vData;
			dat[0] = *valb;
			break;
		default :
			return 0;		
		}

		for( cnt = 0; cnt < size; cnt ++ ){
			DEBUG_CPSCNT_COMMAND(KERN_INFO"   +%d: %x hex", cnt, dat[cnt]);
			// DataWrite UnLock
			cps_common_outw( BaseAddr + OFFSET_CNT_COMMAND_DATALOCK	 , CPS_CNT_DATA_UNLOCK );

			cps_common_outb( BaseAddr + OFFSET_CNT_ADDRESS_DATA  , dat[cnt] );

			// DataWrite Lock
			cps_common_outw( BaseAddr + OFFSET_CNT_COMMAND_DATALOCK	 , CPS_CNT_DATA_LOCK );

		}
		break;
	}
	return 0;
}


/*!
 @name CPSCNT-COMMAND
*/
/// @{

/**
	@~English
	@brief Read Counter　macro.
	@~Japanese
	@brief リードカウンタマクロ
**/
#define CPSCNT_COMMAND_READ_COUNT( addr, ch, data )	\
	cpscnt_command( addr, CPS_CNT_COMMAND_READ, 4, CPS_CNT_COMMAND_COUNT( ch ), data )

/**
	@~English
	@brief Initialize Write Counter　macro.
	@~Japanese
	@brief カウンタ初期化マクロ
**/
#define CPSCNT_COMMAND_WRITE_COUNT( addr, ch, data )	\
	cpscnt_command( addr, CPS_CNT_COMMAND_WRITE, 4, CPS_CNT_COMMAND_COUNT( ch ), data )

/**
	@~English
	@brief set mode　macro.
	@~Japanese
	@brief モード設定マクロ
**/
#define CPSCNT_COMMAND_SET_MODE( addr, ch, data )	\
	cpscnt_command( addr, CPS_CNT_COMMAND_WRITE, 1 , CPS_CNT_COMMAND_MODE( ch ), data )

/**
	@~English
	@brief get status　macro.
	@~Japanese
	@brief ステータス取得マクロ
**/
#define CPSCNT_COMMAND_GET_STATUS( addr, ch, data )	\
	cpscnt_command( addr, CPS_CNT_COMMAND_READ, 1, CPS_CNT_COMMAND_STATUS( ch ), data )

/**
	@~English
	@brief set Z Phase　macro.
	@~Japanese
	@brief Z相設定マクロ
**/
#define CPSCNT_COMMAND_SET_Z_PHASE( addr, ch, data )	\
	cpscnt_command( addr, CPS_CNT_COMMAND_WRITE, 1, CPS_CNT_COMMAND_Z_PHASE( ch ), data )

/**
	@~English
	@brief set compare register　macro.
	@~Japanese
	@brief 比較レジスタ設定マクロ
**/
#define CPSCNT_COMMAND_SET_COMPARE_REG( addr, ch, data )	\
	cpscnt_command( addr, CPS_CNT_COMMAND_WRITE, 4, CPS_CNT_COMMAND_COMPARE_REG( ch ), data )

/**
	@~English
	@brief set digital filter　macro.
	@~Japanese
	@brief デジタルフィルタ設定マクロ
**/
#define CPSCNT_COMMAND_SET_DIGITAL_FILTER( addr, ch, data )	\
	cpscnt_command( addr, CPS_CNT_COMMAND_WRITE, 1, CPS_CNT_COMMAND_DIGITAL_FILTER( ch ), data )

/**
	@~English
	@brief set latch counter　macro.
	@~Japanese
	@brief ラッチカウント設定マクロ
**/
#define CPSCNT_COMMAND_SET_COUNT_LATCH( addr, data )	\
	cpscnt_command( addr, CPS_CNT_COMMAND_WRITE, 1, CPS_CNT_COMMAND_LATCH_COUNT, data )

/**
	@~English
	@brief set interrupt mask　macro.
	@~Japanese
	@brief 割り込みマスク設定マクロ
**/
#define CPSCNT_COMMAND_SET_INTERRUPT_MASK( addr, data )	\
	cpscnt_command( addr, CPS_CNT_COMMAND_WRITE, 1, CPS_CNT_COMMAND_INTERRUPT_MASK, data )

/**
	@~English
	@brief get interrupt mask macro.
	@~Japanese
	@brief 割り込みマスク取得マクロ
**/
#define CPSCNT_COMMAND_GET_INTERRUPT_MASK( addr, data )	\
	cpscnt_command( addr, CPS_CNT_COMMAND_READ, 1, CPS_CNT_COMMAND_INTERRUPT_MASK, data )

/**
	@~English
	@brief reset sence macro.
	@~Japanese
	@brief センスリセットマクロ
**/
#define CPSCNT_COMMAND_RESET_SENCE( addr, data )	\
	cpscnt_command( addr, CPS_CNT_COMMAND_WRITE, 1, CPS_CNT_COMMAND_SENCE_PORT, data )

/**
	@~English
	@brief get sence macro.
	@~Japanese
	@brief センス取得マクロ
**/
#define CPSCNT_COMMAND_GET_SENCE( addr, data )	\
	cpscnt_command( addr, CPS_CNT_COMMAND_READ, 1, CPS_CNT_COMMAND_SENCE_PORT, data )

/**
	@~English
	@brief set timer data.
	@~Japanese
	@brief タイマデータ設定マクロ
**/
#define CPSCNT_COMMAND_SET_TIMER_DATA( addr, data )	\
	cpscnt_command( addr, CPS_CNT_COMMAND_WRITE, 4, CPS_CNT_COMMAND_TIMER_DATA, data )

/**
	@~English
	@brief set timer start.
	@~Japanese
	@brief タイマスタート設定マクロ
**/
#define CPSCNT_COMMAND_SET_TIMER_START( addr, data )	\
	cpscnt_command( addr, CPS_CNT_COMMAND_WRITE, 1, CPS_CNT_COMMAND_TIMER_START, data )

/**
	@~English
	@brief set width of one shot pulse.
	@~Japanese
	@brief ワンショットパルス幅 設定マクロ
**/
#define CPSCNT_COMMAND_SET_ONESHOT_PULSE_WIDTH( addr, data )	\
	cpscnt_command( addr, CPS_CNT_COMMAND_WRITE, 1, CPS_CNT_COMMAND_ONESHOT_PULSE_WIDTH, data )

/**
	@~English
	@brief set selectable of common input.
	@~Japanese
	@brief 汎用入力セレクト設定マクロ
**/
#define CPSCNT_COMMAND_SET_SELECT_COMMON_INPUT( addr, data )	\
	cpscnt_command( addr, CPS_CNT_COMMAND_WRITE, 1, CPS_CNT_COMMAND_SELECT_COMMON_INPUT, data )

/**
	@~English
	@brief get selectable of common input.
	@~Japanese
	@brief 汎用入力セレクト取得マクロ
**/
#define CPSCNT_COMMAND_GET_SELECT_COMMON_INPUT( addr, data )	\
	cpscnt_command( addr, CPS_CNT_COMMAND_READ, 1, CPS_CNT_COMMAND_SELECT_COMMON_INPUT, data )

/// @}


/**
	@~English
	@brief CPS-CNT-3202I start by channel.
	@param BaseAddr : base address
	@param enableCh : Enable Channel Number
	@param pData : data list .
	@~Japanese
	@brief CPS-CNT-3202Iのチャネル毎のスタートを設定する関数
	@param BaseAddr : ベースアドレス
	@param enableCh :　有効チャネル
	@param pData : チャネル・データ
**/
void __cpscnt_32xxi_start_by_channel( unsigned long BaseAddr, int enableCh, PCPSCNT_32XXI_DATA pData )
{

	int cnt;

	// リセットセンス
	CPSCNT_COMMAND_RESET_SENCE( BaseAddr, &(pData->sence) );

	// 選択信号入力
	CPSCNT_COMMAND_GET_SELECT_COMMON_INPUT( BaseAddr, &(pData->input_sig) );
	// ワンショットパルス幅
	CPSCNT_COMMAND_SET_ONESHOT_PULSE_WIDTH( BaseAddr, &pData->pulsewidth);

	// チャネルの値入力
	for( cnt = 0; cnt < pData->max_ch; cnt ++ ){
		DEBUG_CPSCNT_START_COUNT("Enable Ch: %x, max Channel : %d", enableCh, pData->max_ch);
		if( enableCh & (1 << cnt) ){
			// デジタルフィルタ
			CPSCNT_COMMAND_SET_DIGITAL_FILTER( BaseAddr, cnt, &pData->data[cnt].filter);

			//Z相設定
			CPSCNT_COMMAND_SET_Z_PHASE( BaseAddr, cnt, &pData->data[cnt].zPhase);

			//比較レジスタ
			CPSCNT_COMMAND_SET_COMPARE_REG( BaseAddr, cnt, &pData->data[cnt].compare);


			//リセットフラグ解除
			if(!reset_flag_always_on ){
				cpscnt_32xxi_set_reset(cnt, pData, 1);
			}

			//モードレジスタ設定
			CPSCNT_COMMAND_SET_MODE( BaseAddr, cnt, &pData->data[cnt].mode.value );
		}
	}
}


/**
	@~English
	@brief CPS-CNT-3202I stop by channel.
	@param BaseAddr : base address
	@param enableCh : Enable Channel Number
	@param pData : data list .
	@~Japanese
	@brief CPS-CNT-3202Iのチャネル毎のストップを設定する関数
	@param BaseAddr : ベースアドレス
	@param enableCh :　有効チャネル
	@param pData : チャネル・データ
**/
void __cpscnt_32xxi_stop_by_channel( unsigned long BaseAddr, unsigned int enableCh ,PCPSCNT_32XXI_DATA pData )
{
	int cnt;

	for( cnt = 0; cnt < pData->max_ch; cnt ++ ){
		if( enableCh & ( 1 << cnt )){
			if( !reset_flag_always_on ){
				//リセットフラグON
				cpscnt_32xxi_set_reset(cnt, pData, 0);
			}
			//モードレジスタ設定
			CPSCNT_COMMAND_SET_MODE( BaseAddr, cnt, &pData->data[cnt].mode.value );
		}
	}
}



/**
	@~English
	@brief This function get Device Name with Counter I/O device.
	@param node : device node
	@param devName : Device Name
	@return true : 0, failed : otherwise
	@~Japanese
	@brief カウンタ機器のデバイス名を取得する関数
	@param node : ノード
	@param devName : デバイス名
	@return 成功 : 0, 失敗 : 1
**/
static long cpscnt_get_cnt_devname( int node, unsigned char devName[] )
{
	int product_id, cnt;
	product_id = contec_mcs341_device_productid_get(node);

	for(cnt = 0; cps_cnt_data[cnt].ProductNumber != -1; cnt++){
		if( cps_cnt_data[cnt].ProductNumber == product_id ){
			strcpy( devName, cps_cnt_data[cnt].Name );
			return 0;
		}
	}
	return 1;
}

/***** Interrupt function *******************************/
static const int AM335X_IRQ_NMI=7;

/**
	@~English
	@brief cpscnt_isr_func
	@param irq : interrupt
	@param dev_instance : device instance
	@return intreturn ( IRQ_HANDLED or IRQ_NONE )
	@~Japanese
	@brief cpscnt 割り込み処理
	@param irq : IRQ番号
	@param dev_instance : デバイス・インスタンス
	@return IRQ_HANDLED か, IRQ_NONE
**/
irqreturn_t cpscnt_isr_func(int irq, void *dev_instance){

	unsigned short wStatus;
	int handled = 0;
	PCPSCNT_DRV_FILE dev =(PCPSCNT_DRV_FILE) dev_instance;
	
	// Ver.0.9.5 Don't insert interrupt "xx callbacks suppressed" by IRQ_NONE.
	if( !dev ){
		DEBUG_CPSCNT_INTERRUPT_CHECK(KERN_INFO"This interrupt is not CONPROSYS CNT Device.");
		goto END_OF_INTERRUPT_CPSCNT;
	}

	if( !contec_mcs341_device_IsCategory( dev->node , CPS_CATEGORY_CNT ) ){
		DEBUG_CPSCNT_INTERRUPT_CHECK("This interrupt is not Category CNT Device.");
		goto END_OF_INTERRUPT_CPSCNT;
	}

	spin_lock(&dev->lock);

	handled = 1;

//END_OF_INTERRUPT_SPIN_UNLOCK_CPSSSI:
		spin_unlock(&dev->lock);

END_OF_INTERRUPT_CPSCNT:
	
	if(IRQ_RETVAL(handled)){
		if(printk_ratelimit())
			printk("cpscnt Device Number:%d IRQ interrupt !\n",( dev->node ) );
	}

	return IRQ_RETVAL(handled);
}


/***** file operation functions *******************************/

/**
	@~English
	@brief cpscnt_ioctl
	@param filp : struct file pointer
	@param cmd : iocontrol command
	@param arg : argument
	@return Success 0, Failed:otherwise 0. (see errno.h)
	@~Japanese
	@brief cpscnt_ioctl
	@param filp : file構造体ポインタ
	@param cmd : I/O コントロールコマンド
	@param arg : 引数
	@return 成功:0 , 失敗:0以外 (errno.h参照)
**/
static long cpscnt_ioctl( struct file *filp, unsigned int cmd, unsigned long arg )
{
	PCPSCNT_DRV_FILE dev = filp->private_data;
	unsigned char valb = 0;
	unsigned long valdw = 0;
	unsigned long flags;

	unsigned long baseAddr = (unsigned long)dev->baseAddr;
	PCPSCNT_32XXI_DATA pData = (PCPSCNT_32XXI_DATA)dev->data.ChannelData;

	struct cpscnt_ioctl_arg ioc;
	struct cpscnt_ioctl_string_arg ioc_str; // Ver.0.9.4
	memset( &ioc, 0 , sizeof(ioc) );
	memset( &ioc_str, 0, sizeof(ioc_str) ); // Ver.0.9.4

	if ( dev == (PCPSCNT_DRV_FILE)NULL ){
		DEBUG_CPSCNT_IOCTL(KERN_INFO"CPSCNT_DRV_FILE NULL POINTER.");
		return -EFAULT;
	}


	switch( cmd ){

		case IOCTL_CPSCNT_READ_COUNT:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					CPSCNT_COMMAND_READ_COUNT( (unsigned long)dev->baseAddr ,ioc.ch, &valdw);
					ioc.val = valdw;
					spin_unlock_irqrestore(&dev->lock, flags);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;
		case IOCTL_CPSCNT_INIT_COUNT:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					spin_lock_irqsave(&dev->lock, flags);
					valdw = ioc.val;
					CPSCNT_COMMAND_WRITE_COUNT( (unsigned long)dev->baseAddr ,ioc.ch, &valdw );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;
		case IOCTL_CPSCNT_GET_STATUS:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					CPSCNT_COMMAND_GET_STATUS((unsigned long)dev->baseAddr ,ioc.ch, &valb );
					ioc.val = (unsigned long) valb;
					spin_unlock_irqrestore(&dev->lock, flags);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSCNT_SET_MODE:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					spin_lock_irqsave(&dev->lock, flags);
					//debug
					valb = (unsigned char) ioc.val;
					cpscnt_32xxi_set_op_mode( ioc.ch, pData, valb );
//					CPSCNT_COMMAND_SET_MODE( (unsigned long)dev->baseAddr , ioc.ch, &valb );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSCNT_SET_Z_PHASE:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					spin_lock_irqsave(&dev->lock, flags);
					valb = (unsigned char) ioc.val;
					cpscnt_32xxi_set_z_mode( ioc.ch, pData, valb );

#ifdef CPSCNT_DIRECT_INOUT_MODE
/////////////////// Ver.0.9.2
					CPSCNT_COMMAND_SET_Z_PHASE( (unsigned long)dev->baseAddr, ioc.ch, &pData->data[ioc.ch].zPhase);
/////////////////// Ver.0.9.2
#endif
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSCNT_SET_COMPARE_REG:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					spin_lock_irqsave(&dev->lock, flags);
					valdw = ioc.val;
//////////////////////// Ver.0.9.2
					pData->data[ioc.ch].compare = valdw;
//////////////////////// Ver.0.9.2

					CPSCNT_COMMAND_SET_COMPARE_REG( (unsigned long)dev->baseAddr , ioc.ch, &valdw );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSCNT_SET_FILTER:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					spin_lock_irqsave(&dev->lock, flags);
					valb = (unsigned char) ioc.val;
					cpscnt_32xxi_set_digital_filter( ioc.ch, pData, valb );
//					CPSCNT_COMMAND_SET_DIGITAL_FILTER( (unsigned long)dev->baseAddr ,ioc.ch, &valb );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSCNT_GET_FILTER:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					cpscnt_32xxi_get_digital_filter( ioc.ch, pData, &valb );
					ioc.val = (unsigned long) valb;
					spin_unlock_irqrestore(&dev->lock, flags);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSCNT_SET_COUNT_LATCH:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					spin_lock_irqsave(&dev->lock, flags);
					valb = (unsigned char) ioc.val;
					cpscnt_32xxi_set_count_latch( pData, valb );
					CPSCNT_COMMAND_SET_COUNT_LATCH( (unsigned long)dev->baseAddr , &valb );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;
		case IOCTL_CPSCNT_GET_COUNT_LATCH:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					cpscnt_32xxi_get_count_latch( pData, &valb );
					ioc.val = (unsigned long) valb;
					spin_unlock_irqrestore(&dev->lock, flags);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSCNT_SET_INTERRUPT_MASK:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					spin_lock_irqsave(&dev->lock, flags);
					valb = (unsigned char) ioc.val;
					CPSCNT_COMMAND_SET_INTERRUPT_MASK( (unsigned long)dev->baseAddr , &valb );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSCNT_GET_INTERRUPT_MASK:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					CPSCNT_COMMAND_GET_INTERRUPT_MASK( (unsigned long)dev->baseAddr , &valb );
					ioc.val = (unsigned long) valb;
					spin_unlock_irqrestore(&dev->lock, flags);
					
					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSCNT_RESET_SENCE:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					spin_lock_irqsave(&dev->lock, flags);
					valb = (unsigned char) ioc.val;
					CPSCNT_COMMAND_RESET_SENCE( (unsigned long)dev->baseAddr , &valb );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSCNT_GET_SENCE:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					CPSCNT_COMMAND_GET_SENCE( (unsigned long)dev->baseAddr , &valb );
					ioc.val = (unsigned long) valb;
					spin_unlock_irqrestore(&dev->lock, flags);
					
					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSCNT_SET_TIMER_DATA:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					spin_lock_irqsave(&dev->lock, flags);
					valdw = ioc.val;
					CPSCNT_COMMAND_SET_TIMER_DATA( (unsigned long)dev->baseAddr , &valdw );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSCNT_SET_TIMER_START:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					spin_lock_irqsave(&dev->lock, flags);
					valb = (unsigned char)ioc.val;

					CPSCNT_COMMAND_SET_TIMER_START( (unsigned long)dev->baseAddr , &valb );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSCNT_SET_ONESHOT_PULSE_WIDTH:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					spin_lock_irqsave(&dev->lock, flags);
					valb = ioc.val;
					cpscnt_32xxi_set_pulse_width(pData, valb);
#ifdef CPSCNT_DIRECT_INOUT_MODE
////////////////////// Ver.0.9.2
					CPSCNT_COMMAND_SET_ONESHOT_PULSE_WIDTH( (unsigned long)dev->baseAddr, &pData->pulsewidth);
////////////////////// Ver.0.9.2
#endif
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSCNT_GET_ONESHOT_PULSE_WIDTH:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					cpscnt_32xxi_get_pulse_width(pData, &valb);
					ioc.val = (unsigned long) valb;
					spin_unlock_irqrestore(&dev->lock, flags);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;


		case IOCTL_CPSCNT_SET_SELECT_COMMON_INPUT:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					spin_lock_irqsave(&dev->lock, flags);
					valb = ioc.val;
					cpscnt_32xxi_set_selectsignal(ioc.ch, pData, valb);
//					CPSCNT_COMMAND_SET_TIMER_DATA( (unsigned long)dev->baseAddr , &valb );
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSCNT_GET_SELECT_COMMON_INPUT:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					cpscnt_32xxi_get_selectsignal( ioc.ch ,pData, &valb );
//					CPSCNT_COMMAND_GET_SELECT_COMMON_INPUT( (unsigned long)dev->baseAddr , &valb );
					ioc.val = (unsigned long) valb;
					spin_unlock_irqrestore(&dev->lock, flags);
					
					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;

		case IOCTL_CPSCNT_GET_DEVICE_NAME:
			// Ver.0.9.4 Modify using from cpscnt_ioctl_arg to cpscnt_ioctl_string_arg
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc_str, (int __user *)arg, sizeof(ioc_str) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					cpscnt_get_cnt_devname( dev->node , ioc_str.str );
					spin_unlock_irqrestore(&dev->lock, flags);
					
					if( copy_to_user( (int __user *)arg, &ioc_str, sizeof(ioc_str) ) ){
						return -EFAULT;
					}
					break;
		case IOCTL_CPSCNT_START_COUNT:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
							return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
							return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					__cpscnt_32xxi_start_by_channel(baseAddr, ioc.val, pData );
					spin_unlock_irqrestore(&dev->lock, flags);
					break;
		case IOCTL_CPSCNT_STOP_COUNT:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
							return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
							return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					__cpscnt_32xxi_stop_by_channel( baseAddr, ioc.val, pData );
					spin_unlock_irqrestore(&dev->lock, flags);
					break;

		case IOCTL_CPSCNT_SET_DIRECTION:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					valb = ioc.val;
					cpscnt_32xxi_set_count_direction(ioc.ch, pData, valb);
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSCNT_GET_MAX_CHANNELS:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					ioc.val = pData->max_ch;
					spin_unlock_irqrestore(&dev->lock, flags);

					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
			break;

///////////////////////////// Ver.0.9.2
		case IOCTL_CPSCNT_SET_Z_LOGIC:
					if(!access_ok(VERITY_READ, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}

					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}					
					spin_lock_irqsave(&dev->lock, flags);
					valb = ioc.val;
					cpscnt_32xxi_set_z_logic(ioc.ch, pData, valb);
					spin_unlock_irqrestore(&dev->lock, flags);

					break;

		case IOCTL_CPSCNT_GET_Z_LOGIC:
					if(!access_ok(VERITY_WRITE, (void __user *)arg, _IOC_SIZE(cmd) ) ){
						return -EFAULT;
					}
					if( copy_from_user( &ioc, (int __user *)arg, sizeof(ioc) ) ){
						return -EFAULT;
					}
					spin_lock_irqsave(&dev->lock, flags);
					cpscnt_32xxi_get_z_logic(ioc.ch, pData, &valb);
					ioc.val = (unsigned long) valb;
					spin_unlock_irqrestore(&dev->lock, flags);
					
					if( copy_to_user( (int __user *)arg, &ioc, sizeof(ioc) ) ){
						return -EFAULT;
					}
					break;
///////////////////////////// Ver.0.9.2
// Ver.0.9.3 add
// Ver.0.9.4 Modify using from cpscnt_ioctl_arg to cpscnt_ioctl_string_arg
		case IOCTL_CPSCNT_GET_DRIVER_VERSION:
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
static int cpscnt_open(struct inode *inode, struct file *filp )
{
	int ret;
	PCPSCNT_DRV_FILE dev;
	int cnt;
	unsigned char __iomem *allocMem;
	unsigned short product_id;
	int iRet = 0;
	int nodeNo = 0; // Ver 0.9.3

	nodeNo = iminor( inode ); // Ver 0.9.3

	DEBUG_CPSCNT_OPEN(KERN_INFO"node %d\n",iminor( inode ) );

	if (notFirstOpenFlg[nodeNo]) {		// Ver 0.9.3 初回オープンでなければ（segmentation fault暫定対策）
		if ( inode->i_private != (PCPSCNT_DRV_FILE)NULL ){
			dev = (PCPSCNT_DRV_FILE)inode->i_private;
			filp->private_data =  (PCPSCNT_DRV_FILE)dev;

			if( dev->ref ){
				dev->ref++;
				return 0;
			}else{
				return -EFAULT;
			}
		}
	}

	filp->private_data = (PCPSCNT_DRV_FILE)kmalloc( sizeof(CPSCNT_DRV_FILE) , GFP_KERNEL );
	if( filp->private_data == (PCPSCNT_DRV_FILE)NULL ){
		iRet = -ENOMEM;
		goto NOT_MEM_PRIVATE_DATA;
	}
	dev = (PCPSCNT_DRV_FILE)filp->private_data;
	inode->i_private = dev;

	dev->node = iminor( inode );
	
	dev->localAddr = 0x08000010 + (dev->node + 1) * 0x100;

	allocMem = cps_common_mem_alloc( dev->localAddr, 0xF0, "cps-cnt", CPS_COMMON_MEM_REGION );

	if( allocMem != NULL ){
		dev->baseAddr = allocMem;
	}else{
		iRet = -ENOMEM;
		goto NOT_IOMEM_ALLOCATION;
	}

	product_id = contec_mcs341_device_productid_get( dev->node );
	cnt = 0;
	do{
		if( cps_cnt_data[cnt].ProductNumber == -1 ) {
			iRet = -EFAULT;
			DEBUG_CPSCNT_OPEN(KERN_INFO"product_id:%x", product_id);
			goto NOT_FOUND_CNT_PRODUCT;
		}
		if( cps_cnt_data[cnt].ProductNumber == product_id ){
			dev->data = cps_cnt_data[cnt];
			break;
		}
		cnt++;
	}while( 1 );


	//Extension(Channel Data) Get Pointer
	if( ( dev->data.ChannelData = (PCPSCNT_32XXI_DATA) cpscnt_32xxI_get_list_of_device( dev->node ) ) == NULL ){
		printk(KERN_INFO" The channel data failed getting.\n");
		iRet = -ENOMEM;
		goto NOT_FOUND_CNT_PRODUCT;
	}

	ret = request_irq(AM335X_IRQ_NMI, cpscnt_isr_func, IRQF_SHARED, "cps-cnt-intr", dev);

	if( ret ){
		DEBUG_CPSCNT_OPEN(" request_irq failed.(%x) \n",ret);
	}

	// spin_lock initialize
	spin_lock_init( &dev->lock );

	dev->ref = 1;
	notFirstOpenFlg[nodeNo]++;		// Ver.0.9.6 segmentation fault暫定対策フラグインクリメント(Forget 0.9.3...)

	return 0;
NOT_FOUND_CNT_PRODUCT:
	cps_common_mem_release( dev->localAddr,
		0xF0,
		dev->baseAddr ,
		CPS_COMMON_MEM_REGION);

NOT_IOMEM_ALLOCATION:
	kfree( dev );

NOT_MEM_PRIVATE_DATA:
 
	inode->i_private = (PCPSCNT_DRV_FILE)NULL;
	filp->private_data = (PCPSCNT_DRV_FILE)NULL;

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
static int cpscnt_close(struct inode * inode, struct file *filp ){

	PCPSCNT_DRV_FILE dev;

	if ( inode->i_private != (PCPSCNT_DRV_FILE)NULL ){
		dev = (PCPSCNT_DRV_FILE)inode->i_private;

		if( dev->ref > 0 ) dev->ref--;

		if( dev->ref == 0 ){

			free_irq(AM335X_IRQ_NMI, dev);

			dev->data.ChannelData = NULL;

			cps_common_mem_release( dev->localAddr,
				0xF0,
				dev->baseAddr ,
				CPS_COMMON_MEM_REGION);
				
			kfree( dev );
			
			inode->i_private = (PCPSCNT_DRV_FILE)NULL;
			
		}
		filp->private_data = (PCPSCNT_DRV_FILE)NULL;
	}
	return 0;

}

/**
	@struct cpscnt_fops
	@brief CPSCNT file operations
**/
static struct file_operations cpscnt_fops = {
		.owner = THIS_MODULE,///< owner's name
		.open = cpscnt_open,///< open
		.release = cpscnt_close,///< close
		.unlocked_ioctl = cpscnt_ioctl,///< I/O Control
};

/**
	@~English
	@brief cpscnt init function.
	@return Success: 0, Failed: otherwise 0
	@~Japanese
	@brief cpscnt 初期化関数.
	@return 成功: 0, 失敗: 0以外
**/
static int cpscnt_init(void)
{

	dev_t dev = MKDEV(cpscnt_major , 0 );
	int ret = 0;
	int major;
	int cnt;
	struct device *devlp = NULL;
	short product_id;
	int	cntNum = 0; // Ver 0.9.3

	// CPS-MCS341 Device Init
	contec_mcs341_controller_cpsDevicesInit();

	// Get Device Number
	cpscnt_max_devs = contec_mcs341_controller_getDeviceNum();

	printk(" cpscnt : driver init. devices %d\n", cpscnt_max_devs);

	ret = alloc_chrdev_region( &dev, 0, cpscnt_max_devs, CPSCNT_DRIVER_NAME );

	if( ret )	return ret;

	cpscnt_major = major = MAJOR(dev);

	cdev_init( &cpscnt_cdev, &cpscnt_fops );
	cpscnt_cdev.owner = THIS_MODULE;
	cpscnt_cdev.ops	= &cpscnt_fops;
	ret = cdev_add( &cpscnt_cdev, MKDEV(cpscnt_major, cpscnt_minor), cpscnt_max_devs );

	if( ret ){
		unregister_chrdev_region( dev, cpscnt_max_devs );
		return ret;
	}

	cpscnt_class = class_create(THIS_MODULE, CPSCNT_DRIVER_NAME );

	if( IS_ERR(cpscnt_class) ){
		cdev_del( &cpscnt_cdev );
		unregister_chrdev_region( dev, cpscnt_max_devs );
		return PTR_ERR(cpscnt_class);
	}

	for( cnt = cpscnt_minor; cnt < ( cpscnt_minor + cpscnt_max_devs ) ; cnt ++){
		if( contec_mcs341_device_IsCategory(cnt , CPS_CATEGORY_CNT ) ){
			product_id = contec_mcs341_device_productid_get( cnt );
			cpscnt_dev = MKDEV( cpscnt_major, cnt );

			devlp = device_create(
				cpscnt_class, NULL, cpscnt_dev, NULL, CPSCNT_DRIVER_NAME"%d", cnt);

			if( IS_ERR(devlp) ){
				cdev_del( &cpscnt_cdev );
				unregister_chrdev_region( dev, cpscnt_max_devs );
				return PTR_ERR(devlp);
			}

			if( product_id == CPS_DEVICE_CNT_3202I )
			{
				cpscnt_32xxI_allocate_list( cnt, 2 );
			}
			cntNum++;// Ver 0.9.3
		}
	}

	// Ver 0.9.3
	if (cntNum) {
		notFirstOpenFlg = (int *)kzalloc( sizeof(int) * cntNum, GFP_KERNEL );	// segmentation fault暫定対策フラグメモリ確保
	}
	return 0;
}

/**
	@~English
	@brief cpscnt exit function.
	@~Japanese
	@brief cpscnt 終了関数.
**/
static void cpscnt_exit(void)
{

	dev_t dev = MKDEV(cpscnt_major , 0 );
	int cnt;
	short product_id;

	kfree(notFirstOpenFlg);		// Ver 0.9.3 segmentation fault暫定対策フラグメモリ解放

	for( cnt = cpscnt_minor; cnt < ( cpscnt_minor + cpscnt_max_devs ) ; cnt ++){
		if( contec_mcs341_device_IsCategory(cnt , CPS_CATEGORY_CNT ) ){

			product_id = contec_mcs341_device_productid_get( cnt );

			cpscnt_dev = MKDEV( cpscnt_major , cnt );
			device_destroy( cpscnt_class, cpscnt_dev );

			if( product_id == CPS_DEVICE_CNT_3202I )
			{
				cpscnt_32xxI_free_list_of_device( cnt );
			}

		}
	}

	class_destroy(cpscnt_class);

	cdev_del( &cpscnt_cdev );

	unregister_chrdev_region( dev, cpscnt_max_devs );


	//free_irq( AM335X_IRQ_NMI, am335x_irq_dev_id );

	//unregister_chrdev( cpscnt_major, CPSCNT_DRIVER_NAME);

}

module_init(cpscnt_init);
module_exit(cpscnt_exit);
