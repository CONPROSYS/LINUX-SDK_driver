/*** cps.h ******************************/




/* Device ID Infomation */
typedef struct __cps_device_id{
	unsigned long revision;
	unsigned long category;
	unsigned long product;
	unsigned long physicalId;
	unsigned long logicalId;
}cps_device_id;

/**** Child Unit Number Macro ****/
#define CPS_CHILD_UNIT_NONE	0x00	///< None
#define CPS_CHILD_UNIT_INF_MC341B_00	0x01	///< 3G UNIT (SL8084T)
#define CPS_CHILD_UNIT_INF_MC341B_10	0x02	///< RS-422A/485 UNIT
#define CPS_CHILD_UNIT_INF_MC341B_20	0x03 ///< 920MHz UNIT ( CMM-920GP2 )
#define CPS_CHILD_UNIT_INF_MC341B_40 0x04 ///< 3G UNIT (HL8548)
#define CPS_CHILD_UNIT_INF_MC341B_50	0x05 ///< LoRa 920MHz UNIT ( EASEL ES920LR )
#define CPS_CHILD_UNIT_JIG_MC341B_00	0x100 ///< KENSA UNIT

/* Controller Address */
#define CPS_CONTROLLER_MCS341_PRODUCTVERSION_ADDR	(0x00)
#define CPS_CONTROLLER_MCS341_FPGAVERSION_ADDR	(0x01)
#define CPS_CONTROLLER_MCS341_ROTARYSW_RADDR	(0x02)
#define CPS_CONTROLLER_MCS341_SETPINMODE_WADDR	(0x02)
#define CPS_CONTROLLER_MCS341_SYSTEMINIT_WADDR (0x03)
#define CPS_CONTROLLER_MCS341_SYSTEMSTATUS_RADDR (0x03)
#define CPS_CONTROLLER_MCS341_DEVICENUM_RADDR	(0x04)
#define CPS_CONTROLLER_MCS341_RESET_WADDR	(0x04)
#define CPS_CONTROLLER_MCS341_WDTIMER_ADDR	(0x05)
#define CPS_CONTROLLER_MCS341_LED_ADDR	(0x06)
#define CPS_CONTROLLER_MCS341_INTERRUPT_ADDR(x)	(0x08 + x)
#define CPS_CONTROLLER_MCS341_DIO_DIRECTION_ADDR	(0x60)
#define CPS_CONTROLLER_MCS341_DIO_VALUE_ADDR	(0x61)
#define CPS_CONTROLLER_MCS341_SER_CTRL_ADDR	(0x64)

/* Controller PRODUCTVERSION Values MACRO */
#define CPS_MCS341_PRODUCT_VERSION(val)	( val & 0x0F )			// Hardware Version
#define CPS_MCS341_PRODUCT_TYPE(val)	( (val & 0xF0) >> 4 )	// Controller Type
/* Controller ROTARYSW Values MACRO */
#define CPS_MCS341_ROTARYSW_UNITID(val)	( val & 0x0F )
#define CPS_MCS341_ROTARYSW_GROUPID(val)	( (val & 0xF0) >> 4 )
/* Controller SETPINMODE Values MACRO */
#define CPS_MCS341_SETPINMODE_3G3(val)	(val & 0x03)
#define CPS_MCS341_SETPINMODE_3G4(val)	( (val & 0x03) << 2 )
#define CPS_MCS341_SETPINMODE_CTSSUB(val)	( (val & 0x03) << 4 )
#define CPS_MCS341_SETPINMODE_RTSSUB(val)	( (val & 0x03) << 6 )

#define CPS_MCS341_SETPINMODE_3G3_INPUT	( 0x00 )
#define CPS_MCS341_SETPINMODE_3G3_DCAN1_RX	( 0x01 )
#define CPS_MCS341_SETPINMODE_3G3_OUTPUT	( 0x03 )

#define CPS_MCS341_SETPINMODE_3G4_INPUT	( 0x00 )
#define CPS_MCS341_SETPINMODE_3G4_DCAN1_TX	( 0x01 )
#define CPS_MCS341_SETPINMODE_3G4_OUTPUT	( 0x03 )

#define CPS_MCS341_SETPINMODE_CTSSUB_INPUT	( 0x00 )
#define CPS_MCS341_SETPINMODE_CTSSUB_CTS	( 0x01 )
#define CPS_MCS341_SETPINMODE_CTSSUB_DCAN0_RX	( 0x02 )
#define CPS_MCS341_SETPINMODE_CTSSUB_OUTPUT	( 0x03 )

#define CPS_MCS341_SETPINMODE_RTSSUB_INPUT	( 0x00 )
#define CPS_MCS341_SETPINMODE_RTSSUB_RTS	( 0x01 )
#define CPS_MCS341_SETPINMODE_RTSSUB_DCAN0_TX	( 0x02 )
#define CPS_MCS341_SETPINMODE_RTSSUB_OUTPUT	( 0x03 )

/* Controller SYSTEMSTATUS Values MACRO */

#define CPS_MCS341_SYSTEMSTATUS_RESETBUSY ( 0x01 )
#define CPS_MCS341_SYSTEMSTATUS_INITBUSY ( 0x02 )
#define CPS_MCS341_SYSTEMSTATUS_INTERRUPT_END ( 0x04 )
#define CPS_MCS341_SYSTEMSTATUS_INIT_END ( 0x08 )

#define CPS_MCS341_SYSTEMSTATUS_BUSY(val) ( val & (CPS_MCS341_SYSTEMSTATUS_RESETBUSY | CPS_MCS341_SYSTEMSTATUS_INITBUSY) )
#define CPS_MCS341_SYSTEMSTATUS_INITEND(val) ( val & (CPS_MCS341_SYSTEMSTATUS_INTERRUPT_END|CPS_MCS341_SYSTEMSTATUS_INIT_END) )
#define CPS_MCS341_SYSTEMSTATUS_DIPSWITCH_ALL( val )	( ( val & 0xF0 ) >> 4 )
#define CPS_MCS341_SYSTEMSTATUS_DIPSWITCH( n , val ) ( CPS_MCS341_SYSTEMSTATUS_DIPSWITCH_ALL( val ) & (1 << n) )


/* Controller SYSTEMINIT Values MACRO */

#define CPS_MCS341_SYSTEMINIT_SETRESET	(0x01)
#define CPS_MCS341_SYSTEMINIT_SETINTERRUPT	(0x02)
#define CPS_MCS341_SYSTEMINIT_SETEXTEND_POWER	(0x04)
#define CPS_MCS341_SYSTEMINIT_SETEXTEND_RESET	(0x08)
#define CPS_MCS341_SYSTEMINIT_3G3_SETOUTPUT	(0x10)
#define CPS_MCS341_SYSTEMINIT_3G4_SETOUTPUT	(0x20)
#define CPS_MCS341_SYSTEMINIT_CTSSUB_SETOUTPUT	(0x40)
#define CPS_MCS341_SYSTEMINIT_RTSSUB_SETOUTPUT	(0x80)

/* Controller DEVICENUM Values MACRO */
#define CPS_MCS341_DEVICENUM_VALUE(val) (val & 0x3F)
#define CPS_MCS341_DEVICENUM_ERROR(val) ( (val & 0x40) >> 6 )

/* Controller RESET Values MACRO */
#define CPS_MCS341_RESET_DEVICEPOWER		(0x80)
#define CPS_MCS341_RESET_DEVICEALL		(0x40)
#define CPS_MCS341_RESET_SET_LVDS_PWR	(0x20)	///< Ver.1.0.9 (1) added
#define CPS_MCS341_RESET_SET_IDSEL_COMPLETE	(0x01)

/* Controller WatchDog Timer Values MACRO */
#define CPS_MCS341_WDTIMER_ENABLE	( 0x01 )
#define CPS_MCS341_WDTIMER_DELAY(val)	( val << 1 )

/* Controller LED Values MACRO */
#define CPS_MCS341_LED_PWR(val)	( ~(val) & 0x01 )
#define CPS_MCS341_LED_ST1(val)	( val & 0x02 )
#define CPS_MCS341_LED_ST2(val)	( val & 0x04 )
#define CPS_MCS341_LED_ERR(val)	( val & 0x08 )

#define CPS_MCS341_LED_PWR_BIT	( 0x01 )
#define CPS_MCS341_LED_ST1_BIT	( 0x02 )
#define CPS_MCS341_LED_ST2_BIT	( 0x04 )
#define CPS_MCS341_LED_ERR_BIT	( 0x08 )


/* Controller INTERRUPT Values MACRO */
#define CPS_MCS341_INTERRUPT_GROUP_SET(n,val)	( val << n )
#define CPS_MCS341_INTERRUPT_GROUP_GET(n,val)	( ( (val & ( 1 << n ) ) >> n ) )
#define CPS_MCS341_EXTRA_INTERRUPT_OVERCURRENT	( 0x01 )

/* Controller DIO Values MACRO */
#define CPS_MCS341_DIO_DIRECTION_SET(n,val) (val << n)
#define CPS_MCS341_DIO_DIRECTION_GET(n,val) ( val & (1 << n) )
#define CPS_MCS341_DIO_FILTER_GET(val) ( (val & 0xF0) >> 4 )
#define CPS_MCS341_DIO_FILTER_SET(val) ( (val & 0x0F) << 4 )
#define CPS_MCS341_DIO_DIVALUE_GET(val) ( val & 0x0F )
#define CPS_MCS341_DIO_DOECHOVALUE_GET(val)	( (val & 0xF0) >> 4)
#define CPS_MCS341_DIO_DOVALUE_SET(val) ( ( val & 0x0F ) << 4 )

#define CPS_MCS341_DIO_PORT	1	///< Port
#define CPS_MCS341_DIO_BIT	2 ///< Bit

/* Device Address (Common) */
#define CPS_DEVICE_COMMON_REVISION_ADDR	 (0x00)
#define CPS_DEVICE_COMMON_CATEGORY_ADDR		(0x01)
#define CPS_DEVICE_COMMON_PRODUCTID_ADDR		(0x02)
#define CPS_DEVICE_COMMON_PHYSICALID_ADDR	(0x04)
#define CPS_DEVICE_COMMON_LOGICALID_ADDR		(0x06)
#define CPS_DEVICE_COMMON_ROM_WRITE_ADDR		(0x0C)

#define CPS_DEVICE_COMMON_MIRROR_REG_ADDR(x)	(0x0E + x)

<<<<<<< HEAD
#define CPS_DEVICE_COMMON_CATEGORY_GET(val) ( ( val & 0xF0 ) >> 4 )
#define CPS_DEVICE_COMMON_REVISION_GET(val) ( val & 0x0F )
=======
#define CPS_DEVICE_COMMON_REVISION_BOARDVER_GET(val) ( ( val & 0xF0 ) >> 4 )
#define CPS_DEVICE_COMMON_REVISION_FPGAVER_GET(val) ( val & 0x0F )
>>>>>>> cps-drivers/master

#define CPS_DEVICE_COMMON_ROM_WRITE_CMD_FINISHED	(0x0000)
#define CPS_DEVICE_COMMON_ROM_WRITE_CMD_ENABLE	(0x0001)
#define CPS_DEVICE_COMMON_ROM_WRITE_DATA_READ	(0x0004)
#define CPS_DEVICE_COMMON_ROM_WRITE_DATA_ERASE	(0x0008)
#define CPS_DEVICE_COMMON_ROM_WRITE_ACCESS_DISABLE	(0x0010)
#define CPS_DEVICE_COMMON_ROM_WRITE_DATA_WRITE	(0x0020)
#define CPS_DEVICE_COMMON_ROM_WRITE_ADDR_INIT	(0x0040)
#define CPS_DEVICE_COMMON_ROM_WRITE_ACCESS_ENABLE	(0x0080)

/* Device Address (Extension) */
#define CPS_DEVICE_EXTENSION_VALUE(n) (0x02 * (n+1) )

#define CPS_DEVICE_COMMON_ROM_WRITE_PAGE_COMMON 0x00
#define CPS_DEVICE_COMMON_ROM_WRITE_PAGE_AI 0x01
#define CPS_DEVICE_COMMON_ROM_WRITE_PAGE_AO 0x02
#define CPS_DEVICE_COMMON_ROM_WRITE_PAGE_SSI 0x08

#define CPS_DEVICE_COMMON_CLEAR 0x04
#define CPS_DEVICE_COMMON_WRITE	0x02
#define CPS_DEVICE_COMMON_READ	0x01

#define CPS_DEVICE_INIT_TIMEOUT 1000

#define CPS_DEVICE_COMMON_MEMORY_RELEASE	0
#define CPS_DEVICE_COMMON_MEMORY_ALLOCATE	1


#include "cps_def.h"

#include <linux/gpio.h>
#define CPS_CONTROLLER_MCS341_RESET_PIN GPIO_TO_PIN( 2, 23 )		// GPIO 87
#define CPS_CONTROLLER_MCS341_RESET_POUT GPIO_TO_PIN( 3, 9 )		//GPIO 105
#define CPS_CONTROLLER_MCS341_TICK	(HZ / 50) // 20 msec

#define CPS_CONTROLLER_MCS341_FPGA_POWER_PIN	GPIO_TO_PIN(0, 16)	// GPIO INIT END 

/*********************************************************/
