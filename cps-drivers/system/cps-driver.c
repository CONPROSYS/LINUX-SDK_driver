/*
 *  Base Driver for CONPROSYS (only) by CONTEC .
 * Version 1.0.10
 *
 *  Copyright (C) 2015 Syunsuke Okamoto.<okamoto@contec.jp>
 *
 * cps-driver is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *
 * cps-driver driver program is distributed in the hope that it will be useful,
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
//#include <asm/delay.h> // Ver.1.0.7
#include <linux/delay.h> // Ver.1.0.7
#include <linux/time.h>
#include <linux/reboot.h>

#define DRV_VERSION	"1.0.11"

MODULE_LICENSE("GPL");
MODULE_ALIAS("CONTEC CONPROSYS BASE Driver");
MODULE_AUTHOR("syunsuke okamoto");
MODULE_VERSION(DRV_VERSION);

#include "cps_common_io.h"
#include "cps.h"
#include "cps_ids.h"

/**
 @~English
 @name DebugPrint macro
 @~Japanese
 @name デバッグ用表示マクロ
*/
/// @{

#if 0
#define DEBUG_INITMEMORY(fmt...)	printk(fmt)
#else
#define DEBUG_INITMEMORY(fmt...)	do { } while (0)
#endif

#if 0 
#define DEBUG_EXITMEMORY(fmt...)        printk(fmt)
#else
#define DEBUG_EXITMEMORY(fmt...)        do { } while (0)
#endif

#if 0
#define DEBUG_ADDR_VAL_OUT(fmt...)        printk(fmt)
#else
#define DEBUG_ADDR_VAL_OUT(fmt...)        do { } while (0)
#endif

#if 0
#define DEBUG_ADDR_VAL_IN(fmt...)        printk(fmt)
#else
#define DEBUG_ADDR_VAL_IN(fmt...)        do { } while (0)
#endif

#if 0
#define DEBUG_EEPROM_CONTROL(fmt...)        printk(fmt)
#else
#define DEBUG_EEPROM_CONTROL(fmt...)        do { } while (0)
#endif

#if 0
#define DEBUG_SYSTEM_INIT_WRITE_REG(fmt...)        printk(fmt)
#else
#define DEBUG_SYSTEM_INIT_WRITE_REG(fmt...)        do { } while (0)
#endif


#if 0
#define DEBUG_RESET_WRITE_REG(fmt...)        printk(fmt)
#else
#define DEBUG_RESET_WRITE_REG(fmt...)        do { } while (0)
#endif

#if 0
#define DEBUG_MODULE_PARAM(fmt...)        printk(fmt)
#else
#define DEBUG_MODULE_PARAM(fmt...)        do { } while (0)
#endif

/// @}

static unsigned char __iomem *map_baseaddr ;			///< I/O Memory Mapped Base Address (Controller)
static unsigned char __iomem *map_devbaseaddr[CPS_DEVICE_MAX_NUM];	///< I/O Memory Mapped Base Address (Devices)

// 2016.02.17 halt / shutdown button timer counter
static struct timer_list mcs341_timer;	///< timer
static unsigned int reset_count = 0;	///< reset_counter

static unsigned short fpga_ver = 0;	///< fpga_version
static unsigned char mcs341_systeminit_reg = 0; ///< fpga system init data
static unsigned char mcs341_fpga_reset_reg = 0; ///< fpga reset data

//2016.04.29 Error LED timer counter
#define CPS_MCS341_ARRAYNUM_LED_PWR	0
#define CPS_MCS341_ARRAYNUM_LED_ST1	1
#define CPS_MCS341_ARRAYNUM_LED_ST2	2
#define CPS_MCS341_ARRAYNUM_LED_ERR	3

#define CPS_MCS341_MAX_LED_ARRAY_NUM	4

//2016.07.15 Global Spinlock
spinlock_t		mcs341_eeprom_lock;

/**
	@~English
	@brief led timer counter
	@~Japanese
	@brief LEDタイマーカウンタ
**/
static unsigned int led_timer_count[CPS_MCS341_MAX_LED_ARRAY_NUM] = {0};	///< errled_counter
/**
	@~English
	@brief Led ( 0... Disabled, 1...Enabled )
	@~Japanese
	@brief LEDフラグ ( 0 ... 不可 , 1...可 )
**/
static unsigned int ledEnable[CPS_MCS341_MAX_LED_ARRAY_NUM] ={0};
/**
	@~English
	@brief Led bit ( 0... OFF, 1...ON )
	@~Japanese
	@brief エラーLEDビット ( 0 ... OFF , 1...ON )
**/
static unsigned int ledState[CPS_MCS341_MAX_LED_ARRAY_NUM] ={0};




// 2016.02.19 GPIO-87 Push/Pull Test Mode 
static unsigned int reset_button_check_mode = 0;///< gpio-87 test mode ( 1...Enable, 0... Disable )
module_param(reset_button_check_mode, uint, 0644 );

// 2016.04.14  and CPS-MC341-DS2 and CPS-MC341Q-DS1 mode add (Ver.1.0.7)
static unsigned int child_unit = CPS_CHILD_UNIT_NONE;	//CPS-MC341-DS1
module_param(child_unit, uint, 0644 );

// 2016.02.19 GPIO-87 Push/Pull Test Mode 
static unsigned int watchdog_timer_msec = 0;///< gpio-87 test mode ( 0..None, otherwise 0... watchdog on )
//module_param(watchdog_timer_msec, uint, 0644 );

static unsigned char deviceNumber;		///< device number


static unsigned char mcs341_deviceInterrupt[6];	///< device interrupt flag

static const int AM335X_IRQ_NMI=7;	///< IRQ NUMBER

void *am335x_irq_dev_id = (void *)&AM335X_IRQ_NMI;

static int CompleteDevIrqs ;

//static void contec_mcs341_controller_clear_watchdog( void );
//static unsigned char contec_mcs341_controller_setLed( int ledBit, int isEnable );
//static unsigned char contec_mcs341_controller_setSystemInit(void);

/**
	@~English
	@brief am335x_nmi_func
	@param irq : interrupt
	@param dev_instance : device instance
	@return intreturn ( IRQ_HANDLED or IRQ_NONE )
	@~Japanese
	@brief am335x NMI 割り込み処理
	@param irq : IRQ番号
	@param dev_instance : デバイス・インスタンス
	@return IRQ_HANDLED か, IRQ_NONE
**/
irqreturn_t am335x_nmi_isr(int irq, void *dev_instance){

	int cnt = 0;

	int GrpNo, DevIrq;


	if(printk_ratelimit()){
		printk("NMI IRQ interrupt !!\n");
	}
/*
	cnt = CompleteDevIrqs;
 
	// 2015.11.13 add
	while ( cnt < 32 ){
		GrpNo = cnt / 8;
		DevIrq = contec_mcs341_controller_getInterrupt(GrpNo);
		if( DevIrq ){
			//Interrupt 
			for( devcnt = cnt % 8; cnt < 8; cnt ++ ){
				contec_mcs341_device_IsCategory( cnt );
			}		

		}else{
			cnt = GrpNo * 8 ;
		}
	}
*/
	return IRQ_HANDLED;
}

/**
	@~English
	@brief MCS341 Controller's micro second wait time funciton
	@note 14, Apr, 2016 : Change :  If the function more than 1 milisecond wait, it call msleep_interruptible function.
	@param usec : times( micro second order )
	@~Japanese
	@brief MCS341 マイクロ秒ウェイト関数
	@note 2016.04.22 : 1ミリ秒未満の場合, udelay それ以上の場合は msleep_interruptibleに変更
	@note 2016.05.16 : 1ミリ秒未満の場合 udelayから usleep_rangeに変更
	@note 2016.08.09 : スピンロック中にsleepすることが禁止のため、引数を追加
	@param usec : マイクロ秒
**/
static void contec_cps_micro_delay_sleep(unsigned long usec, unsigned int isUsedDelay){

	if( isUsedDelay	){
		while( usec > 0 ){
			udelay( 1 );
			usec--;
		}		
	}
	else{
		if( (usec % 1000) > 0){
			usleep_range( (usec % 1000) , 1000);
		}

		if( (usec / 1000) > 0 ){
			msleep_interruptible( usec / 1000 );
		}
	}
}
EXPORT_SYMBOL_GPL(contec_cps_micro_delay_sleep);

/**
	@~English
	@brief MCS341 Controller's micro second sleep time funciton
	@param usec : times( micro second order )
	@~Japanese
	@brief MCS341 マイクロ秒ウェイト関数

	@param usec : マイクロ秒
**/
static void contec_cps_micro_sleep(unsigned long usec){
	contec_cps_micro_delay_sleep( usec, 0 );
}
EXPORT_SYMBOL_GPL(contec_cps_micro_sleep);

/**
	@~English
	@brief MCS341 Controller's micro second delay time funciton
	@param usec : times( micro second order )
	@~Japanese
	@brief MCS341 マイクロ秒ウェイト関数

	@param usec : マイクロ秒
**/
static void contec_cps_micro_delay(unsigned long usec){
	contec_cps_micro_delay_sleep( usec, 1 );
}
EXPORT_SYMBOL_GPL(contec_cps_micro_delay);

/**
	@~English
	@brief This function waits 5 seconds per device.
	@note MCS341 Controller fpga version is more than 2.
	@~Japanese
	@brief この関数は デバイス毎に5秒待つ関数です。
	@note MCS341 FPGAバージョン 2以降の関数です。
	@par この関数は内部関数です。LVDS電源を落とした後, 所定の電荷に落ちるまでの時間待ちをします。
**/
static void __contec_cps_before_shutdown_wait_devices( void )
{
	int cnt;
	printk(KERN_WARNING"cps_driver: After turning off, please wait at least 5 seconds per device before removing.");
	if( fpga_ver > 1 )
	{
		printk(KERN_WARNING"cps_driver: Wait time [%d] sec.",	deviceNumber * 5 );
		// Wait 5 sec
		for( cnt = 0 ; cnt < deviceNumber; cnt ++ ){
			contec_cps_micro_sleep( 5 * USEC_PER_SEC );
		}
	}

}

/**
	@~English
	@brief MCS341 Controller's address write data.
	@param addr : Address
	@param valb : value
	@~Japanese
	@brief MCS341 Controllerのアドレスにデータを1バイト分書き出す関数
	@param addr : アドレス
	@param valb : 値
**/
static void contec_mcs341_outb(unsigned int addr, unsigned char valb )
{
	DEBUG_ADDR_VAL_OUT(KERN_INFO "[out] cps-system: Offset Address : %x Value %x \n", addr, valb );
	cps_common_outb( (unsigned long)(map_baseaddr + addr), valb );
}

/**
	@~English
	@brief MCS341 Controller's address read data.
	@param addr : Address
	@param valb : value ( unsigned char )
	@~Japanese
	@brief MCS341 Controllerのアドレスにデータを1バイト分読み出す関数
	@param addr : アドレス
	@param valb : 値
**/
static void contec_mcs341_inpb(unsigned int addr, unsigned char *valb )
{
	cps_common_inpb( (unsigned long)(map_baseaddr + addr), valb );
	DEBUG_ADDR_VAL_IN(KERN_INFO "[in]  cps-system: Offset Address : %x Value %x\n", addr, *valb );
}

/**
 @~English
 @name The Controller function
 @~Japanese
 @name コントローラ用関数
*/
/// @{

//------- SET Paramater's -----------------------------------------

/**
	@~English
	@brief MCS341 Controller's sets pin with child board.(optional)
	@param Pin3g3 : Pin Name 3g-3
	@param Pin3g4 : Pin Name 3g-4
	@param Pincts : Pin Name Cts
	@param Pinrts : Pin Name Rts
	@~Japanese
	@brief MCS341 Controllerの子基板のピンを設定する関数
	@param Pin3g3 : 3g-3ピン
	@param Pin3g4 : 3g-4ピン
	@param Pincts : CTSピン
	@param Pinrts : RTSピン
	@warning 子基板のピン設定を間違えると子基板が故障する場合があります。
**/
static unsigned char contec_mcs341_controller_setPinMode(int Pin3g3, int Pin3g4, int Pincts, int Pinrts  ){
	unsigned char valb = 
		CPS_MCS341_SETPINMODE_3G3(Pin3g3) | 
		CPS_MCS341_SETPINMODE_3G4(Pin3g4) |
		CPS_MCS341_SETPINMODE_CTSSUB(Pincts) | 
		CPS_MCS341_SETPINMODE_RTSSUB(Pinrts);
	contec_mcs341_outb( CPS_CONTROLLER_MCS341_SETPINMODE_WADDR, valb );
	//cps_common_outb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_SETPINMODE_WADDR), valb );

	return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_setPinMode);

/**
	@~English
	@brief MCS341 Controller's sets system register.
	@note 2016.05.16 use "mcs341_systeminit_reg"
	@~Japanese
	@brief MCS341 ControllerのSystemInitを設定する関数
	@note 2016.05.16 関数を呼び出す前にmcs341_systeminit_regに値を上書きすること。
**/
static unsigned char contec_mcs341_controller_setSystemInit(void)
{
	contec_mcs341_outb( CPS_CONTROLLER_MCS341_SYSTEMINIT_ADDR, mcs341_systeminit_reg );
	DEBUG_SYSTEM_INIT_WRITE_REG(KERN_INFO"mcs341_systeminit_write_reg %x \n", mcs341_systeminit_reg);
	return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_setSystemInit);


/**
	@~English
	@brief MCS341 Controller's sets fpga reset register.
	@~Japanese
	@brief MCS341 ControllerのRESETを設定する関数
**/
static unsigned char contec_mcs341_controller_setFpgaResetReg(void)
{
	contec_mcs341_outb( CPS_CONTROLLER_MCS341_RESET_WADDR, mcs341_fpga_reset_reg );
	DEBUG_RESET_WRITE_REG(KERN_INFO"mcs341_reset_write_reg %x \n", mcs341_fpga_reset_reg);
	return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_setFpgaResetReg);


/**
	@~English
	@brief MCS341 Controller's set watchdog timer.
	@param wd_timer  : watchdog timer value ( wd_timer * 500(msec)  )
	@param isEnable : 0...disable , 1...Enable
	@note Unimplemented.
	@~Japanese
	@brief MCS341 Controller の ウォッチドッグタイマを設定する関数
	@param wd_timer : ウォッチドッグタイマの値 (  設定値 * 500 ミリ秒 ) 
	@param isEnable : 0...不可能 ,　1...可能
	@note 未実装
**/
static unsigned char contec_mcs341_controller_setWatchdog( int wd_timer , int isEnable ){

	unsigned char valb;

	if ( wd_timer >= 128 ) return 1;
	if ( isEnable > 1 || isEnable < 0 ) return 1;

	valb = CPS_MCS341_WDTIMER_DELAY(wd_timer) | isEnable;

	contec_mcs341_outb(CPS_CONTROLLER_MCS341_WDTIMER_ADDR, valb);

	return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_setWatchdog);

/**
	@~English
	@brief MCS341 Controller's set led.
	@param ledBit : set many LED Bits Data
	@param isEnable : 0...disable , 1...Enable
	@~Japanese
	@brief MCS341 Controller の LEDを設定する関数
	@param ledBit : LED 8ビットデータ
	@param isEnable : 0...不可能 ,　1...可能  
**/
static unsigned char contec_mcs341_controller_setLed( int ledBit, int isEnable ){

	unsigned char valb;

	if ( isEnable > 1 || isEnable < 0 ) return 1;

	contec_mcs341_inpb( CPS_CONTROLLER_MCS341_LED_ADDR, &valb);

	if( ledBit & CPS_MCS341_LED_PWR_BIT )
		valb = (valb & ~CPS_MCS341_LED_PWR_BIT )  | CPS_MCS341_LED_PWR(isEnable);

	if( ledBit & CPS_MCS341_LED_ST1_BIT )
		valb = (valb & ~CPS_MCS341_LED_ST1_BIT )  | CPS_MCS341_LED_ST1(isEnable << 1);

	if( ledBit & CPS_MCS341_LED_ST2_BIT )
		valb = (valb & ~CPS_MCS341_LED_ST2_BIT )  | CPS_MCS341_LED_ST2(isEnable << 2);

	if( ledBit & CPS_MCS341_LED_ERR_BIT )
		valb = (valb & ~CPS_MCS341_LED_ERR_BIT )  | CPS_MCS341_LED_ERR(isEnable << 3);

	contec_mcs341_outb(CPS_CONTROLLER_MCS341_LED_ADDR, valb);

	return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_setLed);

/**
	@~English
	@brief MCS341 Controller's Interrupt.
	@param GroupNum  : Interrupt Groups Number ( from 0 to 4 )
	@param isEnable : 0...disable , 1...Enable
	@~Japanese
	@brief MCS341 Controllerの割り込みを設定する関数
	@param GroupNum  : 割り込みグループ番号 ( 0 から 4まで )
	@param isEnable : 0...不可能 ,　1...可能
**/
static unsigned char contec_mcs341_controller_setInterrupt( int GroupNum , int isEnable ){

	unsigned char valb;

	if ( GroupNum >= 4 ) return 1;
	if ( isEnable > 1 || isEnable < 0 ) return 1;

	valb = mcs341_deviceInterrupt[0] & ~CPS_MCS341_INTERRUPT_GROUP_SET( GroupNum,isEnable );

	contec_mcs341_outb(CPS_CONTROLLER_MCS341_INTERRUPT_ADDR(0), valb);

	mcs341_deviceInterrupt[0] = valb;

	return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_setInterrupt);

/**
	@~English
	@brief MCS341 Controller's Digital I/O Direction.
	@param dioNum  : Digital I/O Number ( from 0 to 3 )
	@param isDir : 0...Input , 1...Output
	@~Japanese
	@brief MCS341 Controllerのデジタル入出力の方向を設定する関数
	@param dioNum  : デジタルビット番号( 0 から 3まで )
	@param isDir : 0...入力,　1...出力
**/
static unsigned char contec_mcs341_controller_setDioDirection( int dioNum , int isDir ){

	unsigned char valb;

	if ( dioNum >= 4 && dioNum < 0 ) return 1;

	if ( isDir < 0 || 1 < isDir )	return 2;
	contec_mcs341_inpb(CPS_CONTROLLER_MCS341_DIO_DIRECTION_ADDR, &valb);

	valb = ( valb & 0xF0 ) | CPS_MCS341_DIO_DIRECTION_SET( isDir, dioNum );

	contec_mcs341_outb(CPS_CONTROLLER_MCS341_DIO_DIRECTION_ADDR, valb);
	return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_setDioDirection);

/**
	@~English
	@brief MCS341 Controller's Digital I/O Filter.
	@param FilterNum  : Digital Filter Number
	@~Japanese
	@brief MCS341 Controllerのデジタル入出力フィルタを設定する関数
	@param FilterNum  : デジタルフィルタ番号
**/
static unsigned char contec_mcs341_controller_setDioFilter( int FilterNum ){

	unsigned char valb;

	if ( FilterNum < 0 || 15 < FilterNum )	return 1;

	contec_mcs341_inpb(CPS_CONTROLLER_MCS341_DIO_DIRECTION_ADDR, &valb);

	valb = ( valb & 0x0F ) | CPS_MCS341_DIO_FILTER_SET(FilterNum);

	contec_mcs341_outb(CPS_CONTROLLER_MCS341_DIO_DIRECTION_ADDR, valb);

	return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_setDioFilter);

/**
	@~English
	@brief MCS341 Controller's Digital Output Values.
	@param value : values ( from 0 to 15 )
	@~Japanese
	@brief MCS341 Controllerのデジタル出力の値を設定する関数
	@param value  : 出力値 ( 0から 15まで )
**/
static unsigned char contec_mcs341_controller_setDoValue( int value ){

	unsigned char valb;

	valb = CPS_MCS341_DIO_DOVALUE_SET( value );
	contec_mcs341_outb( CPS_CONTROLLER_MCS341_DIO_VALUE_ADDR, valb );
	return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_setDoValue);


/***** GET Paramater's ***************/
/**
	@~English
	@brief MCS341 Controller's get Product Version.
	@return product version
	@~Japanese
	@brief MCS341 Controllerの製品バージョンを取得する関数
	@return 製品バージョン
**/
static unsigned char contec_mcs341_controller_getProductVersion(void){
	unsigned char valb = 0;

	contec_mcs341_inpb( CPS_CONTROLLER_MCS341_PRODUCTVERSION_ADDR, &valb );
	return CPS_MCS341_PRODUCT_VERSION(valb);
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_getProductVersion);
/**
	@~English
	@brief MCS341 Controller's get Product Types.
	@return product type
	@~Japanese
	@brief MCS341 Controllerの製品タイプを取得する関数
	@return 製品タイプ
**/
static unsigned char contec_mcs341_controller_getProductType(void){
	unsigned char valb = 0;

	contec_mcs341_inpb( CPS_CONTROLLER_MCS341_PRODUCTVERSION_ADDR, &valb );
	return CPS_MCS341_PRODUCT_TYPE(valb);
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_getProductType);
/**
	@~English
	@brief MCS341 Controller's get FPGA Version.
	@return product type
	@~Japanese
	@brief MCS341 ControllerのFPGAバージョンを取得する関数
	@return FPGAバージョン
**/
static unsigned short contec_mcs341_controller_getFpgaVersion(void){
	unsigned char valb = 0;

	contec_mcs341_inpb( CPS_CONTROLLER_MCS341_FPGAVERSION_ADDR, &valb );
	return valb;
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_getFpgaVersion);

/**
	@~English
	@brief MCS341 Controller's get Unit ID.
	@return unit ID ( 0 to 15 )
	@~Japanese
	@brief MCS341 ControllerのUnit IDを取得する関数
	@return ユニット ID ( 0 to 15 )
**/
static unsigned char contec_mcs341_controller_getUnitId(void){
	unsigned char valb = 0;

	contec_mcs341_inpb( CPS_CONTROLLER_MCS341_ROTARYSW_RADDR, &valb );
	return CPS_MCS341_ROTARYSW_UNITID(valb);
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_getUnitId);
/**
	@~English
	@brief MCS341 Controller's get Group ID.
	@return Group ID ( 0 to 15 )
	@~Japanese
	@brief MCS341 ControllerのGroup IDを取得する関数
	@return グループ ID ( 0 to 15 )
**/
static unsigned char contec_mcs341_controller_getGroupId(void){
	unsigned char valb = 0;

	contec_mcs341_inpb( CPS_CONTROLLER_MCS341_ROTARYSW_RADDR, &valb );
	return CPS_MCS341_ROTARYSW_GROUPID(valb);
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_getGroupId);
/**
	@~English
	@brief The function get Connecting Number in MCS341 Controller.
	@return Device Number
	@~Japanese
	@brief MCS341 Controllerに接続された数を取得する関数。
	@return 接続したデバイス数
**/
static unsigned char contec_mcs341_controller_getDeviceNum(void){
	unsigned char valb = 0;

	contec_mcs341_inpb( CPS_CONTROLLER_MCS341_DEVICENUM_RADDR, &valb );

	return CPS_MCS341_DEVICENUM_VALUE(valb);
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_getDeviceNum);

/**
	@~English
	@brief MCS341 Controller's clear watchdog timer.
	@note Unimplemented.
	@~Japanese
	@brief MCS341 Controller の ウォッチドッグタイマーをクリアする関数
	@note 未実装
**/
static void contec_mcs341_controller_clear_watchdog( void ){

	unsigned char valb;
	contec_mcs341_inpb( CPS_CONTROLLER_MCS341_WDTIMER_ADDR	, &valb);

}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_clear_watchdog);

/**
	@~English
	@brief MCS341 Controller's get led.
	@return ledBit : many LED Bits Data
	@~Japanese
	@brief MCS341 Controller の LEDを設定する関数
	@param ledBit : LED 8ビットデータ
	@return Device Number
**/
static unsigned char contec_mcs341_controller_getLed( void ){

	unsigned char valb;

	contec_mcs341_inpb( CPS_CONTROLLER_MCS341_LED_ADDR, &valb);

	return valb;
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_getLed);

/**
	@~English
	@brief MCS341 Controller's get Interrupt Enable/Disable of Group.
	@param GroupNum  : Interrupt Groups Number ( from 0 to 4 )
	@return interrupt of group ( 0 to 255 )
	@~Japanese
	@brief MCS341 Controllerの割り込みを取得する関数
	@param GroupNum  : 割り込みグループ番号 ( 0 から 4まで )
	@return グループ内の割り込み有効/無効(1グループ 8台分)
**/
static unsigned char contec_mcs341_controller_getInterrupt( int GroupNum ){

	unsigned char valb;

	if ( GroupNum >= 8 ) return 1;

	contec_mcs341_inpb(CPS_CONTROLLER_MCS341_INTERRUPT_ADDR(0), &valb);

	return CPS_MCS341_INTERRUPT_GROUP_GET(GroupNum, valb);
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_getInterrupt);
/**
	@~English
	@brief MCS341 Controller's Digital Echo Output Values.
	@return echo output values ( from 0 to 15 )
	@~Japanese
	@brief MCS341 Controllerのデジタル出力のエコーバックの値を取得する関数
	@return エコーバック出力値 ( 0から 15まで )
**/
static unsigned char contec_mcs341_controller_getDoEchoValue( void ){

	unsigned char valb = 0;
	
	contec_mcs341_inpb(CPS_CONTROLLER_MCS341_DIO_VALUE_ADDR, &valb );

	return CPS_MCS341_DIO_DOECHOVALUE_GET( valb );
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_getDoEchoValue);
/**
	@~English
	@brief MCS341 Controller's Digital Input Values.
	@return input values ( from 0 to 15 )
	@~Japanese
	@brief MCS341 Controllerのデジタル入力の値を取得する関数
	@return  : 入力値 ( 0から 15まで )
**/
static unsigned char contec_mcs341_controller_getDiValue( void ){

	unsigned char valb = 0;

	contec_mcs341_inpb(CPS_CONTROLLER_MCS341_DIO_VALUE_ADDR, &valb );

	return CPS_MCS341_DIO_DIVALUE_GET( valb );
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_getDiValue);

//-------------------------- Timer Function ------------------------

// 2016.02.17 halt / shutdown button timer function
/**
	@~English
	@brief MCS341 Controller's timer function
	@note 17, Feb, 2016 : halt / shutdown button timer function
	@param arg : argument
	@~Japanese
	@brief MCS341 コントローラ用タイマー関数
	@note 2016.02.17 : halt / shutdown ボタン処理用タイマー関数として実装
	@note 2016.06.10 : reset_button_check_modeのとき、gpio87から入力信号が入ると orderly_poweroffが走ってしまう問題を修正
	@param arg : 引数
**/
void mcs341_controller_timer_function(unsigned long arg)
{

	struct timer_list *tick = (struct timer_list *) arg;
	int cnt;

	// Err LED 追加 Ver.1.0.8
	for( cnt = CPS_MCS341_ARRAYNUM_LED_PWR; cnt < CPS_MCS341_MAX_LED_ARRAY_NUM; cnt ++ ){

		if( ledEnable[cnt] ){
			if( led_timer_count[cnt] >= 50 ){ // about 1 sec over
				ledState[cnt] ^= 0x01;	// 点滅
				contec_mcs341_controller_setLed( (1 << cnt) , ledState[cnt] );
				led_timer_count[cnt] = 0;
			}
			else
				led_timer_count[cnt] ++;
		}
	}

	//Ver 1.0.10 [bugfix] 2016.06.10
	if( !reset_button_check_mode ){
		if( gpio_get_value(CPS_CONTROLLER_MCS341_RESET_PIN) ){
			// Ver.1.0.9
			// At first pushed button, LVDS power and systeminit interrupt line off!!
			if(reset_count == 0 && fpga_ver > 1 ){
				// LVDS OFF
				mcs341_fpga_reset_reg &= CPS_MCS341_RESET_SET_LVDS_PWR;
				contec_mcs341_controller_setFpgaResetReg();

				// INTERRUPT LINE OFF
				mcs341_systeminit_reg &= ~CPS_MCS341_SYSTEMINIT_SETINTERRUPT;
				contec_mcs341_controller_setSystemInit();
			}

			reset_count += 1;
			if( reset_count > (5 * 50) ){ //about 5 sec over
				printk(KERN_INFO"RESET !\n");
				gpio_direction_output(CPS_CONTROLLER_MCS341_RESET_POUT, 1);
			}
		}else{
			if( reset_count > 0 ){
				orderly_poweroff(false);
			}
			reset_count = 0;
		}
	}

	if( watchdog_timer_msec ){
		contec_mcs341_controller_clear_watchdog();
	}

	mod_timer(tick, jiffies + CPS_CONTROLLER_MCS341_TICK );
}

//-------------------------- Init Function ------------------------

/**
	@~English
	@brief This function is completed by MCS341 Device ID-Sel.
	@par This function is sub-routine of Initialize.
	@~Japanese
	@brief MCS341 ControllerのID-SELを完了させるための関数。
	@par この関数は内部関数です。初期化を完了させるためのサブルーチンになります。
**/
static void __contec_mcs341_device_idsel_complete( void ){
	int cnt;
	int nInterrupt;

	deviceNumber = contec_mcs341_controller_getDeviceNum();
	DEBUG_INITMEMORY(KERN_INFO " cps-system : device number : %d \n", deviceNumber );
	if( deviceNumber != 0x3f && deviceNumber != 0 ){
		for( cnt = 0; cnt < deviceNumber ; cnt++ ){
			 map_devbaseaddr[cnt] = 
				cps_common_mem_alloc( (0x08000000 + (cnt + 1) * 0x100 ),
				0x10,
				"cps-mcs341-common-dev",
				CPS_COMMON_MEM_NONREGION	 );
				DEBUG_INITMEMORY(KERN_INFO "cps-system: device %d Address:%lx \n",cnt,(unsigned long)map_devbaseaddr[cnt]);
		}
		// 
		cps_common_outb( (unsigned long) ( map_devbaseaddr[0] ) , (deviceNumber | 0x80 ) );
		contec_cps_micro_sleep( 1 * USEC_PER_MSEC );

		nInterrupt = (deviceNumber / 4) + 1;
		for( cnt = 0; cnt < nInterrupt; cnt++ )
			contec_mcs341_controller_setInterrupt( 0 , cnt );
	}
}

/**
	@~English
	@brief This function is CPS-Stack Devices Initialize.
	@return Success 0 , Failed not 0.
	@~Japanese
	@brief MCS341 Controllerのスタックデバイスを初期化する関数。
	@note 2016.05.16 FPGAのRev1以降の場合、
	@return 成功 0, 失敗 0以外.
**/
static int contec_mcs341_controller_cpsDevicesInit(void){
	unsigned char valb = 0;
	unsigned int timeout = 0;

	//cps_common_inpb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_SYSTEMINIT_ADDR), &valb );
	contec_mcs341_inpb( CPS_CONTROLLER_MCS341_SYSTEMINIT_ADDR, &valb );
	valb = valb & 0x0F;

	if( valb != 0x03 && valb != 0x08 && valb != 0x0c ){
		printk(KERN_ERR"cps-driver :[ERROR:INIT] FPGA +3Hex Read %x (Hex)!! Check FPGA Hardware!! \n", valb);
		return -EIO;
	}

	fpga_ver = contec_mcs341_controller_getFpgaVersion();

	printk(KERN_INFO"FPGA_VER:%x\n",fpga_ver);


	if( fpga_ver > 0x01 ){ //FPGA　Revision > Ver 1
		mcs341_fpga_reset_reg |= CPS_MCS341_RESET_SET_LVDS_PWR;
		contec_mcs341_controller_setFpgaResetReg();
	}

	if( CPS_MCS341_SYSTEMINIT_BUSY( valb ) ){

		mcs341_systeminit_reg |= CPS_MCS341_SYSTEMINIT_SETRESET;
		contec_mcs341_controller_setSystemInit();
			do{
			contec_cps_micro_sleep(5);
//		cps_common_inpb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_SYSTEMINIT_ADDR), &valb );
			contec_mcs341_inpb( CPS_CONTROLLER_MCS341_SYSTEMINIT_ADDR, &valb );
			if( timeout >= CPS_DEVICE_INIT_TIMEOUT ) return -ENXIO;
			timeout ++;
		}while( !(valb & CPS_MCS341_SYSTEMINIT_INIT_END) );

		/*
			When many devices was connected more than 15, getDeviceNumber gets 14 values.
			CPS-MCS341 must wait 1 msec.
		*/ 		
		contec_cps_micro_sleep( 1 * USEC_PER_MSEC );	
		__contec_mcs341_device_idsel_complete();
	/*
		cps_common_outb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_RESET_WADDR) ,
			CPS_MCS341_RESET_SET_IDSEL_COMPLETE );
		do{
			contec_cps_micro_sleep(5);
			cps_common_inpb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_SYSTEMINIT_ADDR), &valb);
		}while( valb & CPS_MCS341_SYSTEMINIT_INITBUSY );
	*/
				mcs341_systeminit_reg |= CPS_MCS341_SYSTEMINIT_SETINTERRUPT;
				contec_mcs341_controller_setSystemInit();
		timeout = 0;
		do{
			contec_cps_micro_sleep(5);
//		cps_common_inpb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_SYSTEMINIT_ADDR), &valb );
			contec_mcs341_inpb( CPS_CONTROLLER_MCS341_SYSTEMINIT_ADDR, &valb );
			if( timeout >= CPS_DEVICE_INIT_TIMEOUT ) return -ENXIO;
			timeout ++; 
		}while( !(valb & CPS_MCS341_SYSTEMINIT_INTERRUPT_END)  );
	}
	/*
		When many devices was connected more than 15, getDeviceNumber gets 14 values.
		CPS-MCS341 must wait 1 msec.
	*/ 	
	contec_cps_micro_sleep( 1 * USEC_PER_MSEC );

	return 0;

}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_cpsDevicesInit);

/**
	@~English
	@brief This function is CPS-Child Devices Initialize.
	@param childType: Child Board Type
	@return Success 0 , Failed not 0.
	@~Japanese
	@brief MCS341 Controllerの子基板を初期化する関数。
	@param childType: 子基板番号
	@return 成功 0, 失敗 0以外.
**/
static unsigned int contec_mcs341_controller_cpsChildUnitInit(unsigned int childType)
{
	
	switch( childType ){
	case CPS_CHILD_UNIT_INF_MC341B_00:		// CPS-MCS341G-DS1-111
		contec_mcs341_controller_setPinMode(
			CPS_MCS341_SETPINMODE_3G3_OUTPUT,
			CPS_MCS341_SETPINMODE_3G4_OUTPUT,
			CPS_MCS341_SETPINMODE_CTSSUB_INPUT,
			CPS_MCS341_SETPINMODE_RTSSUB_INPUT
		);
		break;
	case CPS_CHILD_UNIT_INF_MC341B_10:	// CPS-MCS341-DS2
	case CPS_CHILD_UNIT_INF_MC341B_20:	// CPS-MCS341Q-DS1
		contec_mcs341_controller_setPinMode(
			CPS_MCS341_SETPINMODE_3G3_INPUT,
			CPS_MCS341_SETPINMODE_3G4_INPUT,
			CPS_MCS341_SETPINMODE_CTSSUB_CTS,
			CPS_MCS341_SETPINMODE_RTSSUB_RTS
		);
		break;
	case CPS_CHILD_UNIT_JIG_MC341B_00:		// CPS-MCS341-DS1 (JIG)
		contec_mcs341_controller_setPinMode(
			CPS_MCS341_SETPINMODE_3G3_OUTPUT,
			CPS_MCS341_SETPINMODE_3G4_OUTPUT,
			CPS_MCS341_SETPINMODE_CTSSUB_OUTPUT,
			CPS_MCS341_SETPINMODE_RTSSUB_OUTPUT
		);
		break;
	case CPS_CHILD_UNIT_INF_MC341B_40:		// CPS-MCS341G-DS1-110
		contec_mcs341_controller_setPinMode(
			CPS_MCS341_SETPINMODE_3G3_OUTPUT,
			CPS_MCS341_SETPINMODE_3G4_OUTPUT,
			CPS_MCS341_SETPINMODE_CTSSUB_INPUT,
			CPS_MCS341_SETPINMODE_RTSSUB_INPUT
		);
		break;
	case CPS_CHILD_UNIT_NONE:
	default:
		contec_mcs341_controller_setPinMode(
			CPS_MCS341_SETPINMODE_3G3_INPUT,
			CPS_MCS341_SETPINMODE_3G4_INPUT,
			CPS_MCS341_SETPINMODE_CTSSUB_INPUT,
			CPS_MCS341_SETPINMODE_RTSSUB_INPUT
		);
		//break;
		return 0;
	}

	// POWER ON
	mcs341_systeminit_reg |= CPS_MCS341_SYSTEMINIT_SETEXTEND_POWER;
	contec_mcs341_controller_setSystemInit();
	// Wait ( 5sec )
	contec_cps_micro_sleep(5 * USEC_PER_SEC);
	// RESET
	mcs341_systeminit_reg |= CPS_MCS341_SYSTEMINIT_SETEXTEND_RESET;
	contec_mcs341_controller_setSystemInit();

	if( childType == CPS_CHILD_UNIT_INF_MC341B_40 ){
		// fixed HL8528 Bubble Interrupt!
		contec_cps_micro_sleep( 2500 * USEC_PER_MSEC ); // 2.5 sec wait
		mcs341_systeminit_reg |= CPS_MCS341_SYSTEMINIT_3G4_SETOUTPUT;
		contec_mcs341_controller_setSystemInit();

	}

	return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_cpsChildUnitInit);

/// @}

/**
 @~English
 @name The Stack Device function
 @~Japanese
 @name スタックデバイス用関数
*/
/// @{

/**
	@~English
	@brief MCS341 Device's address write 2byte data.
	@param dev : device number
	@param offset : Address Offset
	@param valw : value ( unsigned short )
	@~Japanese
	@brief MCS341 Deviceのアドレスにデータを2バイト分書き出す関数
	@param dev : デバイスID
	@param offset : オフセット
	@param valw : 値
**/
static void contec_mcs341_device_outw(unsigned int dev, unsigned int offset, unsigned short valw )
{
	DEBUG_ADDR_VAL_OUT(KERN_INFO "[out] cps-system: DevNumber : %x Offset Address : %x Value %hx \n", dev, offset, valw );
	cps_common_outw( (unsigned long)(map_devbaseaddr[dev] + offset), valw );
}

/**
	@~English
	@brief MCS341 Device's address read 2byte data.
	@param dev : device number
	@param offset : Address Offset
	@param valw : value ( unsigned short )
	@~Japanese
	@brief MCS341 Deviceのアドレスにデータを2バイト分読み出す関数
	@param dev : デバイスID
	@param offset : オフセット
	@param valw : 値
**/
static void contec_mcs341_device_inpw(unsigned int dev, unsigned int offset, unsigned short *valw )
{
	cps_common_inpw( (unsigned long)(map_devbaseaddr[dev] + offset), valw );
	DEBUG_ADDR_VAL_IN(KERN_INFO "[in]  cps-system: DevNumber : %x Offset Address : %x Value %hx\n", dev, offset, *valw );
}

/**
	@~English
	@brief This function is find CPS-Device Device's Category.
	@param startIndex : Start Index ( >= 1 )
	@param CategoryNum : Product Category Number
	@return Success : 1 , Fail : 0
	@~Japanese
	@brief MCS341 スタックデバイスのカテゴリを探す関数。
	@param startIndex : インデックス番号 ( >= 1 )
	@param CategoryNum : 製品カテゴリ番号
	@return 成功 1, 失敗 0
**/
static unsigned char contec_mcs341_device_FindCategory( int *startIndex,int CategoryNum ){

	int cnt;
	unsigned char valb;
	
	if( *startIndex < 1 || 
	    *startIndex >= deviceNumber
	 ) return 0;

	for( cnt = *startIndex ; cnt < CPS_DEVICE_MAX_NUM ; cnt ++ ){
		cps_common_inpb( (unsigned long)(map_devbaseaddr[cnt] + CPS_DEVICE_COMMON_CATEGORY_ADDR ), &valb );
		if ( valb  == CategoryNum ) break;
 	} 

	if (cnt > CPS_DEVICE_MAX_NUM ) return 0;
	else return 1;

}
EXPORT_SYMBOL_GPL(contec_mcs341_device_FindCategory);

/**
	@~English
	@brief This function is get CPS-Device Device's Category.
	@param targetDevNum : target Device Number (  1 <= targetDevNum <= MAX )
	@param CategoryNum : Product Category Number
	@return Success : 1 , Fail : 0
	@~Japanese
	@brief MCS341 スタックデバイスのカテゴリか判断する関数。
	@param targetDevNum : デバイス番号 (  1 <= targetDevNum <= MAX )
	@param CategoryNum : 製品カテゴリ番号
	@return 成功 1, 失敗 0
**/
static unsigned char contec_mcs341_device_IsCategory( int targetDevNum ,int CategoryNum ){

	unsigned char valb;
	
	if( targetDevNum >= CPS_DEVICE_MAX_NUM ||
		targetDevNum >= deviceNumber ) return 0;

	cps_common_inpb( (unsigned long)(map_devbaseaddr[targetDevNum] + CPS_DEVICE_COMMON_CATEGORY_ADDR ), &valb );
	if ( valb == CategoryNum ) return 1;

 return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_IsCategory);

/**
	@~English
	@brief This function is find CPS-Device.
	@param startIndex : Start Index ( >= 1 )
	@param DeviceNum : Product Device Number
	@return Success : 1 , Fail : 0
	@~Japanese
	@brief MCS341 スタックデバイスを探す関数。
	@param startIndex : インデックス番号 ( >= 1 )
	@param DeviceNum : 製品デバイス番号
	@return 成功 1, 失敗 0
**/
static unsigned char contec_mcs341_device_FindDevice( int *startIndex, int DeviceNum ){

	int cnt;
	unsigned short valw;

	if( *startIndex < 1 || *startIndex >= deviceNumber ) return 0;

	for( cnt = *startIndex ; cnt < CPS_DEVICE_MAX_NUM ; cnt ++ ){
		cps_common_inpw( (unsigned long)(map_devbaseaddr[cnt] + CPS_DEVICE_COMMON_PRODUCTID_ADDR), &valw );
		if ( valw == DeviceNum ) break;
 	} 

	if (cnt > CPS_DEVICE_MAX_NUM ) return 0;
	else return cnt;

}
EXPORT_SYMBOL_GPL(contec_mcs341_device_FindDevice);

/**
	@~English
	@brief This function is get the targeting CPS-Device ID.
	@param dev : Target DeviceNumber ( < deviceNumber )
	@return Success : product Id , Fail : 0
	@~Japanese
	@brief MCS341 ターゲットのスタックデバイス製品番号を取得する関数。
	@param dev : ターゲットのデバイスの製品番号 ( < 接続されたデバイス数 )
	@return 成功  製品番号, 失敗 0
**/
static unsigned short contec_mcs341_device_productid_get( int dev ){

	unsigned short valw;
	
	if( dev >= deviceNumber ) return 0;
	cps_common_inpw( (unsigned long)(map_devbaseaddr[dev] + CPS_DEVICE_COMMON_PRODUCTID_ADDR), &valw );

	return valw;
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_productid_get);

/**
	@~English
	@brief This function is get the targeting CPS-Device Physical ID.
	@param dev : Target DeviceNumber ( < deviceNumber )
	@return Success : product Id , Fail : 0
	@~Japanese
	@brief MCS341 ターゲットの物理接続番号を取得する関数。
	@param dev : ターゲットの物理接続番号 ( < 接続されたデバイス数 )
	@return 成功  物理ID, 失敗 0
**/
static unsigned short contec_mcs341_device_physical_id_get( int dev ){

	unsigned short valw;
	
	if( dev >= deviceNumber ) return 0;
	cps_common_inpw( (unsigned long)(map_devbaseaddr[dev] + CPS_DEVICE_COMMON_PHYSICALID_ADDR), &valw );

	return valw;
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_physical_id_get);

/**
	@~English
	@brief This function is get the targeting CPS-Device mirror register.
	@param dev : Target DeviceNumber ( < deviceNumber )
	@param num : Mirror Address ( 0 or 1 )
	@warning There is a device that does not have the mirror register.
	@return Success : Mirroring Register Values , Fail : 0
	@~Japanese
	@brief MCS341 ターゲットのミラーレジスタを取得する関数。
	@param dev : ターゲットの物理接続番号 ( < 接続されたデバイス数 )
	@param num : ミラーレジスタのアドレス  ( 0 か 1 )
	@warning このレジスタを搭載していないデバイスがあります。
	@return 成功  物理ID, 失敗 0
**/
static unsigned char contec_mcs341_device_mirror_get( int dev , int num ){

	unsigned char valb;

	if( dev >= deviceNumber ) return 0;
	if( num < 0 || num > 1 ) return 0;
	cps_common_inpb( (unsigned long)(map_devbaseaddr[dev] + CPS_DEVICE_COMMON_MIRROR_REG_ADDR(num)), &valb );

	return valb;
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_mirror_get);

/**
	@~English
	@brief This function is written the targeting CPS-Device ROM.
	@param dev : Device Number
	@param valw : value (unsigned short)
	@par This function is internal function.
	@return Success : 0
	@~Japanese
	@brief MCS341 ターゲットのROMに書き込む関数
	@param dev : デバイス番号
	@param valw : 値 (16bit)
	@par この関数は内部関数です。
	@return 成功  物理失敗 0
**/
//static unsigned char __contec_mcs341_device_rom_write_command( unsigned long baseaddr, unsigned short valw )
static unsigned char __contec_mcs341_device_rom_write_command( unsigned int dev, unsigned short valw )
{

	unsigned long flags;

	spin_lock_irqsave(&mcs341_eeprom_lock, flags);
//	cps_common_outw( (unsigned long)(baseaddr + CPS_DEVICE_COMMON_ROM_WRITE_ADDR ),
	contec_mcs341_device_outw( dev, CPS_DEVICE_COMMON_ROM_WRITE_ADDR,
		(valw | CPS_DEVICE_COMMON_ROM_WRITE_CMD_ENABLE ) );
	spin_unlock_irqrestore(&mcs341_eeprom_lock, flags);

	DEBUG_EEPROM_CONTROL(KERN_INFO" <device_rom> : +%x [hex] <- [%x] \n", CPS_DEVICE_COMMON_ROM_WRITE_ADDR, (valw | CPS_DEVICE_COMMON_ROM_WRITE_CMD_ENABLE ) );

	/* wait time */
	switch ( (valw  & 0x80FE )){
		case CPS_DEVICE_COMMON_ROM_WRITE_ACCESS_ENABLE:
		case CPS_DEVICE_COMMON_ROM_WRITE_DATA_READ:
//			contec_cps_micro_sleep(5); break;
			contec_cps_micro_sleep(25); break;
		case CPS_DEVICE_COMMON_ROM_WRITE_DATA_ERASE:
			contec_cps_micro_sleep( 5 * USEC_PER_SEC ); /* 5sec */ break;
		case CPS_DEVICE_COMMON_ROM_WRITE_ADDR_INIT:
		case CPS_DEVICE_COMMON_ROM_WRITE_ACCESS_DISABLE:
//			contec_cps_micro_sleep(1);break;
			contec_cps_micro_sleep(5);break;
		case CPS_DEVICE_COMMON_ROM_WRITE_DATA_WRITE:
			contec_cps_micro_sleep(200);break;
	}

	spin_lock_irqsave(&mcs341_eeprom_lock, flags);
//	cps_common_outw( (unsigned long)(baseaddr + CPS_DEVICE_COMMON_ROM_WRITE_ADDR ),
	contec_mcs341_device_outw( dev, CPS_DEVICE_COMMON_ROM_WRITE_ADDR,
		( (valw & 0x7F00) | CPS_DEVICE_COMMON_ROM_WRITE_CMD_FINISHED) );
	spin_unlock_irqrestore(&mcs341_eeprom_lock, flags);
	DEBUG_EEPROM_CONTROL(KERN_INFO" <device_rom> : +%x [hex] <- [%x] \n", CPS_DEVICE_COMMON_ROM_WRITE_ADDR, ( (valw & 0x7F00) | CPS_DEVICE_COMMON_ROM_WRITE_CMD_FINISHED)  );

	return 0;
}

/**
	@~English
	@brief This function is written (or read or clear) the targeting CPS-Device's logical ID.
	@param dev : Target DeviceNumber ( < deviceNumber )
	@param isWrite : Write ,Read or clear flag
	@param valb : values ( logical id )
	@par This function is internal function.
	@return Success : 1, Failed : 0
	@~Japanese
	@brief MCS341 ターゲットのROMに論理IDを読み込み、書き込み、クリアする関数
	@param dev : ターゲットの物理接続番号 ( < 接続されたデバイス数 )
	@param isWrite : 書き込み、読み込みまたはクリアフラグ
	@param valb : 論理ID
	@par この関数は内部関数です。
	@return 成功  0, 失敗 1
**/
static unsigned char __contec_mcs341_device_logical_id( int dev, int isWrite, unsigned char *valb)
{

	unsigned long flags;

	if( dev >= deviceNumber ) return 1;

	/* Device Id Write */
	if( isWrite == CPS_DEVICE_COMMON_WRITE ){
		spin_lock_irqsave(&mcs341_eeprom_lock, flags);
		cps_common_outb( (unsigned long)(map_devbaseaddr[dev]+ 
			CPS_DEVICE_COMMON_LOGICALID_ADDR) , *valb );
		spin_unlock_irqrestore(&mcs341_eeprom_lock, flags);
	}

	/* ROM ACCESS ENABLE */
//	__contec_mcs341_device_rom_write_command( (unsigned long)map_devbaseaddr[dev],
	__contec_mcs341_device_rom_write_command( dev,
		CPS_DEVICE_COMMON_ROM_WRITE_ACCESS_ENABLE );
	
	/* ROM CLEAR ALL (CLEAR ONLY) */
	if( isWrite == CPS_DEVICE_COMMON_CLEAR ){
//		__contec_mcs341_device_rom_write_command( (unsigned long)map_devbaseaddr[dev],
		__contec_mcs341_device_rom_write_command( dev,
			CPS_DEVICE_COMMON_ROM_WRITE_DATA_ERASE );
	}
	else{	 /* WRITE or READ */
		/* ROM ADDR INITIALIZE */
//		__contec_mcs341_device_rom_write_command( (unsigned long)map_devbaseaddr[dev],
		__contec_mcs341_device_rom_write_command( dev,
			CPS_DEVICE_COMMON_ROM_WRITE_ADDR_INIT );

		/* ROM WRITE or READ FLAG SET */
		if( isWrite == CPS_DEVICE_COMMON_WRITE ){
//			__contec_mcs341_device_rom_write_command( (unsigned long)map_devbaseaddr[dev],
			__contec_mcs341_device_rom_write_command( dev,
				CPS_DEVICE_COMMON_ROM_WRITE_DATA_WRITE );
		}
		else{
//			__contec_mcs341_device_rom_write_command((unsigned long)map_devbaseaddr[dev],
			__contec_mcs341_device_rom_write_command( dev,
				CPS_DEVICE_COMMON_ROM_WRITE_DATA_READ );
 		}
	}
	/* ROM ACCESS DISABLE */
//	__contec_mcs341_device_rom_write_command((unsigned long)map_devbaseaddr[dev],
	__contec_mcs341_device_rom_write_command( dev,
		CPS_DEVICE_COMMON_ROM_WRITE_ACCESS_DISABLE);

	if( isWrite == CPS_DEVICE_COMMON_READ ){
		spin_lock_irqsave(&mcs341_eeprom_lock, flags);
		cps_common_inpb( (unsigned long)(map_devbaseaddr[dev] +
				CPS_DEVICE_COMMON_LOGICALID_ADDR), valb );
		spin_unlock_irqrestore(&mcs341_eeprom_lock, flags);
	}

	return 0;
}

/**
	@~English
	@brief This function is read the targeting CPS-Device's logical ID.
	@param dev : Target DeviceNumber ( < deviceNumber )
	@return Success : logical id
	@~Japanese
	@brief MCS341 ターゲットのROMに論理IDを読み出す関数
	@param dev : ターゲットの物理接続番号 ( < 接続されたデバイス数 )
	@return 成功  論理ID
**/
static unsigned char contec_mcs341_device_logical_id_get( int dev )
{

	unsigned char valb;

	if( dev >= deviceNumber ) return 0;
	
	__contec_mcs341_device_logical_id( dev, CPS_DEVICE_COMMON_READ ,&valb);

	return valb;
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_logical_id_get);

/**
	@~English
	@brief This function is written the targeting CPS-Device's logical ID.
	@param dev : Target DeviceNumber ( < deviceNumber )
	@param valb : logical id values
	@return Success : 0
	@~Japanese
	@brief MCS341 ターゲットのROMに論理IDを書き込む関数
	@param dev : ターゲットの物理接続番号 ( < 接続されたデバイス数 )
	@param valb : 論理ID
	@return 成功  0
**/
static unsigned char contec_mcs341_device_logical_id_set( int dev, unsigned char valb )
{

	if( dev >= deviceNumber ) return 0;
	
	__contec_mcs341_device_logical_id( dev, CPS_DEVICE_COMMON_WRITE ,&valb);

	return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_logical_id_set);

/**
	@~English
	@brief This function is cleared the targeting CPS-Device's logical ID (and all ROM Data).
	@param dev : Target DeviceNumber ( < deviceNumber )
	@warning If you call this function, you own risk.
	@warning This function was running,the device's ROM clear all area.
	@return Success : 0
	@~Japanese
	@brief MCS341 ターゲットのROMに論理IDを書き込む関数
	@param dev : ターゲットの物理接続番号 ( < 接続されたデバイス数 )
	@warning この関数を実行することは自己責任となります。
	@warning 補正などのデバイスに保存した情報もクリアされます。(対象: CPS-AI-1608LI, CPS-AO-1604LI, CPS-SSI-4P )
	@return 成功  0
**/
static unsigned char contec_mcs341_device_logical_id_all_clear( int dev )
{

	if( dev >= deviceNumber ) return 0;
	
	__contec_mcs341_device_logical_id( dev, CPS_DEVICE_COMMON_CLEAR ,NULL );

	return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_logical_id_all_clear);

/**
	@~English
	@brief This function is written (or read or clear) the targeting CPS-Device's Extension Registers.
	@param dev : Target DeviceNumber ( < deviceNumber )
	@param isWrite : Write ,Read or clear flag
	@param cate : Category
	@param num : Page Number
	@param valw : values ( logical id )
	@par This function is internal function.
	@return Success : 1, Failed : 0
	@~Japanese
	@brief MCS341 ターゲットのROMの拡張領域を読み込み、書き込み、クリアする関数
	@param dev : ターゲットの物理接続番号 ( < 接続されたデバイス数 )
	@param isWrite : 書き込み、読み込みまたはクリアフラグ
	@param cate : カテゴリID
	@param num : ページ番号 ( 0 - 4 )
	@param valw : 論理ID
	@par この関数は内部関数です。
	@return 成功  0, 失敗 1
**/
static unsigned char __contec_mcs341_device_extension_value( int dev, int isWrite, unsigned char cate, unsigned char num, unsigned short *valw)
{

	unsigned short valExt , valA;
	unsigned long flags;

	if( dev >= deviceNumber ){
		DEBUG_EEPROM_CONTROL(KERN_INFO" device_extension_value : dev %d\n", dev );
		return 1;
	}
	/* Change Extension Register */
	valExt = (cate << 8) ;
	valA = 0;

	/* value Write */
	if( isWrite == CPS_DEVICE_COMMON_WRITE ){
		spin_lock_irqsave(&mcs341_eeprom_lock, flags);
		cps_common_outw( (unsigned long)(map_devbaseaddr[dev]+ 
			CPS_DEVICE_COMMON_REVISION_ADDR) , valExt );
		DEBUG_EEPROM_CONTROL(KERN_INFO" <device_rom> : +%x [hex] <- [%x] \n", CPS_DEVICE_COMMON_REVISION_ADDR, valExt );

		cps_common_outw( (unsigned long)(map_devbaseaddr[dev]+ 
			CPS_DEVICE_EXTENSION_VALUE(num) ) , *valw );
		DEBUG_EEPROM_CONTROL(KERN_INFO" <device_rom> : +%x [hex] <- [%x] \n", CPS_DEVICE_EXTENSION_VALUE(num), *valw );

		/* Change Extension Register */
		cps_common_outw( (unsigned long)(map_devbaseaddr[dev]+ 
			CPS_DEVICE_COMMON_REVISION_ADDR) , valA );
		DEBUG_EEPROM_CONTROL(KERN_INFO" <device_rom> : +%x [hex] <- [%x] \n", CPS_DEVICE_COMMON_REVISION_ADDR, valA );
		spin_unlock_irqrestore(&mcs341_eeprom_lock, flags);
	}

	/* ROM ACCESS ENABLE */
//	__contec_mcs341_device_rom_write_command( (unsigned long)map_devbaseaddr[dev],
//		(valExt | CPS_DEVICE_COMMON_ROM_WRITE_ACCESS_ENABLE) );
	__contec_mcs341_device_rom_write_command( dev,
			CPS_DEVICE_COMMON_ROM_WRITE_ACCESS_ENABLE );
	/* ROM CLEAR ALL (WRITE ONLY) */
	if( isWrite == CPS_DEVICE_COMMON_CLEAR ){
//		__contec_mcs341_device_rom_write_command( (unsigned long)map_devbaseaddr[dev],
//			(valExt | CPS_DEVICE_COMMON_ROM_WRITE_DATA_ERASE) );
		__contec_mcs341_device_rom_write_command( dev,
			CPS_DEVICE_COMMON_ROM_WRITE_DATA_ERASE );
	}
	else{ /* WRITE or READ */
		/* ROM ADDR INITIALIZE */
//		__contec_mcs341_device_rom_write_command( (unsigned long)map_devbaseaddr[dev],
//		(valExt | CPS_DEVICE_COMMON_ROM_WRITE_ADDR_INIT) );
		__contec_mcs341_device_rom_write_command( dev,
			CPS_DEVICE_COMMON_ROM_WRITE_ADDR_INIT );

		/* ROM WRITE or READ FLAG SET */
		if( isWrite == CPS_DEVICE_COMMON_WRITE ){
//			__contec_mcs341_device_rom_write_command( (unsigned long)map_devbaseaddr[dev],
//			(valExt | CPS_DEVICE_COMMON_ROM_WRITE_DATA_WRITE) );
			__contec_mcs341_device_rom_write_command( dev,
					CPS_DEVICE_COMMON_ROM_WRITE_DATA_WRITE );
		}
		else if( isWrite == CPS_DEVICE_COMMON_READ ) {
//			__contec_mcs341_device_rom_write_command((unsigned long)map_devbaseaddr[dev],
//			( valExt | CPS_DEVICE_COMMON_ROM_WRITE_DATA_READ ) );
			__contec_mcs341_device_rom_write_command( dev,
					CPS_DEVICE_COMMON_ROM_WRITE_DATA_READ);
 		}
	}
	/* ROM ACCESS DISABLE */
//	__contec_mcs341_device_rom_write_command((unsigned long)map_devbaseaddr[dev],
//		(valExt | CPS_DEVICE_COMMON_ROM_WRITE_ACCESS_DISABLE ));
	__contec_mcs341_device_rom_write_command( dev,
			CPS_DEVICE_COMMON_ROM_WRITE_ACCESS_DISABLE );

	if( isWrite == CPS_DEVICE_COMMON_READ ){
		spin_lock_irqsave(&mcs341_eeprom_lock, flags);
		cps_common_outw( (unsigned long)(map_devbaseaddr[dev]+ 
			CPS_DEVICE_COMMON_REVISION_ADDR) , valExt );
		DEBUG_EEPROM_CONTROL(KERN_INFO" <device_rom> : +%x [hex] <- [%x] \n", CPS_DEVICE_COMMON_REVISION_ADDR, valExt );

		cps_common_inpw( (unsigned long)(map_devbaseaddr[dev] +
			CPS_DEVICE_EXTENSION_VALUE(num)), valw );
		DEBUG_EEPROM_CONTROL(KERN_INFO" <device_rom> : +%x [hex] -> [%x] \n", CPS_DEVICE_EXTENSION_VALUE(num), *valw );

		/* Change Extension Register */
		cps_common_outw( (unsigned long)(map_devbaseaddr[dev]+ 
			CPS_DEVICE_COMMON_REVISION_ADDR) , valA );
		DEBUG_EEPROM_CONTROL(KERN_INFO" <device_rom> : +%x [hex] <- [%x] \n", CPS_DEVICE_COMMON_REVISION_ADDR, valA );
		spin_unlock_irqrestore(&mcs341_eeprom_lock, flags);
	}

	return 0;
}

/**
	@~English
	@brief This function is get the targeting CPS-Device's extension register.
	@param dev : Target DeviceNumber ( < deviceNumber )
	@param cate : Category
	@param num : Page Number
	@return Success : get value from extension register, Failed : 0
	@~Japanese
	@brief MCS341 ターゲットのROMから拡張領域を読み出す関数
	@param dev : ターゲットの物理接続番号 ( < 接続されたデバイス数 )
	@param cate : カテゴリID
	@param num : ページ番号 ( 0 - 4 )
	@return 成功  拡張領域から取得した値 失敗 0
**/
static unsigned short contec_mcs341_device_extension_value_get( int dev , int cate ,int num )
{

	unsigned short valw = 0;

	if( dev >= deviceNumber ) return 0;
	
	__contec_mcs341_device_extension_value( dev, CPS_DEVICE_COMMON_READ ,cate, num, &valw);

	return valw;
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_extension_value_get);

/**
	@~English
	@brief This function is written the targeting CPS-Device's extension register.
	@param dev : Target DeviceNumber ( < deviceNumber )
	@param cate : Category
	@param num : Page Number
	@param valw : value
	@return Success : 0 , Failed : 1
	@~Japanese
	@brief MCS341 ターゲットのROMから拡張領域に書き込む関数
	@param dev : ターゲットの物理接続番号 ( < 接続されたデバイス数 )
	@param cate : カテゴリID
	@param num : ページ番号 ( 0 - 4 )
	@param valw : 値
	@return 成功  0 失敗 1
**/
static unsigned short contec_mcs341_device_extension_value_set( int dev, int cate, int num, unsigned short valw )
{

	if( dev >= deviceNumber ) return 1;
	
	__contec_mcs341_device_extension_value( dev, CPS_DEVICE_COMMON_WRITE ,cate, num, &valw);

	return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_extension_value_set);


/**
	@~English
	@brief This function is cleared the targeting CPS-Device's extension register (and all ROM Data).
	@param dev : Target DeviceNumber ( < deviceNumber )
	@param cate : Category
	@warning If you call this function, you own risk.
	@warning This function was running,the device's ROM clear all area.
	@return Success : 0, Failed :1
	@~Japanese
	@brief MCS341 ターゲットのROMの拡張領域をクリアする関数
	@param dev : ターゲットの物理接続番号 ( < 接続されたデバイス数 )
	@param cate : カテゴリID
	@warning この関数を実行することは自己責任となります。
	@warning デバイスに保存した論理ID情報もクリアされます。(対象: CPS-AI-1608LI, CPS-AO-1604LI, CPS-SSI-4P )
	@return 成功  0 失敗 1
**/
static unsigned short contec_mcs341_device_extension_value_all_clear( int dev, int cate )
{

	if( dev >= deviceNumber ) return 1;
	
	__contec_mcs341_device_extension_value( dev, CPS_DEVICE_COMMON_CLEAR ,cate, 0, NULL );

	return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_extension_value_all_clear);

/// @}

/**
	@~English
	@brief This function is get the targeting physical device Number from BaseAddress.
	@param baseAddr : Base Address (but it is not Virtual Memory Address )
	@par This function is used cpscom driver only.
	@return Success : Target Device Number
	@~Japanese
	@brief MCS341 ベースアドレスから物理デバイス番号を取得する関数
	@param baseAddr : ベースアドレス(このベースアドレスは仮想メモリアドレスではありません）
	@par この関数はcpscomドライバのみ使用されています。
	@return 成功  物理デバイス番号
**/
static unsigned char contec_mcs341_device_deviceNum_get( unsigned long baseAddr )
{
		return (  (baseAddr & 0x00002F00 ) >> 8 );
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_deviceNum_get);

/**
	@~English
	@brief This function is get the targeting serial port Number from BaseAddress.
	@param baseAddr : Base Address (but it is not Virtual Memory Address )
	@par This function is used cpscom driver only.
	@return Success : Target Serial Port Number
	@~Japanese
	@brief MCS341 ベースアドレスから使用シリアル番号を取得する関数
	@param baseAddr : ベースアドレス(このベースアドレスは仮想メモリアドレスではありません）
	@par この関数はcpscomドライバのみ使用されています。
	@return 成功  ターゲット戸なるシリアル番号
**/
static unsigned char contec_mcs341_device_serial_channel_get( unsigned long baseAddr )
{
		return (  ( ( baseAddr - 0x10 ) & 0x00000038 ) >> 3);
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_serial_channel_get);



/**
 @~English
 @name Initialize and Exit Functions
 @~Japanese
 @name 初期化/完了用関数
*/
/// @{

/**
	@~English
	@brief cps-driver init function.
	@return Success: 0, Failed: otherwise 0
	@~Japanese
	@brief cps-driver 初期化関数.
	@return 成功: 0, 失敗: 0以外
**/
static int contec_mcs341_controller_init(void)
{
	int ret = 0;
	void __iomem *allocaddr;

	allocaddr = cps_common_mem_alloc(
		 0x08000000,
		 0x100,
		 "cps-mcs341",
		 CPS_COMMON_MEM_REGION	 );

	if( !allocaddr ) return -ENOMEM;
	else map_baseaddr = allocaddr;
	
	cps_fpga_init();

	if( (ret = contec_mcs341_controller_cpsDevicesInit() ) == 0 ){

		DEBUG_MODULE_PARAM("child unit %d\n", child_unit );

		contec_mcs341_controller_cpsChildUnitInit(child_unit);

		//2016.02.17 timer add Ver.1.0.7
		init_timer(&mcs341_timer);
		mcs341_timer.function = mcs341_controller_timer_function;
		mcs341_timer.data = (unsigned long)&mcs341_timer;
		mcs341_timer.expires = jiffies + CPS_CONTROLLER_MCS341_TICK;
		add_timer(&mcs341_timer);

		DEBUG_MODULE_PARAM("reset_button_check_mode %d\n",reset_button_check_mode);

		if( !reset_button_check_mode ){

			gpio_free(CPS_CONTROLLER_MCS341_RESET_PIN);
			gpio_request(CPS_CONTROLLER_MCS341_RESET_PIN, "cps_mcs341_reset");
			gpio_direction_input(CPS_CONTROLLER_MCS341_RESET_PIN);

			gpio_free(CPS_CONTROLLER_MCS341_RESET_POUT);
			gpio_request(CPS_CONTROLLER_MCS341_RESET_POUT, "cps_mcs341_reset_out");
			gpio_export(CPS_CONTROLLER_MCS341_RESET_POUT, true ) ; //2016.02.22  Ver.1.0.7
		}

		// Ver.1.0.8 watchdog timer mode add
		if( watchdog_timer_msec ){
			printk(KERN_INFO"cps-driver : FPGA Watchdog timer Enable %d msec.\n", watchdog_timer_msec*500 );
			contec_mcs341_controller_setWatchdog(watchdog_timer_msec, CPS_MCS341_WDTIMER_ENABLE );
			ledEnable[CPS_MCS341_ARRAYNUM_LED_ST1] = 1;
		}
	}else{

		if( map_baseaddr ){
			cps_common_mem_release( 0x08000000,
				0x100,
				map_baseaddr ,
				CPS_COMMON_MEM_REGION);
		}
	}

	if( !ret ){
		// spin_lock initialize
		spin_lock_init( &mcs341_eeprom_lock );
	}

	return ret;
}

/**
	@~English
	@brief cps-driver exit function.
	@~Japanese
	@brief cps-driver 終了関数.
	@note 2016.06.10 : CPS_COMMON_MEM_NONREGIONから CPS_COMMON_MEM_REGIONに修正。( rmmod後再度insmodするとエラーになる問題を修正 )
**/
static void contec_mcs341_controller_exit(void)
{
	int cnt = 0;

	if( !reset_button_check_mode ){
		del_timer_sync(&mcs341_timer);		//2016.02.17 timer
		gpio_free(CPS_CONTROLLER_MCS341_RESET_PIN);
	}

	if( deviceNumber != 0x3f ){
		for( cnt = 0; cnt < deviceNumber ; cnt++ ){
			cps_common_mem_release( (0x08000000 + (cnt + 1) * 0x100 ),
				0x10,
				map_devbaseaddr[cnt],
//			CPS_COMMON_MEM_NONREGION );
				CPS_COMMON_MEM_REGION	 );
		}
	} 

	cps_common_mem_release( 0x08000000,
		0x100,
		map_baseaddr ,
		CPS_COMMON_MEM_REGION);
	return;
}

module_init(contec_mcs341_controller_init);
module_exit(contec_mcs341_controller_exit);

/// @}


