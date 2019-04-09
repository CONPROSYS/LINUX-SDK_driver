/*** cps_iolib.h ******************************/
#include <linux/ioctl.h>

/**
	@struct cpsio_ioctl argument
	@brief I/O Control Argument
**/
struct cpsio_ioctl_arg{
	unsigned int addr;
	unsigned int val;
};

/**
	@struct cpsio_ioctl_string_arg
	@~English
	@brief I/O Control structure ( String )
	@~Japanese
	@brief I/O コントロール 構造体(文字列用)
**/
struct cpsio_ioctl_string_arg{
	unsigned long index;///< index
	unsigned char str[32];	///< string
};

#define CPSIO_MAGIC	'd'

/// @name I/O CONTROL CODE
/// @{

#define IOCTL_CPSIO_INIT	_IORW(CPSIO_MAGIC, 1, struct cpsio_ioctl_arg)	///< INIT
#define IOCTL_CPSIO_EXIT	_IORW(CPSIO_MAGIC, 2, struct cpsio_ioctl_arg)	///< EXIT

#define IOCTL_CPSIO_INPWORD		_IOR(CPSIO_MAGIC, 3, struct cpsio_ioctl_arg)	///< INPUT WORD (16bit Read Access)
#define IOCTL_CPSIO_OUTWORD		_IOW(CPSIO_MAGIC, 4, struct cpsio_ioctl_arg)	///< OUTPUT WORD (16bit Write Access)
#define IOCTL_CPSIO_INPBYTE		_IOR(CPSIO_MAGIC, 5, struct cpsio_ioctl_arg)	///< INPUT BYTE (8bit Read Access)
#define IOCTL_CPSIO_OUTBYTE		_IOW(CPSIO_MAGIC, 6, struct cpsio_ioctl_arg)	///< OUTPUT BYTE (8bit Write Access)
//#define IOCTL_CPSIO_INPDWORD		_IOR(CPSIO_MAGIC, 7, struct cpsio_ioctl_arg)	///< INPUT DWORD (32bit Read Access)
//#define IOCTL_CPSIO_OUTDWORD		_IOW(CPSIO_MAGIC, 8, struct cpsio_ioctl_arg)	///< OUTPUT DWORD (32bit Write Access)
#define IOCTL_CPSIO_GET_DRIVER_VERSION	_IOR(CPSIO_MAGIC, 9, struct cpsio_ioctl_string_arg)
/// @}
/*********************************************************/
