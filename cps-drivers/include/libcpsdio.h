#include "cps_def.h"

#define DIO_POWERSUPPLY_EXTERNAL	0	///< 外部電源
#define DIO_POWERSUPPLY_INTERNAL	1	///< 内部電源

#define DIO_ERR_SUCCESS		0
#define DIO_ERR_INI_RESOURCE	1
#define DIO_ERR_INI_INTERRUPT	2
#define DIO_ERR_INI_MEMORY	3
#define DIO_ERR_INI_REGISTRY	4
#define DIO_ERR_INI_BOARD_ID	5

#define DIO_ERR_DLL_INVALID_ID		10001
#define DIO_ERR_DLL_CALL_DRIVER		10002
#define DIO_ERR_DLL_CREATE_FILE		10003
#define DIO_ERR_DLL_CLOSE_FILE		10004
#define DIO_ERR_DLL_CREATE_THREAD	10005

#define DIO_ERR_INFO_INVALID_DEVICE	10050
#define DIO_ERR_INFO_NOT_FIND_DEVICE	10051
#define DIO_ERR_INFO_INVALID_INFOTYPE	10052

#define DIO_ERR_DLL_BUFF_ADDRESS	10100
#define DIO_ERR_DLL_TRG_KIND		10300
#define DIO_ERR_DLL_CALLBACK		10400
#define DIO_ERR_DLL_DIRECTION		10500

#define DIOM_INTERRUPT	0x1300

#define DIO_INT_NONE	0
#define DIO_INT_RISE	1
#define DIO_INT_FALL	2


/****  Structure ****/
typedef struct __contec_cps_dio_int_callback_data__
{
	short id;
	short Message;
	short	wParam;
	long		lParam;
	void*	Param;

}CONTEC_CPS_DIO_INT_CALLBACK_DATA, *PCONTEC_CPS_DIO_INT_CALLBACK_DATA;

typedef void (*PCONTEC_CPS_DIO_INT_CALLBACK)(short, short, long, long, void *);

/**** Common Functions ****/
extern unsigned long ContecCpsDioInit( char *DeviceName, short *Id );
extern unsigned long ContecCpsDioExit( short Id );
extern unsigned long ContecCpsDioGetErrorStrings( unsigned long code, char *Str );
extern unsigned long ContecCpsDioQueryDeviceName( short Index, char *DeviceName, char *Device );
extern unsigned long ContecCpsDioGetMaxPort( short Id, short *InPortNum, short *OutPortNum );

/**** Input Functions ****/
extern unsigned long ContecCpsDioInpByte( short Id, short Num, unsigned char *Data);
extern unsigned long ContecCpsDioInpBit( short Id, short Num, unsigned char *Data );
extern unsigned long ContecCpsDioOutByte( short Id, short Num, unsigned char Data);
extern unsigned long ContecCpsDioOutBit( short Id, short Num, unsigned char Data );
extern unsigned long ContecCpsDioEchoBackByte( short Id, short Num, unsigned char *Data);
extern unsigned long ContecCpsDioEchoBackbit( short Id, short Num, unsigned char *Data);

/**** Input Functions ****/
extern unsigned long ContecCpsDioInpMultiByte( short Id, short Ports[], short PortsDimensionNum, unsigned char Data[] );
extern unsigned long ContecCpsDioInpMultiBit( short Id, short Bits[],short BitsDimensionNum, char Data[] );
extern unsigned long ContecCpsDioOutMultiByte( short Id, short Ports[], short PortsDimensionNum, unsigned char Data[] );
extern unsigned long ContecCpsDioOutMultiBit( short Id, short Bits[],short BitsDimensionNum, char Data[] );
extern unsigned long ContecCpsDioEchoBackMultiByte( short Id, short Ports[], short PortsDimensionNum, unsigned char Data[] );
extern unsigned long ContecCpsDioEchoBackMultiBit( short Id, short Bits[],short BitsDimensionNum, char Data[] );

/**** Digital Filter Functions ****/
extern unsigned long ContecCpsDioSetDigitalFilter( short Id, unsigned char FilterValue );
extern unsigned long ContecCpsDioGetDigitalFilter( short Id, unsigned char *FilterValue );

/**** Internal Power Supply Functions ****/
extern unsigned long ContecCpsDioSetInternalPowerSupply( short Id, unsigned char isInternal );
extern unsigned long ContecCpsDioGetInternalPowerSupply( short Id, unsigned char *isInternal );

/**** INTERRUPT Event Functions ****/
extern unsigned long ContecCpsDioNotifyInterrupt( short Id, short BitNum, short Logic );
extern unsigned long ContecCpsDioSetInterruptCallBackProc( short Id, PCONTEC_CPS_DIO_INT_CALLBACK cb, void* Param );

