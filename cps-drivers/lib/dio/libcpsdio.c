/*
 *  Lib for CONTEC CONPROSYS Digital I/O (CPS-DIO) Series.
 *
 *  Copyright (C) 2015 Syunsuke Okamoto.
 *
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
* 
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
* 
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, see
   <http://www.gnu.org/licenses/>.  */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>

#include "cpsdio.h"

#ifdef CONFIG_CONPROSYS_SDK
 #include "../include/libcpsdio.h"
#else
 #include "libcpsdio.h"
#endif

#define CONTEC_CPSDIO_LIB_VERSION	"1.0.6"

/***
 @~English
 @name DebugPrint macro
 @~Japanese
 @name デバッグ用表示マクロ
***/
/// @{

#if 0
#define DEBUG_LIB_CPSDIO_INTERRUPT_CHECK(fmt...)	printf(fmt)
#else
#define DEBUG_LIB_CPSDIO_INTERRUPT_CHECK(fmt...)	do { } while (0)
#endif

/// @}
///


typedef struct __contec_cps_dio_int_callback__
{
	PCONTEC_CPS_DIO_INT_CALLBACK func;
	CONTEC_CPS_DIO_INT_CALLBACK_DATA data;
}CONTEC_CPS_DIO_INT_CALLBACK_LIST, *PCONTEC_CPS_DIO_INT_CALLBACK_LIST;

CONTEC_CPS_DIO_INT_CALLBACK_LIST contec_cps_dio_cb_list[CPS_DEVICE_MAX_NUM];

/**
	@~English
	@brief callback process function.(The running process is called to receive user's signal.)
	@param signo : signal number
	@~Japanese
	@param signo : シグナルナンバー
	@brief コールバック内部関数　（ユーザシグナル受信で動作）
**/
void _contec_signal_proc( int signo )
{
	int cnt;
	DEBUG_LIB_CPSDIO_INTERRUPT_CHECK("------ signal_proc: signo=%u\n", signo);
	if( signo == SIGUSR2 ){
		for( cnt = 0; cnt < CPS_DEVICE_MAX_NUM ; cnt ++){
			if( contec_cps_dio_cb_list[cnt].func != (PCONTEC_CPS_DIO_INT_CALLBACK)NULL ){
				
				contec_cps_dio_cb_list[cnt].func(
					contec_cps_dio_cb_list[cnt].data.id,
					DIOM_INTERRUPT,
					contec_cps_dio_cb_list[cnt].data.wParam,
					contec_cps_dio_cb_list[cnt].data.lParam,
					contec_cps_dio_cb_list[cnt].data.Param
				);
			}
		}
	}
}

/**
	@~English
	@brief DIO Library Initialize.
	@param DeviceName : Device node name ( cpsdioX )
	@param Id : Device Access Id
	@return Success: DIO_ERR_SUCCESS, Failed: otherwise DIO_ERR_SUCCESS
	@~Japanese
	@brief 初期化関数.
	@param DeviceName : デバイスノード名  ( cpsdioX )
	@param Id : デバイスID
	@return 成功: DIO_ERR_SUCCESS, 失敗: DIO_ERR_SUCCESS 以外

**/
unsigned long ContecCpsDioInit( char *DeviceName, short *Id )
{
	// open
	char Name[32];


	memset(&contec_cps_dio_cb_list[0], 0, sizeof(contec_cps_dio_cb_list));

	strcpy(Name, "/dev/");
	strcat(Name, DeviceName);

	*Id = open( Name, O_RDWR );

	if( *Id <= 0 ) return DIO_ERR_DLL_CREATE_FILE;

	return DIO_ERR_SUCCESS;

}

/**
	@~English
	@brief DIO Library Exit.
	@param Id : Device ID
	@return Success: DIO_ERR_SUCCESS
	@~Japanese
	@brief 終了関数.
	@param Id : デバイスID
	@return 成功:  DIO_ERR_SUCCESS
**/
unsigned long ContecCpsDioExit( short Id )
{
	// close
	close( Id );
	return DIO_ERR_SUCCESS;

}

/**
	@~English
	@brief DIO Library output from ErrorNumber to ErrorString.
	@param code : Error Code
	@param Str : Error String
	@return Success: DIO_ERR_SUCCESS
	@~Japanese
	@brief エラー文字列出力関数（未実装）
	@param code : エラーコード
	@param Str : エラー文字列
	@return 成功: DIO_ERR_SUCCESS
**/
unsigned long ContecCpsDioGetErrorStrings( unsigned long code, char *Str )
{

	return DIO_ERR_SUCCESS;
}

/**
	@~English
	@brief DIO Library query Device.
	@param Id : Device ID
	@param DeviceName : Device Node Name ( cpsdioX )
	@param Device : Device Name ( CPS-DIO-0808L , etc )
	@return Success: DIO_ERR_SUCCESS
	@~Japanese
	@brief クエリデバイス関数
	@param Id : デバイスID
	@param DeviceName : デバイスノード名 ( cpsdioX )
	@param Device : デバイス型式名 (  CPS-DIO-0808Lなど )
	@return 成功: DIO_ERR_SUCCESS
**/
unsigned long ContecCpsDioQueryDeviceName( short Index, char *DeviceName, char *Device )
{
	struct cpsdio_ioctl_string_arg	arg;
	int len;

	char tmpDevName[16];
	char baseDeviceName[16]="cpsdio";
	char strNum[2]={0};
	int findNum=0, cnt, ret;

	short tmpId = 0;

	for(cnt = 0;cnt < CPS_DEVICE_MAX_NUM ; cnt ++ ){
		sprintf(tmpDevName,"%s%x",baseDeviceName, cnt);
		ret = ContecCpsDioInit(tmpDevName, &tmpId);

		if( ret == 0 ){
			ioctl(tmpId, IOCTL_CPSDIO_GET_DEVICE_NAME, &arg);
			ContecCpsDioExit(tmpId);

			if(findNum == Index){
				sprintf(DeviceName,"%s",tmpDevName);
				sprintf(Device,"%s", arg.str);
				return DIO_ERR_SUCCESS;
			}else{
				findNum ++;
			}
			memset(&tmpDevName,0x00, 16);
			memset(&arg.str, 0x00, sizeof(arg.str)/sizeof(arg.str[0]));

		}
	}

	return DIO_ERR_INFO_NOT_FIND_DEVICE;
}

/**
	@~English
	@brief DIO Library get maximum ports.
	@param Id : Device ID
	@param InPortNum : in port number.
	@param OutPortNum : out port number.
	@return Success: DIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスの最大のポート数を取得します
	@param Id : デバイスID
	@param InPortNum :入力ポート数
	@param OutPortNum : 出力ポート数
	@return 成功: DIO_ERR_SUCCESS
**/
unsigned long ContecCpsDioGetMaxPort( short Id, short *InPortNum, short *OutPortNum )
{

	struct cpsdio_ioctl_arg	arg;

	ioctl( Id, IOCTL_CPSDIO_GET_INP_PORTNUM, &arg );

	*InPortNum = (short)( arg.val );

	ioctl( Id, IOCTL_CPSDIO_GET_OUTP_PORTNUM, &arg );

	*OutPortNum = (short)( arg.val );

	return DIO_ERR_SUCCESS;
}

/**
	@~English
	@brief DIO Library gets driver and library version.
	@param Id : Device ID
	@param libVer : library version
	@param drvVer : driver version
	@return Success: DIO_ERR_SUCCESS
	@~Japanese
	@brief デジタルデバイスのドライバとライブラリのバージョン文字列を取得します。
	@param Id : デバイスID
	@param libVer : ライブラリバージョン
	@param drvVer : ドライババージョン
	@note Ver.1.0.5 Change from cpsaio_ioctl_arg to cpsaio_ioctl_string_arg.
	@return 成功: DIO_ERR_SUCCESS
**/
unsigned long ContecCpsDioGetVersion( short Id , unsigned char libVer[] , unsigned char drvVer[] )
{

	struct cpsdio_ioctl_string_arg	arg;
	int len;


	ioctl( Id, IOCTL_CPSDIO_GET_DRIVER_VERSION, &arg );

	len = sizeof( arg.str ) / sizeof( arg.str[0] );
	memcpy(drvVer, arg.str, len);
//	strcpy_s(drvVer, arg.str);

	len = sizeof( CONTEC_CPSDIO_LIB_VERSION ) /sizeof( unsigned char );
	memcpy(libVer, CONTEC_CPSDIO_LIB_VERSION, len);

	return DIO_ERR_SUCCESS;

}


/**** Single Functions ****/
/**
	@~English
	@brief DIO Library get input data.(byte size).
	@param Id : Device ID
	@param Num : in port number.
	@param Data : data.
	@return Success: DIO_ERR_SUCCESS
	@~Japanese
	@brief 指定されたポートのデータをバイト単位で取得します
	@param Id : デバイスID
	@param Num :入力ポート
	@param Data : データ
	@return 成功: DIO_ERR_SUCCESS
**/
unsigned long ContecCpsDioInpByte( short Id, short Num, unsigned char *Data)
{
	struct cpsdio_ioctl_arg	arg;

	arg.port = Num;

	ioctl( Id, IOCTL_CPSDIO_INP_PORT, &arg );

	*Data = ( arg.val );

	return DIO_ERR_SUCCESS;
}

/**
	@~English
	@brief DIO Library get input data.(bit size).
	@param Id : Device ID
	@param Num : in bit number.
	@param Data : data.( 0 or 1 )
	@return Success: DIO_ERR_SUCCESS
	@~Japanese
	@brief 指定されたポートのデータをビット単位で取得します
	@param Id : デバイスID
	@param Num :入力ビット
	@param Data : データ (  0 or 1 )
	@return 成功: DIO_ERR_SUCCESS
**/
unsigned long ContecCpsDioInpBit( short Id, short Num, unsigned char *Data )
{

	struct cpsdio_ioctl_arg	arg;

	arg.port = Num / 8;

	ioctl( Id, IOCTL_CPSDIO_INP_PORT, &arg );

//////////////////////////////// Ver.1.0.3
	*Data = ( arg.val  & (1 << (Num % 8) ) ) >> (Num % 8);
//////////////////////////////// Ver.1.0.3

	return DIO_ERR_SUCCESS;
}

/**
	@~English
	@brief DIO Library set output data.(byte size).
	@param Id : Device ID
	@param Num : out port number.
	@param Data : data.
	@return Success: DIO_ERR_SUCCESS
	@~Japanese
	@brief 指定されたポートのデータをバイト単位で設定します
	@param Id : デバイスID
	@param Num :出力ポート
	@param Data : データ
	@return 成功: DIO_ERR_SUCCESS
**/
unsigned long ContecCpsDioOutByte( short Id, short Num, unsigned char Data)
{


	struct cpsdio_ioctl_arg	arg;

	arg.port = Num;
	arg.val = Data;

	ioctl( Id, IOCTL_CPSDIO_OUT_PORT , &arg );

	return DIO_ERR_SUCCESS;
}

/**
	@~English
	@brief DIO Library set output data.(bit size).
	@param Id : Device ID
	@param Num : out bit number.
	@param Data : data.( 0 or 1 )
	@return Success: DIO_ERR_SUCCESS
	@~Japanese
	@brief 指定されたポートのデータをビット単位で設定します
	@param Id : デバイスID
	@param Num :出力ビット
	@param Data : データ (  0 or 1 )
	@return 成功: DIO_ERR_SUCCESS
**/
unsigned long ContecCpsDioOutBit( short Id, short Num, unsigned char Data )
{

	struct cpsdio_ioctl_arg	arg;

	ioctl( Id, IOCTL_CPSDIO_INP_PORT , &arg );	
	arg.port = Num / 8;

//////////////////////////////// Ver.1.0.3
	arg.val = arg.val & (  ~(1 << (Num % 8) ) ) | ( Data << (Num % 8 ) );
//////////////////////////////// Ver.1.0.3

	ioctl( Id, IOCTL_CPSDIO_OUT_PORT , &arg );

	return DIO_ERR_SUCCESS;
}

/**
	@~English
	@brief DIO Library get output  echo data.(byte size).
	@param Id : Device ID
	@param Num : in byte number.
	@param Data : data.
	@return Success: DIO_ERR_SUCCESS
	@~Japanese
	@brief 指定されたポートのデータをバイト単位で取得します
	@param Id : デバイスID
	@param Num :出力ポート
	@param Data : データ
	@return 成功: DIO_ERR_SUCCESS
**/
unsigned long ContecCpsDioEchoBackByte( short Id, short Num, unsigned char *Data)
{
	struct cpsdio_ioctl_arg	arg;

	arg.port = Num;

	ioctl( Id, IOCTL_CPSDIO_OUT_PORT_ECHO , &arg );	
		
	*Data = arg.val;


	return DIO_ERR_SUCCESS;
}

/**
	@~English
	@brief DIO Library get output ehco data.(bit size).
	@param Id : Device ID
	@param Num : out bit number.
	@param Data : data.( 0 or 1 )
	@return Success: DIO_ERR_SUCCESS
	@~Japanese
	@brief 指定されたポートのデータをビット単位で取得します
	@param Id : デバイスID
	@param Num :出力ビット
	@param Data : データ (  0 or 1 )
	@return 成功: DIO_ERR_SUCCESS
**/
unsigned long ContecCpsDioEchoBackBit( short Id, short Num, unsigned char *Data)
{

	struct cpsdio_ioctl_arg	arg;

	arg.port = Num / 8;
		
	ioctl( Id, IOCTL_CPSDIO_OUT_PORT_ECHO, &arg );

//////////////////////////////// Ver.1.0.3
	*Data = ( arg.val  & (1 << (Num % 8) ) ) >> (Num % 8);
//////////////////////////////// Ver.1.0.3

	return DIO_ERR_SUCCESS;
}

// Multi Functions -----
/**
	@~English
	@brief DIO Library get many input data.(byte size).
	@param Id : Device ID
	@param Ports : in Port number Array.
	@param PortsDimensionNum : in Port Array Length Number 
	@param Data : data array.
	@return Success: DIO_ERR_SUCCESS
	@~Japanese
	@brief 指定された複数ポートのデータをバイト単位で取得します
	@param Id : デバイスID
	@param Ports : 入力ポート配列
	@param PortsDimensionNum :入力ポート配列数
	@param Data : データ配列
	@return 成功: DIO_ERR_SUCCESS
**/
unsigned long ContecCpsDioInpMultiByte( short Id, short Ports[], short PortsDimensionNum, unsigned char Data[] )
{
	short count;

	for( count = 0; count < PortsDimensionNum; count ++ )
	{		
		ContecCpsDioInpByte( Id, Ports[count], &Data[count] );
	}

	return DIO_ERR_SUCCESS;
}
/**
	@~English
	@brief DIO Library get many input data.(bit size).
	@param Id : Device ID
	@param Bits : in Bit number Array.
	@param BitsDimensionNum : in Bit Array Length Number 
	@param Data : data array.( Value : 0 or 1 )
	@return Success: DIO_ERR_SUCCESS
	@~Japanese
	@brief 指定された複数ポートのデータをビット単位で取得します
	@param Id : デバイスID
	@param Bits : 入力ポート配列
	@param BitsDimensionNum :入力ポート配列数
	@param Data : データ配列 ( 値: 0 or 1 )
	@return 成功: DIO_ERR_SUCCESS
**/
unsigned long ContecCpsDioInpMultiBit( short Id, short Bits[],short BitsDimensionNum, char Data[])
{

	short count;

	for( count = 0; count < BitsDimensionNum; count ++ )
	{		
		ContecCpsDioInpBit( Id, Bits[count], &Data[count] );
	}

	return DIO_ERR_SUCCESS;
}

/**
	@~English
	@brief DIO Library set many output data.(byte size).
	@param Id : Device ID
	@param Ports : out Port number Array.
	@param PortsDimensionNum : out Port Array Length Number 
	@param Data : data array.
	@return Success: DIO_ERR_SUCCESS
	@~Japanese
	@brief 指定された複数ポートのデータをバイト単位で設定します
	@param Id : デバイスID
	@param Ports : 出力ポート配列
	@param PortsDimensionNum :出力ポート配列数
	@param Data : データ配列
	@return 成功: DIO_ERR_SUCCESS
**/
unsigned long ContecCpsDioOutMultiByte( short Id, short Ports[], short PortsDimensionNum, unsigned char Data[] )
{
	short count;

	for( count = 0; count < PortsDimensionNum; count ++ )
	{		
		ContecCpsDioOutByte( Id, Ports[count], Data[count] );
	}
	return DIO_ERR_SUCCESS;
}

/**
	@~English
	@brief DIO Library set many output data.(bit size).
	@param Id : Device ID
	@param Bits : out Bit number Array.
	@param BitsDimensionNum : out Bit Array Length Number 
	@param Data : data array.( Value : 0 or 1 )
	@return Success: DIO_ERR_SUCCESS
	@~Japanese
	@brief 指定された複数ポートのデータをビット単位で設定します
	@param Id : デバイスID
	@param Bits : 出力ポート配列
	@param BitsDimensionNum :出力ポート配列数
	@param Data : データ配列 ( 値: 0 or 1 )
	@return 成功: DIO_ERR_SUCCESS
**/
unsigned long ContecCpsDioOutMultiBit( short Id, short Bits[],short BitsDimensionNum, char Data[])
{
	short count;

	for( count = 0; count < BitsDimensionNum; count ++ )
	{		
		ContecCpsDioOutBit( Id, Bits[count], Data[count] );
	}
	return DIO_ERR_SUCCESS;
}

/**
	@~English
	@brief DIO Library get many output echo data.(byte size).
	@param Id : Device ID
	@param Ports : out Port number Array.
	@param PortsDimensionNum : out Port Array Length Number 
	@param Data : data array.
	@return Success: DIO_ERR_SUCCESS
	@~Japanese
	@brief 指定された複数ポートのデータをバイト単位で取得します
	@param Id : デバイスID
	@param Ports : 出力ポート配列
	@param PortsDimensionNum :出力ポート配列数
	@param Data : データ配列
	@return 成功: DIO_ERR_SUCCESS
**/
unsigned long ContecCpsDioEchoBackMultiByte( short Id, short Ports[], short PortsDimensionNum, unsigned char Data[] )
{
	short count;

	for( count = 0; count < PortsDimensionNum; count ++ )
	{		
		ContecCpsDioEchoBackByte( Id, Ports[count], &Data[count] );
	}
	return DIO_ERR_SUCCESS;
}

/**
	@~English
	@brief DIO Library get many output echo data.(bit size).
	@param Id : Device ID
	@param Bits : out Bit number Array.
	@param BitsDimensionNum : out Bit Array Length Number 
	@param Data : data array.( Value : 0 or 1 )
	@return Success: DIO_ERR_SUCCESS
	@~Japanese
	@brief 指定された複数ポートのデータをビット単位で取得します
	@param Id : デバイスID
	@param Bits : 出力ポート配列
	@param BitsDimensionNum :出力ポート配列数
	@param Data : データ配列 ( 値: 0 or 1 )
	@return 成功: DIO_ERR_SUCCESS
**/
unsigned long ContecCpsDioEchoBackMultiBit( short Id, short Bits[],short BitsDimensionNum, char Data[])
{
	short count;

	for( count = 0; count < BitsDimensionNum; count ++ )
	{		
		ContecCpsDioEchoBackBit( Id, Bits[count], &Data[count] );
	}

	return DIO_ERR_SUCCESS;
}


//-- Digital Filter Functions -----------------
/**
	@~English
	@brief DIO Library set digital filter by the device.
	@param Id : Device ID
	@param FilterValue : filter Values
	@return Success: DIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスにデジタルフィルタを設定します。
	@param Id : デバイスID
	@param FilterValue : フィルタ定数
	@return 成功: DIO_ERR_SUCCESS
**/
unsigned long ContecCpsDioSetDigitalFilter( short Id, unsigned char FilterValue )
{
	struct cpsdio_ioctl_arg	arg;
	
	arg.val = (FilterValue & 0x3F);

	ioctl( Id, IOCTL_CPSDIO_SET_FILTER	 , &arg );

	return DIO_ERR_SUCCESS;
}

/**
	@~English
	@brief DIO Library get digital filter by the device.
	@param Id : Device ID
	@param FilterValue : filter Values
	@return Success: DIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスにデジタルフィルタを取得します。
	@param Id : デバイスID
	@param FilterValue : フィルタ定数
	@return 成功: DIO_ERR_SUCCESS
**/
unsigned long ContecCpsDioGetDigitalFilter( short Id, unsigned char *FilterValue )
{
	struct cpsdio_ioctl_arg	arg;

	ioctl( Id, IOCTL_CPSDIO_GET_FILTER, &arg );

	*FilterValue = ( arg.val );

	return DIO_ERR_SUCCESS;
}


/**** Internal Power Supply Functions ****/

/**
	@~English
	@brief DIO Library set internal power supply by the device.
	@param Id : Device ID
	@param isInternal : 0... External, 1... Internal
	@par This function is called by CPS-DIO-****BL Series.
	@return Success: DIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスに内部電源か外部電源の設定を行います。
	@param Id : デバイスID
	@param isInternal : 電源フラグ (0… 外部電源, 1...内部電源 )
	@par この関数は CPS-DIO-****BL Series のみ使用できます。
	@return 成功: DIO_ERR_SUCCESS
**/
unsigned long ContecCpsDioSetInternalPowerSupply( short Id, unsigned char isInternal )
{
	struct cpsdio_ioctl_arg	arg;

	arg.val = (isInternal & 0x01);

	ioctl( Id, IOCTL_CPSDIO_SET_INTERNAL_POW	 , &arg );

	return DIO_ERR_SUCCESS;
}

/**
	@~English
	@brief DIO Library get internal power supply by the device.
	@param Id : Device ID
	@param isInternal : 0... External, 1... Internal
	@return Success: DIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスの内部電源か外部電源か取得します。
	@param Id : デバイスID
	@param isInternal : 電源フラグ (0… 外部電源, 1...内部電源 )
	@return 成功: DIO_ERR_SUCCESS
**/
unsigned long ContecCpsDioGetInternalPowerSupply( short Id, unsigned char *isInternal )
{
	struct cpsdio_ioctl_arg	arg;

	ioctl( Id, IOCTL_CPSDIO_GET_INTERNAL_POW, &arg );

	*isInternal = ( arg.val );

	return DIO_ERR_SUCCESS;
}

/**** Interrupt Event Functions ****/

/**
	@~English
	@brief DIO Library set notify interrupt.
	@param Id : Device ID
	@param BitNum : Bit number
	@param Logic :
	@return Success: DIO_ERR_SUCCESS
	@~Japanese
	@brief を取得します。
	@param Id : デバイスID
	@param BitNum : ビット番号
	@param Logic :
	@return 成功: DIO_ERR_SUCCESS
**/
unsigned long ContecCpsDioNotifyInterrupt( short Id, short BitNum, short Logic )
{
	struct cpsdio_ioctl_arg	arg;

	/**** Mask Set ****/
	arg.val = ~(1 << BitNum);

	ioctl( Id, IOCTL_CPSDIO_SET_INT_MASK, &arg );

	/**** Egde Set ****/
	arg.val = (Logic << BitNum);

	ioctl( Id, IOCTL_CPSDIO_SET_INT_EGDE, &arg );

	/****  process_id Set ****/

	arg.val = getpid();

	ioctl( Id, IOCTL_CPSDIO_SET_CALLBACK_PROCESS, &arg );


	DEBUG_LIB_CPSDIO_INTERRUPT_CHECK("------ signal: SIGUSR2\n");
	/*** signal ***/
	signal( SIGUSR2, _contec_signal_proc );
	return DIO_ERR_SUCCESS;
}

/**
	@~English
	@brief DIO Library set callback proc.
	@param Id : Device ID
	@param cb : Callback Funciton
	@param Param : Parameters
	@return Success: DIO_ERR_SUCCESS
	@~Japanese
	@brief コールバック関数を設定する関数です。(未実装)
	@param Id : デバイスID
	@param cb : コールバック関数
	@param Param : パラメータ
	@return 成功: DIO_ERR_SUCCESS
**/
unsigned long ContecCpsDioSetInterruptCallBackProc( short Id, PCONTEC_CPS_DIO_INT_CALLBACK cb, void* Param )
{
	struct cpsdio_ioctl_arg	arg;

// debug test start
	contec_cps_dio_cb_list[0].func        = cb;
	contec_cps_dio_cb_list[0].data.id     = Id;
	contec_cps_dio_cb_list[0].data.wParam = 0;
	contec_cps_dio_cb_list[0].data.lParam = 0;
	contec_cps_dio_cb_list[0].data.Param  = Param;
// debug test end

	return DIO_ERR_SUCCESS;
}

