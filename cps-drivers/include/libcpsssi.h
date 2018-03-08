#ifndef __LIB_CPS_SSI__
#define __LIB_CPS_SSI__
#include "cps_def.h"

#define SSI_ERR_SUCCESS		0
#define SSI_ERR_INI_RESOURCE	1
#define SSI_ERR_INI_INTERRUPT	2
#define SSI_ERR_INI_MEMORY	3
#define SSI_ERR_INI_REGISTRY	4
#define SSI_ERR_INI_BOARD_ID	5

#define SSI_ERR_DLL_INVALID_ID		10001
#define SSI_ERR_DLL_CALL_DRIVER		10002
#define SSI_ERR_DLL_CREATE_FILE		10003
#define SSI_ERR_DLL_CLOSE_FILE		10004
#define SSI_ERR_DLL_CREATE_THREAD	10005

#define SSI_ERR_INFO_INVALID_DEVICE	10050
#define SSI_ERR_INFO_NOT_FIND_DEVICE	10051
#define SSI_ERR_INFO_INVALID_INFOTYPE	10052

#define SSIM_INTERRUPT	0x1300	///< Interrupt Message ID

#define SSI_CHANNEL_3WIRE	0x00	///< 3-Wire
#define SSI_CHANNEL_4WIRE	0x01	///< 4-Wire

#define SSI_CHANNEL_PT	0x00	///< PT (EU)
#define SSI_CHANNEL_JPT	0x01	///< JPT (JP)

#define CPSSSI_CALIBRATION_CLEAR_RAM		0x01	///< ROM CLEAR FLAG
#define CPSSSI_CALIBRATION_CLEAR_ROM		0x02	///< FPGA CLEAR FLAG

#define CPSSSI_CALIBRATION_CLEAR_ALL		( CPSSSI_CALIBRATION_CLEAR_RAM | CPSSSI_CALIBRATION_CLEAR_ROM )	// ALL CLEAR FLAG

/****  Structure ****/
typedef struct __contec_cps_ssi_int_callback_data__
{
	short id;
	short Message;
	short	wParam;
	long		lParam;
	void*	Param;

}CONTEC_CPS_SSI_INT_CALLBACK_DATA, *PCONTEC_CPS_SSI_INT_CALLBACK_DATA;

typedef void (*PCONTEC_CPS_SSI_INT_CALLBACK)(short, short, long, long, void *);

/**** Common Functions ****/
extern unsigned long ContecCpsSsiInit( char *DeviceName, short *Id );
extern unsigned long ContecCpsSsiExit( short Id );
extern unsigned long ContecCpsSsiGetErrorStrings( unsigned long code, char *Str );
extern unsigned long ContecCpsSsiQueryDeviceName( short Index, char *DeviceName, char *Device );

/**** Sensor Input Functions ****/
extern unsigned long ContecCpsSsiSetChannel( short Id, short SsiChannel, unsigned int iWire, unsigned int iJpt );
extern unsigned long ContecCpsSsiGetChannel( short Id, short SsiChannel , unsigned int *iWire, unsigned int *iJpt );
extern unsigned long ContecCpsSsiSetSenseResistor( short Id, double sence );
extern unsigned long ContecCpsSsiGetSenseResistor( short Id, double *sence );

extern unsigned long ContecCpsSsiGetStatus( short Id, unsigned long *SsiStatus );

extern unsigned long ContecCpsSsiStart( short Id, short SsiChannel );
extern unsigned long ContecCpsIsConversionStartBusyStatus( short Id, unsigned long *SsiStatus );
extern unsigned long ContecCpsSsiGetData( short Id, short SsiChannel, long *SsiData );

extern unsigned long ContecCpsSsiSingle( short Id, short SsiChannel, long *SsiData );
extern unsigned long ContecCpsSsiSingleTemperature( short Id, short SsiChannel, double *SsiTempData );
extern unsigned long ContecCpsSsiSingleResistance( short Id, short SsiChannel, double *SsiRegistance );

/**** ROM Write / Read Functions ****/
extern unsigned long ContecCpsSsiSetCalibrationOffsetToUShort( short Id, unsigned char ch, unsigned int iWire, unsigned short data );
extern unsigned long ContecCpsSsiSetCalibrationOffset( short Id, unsigned char ch, unsigned int iWire, double data );
extern unsigned long ContecCpsSsiGetCalibrationOffsetToUShort( short Id, unsigned char ch, unsigned int iWire, unsigned short *data );
extern unsigned long ContecCpsSsiGetCalibrationOffset( short Id, unsigned char ch, unsigned int iWire, double *data );
extern unsigned long ContecCpsSsiSetCalibrationGain( short Id, double data );
extern unsigned long ContecCpsSsiSetCalibrationGainToUShort( short Id, unsigned short data );
extern unsigned long ContecCpsSsiGetCalibrationGain( short Id, double *data );
extern unsigned long ContecCpsSsiWriteCalibrationGain( short Id, double value );
extern unsigned long ContecCpsSsiWriteCalibrationGainToUShort( short Id, unsigned short value );
extern unsigned long ContecCpsSsiWriteCalibrationOffset( short Id, unsigned char ch, double wire3Value, double wire4Value );
extern unsigned long ContecCpsSsiWriteCalibrationOffsetToUChar( short Id, unsigned char ch, unsigned char cWire3Val, unsigned char cWire4Val );
extern unsigned long ContecCpsSsiReadCalibrationGain( short Id, double *value );
extern unsigned long ContecCpsSsiReadCalibrationOffset( short Id, unsigned char ch, double *wire3Value, double *wire4Value );
extern unsigned long ContecCpsSsiClearCalibrationData( short Id, int iClear );

// The Spell Misstake functions
#define ContecCpsSsiSetSenceRegister ContecCpsSsiSetSenseResistor 
#define ContecCpsSsiGetSenceRegister ContecCpsSsiGetSenseResistor
#define ContecCpsSsiSingleRegistance ContecCpsSsiSingleResistance
#endif
