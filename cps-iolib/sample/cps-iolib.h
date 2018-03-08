/*** cps_iolib.h ******************************/
#include <linux/ioctl.h>

struct cpsio_ioctl_arg{
	unsigned int addr;
	unsigned int val;
};

#define CPSIO_MAGIC	'd'

#define IOCTL_CPSIO_INIT	_IORW(CPSIO_MAGIC, 1, struct cpsio_ioctl_arg)
#define IOCTL_CPSIO_EXIT	_IORW(CPSIO_MAGIC, 2, struct cpsio_ioctl_arg)

#define IOCTL_CPSIO_INPWORD		_IOR(CPSIO_MAGIC, 3, struct cpsio_ioctl_arg)
#define IOCTL_CPSIO_OUTWORD		_IOW(CPSIO_MAGIC, 4, struct cpsio_ioctl_arg)
#define IOCTL_CPSIO_INPBYTE		_IOR(CPSIO_MAGIC, 5, struct cpsio_ioctl_arg)
#define IOCTL_CPSIO_OUTBYTE		_IOW(CPSIO_MAGIC, 6, struct cpsio_ioctl_arg)
/*********************************************************/
