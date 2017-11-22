/*
 *  Base Driver for CONPROSYS (only) by CONTEC .
 * Version 1.0.6
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
#include <asm/delay.h>
#include <linux/time.h>
#include <linux/reboot.h>

#define DRV_VERSION	"1.0.5"

MODULE_LICENSE("GPL");
MODULE_ALIAS("CONTEC CONPROSYS BASE Driver");
MODULE_AUTHOR("syunsuke okamoto");
MODULE_VERSION(DRV_VERSION);

#include "../include/cps.h"
#include "../include/cps_ids.h"
#include "../include/cps_common_io.h"

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

static unsigned char __iomem *map_baseaddr ;
static unsigned char __iomem *map_devbaseaddr[CPS_DEVICE_MAX_NUM];

// 2016.02.17 halt / shutdown button timer counter
static struct timer_list mcs341_timer;	// timer
static unsigned int reset_count = 0;	// reset_counter

// 2016.02.19 GPIO-87 Push/Pull Test Mode 
static unsigned int reset_button_check_mode = 0;// gpio-87 test mode ( 1...Enable, 0... Disable )
module_param(reset_button_check_mode, uint, 0644 );


static unsigned char deviceNumber;


static unsigned char mcs341_deviceInterrupt[6];

static const int AM335X_IRQ_NMI=7;

void *am335x_irq_dev_id = (void *)&AM335X_IRQ_NMI;

static int CompleteDevIrqs ;

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

// 2016.02.17 halt / shutdown button timer function
void mcs341_controller_timer_function(unsigned long arg)
{

	struct timer_list *tick = (struct timer_list *) arg;

	if( gpio_get_value(CPS_CONTROLLER_MCS341_RESET_PIN) ){
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

	mod_timer(tick, jiffies + CPS_CONTROLLER_MCS341_TICK );
}


static void contec_cps_micro_delay_sleep(unsigned long usec ){
	while( usec > 0 ){
		udelay( 1 );
		usec--;
	}
}
EXPORT_SYMBOL_GPL(contec_cps_micro_delay_sleep);

static void contec_mcs341_outb(unsigned int addr, unsigned char valb )
{
	DEBUG_ADDR_VAL_OUT(KERN_INFO " cps-system: Offset Address : %x Value %x \n", addr, valb );
	cps_common_outb( (unsigned long)(map_baseaddr + addr), valb );
}

static void contec_mcs341_inpb(unsigned int addr, unsigned char *valb )
{
	cps_common_inpb( (unsigned long)(map_baseaddr + addr), valb );
	DEBUG_ADDR_VAL_IN(KERN_INFO " cps-system: Offset Address : %x Value %x\n", addr, *valb );
}

/***** SET Paramater's ***************/

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

static unsigned char contec_mcs341_controller_setInterrupt( int GroupNum , int isEnable ){

	unsigned char valb;

	if ( GroupNum >= 4 ) return 1;
	if ( isEnable > 1 || isEnable < 0 ) return 1;

	valb = mcs341_deviceInterrupt[0] & ~CPS_MCS341_INTERRUPT_GROUP_SET( GroupNum,isEnable );

	contec_mcs341_outb(CPS_CONTROLLER_MCS341_INTERRUPT_ADDR(0), valb);

	return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_setInterrupt);

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


static unsigned char contec_mcs341_controller_setDioFilter( int FilterNum ){

	unsigned char valb;

	if ( FilterNum < 0 || 15 < FilterNum )	return 1;

	contec_mcs341_inpb(CPS_CONTROLLER_MCS341_DIO_DIRECTION_ADDR, &valb);

	valb = ( valb & 0x0F ) | CPS_MCS341_DIO_FILTER_SET(FilterNum);

	contec_mcs341_outb(CPS_CONTROLLER_MCS341_DIO_DIRECTION_ADDR, valb);

	return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_setDioFilter);

static unsigned char contec_mcs341_controller_setDoValue( int dioNum, int value ){

	unsigned char valb;

	valb = CPS_MCS341_DIO_DOVALUE_SET( dioNum, value );

	cps_common_outb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_DIO_VALUE_ADDR), valb );

	return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_setDoValue);


/***** GET Paramater's ***************/

static unsigned char contec_mcs341_controller_getProductVersion(void){
	unsigned char valb = 0;

	cps_common_inpb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_PRODUCTVERSION_ADDR), &valb );

	return CPS_MCS341_PRODUCT_VERSION(valb);
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_getProductVersion);

static unsigned char contec_mcs341_controller_getProductType(void){
	unsigned char valb = 0;

	cps_common_inpb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_PRODUCTVERSION_ADDR), &valb );

	return CPS_MCS341_PRODUCT_TYPE(valb);
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_getProductType);

static unsigned short contec_mcs341_controller_getFpgaVersion(void){
	unsigned char valb = 0;

	cps_common_inpb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_FPGAVERSION_ADDR), &valb );
	return valb;
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_getFpgaVersion);


static unsigned char contec_mcs341_controller_getUnitId(void){
	unsigned char valb = 0;

	cps_common_inpb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_ROTARYSW_RADDR), &valb );

	return CPS_MCS341_ROTARYSW_UNITID(valb);
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_getUnitId);

static unsigned char contec_mcs341_controller_getGroupId(void){
	unsigned char valb = 0;

	cps_common_inpb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_ROTARYSW_RADDR), &valb );

	return CPS_MCS341_ROTARYSW_GROUPID(valb);
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_getGroupId);

static unsigned char contec_mcs341_controller_getDeviceNum(void){
	unsigned char valb = 0;

	cps_common_inpb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_DEVICENUM_RADDR), &valb );

	return CPS_MCS341_DEVICENUM_VALUE(valb);
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_getDeviceNum);

static unsigned char contec_mcs341_controller_getInterrupt( int GroupNum ){

	unsigned char valb;

	if ( GroupNum >= 8 ) return 1;

	contec_mcs341_inpb(CPS_CONTROLLER_MCS341_INTERRUPT_ADDR(0), &valb);

	return CPS_MCS341_INTERRUPT_GROUP_GET(GroupNum, valb);
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_getInterrupt);

static unsigned char contec_mcs341_controller_getDoEchoValue( int dioNum ){

	unsigned char valb = 0;
	
	if( dioNum < 0 || dioNum > 3 ) return -1;
	cps_common_inpb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_DIO_VALUE_ADDR), &valb );

	return CPS_MCS341_DIO_DOECHOVALUE_GET(dioNum, valb);
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_getDoEchoValue);

static unsigned char contec_mcs341_controller_getDiValue( int dioNum ){

	unsigned char valb = 0;

	if( dioNum < 0 || dioNum > 3 ) return -1;
	cps_common_inpb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_DIO_VALUE_ADDR), &valb );

	return CPS_MCS341_DIO_DIVALUE_GET(dioNum, valb);
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_getDiValue);

/*
	@note :: Id-Sel of MCS341 complete sub routine.
*/ 
static void contec_mcs341_device_idsel_complete( void ){
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
		contec_cps_micro_delay_sleep( 1 * USEC_PER_MSEC );

		nInterrupt = (deviceNumber / 4) + 1;
		for( cnt = 0; cnt < nInterrupt; cnt++ )
			contec_mcs341_controller_setInterrupt( 0 , cnt );
	}
}

/*
	@note :: cps-driver CPS-Devices Initialize. Success 0 , Failed not 0.
*/ 
static unsigned char contec_mcs341_controller_cpsDevicesInit(void){
	unsigned char valb = 0;
	unsigned int timeout = 0;
	cps_common_inpb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_SYSTEMINIT_ADDR), &valb ); 
	if( CPS_MCS341_SYSTEMINIT_BUSY( valb ) ){
		cps_common_outb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_SYSTEMINIT_ADDR),
			CPS_MCS341_SYSTEMINIT_SETRESET );

		do{
			contec_cps_micro_delay_sleep(5);
			cps_common_inpb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_SYSTEMINIT_ADDR), &valb );
			if( timeout >= CPS_DEVICE_INIT_TIMEOUT ) return 1;
			timeout ++;
		}while( !(valb & CPS_MCS341_SYSTEMINIT_INIT_END) );

		/*
			When many devices was connected more than 15, getDeviceNumber gets 14 values.
			CPS-MCS341 must wait 1 msec.
		*/ 		
		contec_cps_micro_delay_sleep( 1 * USEC_PER_MSEC );	
		contec_mcs341_device_idsel_complete();

/*
		cps_common_outb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_RESET_WADDR) ,
			CPS_MCS341_RESET_SET_IDSEL_COMPLETE );
		do{
			contec_cps_micro_delay_sleep(5);
			cps_common_inpb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_SYSTEMINIT_ADDR), &valb);
		}while( valb & CPS_MCS341_SYSTEMINIT_INITBUSY );
*/
		cps_common_outb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_SYSTEMINIT_ADDR),
			( CPS_MCS341_SYSTEMINIT_SETRESET | CPS_MCS341_SYSTEMINIT_SETINTERRUPT) );
		
		timeout = 0;
		do{
			contec_cps_micro_delay_sleep(5);
			cps_common_inpb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_SYSTEMINIT_ADDR), &valb );
			if( timeout >= CPS_DEVICE_INIT_TIMEOUT ) return 2;
			timeout ++; 
		}while( !(valb & CPS_MCS341_SYSTEMINIT_INTERRUPT_END)  );
	}
		/*
			When many devices was connected more than 15, getDeviceNumber gets 14 values.
			CPS-MCS341 must wait 1 msec.
		*/ 	
	contec_cps_micro_delay_sleep( 1 * USEC_PER_MSEC );
	return 0;

}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_cpsDevicesInit);

static unsigned int contec_mcs341_controller_cpsChildUnitInit(unsigned int childType)
{
	
	switch( childType ){
	case CPS_CHILD_UNIT_INF_MC341B_00:		// CPS-MCS341G-DS1
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
	case CPS_CHILD_UNIT_JIG_MC341B_00:		// CPS-MCS341-DS1 (JIG)
		contec_mcs341_controller_setPinMode(
			CPS_MCS341_SETPINMODE_3G3_OUTPUT,
			CPS_MCS341_SETPINMODE_3G4_OUTPUT,
			CPS_MCS341_SETPINMODE_CTSSUB_OUTPUT,
			CPS_MCS341_SETPINMODE_RTSSUB_OUTPUT
		);
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
	cps_common_outb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_SYSTEMINIT_ADDR),
		( CPS_MCS341_SYSTEMINIT_SETRESET | CPS_MCS341_SYSTEMINIT_SETINTERRUPT | CPS_MCS341_SYSTEMINIT_SETEXTEND_POWER) );
	// Wait ( 5sec )
	contec_cps_micro_delay_sleep(5 * USEC_PER_SEC);
	// RESET
	cps_common_outb( (unsigned long)(map_baseaddr + CPS_CONTROLLER_MCS341_SYSTEMINIT_ADDR),
		( CPS_MCS341_SYSTEMINIT_SETRESET | CPS_MCS341_SYSTEMINIT_SETINTERRUPT | CPS_MCS341_SYSTEMINIT_SETEXTEND_POWER | CPS_MCS341_SYSTEMINIT_SETEXTEND_RESET) );
	return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_controller_cpsChildUnitInit);

/*
	@param startIndex : Start Index ( >= 1 )
	@param CategoryNum : Product Category Number
	@note :: Success : 1 , Fail : 0
*/  
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

/*
	@param startIndex : Start Index ( >= 1 )
	@param CategoryNum : Product Category Number
	@note :: Success : 1 , Fail : 0
*/  
static unsigned char contec_mcs341_device_IsCategory( int targetDevNum ,int CategoryNum ){

	unsigned char valb;
	
	if( targetDevNum >= CPS_DEVICE_MAX_NUM ||
		targetDevNum >= deviceNumber ) return 0;

	cps_common_inpb( (unsigned long)(map_devbaseaddr[targetDevNum] + CPS_DEVICE_COMMON_CATEGORY_ADDR ), &valb );
	if ( valb == CategoryNum ) return 1;

 return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_IsCategory);

/*
	@param startIndex : Start Index ( >= 1 )
	@param DeviceNum : Product Device Number
	@note :: Success : Find Physical Number , Fail : 0
*/  
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

/*
	@param dev : Target DeviceNumber ( < deviceNumber )
	@note :: Success : product Id , Fail : 0
*/  
static unsigned short contec_mcs341_device_productid_get( int dev ){

	unsigned short valw;
	
	if( dev >= deviceNumber ) return 0;
	cps_common_inpw( (unsigned long)(map_devbaseaddr[dev] + CPS_DEVICE_COMMON_PRODUCTID_ADDR), &valw );

	return valw;
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_productid_get);

/*
	@param dev : Target DeviceNumber ( < deviceNumber )
	@note :: Success : product Id , Fail : 0
*/  
static unsigned short contec_mcs341_device_physical_id_get( int dev ){

	unsigned short valw;
	
	if( dev >= deviceNumber ) return 0;
	cps_common_inpw( (unsigned long)(map_devbaseaddr[dev] + CPS_DEVICE_COMMON_PHYSICALID_ADDR), &valw );

	return valw;
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_physical_id_get);



/*
	@param dev : Target DeviceNumber ( < deviceNumber )
	@param num : Mirror Address ( 0 or 1 )
	@note :: Success : Mirroring Register Values Get , Fail : 0
*/  
static unsigned char contec_mcs341_device_mirror_get( int dev , int num ){

	unsigned char valb;

	if( dev >= deviceNumber ) return 0;
	if( num < 0 || num > 1 ) return 0;
	cps_common_inpb( (unsigned long)(map_devbaseaddr[dev] + CPS_DEVICE_COMMON_MIRROR_REG_ADDR(num)), &valb );

	return valb;
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_mirror_get);


/*
	@param dev : Target DeviceNumber ( < deviceNumber )
	@param addr : ROM Address ( 0 or 1 )
	@param valw : value (ushort)
*/  
static unsigned char __contec_mcs341_device_rom_write_command( unsigned long baseaddr, unsigned short valw )
{
	cps_common_outw( (unsigned long)(baseaddr + CPS_DEVICE_COMMON_ROM_WRITE_ADDR ), 
		(valw | CPS_DEVICE_COMMON_ROM_WRITE_CMD_ENABLE ) );

	DEBUG_EEPROM_CONTROL(KERN_INFO" <device_rom> : +%x [hex] <- [%x] \n", CPS_DEVICE_COMMON_ROM_WRITE_ADDR, (valw | CPS_DEVICE_COMMON_ROM_WRITE_CMD_ENABLE ) );

	/* wait time */
	switch ( (valw  & 0x80FE )){
		case CPS_DEVICE_COMMON_ROM_WRITE_ACCESS_ENABLE:
		case CPS_DEVICE_COMMON_ROM_WRITE_DATA_READ:
//			contec_cps_micro_delay_sleep(5); break;
			contec_cps_micro_delay_sleep(25); break;
		case CPS_DEVICE_COMMON_ROM_WRITE_DATA_ERASE:
			contec_cps_micro_delay_sleep( 5 * USEC_PER_SEC ); /* 5sec */ break;
		case CPS_DEVICE_COMMON_ROM_WRITE_ADDR_INIT:
		case CPS_DEVICE_COMMON_ROM_WRITE_ACCESS_DISABLE:
//			contec_cps_micro_delay_sleep(1);break;
			contec_cps_micro_delay_sleep(5);break;
		case CPS_DEVICE_COMMON_ROM_WRITE_DATA_WRITE:
			contec_cps_micro_delay_sleep(200);break;
	}

	cps_common_outw( (unsigned long)(baseaddr + CPS_DEVICE_COMMON_ROM_WRITE_ADDR ),
		( (valw & 0x7F00) | CPS_DEVICE_COMMON_ROM_WRITE_CMD_FINISHED) );
	DEBUG_EEPROM_CONTROL(KERN_INFO" <device_rom> : +%x [hex] <- [%x] \n", CPS_DEVICE_COMMON_ROM_WRITE_ADDR, ( (valw & 0x7F00) | CPS_DEVICE_COMMON_ROM_WRITE_CMD_FINISHED)  );

	return 0;
}

/*
	@param dev : Target DeviceNumber ( < deviceNumber )
	@param isWrite : Write or Read flag ( 0 or 1 )
	@param valb : values ( logical id )
	@note :: Success : 1 , Fail : 0
*/  
static unsigned char __contec_mcs341_device_logical_id( int dev, int isWrite, unsigned char *valb)
{

	if( dev >= deviceNumber ) return 0;

	/* Device Id Write */
	if( isWrite == CPS_DEVICE_COMMON_WRITE ){
		cps_common_outb( (unsigned long)(map_devbaseaddr[dev]+ 
			CPS_DEVICE_COMMON_LOGICALID_ADDR) , *valb );
	}

	/* ROM ACCESS ENABLE */
	__contec_mcs341_device_rom_write_command( (unsigned long)map_devbaseaddr[dev], 
		CPS_DEVICE_COMMON_ROM_WRITE_ACCESS_ENABLE );
	
	/* ROM CLEAR ALL (CLEAR ONLY) */
	if( isWrite == CPS_DEVICE_COMMON_CLEAR ){
		__contec_mcs341_device_rom_write_command( (unsigned long)map_devbaseaddr[dev],
			CPS_DEVICE_COMMON_ROM_WRITE_DATA_ERASE );
	}
	else{	 /* WRITE or READ */
		/* ROM ADDR INITIALIZE */
		__contec_mcs341_device_rom_write_command( (unsigned long)map_devbaseaddr[dev],
			CPS_DEVICE_COMMON_ROM_WRITE_ADDR_INIT );

		/* ROM WRITE or READ FLAG SET */
		if( isWrite == CPS_DEVICE_COMMON_WRITE ){
			__contec_mcs341_device_rom_write_command( (unsigned long)map_devbaseaddr[dev],
				CPS_DEVICE_COMMON_ROM_WRITE_DATA_WRITE );
		}
		else{
			__contec_mcs341_device_rom_write_command((unsigned long)map_devbaseaddr[dev],
				CPS_DEVICE_COMMON_ROM_WRITE_DATA_READ );
 		}
	}
	/* ROM ACCESS DISABLE */
	__contec_mcs341_device_rom_write_command((unsigned long)map_devbaseaddr[dev],
		CPS_DEVICE_COMMON_ROM_WRITE_ACCESS_DISABLE);

	if( isWrite == CPS_DEVICE_COMMON_READ ){
	cps_common_inpb( (unsigned long)(map_devbaseaddr[dev] +
		CPS_DEVICE_COMMON_LOGICALID_ADDR), valb );
	}

	return 0;
}

/*
	@param dev : Target DeviceNumber ( < deviceNumber )
	@note :: Success : Logical ID get Fail : 0
*/  
static unsigned char contec_mcs341_device_logical_id_get( int dev )
{

	unsigned char valb;

	if( dev >= deviceNumber ) return 0;
	
	__contec_mcs341_device_logical_id( dev, CPS_DEVICE_COMMON_READ ,&valb);

	return valb;
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_logical_id_get);

/*
	@param dev : Target DeviceNumber ( < deviceNumber )
	@param valb : id values ( 0 or 1 )
	@note :: Success : 0
*/  
static unsigned char contec_mcs341_device_logical_id_set( int dev, unsigned char valb )
{

	if( dev >= deviceNumber ) return 0;
	
	__contec_mcs341_device_logical_id( dev, CPS_DEVICE_COMMON_WRITE ,&valb);

	return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_logical_id_set);

/*
	@param dev : Target DeviceNumber ( < deviceNumber )
	@param valb : id values ( 0 or 1 )
	@note :: Success : 0
*/  
static unsigned char contec_mcs341_device_logical_id_all_clear( int dev )
{

	if( dev >= deviceNumber ) return 0;
	
	__contec_mcs341_device_logical_id( dev, CPS_DEVICE_COMMON_CLEAR ,NULL );

	return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_logical_id_all_clear);

/*
	@param dev : Target DeviceNumber ( < deviceNumber )
	@param isWrite : Write or Read flag ( 0 or 1 )
	@param valb : values ( logical id )
	@note :: Success : 1 , Fail : 0
*/  
static unsigned char __contec_mcs341_device_extension_value( int dev, int isWrite, unsigned char cate, unsigned char num, unsigned short *valw)
{

	unsigned short valExt , valA;
	if( dev >= deviceNumber ){
		DEBUG_EEPROM_CONTROL(KERN_INFO" device_extension_value : dev %d\n", dev );
		return 0;
	}
	/* Change Extension Register */
	valExt = (cate << 8) ;
	valA = 0;

	/* value Write */
	if( isWrite == CPS_DEVICE_COMMON_WRITE ){
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

	}

	/* ROM ACCESS ENABLE */
	__contec_mcs341_device_rom_write_command( (unsigned long)map_devbaseaddr[dev], 
//		(valExt | CPS_DEVICE_COMMON_ROM_WRITE_ACCESS_ENABLE) );
			CPS_DEVICE_COMMON_ROM_WRITE_ACCESS_ENABLE );
	/* ROM CLEAR ALL (WRITE ONLY) */
	if( isWrite == CPS_DEVICE_COMMON_CLEAR ){
		__contec_mcs341_device_rom_write_command( (unsigned long)map_devbaseaddr[dev],
//			(valExt | CPS_DEVICE_COMMON_ROM_WRITE_DATA_ERASE) );
			CPS_DEVICE_COMMON_ROM_WRITE_DATA_ERASE );
	}
	else{ /* WRITE or READ */
		/* ROM ADDR INITIALIZE */
		__contec_mcs341_device_rom_write_command( (unsigned long)map_devbaseaddr[dev],
//		(valExt | CPS_DEVICE_COMMON_ROM_WRITE_ADDR_INIT) );
			CPS_DEVICE_COMMON_ROM_WRITE_ADDR_INIT );

		/* ROM WRITE or READ FLAG SET */
		if( isWrite == CPS_DEVICE_COMMON_WRITE ){
			__contec_mcs341_device_rom_write_command( (unsigned long)map_devbaseaddr[dev],
//			(valExt | CPS_DEVICE_COMMON_ROM_WRITE_DATA_WRITE) );
				CPS_DEVICE_COMMON_ROM_WRITE_DATA_WRITE );
		}
		else if( isWrite == CPS_DEVICE_COMMON_READ ) {
			__contec_mcs341_device_rom_write_command((unsigned long)map_devbaseaddr[dev],
//			( valExt | CPS_DEVICE_COMMON_ROM_WRITE_DATA_READ ) );
				CPS_DEVICE_COMMON_ROM_WRITE_DATA_READ);
 		}
	}
	/* ROM ACCESS DISABLE */
	__contec_mcs341_device_rom_write_command((unsigned long)map_devbaseaddr[dev],
//		(valExt | CPS_DEVICE_COMMON_ROM_WRITE_ACCESS_DISABLE ));
		CPS_DEVICE_COMMON_ROM_WRITE_ACCESS_DISABLE );

	if( isWrite == CPS_DEVICE_COMMON_READ ){

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

	}

	return 0;
}

/*
	@param dev : Target DeviceNumber ( < deviceNumber )
	@note :: Success : Logical ID get Fail : 0
*/  
static unsigned short contec_mcs341_device_extension_value_get( int dev , int cate ,int num )
{

	unsigned short valw = 0;

	if( dev >= deviceNumber ) return 0;
	
	__contec_mcs341_device_extension_value( dev, CPS_DEVICE_COMMON_READ ,cate, num, &valw);

	return valw;
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_extension_value_get);

/*
	@param dev : Target DeviceNumber ( < deviceNumber )
	@param valb : id values ( 0 or 1 )
	@note :: Success : 0
*/  
static unsigned short contec_mcs341_device_extension_value_set( int dev, int cate, int num, unsigned short valw )
{

	if( dev >= deviceNumber ) return 0;
	
	__contec_mcs341_device_extension_value( dev, CPS_DEVICE_COMMON_WRITE ,cate, num, &valw);

	return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_extension_value_set);


/*
	@param dev : Target DeviceNumber ( < deviceNumber )
	@param valb : id values ( 0 or 1 )
	@note :: Success : 0
*/  
static unsigned short contec_mcs341_device_extension_value_all_clear( int dev, int cate )
{

	if( dev >= deviceNumber ) return 0;
	
	__contec_mcs341_device_extension_value( dev, CPS_DEVICE_COMMON_CLEAR ,cate, 0, NULL );

	return 0;
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_extension_value_all_clear);


/*
	@param BaseAddr : Base Address ( not Virtual Memory Address )
	@note :: Is used cpscom driver. return Target Device Number
*/ 
static unsigned char contec_mcs341_device_deviceNum_get( unsigned long baseAddr )
{
		return (  (baseAddr & 0x00002F00 ) >> 8 );
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_deviceNum_get);

/*
	@param BaseAddr : Base Address ( not Virtual Memory Address )
	@note :: Is used cpscom driver. return Current Serial Channel. 
*/ 
static unsigned char contec_mcs341_device_serial_channel_get( unsigned long baseAddr )
{
		return (  ( ( baseAddr - 0x10 ) & 0x00000038 ) >> 3);
}
EXPORT_SYMBOL_GPL(contec_mcs341_device_serial_channel_get);

/*
	@note :: cps-driver initialize. 
*/ 
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

	contec_mcs341_controller_cpsDevicesInit();

	contec_mcs341_controller_cpsChildUnitInit(CPS_CHILD_UNIT_NONE);

	//2016.02.17 timer add
	if( !reset_button_check_mode ){
		init_timer(&mcs341_timer);
		mcs341_timer.function = mcs341_controller_timer_function;
		mcs341_timer.data = (unsigned long)&mcs341_timer;
		mcs341_timer.expires = jiffies + CPS_CONTROLLER_MCS341_TICK;
		add_timer(&mcs341_timer);
		gpio_free(CPS_CONTROLLER_MCS341_RESET_PIN);
		gpio_request(CPS_CONTROLLER_MCS341_RESET_PIN, "cps_mcs341_reset");
		gpio_direction_input(CPS_CONTROLLER_MCS341_RESET_PIN);

		gpio_free(CPS_CONTROLLER_MCS341_RESET_POUT);
		gpio_request(CPS_CONTROLLER_MCS341_RESET_POUT, "cps_mcs341_reset_out");
		gpio_export(CPS_CONTROLLER_MCS341_RESET_POUT, true ) ; //2016.02.22 
	}

	return ret;
}

/*
	@note :: cps-driver exit. 
*/ 
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
				CPS_COMMON_MEM_NONREGION	 );
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



