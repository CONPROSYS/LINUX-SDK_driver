/*** cpsaio.h ******************************/
#include <linux/ioctl.h>
#include <linux/sched.h>

#define CPSAIO_MAX_BUFFER	8192	//</ max buffer size

/* structure */
typedef struct __cpsaio_inout_data{
	unsigned int Resolution;
	unsigned int Channel;
	unsigned int Range;
}CPSAIO_INOUT_DATA, *PCPSAIO_INOUT_DATA;

typedef struct __cpsaio_device_data{
	char Name[32];
	unsigned int ProductNumber;
	unsigned int Ability;
	CPSAIO_INOUT_DATA ai;
	CPSAIO_INOUT_DATA ao;
/*	
	unsigned int aiResolution;
	unsigned int aiChannel;
	unsigned int aiRange;
	unsigned int aoResolution;
	unsigned int aoChannel;
	unsigned int aoRange;
*/
}CPSAIO_DEV_DATA, *PCPSAIO_DEV_DATA;
	
struct cpsaio_ioctl_arg{
	unsigned char inout;	//</ in or out
	unsigned short ch;	//</ channel
	unsigned long val;	//</ value
};



struct cpsaio_direct_arg{
	unsigned long addr;	//</ address
	unsigned long val;	//</ value
};

struct cpsaio_direct_command_arg{
	unsigned long addr;	//</ address
	unsigned long val;	//</ value
	unsigned char isEcu;	//</ ecu or 
	unsigned int size;	//</ size ( 1 , 2 or 4 )
};


/*  Error Code */

#define AIO_ERR_SYS_MEMORY										20000
#define AIO_ERR_SYS_NOT_SUPPORTED					20001
#define AIO_ERR_SYS_BOARD_EXECUTING				20002
#define AIO_ERR_SYS_USING_OTHER_PROCESS	20003

/* Ability Code */
#define CPS_AIO_ABILITY_ECU	0x0001
#define CPS_AIO_ABILITY_AI	0x0002
#define CPS_AIO_ABILITY_AO	0x0004
#define CPS_AIO_ABILITY_DI	0x0008
#define CPS_AIO_ABILITY_DO	0x0010
#define CPS_AIO_ABILITY_CNT	0x0020
#define CPS_AIO_ABILITY_MEM	0x0040
#define CPS_AIO_ABILITY_OTHER	0x0080
#define CPS_AIO_ABILITY_SSI	0x0100

/* STATUS Code */

/* INOUT */
#define CPS_AIO_INOUT_AI	0x01
#define CPS_AIO_INOUT_AO	0x02

/* CPS-AIO AI STATUS */
#define CPS_AIO_AI_STATUS_ADC_BUSY		0x01
#define CPS_AIO_AI_STATUS_SAMPLING_BEFORE_TRIGGER_BUSY	0x02
#define CPS_AIO_AI_STATUS_START_DISABLE	0x08
#define CPS_AIO_AI_STATUS_AI_ENABLE	0x80
#define CPS_AIO_AI_STATUS_CALIBRATION_BUSY	0x100

#define CPS_AIO_AI_STATUS_MOTION_END	0x8000

/* CPS-AIO AO STATUS */
#define CPS_AIO_AO_STATUS_CALIBRATION_BUSY	0x100

/* Driver's Code */

#define CPSAIO_MAGIC	'f'

#define IOCTL_CPSAIO_INIT	_IOWR(CPSAIO_MAGIC, 1, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_EXIT	_IOWR(CPSAIO_MAGIC, 2, struct cpsaio_ioctl_arg)

#define IOCTL_CPSAIO_INDATA	_IOR(CPSAIO_MAGIC, 3, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_OUTDATA	_IOW(CPSAIO_MAGIC, 4, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_INSTATUS	_IOR(CPSAIO_MAGIC, 5, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_OUTSTATUS	_IOR(CPSAIO_MAGIC, 6, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_SETCHANNEL_AI	_IOW(CPSAIO_MAGIC, 7, struct cpsaio_ioctl_arg)
//#define IOCTL_CPSAIO_SETSAMPNUM_AI	_IOW(CPSAIO_MAGIC, 8, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_SETTRANSFER_MODE_AI	_IOW(CPSAIO_MAGIC, 9, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_START_AI	_IO(CPSAIO_MAGIC, 10)
#define IOCTL_CPSAIO_STOP_AI	_IO(CPSAIO_MAGIC, 11)
#define IOCTL_CPSAIO_SETTRANSFER_MODE_AO	_IOW(CPSAIO_MAGIC, 12, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_START_AO	_IO(CPSAIO_MAGIC, 13)
#define IOCTL_CPSAIO_STOP_AO	_IO(CPSAIO_MAGIC, 14)
#define IOCTL_CPSAIO_SETCHANNEL_AO	_IOW(CPSAIO_MAGIC, 15, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_SETSAMPNUM_AO	_IOW(CPSAIO_MAGIC, 16, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_GETCHANNEL_AI	_IOR(CPSAIO_MAGIC, 17, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_GETCHANNEL_AO	_IOR(CPSAIO_MAGIC, 18, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_GETRESOLUTION	_IOR(CPSAIO_MAGIC, 19, struct cpsaio_ioctl_arg)

#define IOCTL_CPSAIO_SETECU_SIGNAL	_IOW(CPSAIO_MAGIC, 21, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_SET_OUTPULSE0	_IO(CPSAIO_MAGIC, 22)
#define IOCTL_CPSAIO_SET_CLOCK_AI	_IOW(CPSAIO_MAGIC, 23, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_SET_CLOCK_AO	_IOW(CPSAIO_MAGIC, 24, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_SET_SAMPNUM_AI	_IOW(CPSAIO_MAGIC, 25, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_SET_SAMPNUM_AO	_IOW(CPSAIO_MAGIC, 26, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_SET_CALIBRATION_AI	_IOW(CPSAIO_MAGIC, 27, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_SET_CALIBRATION_AO	_IOW(CPSAIO_MAGIC, 28, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_GET_INTERRUPT_FLAG_AI _IOR(CPSAIO_MAGIC, 29, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_GET_SAMPNUM_AI	_IOW(CPSAIO_MAGIC, 30, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_GET_CLOCK_AI	_IOW(CPSAIO_MAGIC, 31, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_GET_INTERRUPT_FLAG_AO _IOR(CPSAIO_MAGIC, 32, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_WRITE_EEPROM_AI _IOW(CPSAIO_MAGIC, 33, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_READ_EEPROM_AI _IOR(CPSAIO_MAGIC, 34, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_WRITE_EEPROM_AO _IOW(CPSAIO_MAGIC, 35, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_READ_EEPROM_AO _IOR(CPSAIO_MAGIC, 36, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_GET_CALIBRATION_AI	_IOW(CPSAIO_MAGIC, 37, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_GET_CALIBRATION_AO	_IOW(CPSAIO_MAGIC, 38, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_CLEAR_EEPROM _IO(CPSAIO_MAGIC, 39)
#define IOCTL_CPSAIO_GETMAXCHANNEL _IOR(CPSAIO_MAGIC, 40, struct cpsaio_ioctl_arg)


#define IOCTL_CPSAIO_DIRECT_OUTPUT	_IOW(CPSAIO_MAGIC, 64, struct cpsaio_direct_arg)
#define IOCTL_CPSAIO_DIRECT_INPUT _IOR(CPSAIO_MAGIC, 65, struct cpsaio_direct_arg)
#define IOCTL_CPSAIO_DIRECT_COMMAND_OUTPUT	_IOW(CPSAIO_MAGIC, 64, struct cpsaio_direct_command_arg)
#define IOCTL_CPSAIO_DIRECT_COMMAND_INPUT _IOR(CPSAIO_MAGIC, 65, struct cpsaio_direct_command_arg)
#define IOCTL_CPSAIO_DIRECT_COMMAND_CALL	_IO(CPSAIO_MAGIC, 66)

/**************************************************/
