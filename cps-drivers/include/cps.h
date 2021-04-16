/*** cps.h ******************************/




/* Device ID Infomation */
typedef struct __cps_device_id{
	unsigned long revision;
	unsigned long category;
	unsigned long product;
	unsigned long physicalId;
	unsigned long logicalId;
}cps_device_id;

#include "cps_mcs341_def.h"

#include <linux/gpio.h>
#define CPS_CONTROLLER_MCS341_RESET_PIN GPIO_TO_PIN( 2, 23 )		// GPIO 87
#define CPS_CONTROLLER_MCS341_RESET_POUT GPIO_TO_PIN( 3, 9 )		//GPIO 105
#define CPS_CONTROLLER_MCS341_TICK	(HZ / 50) // 20 msec

#define CPS_CONTROLLER_MCS341_FPGA_RESET GPIO_TO_PIN( 0, 16 )		//GPIO 16	// GPIO INIT END
/*********************************************************/
