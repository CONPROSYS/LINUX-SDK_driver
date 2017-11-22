/*** cpsdio.h ******************************/
#include <linux/ioctl.h>
#include <linux/sched.h>
#include "cps_def.h"

/* structure */
typedef struct __cpsdio_device_data{
	char Name[32];
	unsigned int ProductNumber;
	unsigned int inPortNum;
	unsigned int outPortNum;
}CPSDIO_DEV_DATA, *PCPSDIO_DEV_DATA;

	
struct cpsdio_ioctl_arg{
	unsigned int port;
	unsigned int val;
	unsigned char str[32];
};


/*  Error Code */

#define DIO_ERR_SYS_MEMORY										20000
#define DIO_ERR_SYS_NOT_SUPPORTED					20001
#define DIO_ERR_SYS_BOARD_EXECUTING				20002
#define DIO_ERR_SYS_USING_OTHER_PROCESS	20003

#define DIO_ERR_SYS_PORT_NO									20100
#define DIO_ERR_SYS_PORT_NUM									20101
#define DIO_ERR_SYS_BIT_NO										20102
#define DIO_ERR_SYS_BIT_NUM									20103
#define DIO_ERR_SYS_BIT_DATA									20104

#define DIO_ERR_SYS_INT_BIT									20200
#define DIO_ERR_SYS_INT_LOGIC								20201

/* Driver's Code */

#define CPSDIO_MAGIC	'e'

#define IOCTL_CPSDIO_INIT	_IORW(CPSDIO_MAGIC, 1, struct cpsdio_ioctl_arg)
#define IOCTL_CPSDIO_EXIT	_IORW(CPSDIO_MAGIC, 2, struct cpsdio_ioctl_arg)

#define IOCTL_CPSDIO_INP_PORT	_IOR(CPSDIO_MAGIC, 3, struct cpsdio_ioctl_arg)
#define IOCTL_CPSDIO_OUT_PORT	_IOW(CPSDIO_MAGIC, 4, struct cpsdio_ioctl_arg)
#define IOCTL_CPSDIO_OUT_PORT_ECHO	_IOR(CPSDIO_MAGIC, 5, struct cpsdio_ioctl_arg)
#define IOCTL_CPSDIO_SET_FILTER	_IOW(CPSDIO_MAGIC, 6, struct cpsdio_ioctl_arg)
#define IOCTL_CPSDIO_GET_FILTER	_IOR(CPSDIO_MAGIC, 7, struct cpsdio_ioctl_arg)

#define IOCTL_CPSDIO_SET_INTERNAL_BAT	_IOW(CPSDIO_MAGIC, 8, struct cpsdio_ioctl_arg)
#define IOCTL_CPSDIO_SET_INT_MASK	_IOW(CPSDIO_MAGIC, 9, struct cpsdio_ioctl_arg)
#define IOCTL_CPSDIO_GET_INT_STATUS	_IOR(CPSDIO_MAGIC, 10, struct cpsdio_ioctl_arg)
#define IOCTL_CPSDIO_SET_INT_EGDE	_IOW(CPSDIO_MAGIC, 11, struct cpsdio_ioctl_arg)
#define IOCTL_CPSDIO_GET_INT_EGDE	_IOR(CPSDIO_MAGIC, 12, struct cpsdio_ioctl_arg)
#define IOCTL_CPSDIO_GET_INP_PORTNUM	_IOR(CPSDIO_MAGIC, 13, struct cpsdio_ioctl_arg)
#define IOCTL_CPSDIO_GET_OUTP_PORTNUM	_IOR(CPSDIO_MAGIC, 14, struct cpsdio_ioctl_arg)
#define IOCTL_CPSDIO_GET_DEVICE_NAME	_IOR(CPSDIO_MAGIC, 15, struct cpsdio_ioctl_arg)
#define IOCTL_CPSDIO_SET_CALLBACK_PROCESS _IOW(CPSDIO_MAGIC, 16, struct cpsdio_ioctl_arg)
/**************************************************/
