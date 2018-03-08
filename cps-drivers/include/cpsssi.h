/*** cpsssi.h ******************************/
#include <linux/ioctl.h>
#include <linux/sched.h>

/* structure */
/**
	@struct __cpsssi_device_data
	@~English
	@brief structure device data
	@~Japanese
	@brief デバイスデータ構造体
**/
typedef struct __cpsssi_device_data{
	char Name[32];///< デバイス名
	unsigned int ProductNumber;///< 製品番号
	unsigned int ssiChannel;///< チャネル数
	void* ChannelData;///< チャネルデータ
}CPSSSI_DEV_DATA, *PCPSSSI_DEV_DATA;


/**
	@struct __cpsssi_4p_channel_data
	@~English
	@brief CPS-SSI-4P channel data
	@~Japanese
	@brief CPS-SSI-4P チャネルデータ
**/
typedef struct __cpsssi_4p_channel_data{
	unsigned short Gain_RSense;					///< Gain( ( Delta-RSense )
	unsigned char Current_Wire_Status;	///< 3-Wire or 4-Wire
	unsigned char Current_Rtd_Standard;	///< PT or JPT
	unsigned char lastStatus;						///< Measure Last Status

}CPSSSI_4P_CHANNEL_DATA, *PCPSSSI_4P_CHANNEL_DATA;

/**
	@struct cpsssi_ioctl_arg
	@~English
	@brief I/O Control structure
	@~Japanese
	@brief I/O コントロール 構造体
**/
struct cpsssi_ioctl_arg{
	unsigned int ch;	///< channel
	unsigned long val;	///< value
};

/**
	@struct cpsssi_ioctl_string_arg
	@~English
	@brief I/O Control structure ( String )
	@~Japanese
	@brief I/O コントロール 構造体(文字列用)
**/
struct cpsssi_ioctl_string_arg{
	unsigned long index;///< index
	unsigned char str[32];	///< string
};

/**
	@struct cpsssi_direct_command_arg
	@~English
	@brief structure direct command I/O
	@~Japanese
	@brief 直接I/Oコマンド　構造体
**/
struct cpsssi_direct_command_arg{
	unsigned long addr;	///< address
	unsigned long val;	///< value
};


/*  Error Code */

#define SSI_ERR_SYS_MEMORY										20000
#define SSI_ERR_SYS_NOT_SUPPORTED					20001
#define SSI_ERR_SYS_BOARD_EXECUTING				20002
#define SSI_ERR_SYS_USING_OTHER_PROCESS	20003


#define SSI_4P_CHANNEL_RTD_PT_10	( 0x0A )
#define SSI_4P_CHANNEL_RTD_PT_50	( 0x0B )
#define SSI_4P_CHANNEL_RTD_PT_100	( 0x0C )
#define SSI_4P_CHANNEL_RTD_PT_200	( 0x0D )
#define SSI_4P_CHANNEL_RTD_PT_500	( 0x0E )
#define SSI_4P_CHANNEL_RTD_PT_1000	( 0x0F )
#define SSI_4P_CHANNEL_RTD_1000	( 0x10 )
#define SSI_4P_CHANNEL_RTD_NI_120	( 0x11 )

#define SSI_4P_CHANNEL_RTD_SENSE_POINTER_CH3TOCH2	( 0x03 )

#define SSI_4P_CHANNEL_RTD_WIRE_3	( 0x05 )
#define SSI_4P_CHANNEL_RTD_WIRE_4	( 0x0D )

#define SSI_4P_CHANNEL_RTD_EXCITATION_CURRENT_5UA	( 0x01 )
#define SSI_4P_CHANNEL_RTD_EXCITATION_CURRENT_10UA	( 0x02 )
#define SSI_4P_CHANNEL_RTD_EXCITATION_CURRENT_25UA	( 0x03 )
#define SSI_4P_CHANNEL_RTD_EXCITATION_CURRENT_50UA	( 0x04 )
#define SSI_4P_CHANNEL_RTD_EXCITATION_CURRENT_100UA	( 0x05 )
#define SSI_4P_CHANNEL_RTD_EXCITATION_CURRENT_250UA	( 0x06 )
#define SSI_4P_CHANNEL_RTD_EXCITATION_CURRENT_500UA	( 0x07 )
#define SSI_4P_CHANNEL_RTD_EXCITATION_CURRENT_1MA	( 0x08 )

#define SSI_4P_CHANNEL_STANDARD_EU	0x00
#define SSI_4P_CHANNEL_STANDARD_AM	0x01
#define SSI_4P_CHANNEL_STANDARD_JP	0x02
#define SSI_4P_CHANNEL_STANDARD_ITS_90	0x03

#define SSI_4P_CHANNEL_SET_RTD( pt, sense, wire, excit, std )	\
	( ( pt << 27 ) | ( sense << 22 ) | ( wire << 18 ) | (excit << 14) | (std << 12 ) )

#define SSI_4P_CHANNEL_GET_RTD_PT( rtd )	( (rtd & 0xF8000000)  >> 27 )
#define SSI_4P_CHANNEL_GET_RTD_SENSE_POINTER( rtd )	( ( rtd & 0x07C00000 ) >> 22 )
#define SSI_4P_CHANNEL_GET_RTD_WIRE_MODE( rtd )	( ( rtd & 0x003C0000 ) >> 18 )
#define SSI_4P_CHANNEL_GET_RTD_EXCITATION_CURRENT( rtd )	( ( rtd & 0x0003C000 ) >> 14 )
#define SSI_4P_CHANNEL_GET_RTD_STANDARD( rtd )	( ( rtd & 0x00003000 ) >> 12 )

#define SSI_4P_SENSE	( 0x1D )

#define SSI_4P_CHANNEL_SET_SENSE( val ) 	\
	( ( SSI_4P_SENSE << 27 ) | ( val & 0x07FFFFFF ) )

#define SSI_4P_CHANNEL_GET_SENSE( val )	( val & 0x07FFFFFF )

#define SSI_4P_SENSE_DEFAULT_VALUE	( 2000.0 )


/* Driver's Code */

#define CPSSSI_MAGIC	'g'

#define IOCTL_CPSSSI_INIT	_IOWR(CPSSSI_MAGIC, 1, struct cpsssi_ioctl_arg)
#define IOCTL_CPSSSI_EXIT	_IOWR(CPSSSI_MAGIC, 2, struct cpsssi_ioctl_arg)

#define IOCTL_CPSSSI_INDATA	_IOR(CPSSSI_MAGIC, 3, struct cpsssi_ioctl_arg)
#define IOCTL_CPSSSI_INSTATUS	_IOR(CPSSSI_MAGIC, 4, struct cpsssi_ioctl_arg)
#define IOCTL_CPSSSI_SET_SENSE_RESISTANCE	_IOW(CPSSSI_MAGIC, 5, struct cpsssi_ioctl_arg)
#define IOCTL_CPSSSI_SET_CHANNEL	_IOW(CPSSSI_MAGIC, 6, struct cpsssi_ioctl_arg)
#define IOCTL_CPSSSI_START	_IOW(CPSSSI_MAGIC, 7, struct cpsssi_ioctl_arg)
#define IOCTL_CPSSSI_GET_DEVICE_NAME	_IOR(CPSSSI_MAGIC, 8, struct cpsssi_ioctl_string_arg)
#define IOCTL_CPSSSI_WRITE_EEPROM_SSI _IOW(CPSSSI_MAGIC, 9, struct cpsssi_ioctl_arg)
#define IOCTL_CPSSSI_READ_EEPROM_SSI _IOR(CPSSSI_MAGIC, 10, struct cpsssi_ioctl_arg)
#define IOCTL_CPSSSI_CLEAR_EEPROM _IO(CPSSSI_MAGIC, 11 )
#define IOCTL_CPSSSI_SET_OFFSET	_IOW(CPSSSI_MAGIC, 12, struct cpsssi_ioctl_arg)
#define IOCTL_CPSSSI_GET_OFFSET	_IOR(CPSSSI_MAGIC, 13, struct cpsssi_ioctl_arg)
#define IOCTL_CPSSSI_GET_CHANNEL	_IOR(CPSSSI_MAGIC, 14, struct cpsssi_ioctl_arg)
#define IOCTL_CPSSSI_GET_SENSE_RESISTANCE	_IOR(CPSSSI_MAGIC, 15, struct cpsssi_ioctl_arg)
#define IOCTL_CPSSSI_STARTBUSYSTATUS	_IOR(CPSSSI_MAGIC, 16, struct cpsssi_ioctl_arg)

#define IOCTL_CPSSSI_GET_DRIVER_VERSION	_IOR(CPSSSI_MAGIC, 17, struct cpsssi_ioctl_string_arg)

#define IOCTL_CPSSSI_DIRECT_COMMAND_OUTPUT	_IOW(CPSSSI_MAGIC, 64, struct cpsssi_direct_command_arg)
#define IOCTL_CPSSSI_DIRECT_COMMAND_INPUT _IOR(CPSSSI_MAGIC, 65, struct cpsssi_direct_command_arg)

/**************************************************/
