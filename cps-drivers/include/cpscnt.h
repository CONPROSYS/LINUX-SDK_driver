/*** cpscnt.h ******************************/
#include <linux/ioctl.h>
#include <linux/sched.h>
#include "cps_def.h"

/* structure */
/**
	@struct __cpscnt_device_data
	@~English
	@brief structure device data
	@~Japanese
	@brief デバイスデータ構造体
**/
typedef struct __cpscnt_device_data{
	char Name[32];///< Device Name
	unsigned int ProductNumber; ///< Product Number
	unsigned int channelNum;///< Channel number
	void* ChannelData;///< チャネルデータ
}CPSCNT_DEV_DATA, *PCPSCNT_DEV_DATA;

/**
	@struct cpscnt_ioctl_arg
	@~English
	@brief I/O Control structure
	@~Japanese
	@brief I/O コントロール 構造体
**/
struct cpscnt_ioctl_arg{
	unsigned long val;///< value
	unsigned int ch;///< Channel
};

/**
	@struct cpscnt_ioctl_string_arg
	@~English
	@brief I/O Control structure ( String )
	@~Japanese
	@brief I/O コントロール 構造体(文字列用)
**/
struct cpscnt_ioctl_string_arg{
	unsigned long index;///< index
	unsigned char str[32];	///< string
};

/*  Error Code */

#define CNT_ERR_SYS_MEMORY										20000
#define CNT_ERR_SYS_NOT_SUPPORTED					20001
#define CNT_ERR_SYS_BOARD_EXECUTING				20002
#define CNT_ERR_SYS_USING_OTHER_PROCESS	20003

#define CNT_ERR_SYS_PORT_NO									20100
#define CNT_ERR_SYS_PORT_NUM									20101
#define CNT_ERR_SYS_BIT_NO										20102
#define CNT_ERR_SYS_BIT_NUM									20103
#define CNT_ERR_SYS_BIT_DATA									20104

#define CNT_ERR_SYS_INT_BIT									20200
#define CNT_ERR_SYS_INT_LOGIC								20201

/* Driver's Code */

#define CPSCNT_MAGIC	'h'

#define IOCTL_CPSCNT_INIT					_IORW(CPSCNT_MAGIC, 1, struct cpscnt_ioctl_arg)
#define IOCTL_CPSCNT_EXIT					_IORW(CPSCNT_MAGIC, 2, struct cpscnt_ioctl_arg)

#define IOCTL_CPSCNT_READ_COUNT				_IOR(CPSCNT_MAGIC, 3, struct cpscnt_ioctl_arg)
#define IOCTL_CPSCNT_INIT_COUNT				_IOW(CPSCNT_MAGIC, 4, struct cpscnt_ioctl_arg)

#define IOCTL_CPSCNT_GET_STATUS				_IOR(CPSCNT_MAGIC, 5, struct cpscnt_ioctl_arg)
#define IOCTL_CPSCNT_SET_MODE				_IOW(CPSCNT_MAGIC, 6, struct cpscnt_ioctl_arg)

#define IOCTL_CPSCNT_SET_Z_PHASE			_IOW(CPSCNT_MAGIC, 7, struct cpscnt_ioctl_arg)
#define IOCTL_CPSCNT_SET_COMPARE_REG			_IOW(CPSCNT_MAGIC, 8, struct cpscnt_ioctl_arg)

#define IOCTL_CPSCNT_SET_FILTER				_IOW(CPSCNT_MAGIC, 9, struct cpscnt_ioctl_arg)
#define IOCTL_CPSCNT_SET_COUNT_LATCH			_IOW(CPSCNT_MAGIC, 10, struct cpscnt_ioctl_arg)

#define IOCTL_CPSCNT_SET_INTERRUPT_MASK		_IOW(CPSCNT_MAGIC, 11, struct cpscnt_ioctl_arg)
#define IOCTL_CPSCNT_GET_INTERRUPT_MASK		_IOR(CPSCNT_MAGIC, 12, struct cpscnt_ioctl_arg)

#define IOCTL_CPSCNT_RESET_SENCE			_IOW(CPSCNT_MAGIC, 13, struct cpscnt_ioctl_arg)
#define IOCTL_CPSCNT_GET_SENCE				_IOR(CPSCNT_MAGIC, 14, struct cpscnt_ioctl_arg)

#define IOCTL_CPSCNT_SET_TIMER_DATA			_IOW(CPSCNT_MAGIC, 15, struct cpscnt_ioctl_arg)

#define IOCTL_CPSCNT_SET_TIMER_START			_IOW(CPSCNT_MAGIC, 16, struct cpscnt_ioctl_arg)
#define IOCTL_CPSCNT_SET_ONESHOT_PULSE_WIDTH	_IOW(CPSCNT_MAGIC, 17, struct cpscnt_ioctl_arg)

#define IOCTL_CPSCNT_SET_SELECT_COMMON_INPUT	_IOW(CPSCNT_MAGIC, 18, struct cpscnt_ioctl_arg)
#define IOCTL_CPSCNT_GET_SELECT_COMMON_INPUT	_IOR(CPSCNT_MAGIC, 19, struct cpscnt_ioctl_arg)

#define IOCTL_CPSCNT_GET_DEVICE_NAME			_IOR(CPSCNT_MAGIC, 20, struct cpscnt_ioctl_string_arg)

#define IOCTL_CPSCNT_START_COUNT			_IOW(CPSCNT_MAGIC, 21, struct cpscnt_ioctl_arg)
#define IOCTL_CPSCNT_STOP_COUNT			_IOW(CPSCNT_MAGIC, 22, struct cpscnt_ioctl_arg)

#define IOCTL_CPSCNT_SET_Z_LOGIC			_IOW(CPSCNT_MAGIC, 23, struct cpscnt_ioctl_arg)
#define IOCTL_CPSCNT_SET_DIRECTION			_IOW(CPSCNT_MAGIC, 24, struct cpscnt_ioctl_arg)
#define IOCTL_CPSCNT_GET_MAX_CHANNELS	_IOR(CPSCNT_MAGIC, 25, struct cpscnt_ioctl_arg)

#define IOCTL_CPSCNT_GET_Z_PHASE			_IOR(CPSCNT_MAGIC, 26, struct cpscnt_ioctl_arg)
#define IOCTL_CPSCNT_GET_COMPARE_REG			_IOR(CPSCNT_MAGIC, 27, struct cpscnt_ioctl_arg)

#define IOCTL_CPSCNT_GET_FILTER				_IOR(CPSCNT_MAGIC, 28, struct cpscnt_ioctl_arg)
#define IOCTL_CPSCNT_GET_COUNT_LATCH			_IOR(CPSCNT_MAGIC, 29, struct cpscnt_ioctl_arg)

#define IOCTL_CPSCNT_SET_TIMER_STOP			_IOW(CPSCNT_MAGIC, 30, struct cpscnt_ioctl_arg)
#define IOCTL_CPSCNT_GET_ONESHOT_PULSE_WIDTH	_IOR(CPSCNT_MAGIC, 31, struct cpscnt_ioctl_arg)

#define IOCTL_CPSCNT_GET_MODE				_IOW(CPSCNT_MAGIC, 32, struct cpscnt_ioctl_arg)

#define IOCTL_CPSCNT_GET_Z_LOGIC			_IOW(CPSCNT_MAGIC, 33, struct cpscnt_ioctl_arg)

#define IOCTL_CPSCNT_GET_DRIVER_VERSION	_IOR(CPSCNT_MAGIC, 34, struct cpscnt_ioctl_string_arg)

/**************************************************/
