/*** cpsaio.h ******************************/
#include <linux/ioctl.h>
#include <linux/sched.h>

#define CPSAIO_MAX_BUFFER	8192	///< max buffer size

/* structure */
/**
	@struct __cpsaio_buffer
	@~English
	@brief structure Data Buffer
	@~Japanese
	@brief データバッファ構造体
**/
typedef struct __cpsaio_buffer{
	unsigned char *data;
	unsigned long size;
}CPSAIO_BUFFER_DATA, *PCPSAIO_BUFFER_DATA;

/**
	@struct __cpsaio_inout_data
	@~English
	@brief structure union paramter
	@~Japanese
	@brief 共有パラメータ構造体
**/
typedef struct __cpsaio_inout_data{
	unsigned int Resolution;
	unsigned int Channel;	//Max Channel 
	unsigned int Range;
}CPSAIO_INOUT_DATA, *PCPSAIO_INOUT_DATA;
/**
	@struct __cpsaio_user_data
	@~English
	@brief structure union paramter
	@~Japanese
	@brief 共有パラメータ構造体
**/
typedef struct __cpsaio_user_data{
	unsigned long Clock;
	unsigned long SamplingTimes;
	unsigned long RepeatTimes; // Ver 1.1.0
	unsigned short Channel;
	short StopTriggerType;
	unsigned short MemoryType;
}CPSAIO_USER_DATA, *PCPSAIO_USER_DATA;

/**
	@struct __cpsaio_current_data
	@~English
	@brief structure union paramter
	@~Japanese
	@brief 共有パラメータ構造体
**/
typedef struct __cpsaio_current_data{
	CPSAIO_INOUT_DATA fixed;
	CPSAIO_USER_DATA user;
}CPSAIO_CURRENT_DATA, *PCPSAIO_CURRENT_DATA;



/**
	@struct __cpsaio_device_data
	@~English
	@brief structure device data
	@~Japanese
	@brief デバイスデータ構造体
**/
typedef struct __cpsaio_device_data{
	char Name[32];	///< デバイス名
	unsigned int ProductNumber;///< 製品番号
	unsigned int Ability;///< 能力 ( AI, AO, ECU )
	CPSAIO_CURRENT_DATA ai;///< AI Data
	CPSAIO_CURRENT_DATA ao;///< AO Data

/*	
	unsigned int aiResolution;
	unsigned int aiChannel;
	unsigned int aiRange;
	unsigned int aoResolution;
	unsigned int aoChannel;
	unsigned int aoRange;
*/
}CPSAIO_DEV_DATA, *PCPSAIO_DEV_DATA;
	
/**
	@struct cpsaio_ioctl_arg
	@~English
	@brief I/O Control structure
	@~Japanese
	@brief I/O コントロール 構造体
**/
struct cpsaio_ioctl_arg{
	unsigned char inout;	///< analog input or output
	unsigned char isDerection; ///< Function Set or Get < Reserved >
	unsigned short ch;	///< channel
	unsigned long val;	///< value
};

/**
	@struct cpsaio_ioctl_string_arg
	@~English
	@brief I/O Control structure ( String )
	@~Japanese
	@brief I/O コントロール 構造体(文字列用)
**/
struct cpsaio_ioctl_string_arg{
	unsigned long index;	///< index
	unsigned char str[32];	///< string
};



/**
	@struct __cpsaio_device_data
	@~English
	@brief structure direct I/O
	@~Japanese
	@brief 直接I/O　構造体
**/
struct cpsaio_direct_arg{
	unsigned long addr;	///< address
	unsigned long val;	///< value
};

/**
	@struct __cpsaio_device_data
	@~English
	@brief structure direct command I/O
	@~Japanese
	@brief 直接I/Oコマンド　構造体
**/
struct cpsaio_direct_command_arg{
	unsigned long addr;	///< address
	unsigned long val;	///< value
	unsigned char isEcu;	///< ecu or command 
	unsigned int size;	///< size ( 1 , 2 or 4 )
};


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
/* DERECTION (Function) */
#define CPS_AIO_DERECTION_SET	0x01
#define CPS_AIO_DERECTION_GET	0x02

/* */

/* Event Controller dest signal */
#define CPS_AIO_ECU_DEST_AI_CLK	4
#define CPS_AIO_ECU_DEST_AI_START	0
#define CPS_AIO_ECU_DEST_AI_STOP	2
#define CPS_AIO_ECU_DEST_AO_CLK	36
#define CPS_AIO_ECU_DEST_AO_START	32
#define CPS_AIO_ECU_DEST_AO_STOP	34

/* Event Controller src signal */
#define CPS_AIO_ECU_SRC_NON_CONNECT	0
#define CPS_AIO_ECU_SRC_AI_CLK	4
#define CPS_AIO_ECU_SRC_AI_STOP	17
#define CPS_AIO_ECU_SRC_AO_CLK	66
#define CPS_AIO_ECU_SRC_AO_STOP_RING	80
#define CPS_AIO_ECU_SRC_AO_STOP_FIFO	352
#define CPS_AIO_ECU_SRC_START	384

#define CPS_AIO_ECU_DESTSRC_SIGNAL(dest, src)	( (src << 16) | dest )

/* CPS-AIO AI STATUS */
#define CPS_AIO_AI_STATUS_ADC_BUSY						0x0001
#define CPS_AIO_AI_STATUS_SAMPLING_BEFORE_TRIGGER_BUSY	0x0002
#define CPS_AIO_AI_STATUS_START_DISABLE					0x0010 ///< bugfix 1.0.3
#define CPS_AIO_AI_STATUS_AI_ENABLE						0x0080
#define CPS_AIO_AI_STATUS_CALIBRATION_BUSY				0x0100

//#define CPS_AIO_AI_STATUS_MOTION_END					0x00008000

/*!
 @~English
 @name Status macro
 @~Japanese
 @name ステータスマクロ
*/
/// @{
#define CPS_AIO_AIS_BUSY		0x0001
#define CPS_AIO_AIS_START_TRG	0x0002
#define CPS_AIO_AIS_DATA_NUM	0x0010
#define CPS_AIO_AIS_OFERR		0x00010000
#define CPS_AIO_AIS_SCERR		0x00020000
/// @}

/*!
 @~English
 @name Analog Input Flag macro
 @~Japanese
 @name アナログ入力フラグマクロ
*/
/// @{
#define CPS_AIO_AI_FLAG_SCAN_END	0x0001
#define CPS_AIO_AI_FLAG_DATANUM_END	0x0002
#define	CPS_AIO_AI_FLAG_CLOCKERROR	0x0100
#define	CPS_AIO_AI_FLAG_MOTION_END	0x8000
/// @}

/*!
 @~English
 @name Analog Output Flag macro
 @~Japanese
 @name アナログ出力フラグマクロ
*/
/// @{
#define CPS_AIO_AO_FLAG_DATANUM_END	0x0002
#define	CPS_AIO_AO_FLAG_CLOCKERROR	0x0100
#define	CPS_AIO_AO_FLAG_MOTION_END	0x8000
/// @}

/*!
 @~English
 @name Memory Flag macro
 @~Japanese
 @name メモリフラグマクロ
*/
/// @{
#define	CPS_AIO_MEM_FLAG_OVERFLOW	0x0002
/// @}

/* CPS-AIO AO STATUS */
#define CPS_AIO_AO_STATUS_CALIBRATION_BUSY	0x100

/*!
 @~English
 @name MEM STATUS (define)
 @~Japanese
 @name メモリステータス
*/
/// @{
#define CPU_AIO_MEMSTATUS_DRE	0x0001	///< Data Read Enable bit
#define CPU_AIO_MEMSTATUS_MDRE	0x0002	///< Multi Data Read Enable bit
#define CPU_AIO_MEMSTATUS_DWE	0x0100 ///< Data Write Enable bit
#define CPU_AIO_MEMSTATUS_MDWE	0x0200	///< Multi Data Write Enable bit
/// @}

/* Driver's Code */

#define CPSAIO_MAGIC	'f'

#define IOCTL_CPSAIO_INIT	_IOWR(CPSAIO_MAGIC, 1, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_EXIT	_IOWR(CPSAIO_MAGIC, 2, struct cpsaio_ioctl_arg)

#define IOCTL_CPSAIO_INDATA	_IOR(CPSAIO_MAGIC, 3, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_OUTDATA	_IOW(CPSAIO_MAGIC, 4, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_INSTATUS	_IOR(CPSAIO_MAGIC, 5, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_OUTSTATUS	_IOR(CPSAIO_MAGIC, 6, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_SETCHANNEL_AI	_IOW(CPSAIO_MAGIC, 7, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_GET_DEVICE_NAME	_IOR(CPSAIO_MAGIC, 8, struct cpsaio_ioctl_string_arg)
#define IOCTL_CPSAIO_SETTRANSFER_MODE_AI	_IOW(CPSAIO_MAGIC, 9, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_START_AI	_IO(CPSAIO_MAGIC, 10)
#define IOCTL_CPSAIO_STOP_AI	_IO(CPSAIO_MAGIC, 11)
#define IOCTL_CPSAIO_SETTRANSFER_MODE_AO	_IOW(CPSAIO_MAGIC, 12, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_START_AO	_IO(CPSAIO_MAGIC, 13)
#define IOCTL_CPSAIO_STOP_AO	_IO(CPSAIO_MAGIC, 14)
#define IOCTL_CPSAIO_SETCHANNEL_AO	_IOW(CPSAIO_MAGIC, 15, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_SETSAMPNUM_AO	_IOW(CPSAIO_MAGIC, 16, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_GETCHANNEL	_IOWR(CPSAIO_MAGIC, 17, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_GETCHANNEL_AO	_IOR(CPSAIO_MAGIC, 18, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_GETRESOLUTION	_IOR(CPSAIO_MAGIC, 19, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_GETMEMSTATUS	_IOR(CPSAIO_MAGIC, 20, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_SETECU_SIGNAL	_IOW(CPSAIO_MAGIC, 21, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_SET_OUTPULSE0	_IO(CPSAIO_MAGIC, 22)
#define IOCTL_CPSAIO_SET_CLOCK_AI	_IOW(CPSAIO_MAGIC, 23, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_SET_CLOCK_AO	_IOW(CPSAIO_MAGIC, 24, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_SET_SAMPNUM_AI	_IOW(CPSAIO_MAGIC, 25, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_SET_SAMPNUM_AO	_IOW(CPSAIO_MAGIC, 26, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_SET_CALIBRATION_AI	_IOW(CPSAIO_MAGIC, 27, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_SET_CALIBRATION_AO	_IOW(CPSAIO_MAGIC, 28, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_GET_INTERRUPT_FLAG_AI _IOR(CPSAIO_MAGIC, 29, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_GET_SAMPNUM	_IOWR(CPSAIO_MAGIC, 30, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_GET_CLOCK	_IOWR(CPSAIO_MAGIC, 31, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_GET_INTERRUPT_FLAG_AO _IOR(CPSAIO_MAGIC, 32, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_WRITE_EEPROM_AI _IOW(CPSAIO_MAGIC, 33, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_READ_EEPROM_AI _IOR(CPSAIO_MAGIC, 34, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_WRITE_EEPROM_AO _IOW(CPSAIO_MAGIC, 35, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_READ_EEPROM_AO _IOR(CPSAIO_MAGIC, 36, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_GET_CALIBRATION_AI	_IOW(CPSAIO_MAGIC, 37, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_GET_CALIBRATION_AO	_IOW(CPSAIO_MAGIC, 38, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_CLEAR_EEPROM _IO(CPSAIO_MAGIC, 39)
#define IOCTL_CPSAIO_GETMAXCHANNEL _IOR(CPSAIO_MAGIC, 40, struct cpsaio_ioctl_arg)

#define IOCTL_CPSAIO_SET_INTERRUPT_FLAG_AI _IOR(CPSAIO_MAGIC, 41, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_SET_INTERRUPT_FLAG_AO _IOR(CPSAIO_MAGIC, 42, struct cpsaio_ioctl_arg)

#define IOCTL_CPSAIO_GET_DRIVER_VERSION	_IOR(CPSAIO_MAGIC, 43, struct cpsaio_ioctl_string_arg)

#define IOCTL_CPSAIO_GET_SAMPLING_COUNT_AI	_IOR(CPSAIO_MAGIC, 44, struct cpsaio_ioctl_arg)

#define IOCTL_CPSAIO_RESET_STATUS	_IOW(CPSAIO_MAGIC, 45, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_RESET_MEMORY	_IOW(CPSAIO_MAGIC, 46, struct cpsaio_ioctl_arg)
#define IOCTL_CPSAIO_STOPTRIGGER_TYPE	_IOWR(CPSAIO_MAGIC, 47, struct cpsaio_ioctl_arg)

#define IOCTL_CPSAIO_MEMORY_TYPE	_IOWR(CPSAIO_MAGIC, 48, struct cpsaio_ioctl_arg)

#define IOCTL_CPSAIO_DIRECT_OUTPUT	_IOW(CPSAIO_MAGIC, 64, struct cpsaio_direct_arg)
#define IOCTL_CPSAIO_DIRECT_INPUT _IOR(CPSAIO_MAGIC, 65, struct cpsaio_direct_arg)
#define IOCTL_CPSAIO_DIRECT_COMMAND_OUTPUT	_IOW(CPSAIO_MAGIC, 66, struct cpsaio_direct_command_arg)
#define IOCTL_CPSAIO_DIRECT_COMMAND_INPUT _IOR(CPSAIO_MAGIC, 67, struct cpsaio_direct_command_arg)
#define IOCTL_CPSAIO_DIRECT_COMMAND_CALL	_IO(CPSAIO_MAGIC, 68)

/**************************************************/
