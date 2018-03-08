/*
 *  Lib for CONTEC ConProSys SenSor Input (CPS-SSI) Series.
 *
 *  Copyright (C) 2016 Syunsuke Okamoto.
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
//#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <stdio.h>

#include "cpsssi.h"

#ifdef CONFIG_CONPROSYS_SDK
 #include "../include/libcpsssi.h"
#else
 #include "libcpsssi.h"
#endif

#define CONTEC_CPSSSI_LIB_VERSION	"1.0.6"

typedef struct __contec_cps_ssi_int_callback__
{
	PCONTEC_CPS_SSI_INT_CALLBACK func;
	CONTEC_CPS_SSI_INT_CALLBACK_DATA data;
}CONTEC_CPS_SSI_INT_CALLBACK_LIST, *PCONTEC_CPS_SSI_INT_CALLBACK_LIST;

CONTEC_CPS_SSI_INT_CALLBACK_LIST contec_cps_ssi_cb_list[CPS_DEVICE_MAX_NUM];

/**
	@~English
	@brief callback process function.(The running process is called to receive user's signal.)
	@param signo : signal number
	@~Japanese
	@param signo : シグナルナンバー
	@brief コールバック内部関数　（ユーザシグナル受信で動作）
**/
void _contec_cpsssi_signal_proc( int signo )
{
	int cnt;
	if( signo == SIGUSR2 ){
		for( cnt = 0; cnt < CPS_DEVICE_MAX_NUM ; cnt ++){
			if( contec_cps_ssi_cb_list[cnt].func != (PCONTEC_CPS_SSI_INT_CALLBACK)NULL ){
				
				contec_cps_ssi_cb_list[cnt].func(
					contec_cps_ssi_cb_list[cnt].data.id,
					SSIM_INTERRUPT,
					contec_cps_ssi_cb_list[cnt].data.wParam,
					contec_cps_ssi_cb_list[cnt].data.lParam,
					contec_cps_ssi_cb_list[cnt].data.Param
				);
			}
		}
	}
}

/**
	@~English
	@brief SSI Library Initialize.
	@param DeviceName : Device node name ( cpsssiX )
	@param Id : Device Access Id
	@return Success: SSI_ERR_SUCCESS, Failed: otherwise SSI_ERR_SUCCESS
	@~Japanese
	@brief 初期化関数.
	@param DeviceName : デバイスノード名  ( cpsssiX )
	@param Id : デバイスID
	@return 成功: SSI_ERR_SUCCESS, 失敗: SSI_ERR_SUCCESS 以外
**/
unsigned long ContecCpsSsiInit( char *DeviceName, short *Id )
{
	// open
	char Name[32];
	struct cpsssi_ioctl_arg	arg;

	strcpy(Name, "/dev/");
	strcat(Name, DeviceName);

	*Id = open( Name, O_RDWR );

	if( *Id <= 0 ) return SSI_ERR_DLL_CREATE_FILE;

	ioctl( *Id, IOCTL_CPSSSI_INIT, &arg );

	ContecCpsSsiSetSenseResistor( *Id, 2000.0 ); 
	
	return SSI_ERR_SUCCESS;

}

/**
	@~English
	@brief SSI Library Exit.
	@param Id : Device ID
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief 終了関数.
	@param Id : デバイスID
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiExit( short Id )
{
	struct cpsssi_ioctl_arg	arg;
	arg.val = 0;

	ioctl( Id, IOCTL_CPSSSI_EXIT, &arg );
	// close
	close( Id );
	return SSI_ERR_SUCCESS;

}

/**
	@~English
	@brief SSI Library output from ErrorNumber to ErrorString.
	@param code : Error Code
	@param Str : Error String
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief エラー文字列出力関数（未実装）
	@param code : エラーコード
	@param Str : エラー文字列
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiGetErrorStrings( unsigned long code, char *Str )
{

	return SSI_ERR_SUCCESS;
}

/**
	@~English
	@brief SSI Library query Device.
	@param Id : Device ID
	@param DeviceName : Device Node Name ( cpsssiX )
	@param Device : Device Name ( CPS-SSI-4P , etc )
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief クエリデバイス関数（未実装）
	@param Id : デバイスID
	@param DeviceName : デバイスノード名 ( cpsssiX )
	@param Device : デバイス型式名 ( CPS-SSI-4Pなど )
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiQueryDeviceName( short Index, char *DeviceName, char *Device )
{
	struct cpsssi_ioctl_string_arg	arg;
	int len;

	char tmpDevName[16];
	char baseDeviceName[16]="cpsssi";
	char strNum[2]={0};
	int findNum=0, cnt, ret;

	short tmpId = 0;

	for(cnt = 0;cnt < CPS_DEVICE_MAX_NUM ; cnt ++ ){
		sprintf(tmpDevName,"%s%x",baseDeviceName, cnt);
		ret = ContecCpsSsiInit(tmpDevName, &tmpId);

		if( ret == 0 ){
			ioctl(tmpId, IOCTL_CPSSSI_GET_DEVICE_NAME, &arg);
			ContecCpsSsiExit(tmpId);

			if(findNum == Index){
				sprintf(DeviceName,"%s",tmpDevName);
				sprintf(Device,"%s", arg.str);
				return SSI_ERR_SUCCESS;
			}else{
				findNum ++;
			}
			memset(&tmpDevName,0x00, 16);
			memset(&arg.str, 0x00, sizeof(arg.str)/sizeof(arg.str[0]));

		}
	}

	return SSI_ERR_INFO_NOT_FIND_DEVICE;
}

/**
	@~English
	@brief SSI Library gets driver and library version.
	@param Id : Device ID
	@param libVer : library version
	@param drvVer : driver version
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief センサ入力デバイスのドライバとライブラリのバージョン文字列を取得します。
	@param Id : デバイスID
	@param libVer : ライブラリバージョン
	@param drvVer : ドライババージョン
	@note Ver.1.0.5 Change from cpsaio_ioctl_arg to cpsaio_ioctl_string_arg.
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiGetVersion( short Id , unsigned char libVer[] , unsigned char drvVer[] )
{

	struct cpsssi_ioctl_string_arg	arg;
	int len;


	ioctl( Id, IOCTL_CPSSSI_GET_DRIVER_VERSION, &arg );

	len = sizeof( arg.str ) / sizeof( arg.str[0] );
	memcpy(drvVer, arg.str, len);
//	strcpy_s(drvVer, arg.str);

	len = sizeof( CONTEC_CPSSSI_LIB_VERSION ) /sizeof( unsigned char );
	memcpy(libVer, CONTEC_CPSSSI_LIB_VERSION, len);

	return SSI_ERR_SUCCESS;

}

/**
	@~English
	@brief SSI Library set channel's paramter.
	@param Id : Device ID
	@param SsiChannel : Channel Number
	@param iWire : Wire type ( SSI_CHANNEL_3WIRE or SSI_CHANNEL_4WIRE )
	@param iJpt : PT Type ( SSI_CHANNEL_JPT or SSI_CHANNEL_PT  )
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief チャネル情報設定関数
	@param Id : デバイスID
	@param SsiChannel : チャネル番号
	@param iWire : 三線式か四線式  ( SSI_CHANNEL_3WIRE か SSI_CHANNEL_4WIRE )
	@param iJpt : PT100か JPT100 ( SSI_CHANNEL_PT か SSI_CHANNEL_JPT )
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiSetChannel( short Id, short SsiChannel , unsigned int iWire, unsigned int iJpt )
{
	struct cpsssi_ioctl_arg	arg;
	unsigned char isWire = 0;
	unsigned char isCountry = 0;

	switch( iWire ){
	case SSI_CHANNEL_3WIRE:
		isWire = SSI_4P_CHANNEL_RTD_WIRE_3;
		break;
	case SSI_CHANNEL_4WIRE:
	default:
		isWire = SSI_4P_CHANNEL_RTD_WIRE_4;
		break;
	}

	switch( iJpt ){
	case SSI_CHANNEL_JPT:
		isCountry = SSI_4P_CHANNEL_STANDARD_JP;
		break;
	case SSI_CHANNEL_PT:
	default:
		isCountry = SSI_4P_CHANNEL_STANDARD_EU;
		break;
	}

	arg.ch = SsiChannel;
	arg.val = SSI_4P_CHANNEL_SET_RTD(
		SSI_4P_CHANNEL_RTD_PT_100,
		SSI_4P_CHANNEL_RTD_SENSE_POINTER_CH3TOCH2,
		isWire,
		SSI_4P_CHANNEL_RTD_EXCITATION_CURRENT_250UA,
		isCountry
	); // 0x60F5C000; // omajinai
	
	ioctl( Id, IOCTL_CPSSSI_SET_CHANNEL, &arg );

	return SSI_ERR_SUCCESS;
}

/**
	@~English
	@brief SSI Library get channel's parameter.
	@param Id : Device ID
	@param SsiChannel : Channel Number
	@param iWire : Wire type ( SSI_CHANNEL_3WIRE or SSI_CHANNEL_4WIRE )
	@param iJpt : PT Type ( SSI_CHANNEL_JPT or SSI_CHANNEL_PT  )
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief チャネル情報取得関数
	@param Id : デバイスID
	@param SsiChannel : チャネル番号
	@param iWire : 三線式か四線式  ( SSI_CHANNEL_3WIRE か SSI_CHANNEL_4WIRE )
	@param iJpt : PT100か JPT100 ( SSI_CHANNEL_PT か SSI_CHANNEL_JPT )
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiGetChannel( short Id, short SsiChannel , unsigned int *iWire, unsigned int *iJpt )
{
	struct cpsssi_ioctl_arg	arg;
	unsigned char isWire = 0;
	unsigned char isCountry = 0;

	arg.ch = SsiChannel;
	ioctl( Id, IOCTL_CPSSSI_GET_CHANNEL, &arg );

	isWire = SSI_4P_CHANNEL_GET_RTD_WIRE_MODE( arg.val );
	isCountry = SSI_4P_CHANNEL_GET_RTD_STANDARD( arg.val );

	if( iWire != NULL ){
		switch( isWire ){
		case SSI_4P_CHANNEL_RTD_WIRE_3:
			*iWire = SSI_CHANNEL_3WIRE;
			break;
		case SSI_4P_CHANNEL_RTD_WIRE_4:
		default:
			*iWire = SSI_CHANNEL_4WIRE;
			break;
		}
	}

	if( iJpt != NULL ){
		switch( isCountry ){
		case SSI_4P_CHANNEL_STANDARD_EU :
			*iJpt = SSI_CHANNEL_PT;
			break;
		case SSI_4P_CHANNEL_STANDARD_JP :
			*iJpt = SSI_CHANNEL_JPT;
			break;
		}
	}

	return SSI_ERR_SUCCESS;
}


/**
	@~English
	@brief SSI Library set the sense resistor.
	@param Id : Device ID
	@param sense : sense resistor value.
	@warning Default sense resistor 2k Ohm. If you change the sense resistor, you own risk.
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief センス抵抗設定関数
	@param Id : デバイスID
	@param sense : センス抵抗値
	@warning センス抵抗の標準値は 2KΩです。もしセンス抵抗値を変える場合、自己責任になります。
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiSetSenseResistor( short Id, double sense )
{
	struct cpsssi_ioctl_arg	arg;
	unsigned long ulSense;

	ulSense = (unsigned long)( sense * pow(2.0, 10.0 ) );
	
	arg.val = SSI_4P_CHANNEL_SET_SENSE(
		ulSense
	); // 0xe81f4000; // omajinai


	ioctl( Id, IOCTL_CPSSSI_SET_SENSE_RESISTANCE, &arg );

	return SSI_ERR_SUCCESS;
}

/**
	@~English
	@brief SSI Library get the sense resistor.
	@param Id : Device ID
	@param sense : sense resistor value.
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief センス抵抗取得関数
	@param Id : デバイスID
	@param sense : センス抵抗値
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiGetSenseResistor( short Id, double *sense )
{
	struct cpsssi_ioctl_arg	arg;
	unsigned long ulSense;

	ioctl( Id, IOCTL_CPSSSI_GET_SENSE_RESISTANCE, &arg );

	ulSense = SSI_4P_CHANNEL_GET_SENSE( arg.val );

	*sense = ( (double)ulSense ) /  pow(2.0, 10.0 ) ;

	return SSI_ERR_SUCCESS;
}

//**** Running Functions **********************************************

/**
	@~English
	@brief SSI Library get the status.
	@param Id : Device ID
	@param SsiStatus : status
	@note The function get status all channel. The 0 channel's status from 0 to 8 bits, the 1 channel's status from 7 to 15 bits, the 2 channel's status from 16 to 23 bits, the 3 channel's status from 23 to 31 bits.
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief ステータス取得関数
	@param Id : デバイスID
	@param SsiStatus : ステータス
	@note ステータスは 32bitで4ch分すべて取得できます。 0-7bitが0ch, 8-15bitが1ch, 16-23bitが2ch, 24-31bitが3chのステータスになります。
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiGetStatus( short Id, unsigned long *SsiStatus )
{
	struct cpsssi_ioctl_arg	arg;

	ioctl( Id, IOCTL_CPSSSI_INSTATUS, &arg );

	*SsiStatus = arg.val;

	return SSI_ERR_SUCCESS;
}

/**
	@~English
	@brief SSI Library Start function.
	@param Id : Device ID
	@param SsiChannel : Channel Number
	@note It is easier to use the ContecCpsSsiSingleTemperature function than using this function.
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief スタート関数
	@param Id : デバイスID
	@param SsiChannel : チャネル番号
	@note この関数を使用するより、ContecCpsSsiSingleTemperature関数を使用する方が簡単です。
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiStart( short Id, short SsiChannel )
{
	struct cpsssi_ioctl_arg	arg;

	arg.ch = SsiChannel;
	arg.val = 0;

	// Start SSI Channel
	ioctl( Id, IOCTL_CPSSSI_START, &arg );
	return SSI_ERR_SUCCESS;
}

/**
	@~English
	@brief SSI Library get to convert the start busy status.
	@param Id : Device ID
	@param SsiStatus : status
	@par This function uses after the ContecCpsSsiStart function.
	@note It is easier to use the ContecCpsSsiSingleTemperature function than using this function.
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief ビジーかどうかを取得する関数
	@param Id : デバイスID
	@param SsiStatus : ステータス
	@par この関数は ContecCpsSsiStartを実行してから使用してください。
	@note この関数を使用するより、ContecCpsSsiSingleTemperature関数を使用する方が簡単です。
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsIsConversionStartBusyStatus( short Id, unsigned long *SsiStatus )
{
	struct cpsssi_ioctl_arg	arg;

	ioctl( Id, IOCTL_CPSSSI_STARTBUSYSTATUS, &arg );

	*SsiStatus = arg.val;

	return SSI_ERR_SUCCESS;
}

/**
	@~English
	@brief SSI Library get the data by the channel.
	@param Id : Device ID
	@param SsiChannel : Channel Number
	@param SsiData : The get data of channel.
	@par This function uses after the no conversion busy bit. (Check the ContecCpsIsConversionStartBusyStatus function )
	@note It is easier to use the ContecCpsSsiSingleTemperature function than using this function.
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief チャネルのデータを取得する関数
	@param Id : デバイスID
	@param SsiChannel : チャネル番号
	@param SsiData : チャネルのデータ
	@par この関数はビジービットが解除したあと、使用してください。( ContecCpsIsConversionStartBusyStatusで確認してください)
	@note この関数を使用するより、ContecCpsSsiSingleTemperature関数を使用する方が簡単です。
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiGetData( short Id, short SsiChannel, long *SsiData )
{
	struct cpsssi_ioctl_arg	arg;

	arg.ch = SsiChannel;
	arg.val = 0;

	ioctl( Id, IOCTL_CPSSSI_INDATA, &arg );
	*SsiData = (long)( arg.val );

	return SSI_ERR_SUCCESS;
}

/**
	@~English
	@brief SSI Library start and get the data.
	@param Id : Device ID
	@param SsiChannel : Channel Number
	@param SsiData : The get data of channel.
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief スタートしてからデータを取得する関数
	@param Id : デバイスID
	@param SsiChannel : チャネル番号
	@param SsiData : チャネルのデータ
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiSingle( short Id, short SsiChannel, long *SsiData )
{

	struct cpsssi_ioctl_arg	arg;
	long status = 0;

	arg.ch = SsiChannel;
	arg.val = 0;

	// Start SSI Channel
	ioctl( Id, IOCTL_CPSSSI_START, &arg );
	// SSI Status Check
	do{
		ContecCpsIsConversionStartBusyStatus( Id, &status );
	}while( !(status & 0x40) );
	// Get SSI Data
	ioctl( Id, IOCTL_CPSSSI_INDATA, &arg );
	*SsiData = (long)( arg.val );

	return SSI_ERR_SUCCESS;
}

/**
	@~English
	@brief SSI Library start and get the temperature　data.
	@param Id : Device ID
	@param SsiChannel : Channel Number
	@param SsiTempData : The get temperature　data of channel.
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief スタートしてから温度データを取得する関数
	@param Id : デバイスID
	@param SsiChannel : チャネル番号
	@param SsiTempData : チャネルの温度データ
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiSingleTemperature( short Id, short SsiChannel, double *SsiTempData )
{
	long val;
	double tmpVal;

	ContecCpsSsiSingle( Id, SsiChannel, &val );

	tmpVal = (double) ( ( val & 0x007FFFFF ) - (val & 0x00800000 ) );

	*SsiTempData = (tmpVal / 1024.0); 

	return SSI_ERR_SUCCESS;

}

/**
	@~English
	@brief SSI Library start and calculate the resistance data to get the temperature　data.
	@param Id : Device ID
	@param SsiChannel : Channel Number
	@param SsiResistance : The resistance　data of channel.
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief スタートしてから温度を取得し、近似した抵抗値を算出する関数
	@param Id : デバイスID
	@param SsiChannel : チャネル番号
	@param SsiResistance : チャネルの抵抗データ
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiSingleResistance( short Id, short SsiChannel, double *SsiResistance )
{

	double tmpVal;

	double R0 = 100.0;
	double alpha;
	double a;
	double b;
	double c;
	int iJpt;

	ContecCpsSsiSingleTemperature( Id, SsiChannel, &tmpVal );

	ContecCpsSsiGetChannel( Id, SsiChannel, NULL, &iJpt );

	switch( iJpt ){

	case	SSI_CHANNEL_JPT	:
		alpha = 0.003916;
		a = 3.97390 * pow( 10.0, -3.0 );
		b = -5.870 * pow( 10.0, -7.0 );
		c = -4.40 * pow( 10.0, -12.0 );
		break;
	case	SSI_CHANNEL_PT	:
	default:
		alpha = 0.00385;
		a = 3.9083 * pow( 10.0, -3.0 );
		b = -5.775 * pow( 10.0, -7.0 );
		c = -4.183 * pow( 10.0, -12.0 );
		break;
	}

	if( tmpVal > 0.0 ){
		*SsiResistance = R0 * ( 1.0 + a * tmpVal + b * pow( tmpVal, 2.0 ) );
	}else if( tmpVal < 0.0 ){
		*SsiResistance = R0 * ( 1.0 + a * tmpVal + b * pow( tmpVal, 2.0 ) + ( tmpVal -100.0 ) * c * pow( tmpVal, 3.0 ) );
	}else{
		*SsiResistance = R0;
	}

	return SSI_ERR_SUCCESS;
}

/**
	@~English
	@brief SSI Library calculate the offset data from double data to unsigned char　data.
	@param data : offset data of double type
	@param pw : bit shift of double type
	@par This function is CPS-SSI-4P only, and internal function.
	@note The offset data returns from 3.96875 to -4.
	@return cVal : temperature offset.
	@~Japanese
	@brief 温度オフセットを符号なしキャラクタ型データへ変換する関数
	@param data : 補正オフセットデータ( double型 )
	@param pw : bit shift用データ
	@par この関数は CPS-SSI-4P専用です。また、内部関数です。
	@note 変換するオフセットデータの 整数部は 3bit(そのうち 上位1ビットは符号bit), 小数部は5ビットです。( +3.96875 ～ -4 )
	@return cVal : 補正温度オフセット
**/
unsigned char _contec_cpsssi_4p_offset_double2uchar( double data , double pw )
{
	unsigned char cVal;

	if( data >= 0.0 )
		cVal = (unsigned char)( data * pw );
	else	// マイナスの場合は 2の補数に変換
		cVal = ((unsigned char)( data * -1 * pw ) ^ 0xFF ) + 1;

	return cVal;

}

/**
	@~English
	@brief SSI Library calculate the gain data from double data to unsigned short　data.
	@param data : gain data of double type
	@param pw : bit shift of double type
	@par This function is CPS-SSI-4P only, and internal function.
	@par The gain data returns from 31.9990234275 to -32.
	@return sVal : temperature gain.
	@~Japanese
	@brief 補正温度ゲインを符号なしショート型データへ変換する関数
	@param data : 補正ゲインデータ( double型 )
	@param pw : bit shift用データ
	@par この関数は CPS-SSI-4P専用です。また、内部関数です。
	@note 変換するゲインデータの 整数部は 6bit(そのうち 上位1ビットは符号bit), 小数部は10ビットです。( +31.9990234275 ～ -32 )
	@return sVal : 補正温度ゲイン
**/
unsigned short _contec_cpsssi_4p_gain_double2ushort( double data , double pw )
{
	unsigned short sVal;

	if( data >= 0.0 )
		sVal = (unsigned short)( data * pw );
	else	// マイナスの場合は 2の補数に変換
		sVal = ((unsigned short)( data * -1 * pw ) ^ 0xFFFF ) + 1;

	return sVal;
}

/**
	@~English
	@brief SSI Library calculate the offset data from unsigned char　data to double　data.
	@param cVal : offset data of unsigned char type
	@param pw : bit shift of double type
	@par This function is CPS-SSI-4P only, and internal function.
	@return offset : temperature offset.
	@~Japanese
	@brief 符号なしキャラクタデータを補正温度オフセットに変換する関数
	@param cVal : オフセットデータ( unsigned char　data )
	@param pw : bit shift用データ
	@par この関数は CPS-SSI-4P専用です。また、内部関数です。
	@return offset : 補正温度オフセット
**/
double _contec_cpsssi_4p_offset_uchar2double( unsigned char cVal, double pw )
{
	double dblVal;

	if( cVal & 0x80 )
		dblVal = 	(-1.0) * (double)( 0x80 - (cVal & 0x7F) ) / pw;
	else
		dblVal = 	(double)( cVal & 0x7F ) / pw;

	return dblVal;
}

/**
	@~English
	@brief SSI Library calculate the gain data from unsigned short to double　data.
	@param data : gain data of unsigned short type
	@param pw : bit shift of double type
	@par This function is CPS-SSI-4P only, and internal function.
	@return dblVal : temperature gain.
	@~Japanese
	@brief 符号なしショート型データを補正温度ゲインへ変換する関数
	@param data : ゲインデータ( unsigned short型 )
	@param pw : bit shift用データ
	@par この関数は CPS-SSI-4P専用です。また、内部関数です。
	@return 補正温度ゲインデータ
**/
double _contec_cpsssi_4p_gain_ushort2double( unsigned short data, double pw )
{
	double dblVal;
	if( data & 0x8000 )
		dblVal = ( -1.0 ) * (double)( 0x8000 - (data & 0x7FFF) ) / pw; 
	else
		dblVal = (double)( data & 0x7FFF ) / pw;

	return dblVal;
}


/**
	@~English
	@brief SSI Library sets temperature　offset. ( unsigned short type )
	@param Id : Device ID
	@param ch : Channel Number
	@param iWire : Wire type ( SSI_CHANNEL_3WIRE or SSI_CHANNEL_4WIRE )
	@param data : The offset data. ( unsigned short data )
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief 温度オフセットを設定する関数 ( 符号なしショート型）
	@param Id : デバイスID
	@param ch : チャネル番号
	@param iWire : 三線式か四線式  ( SSI_CHANNEL_3WIRE か SSI_CHANNEL_4WIRE )
	@param data : オフセットデータ( unsigned short型 )
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiSetCalibrationOffsetToUShort( short Id, unsigned char ch, unsigned int iWire, unsigned short data )
{
	struct cpsssi_ioctl_arg	arg;
	unsigned int iTmpJpt = 0, iTmpWire = 0;

	ContecCpsSsiGetChannel( Id, ch , &iTmpWire, &iTmpJpt );
	ContecCpsSsiSetChannel( Id, ch , iWire, iTmpJpt );

	arg.ch = ch;
	arg.val = data;
	ioctl( Id, IOCTL_CPSSSI_SET_OFFSET, &arg);

	ContecCpsSsiSetChannel( Id, ch , iTmpWire, iTmpJpt );

	return SSI_ERR_SUCCESS;

} 

/**
	@~English
	@brief SSI Library sets temperature　offset.( double type )
	@param Id : Device ID
	@param ch : Channel Number
	@param iWire : Wire type ( SSI_CHANNEL_3WIRE or SSI_CHANNEL_4WIRE )
	@param data : The offset data. ( double data )
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief 温度オフセットを設定する関数(浮動小数点型)
	@param Id : デバイスID
	@param ch : チャネル番号
	@param iWire : 三線式か四線式  ( SSI_CHANNEL_3WIRE か SSI_CHANNEL_4WIRE )
	@param data : オフセットデータ( double型 )
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiSetCalibrationOffset( short Id, unsigned char ch, unsigned int iWire, double data )
{
	unsigned char cVal = 0;
	
	cVal = _contec_cpsssi_4p_offset_double2uchar( data , 32.0 );

	return ContecCpsSsiSetCalibrationOffsetToUShort( Id, ch, iWire, cVal );

} 

/**
	@~English
	@brief SSI Library gets temperature　offset. ( unsigned short type )
	@param Id : Device ID
	@param ch : Channel Number
	@param iWire : Wire type ( SSI_CHANNEL_3WIRE or SSI_CHANNEL_4WIRE )
	@param data : The offset data. ( unsigned short data )
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief 温度オフセットを取得する関数 ( 符号なしショート型）
	@param Id : デバイスID
	@param ch : チャネル番号
	@param iWire : 三線式か四線式  ( SSI_CHANNEL_3WIRE か SSI_CHANNEL_4WIRE )
	@param data : オフセットデータ( unsigned short型 )
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiGetCalibrationOffsetToUShort( short Id, unsigned char ch, unsigned int iWire, unsigned short *data )
{
	struct cpsssi_ioctl_arg	arg;
	unsigned int iTmpJpt = 0, iTmpWire = 0;

	ContecCpsSsiGetChannel( Id, ch , &iTmpWire, &iTmpJpt );
	ContecCpsSsiSetChannel( Id, ch , iWire, iTmpJpt );

	arg.ch = ch;
	ioctl( Id, IOCTL_CPSSSI_GET_OFFSET, &arg);
	*data = 	arg.val;

	ContecCpsSsiSetChannel( Id, ch , iTmpWire, iTmpJpt );

	return SSI_ERR_SUCCESS;
} 

/**
	@~English
	@brief SSI Library gets temperature　offset.( double type )
	@param Id : Device ID
	@param ch : Channel Number
	@param iWire : Wire type ( SSI_CHANNEL_3WIRE or SSI_CHANNEL_4WIRE )
	@param data : The offset data. ( double data )
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief 温度オフセットを取得する関数(浮動小数点型)
	@param Id : デバイスID
	@param ch : チャネル番号
	@param iWire : 三線式か四線式  ( SSI_CHANNEL_3WIRE か SSI_CHANNEL_4WIRE )
	@param data : オフセットデータ( double型 )
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiGetCalibrationOffset( short Id, unsigned char ch, unsigned int iWire, double *data )
{
	unsigned short usData = 0;
	unsigned long lRet = 0;

	lRet = ContecCpsSsiGetCalibrationOffsetToUShort( Id, ch, iWire, &usData );
	*data = _contec_cpsssi_4p_offset_uchar2double((unsigned char)usData , 32.0 );

	return lRet;
} 

/**
	@~English
	@brief SSI Library sets temperature　calibration　gain. ( double type )
	@param Id : Device ID
	@param data : The gain data. ( double data )
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief 温度補正ゲインを設定する関数 ( 浮動小数点型）
	@param Id : デバイスID
	@param data : ゲインデータ( double 型 )
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiSetCalibrationGain( short Id, double data )
{
	return ContecCpsSsiSetSenseResistor( Id, (data + SSI_4P_SENSE_DEFAULT_VALUE) );
} 

/**
	@~English
	@brief SSI Library sets temperature　calibration　gain. ( unsigned short type )
	@param Id : Device ID
	@param data : The gain data. ( unsigned short data )
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief 温度補正ゲインを設定する関数 ( 符号なしショート型）
	@param Id : デバイスID
	@param data : ゲインデータ( 符号なしショート型 )
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiSetCalibrationGainToUShort( short Id, unsigned short data )
{
	double sense;

	sense = _contec_cpsssi_4p_gain_ushort2double( data, pow( 2.0, 10.0 ) );

	return ContecCpsSsiSetSenseResistor( Id, (sense + SSI_4P_SENSE_DEFAULT_VALUE) );

} 

/**
	@~English
	@brief SSI Library gets temperature　calibration　gain. ( double type )
	@param Id : Device ID
	@param data : The gain data. ( double data )
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief 温度補正ゲインを取得する関数 ( 浮動小数点型）
	@param Id : デバイスID
	@param data : ゲインデータ( double 型 )
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiGetCalibrationGain( short Id, double *data )
{
	double tmpData = 0.0;
	unsigned long lRet;

	lRet = ContecCpsSsiGetSenseResistor( Id, &tmpData );
	*data = ( tmpData - SSI_4P_SENSE_DEFAULT_VALUE );
	return lRet;
} 

/**
	@~English
	@brief SSI Library the rom is saved by temperature　calibration　gain. ( unsigned short type )
	@param Id : Device ID
	@param value : The gain data. ( unsigned short data )
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief ROMに 温度ゲインを書き込む関数 ( 符号なしショート型）
	@param Id : デバイスID
	@param value : ゲインデータ( 符号なしショート型 )
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiWriteCalibrationGainToUShort( short Id, unsigned short value )
{
	struct cpsssi_ioctl_arg	arg;

	arg.ch = 0;
	arg.val = value;
	ioctl( Id, IOCTL_CPSSSI_WRITE_EEPROM_SSI, &arg);

	return SSI_ERR_SUCCESS;	

}

/**
	@~English
	@brief SSI Library the rom is saved by temperature　calibration　gain. ( double type )
	@param Id : Device ID
	@param dblVal : The gain data. ( double data )
	@warning If you overwrite the gain value, you own risk.
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief ROMに 温度ゲインを書き込む関数 ( 浮動小数点型）
	@param Id : デバイスID
	@param dblVal : ゲインデータ( 浮動小数点型 )
	@warning もしROMにゲインの上書きを行った場合、自己責任になります。
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiWriteCalibrationGain( short Id, double dblVal )
{

	unsigned short value;

	value = _contec_cpsssi_4p_gain_double2ushort( dblVal , pow( 2.0, 10.0 ) );

	return ContecCpsSsiWriteCalibrationGainToUShort( Id, value );

}

/**
	@~English
	@brief SSI Library ROM is written by temperature　offset.(unsigned char type )
	@param Id : Device ID
	@param ch : Channel Number
	@param cWire3Val : The offset data of 3-Wire. ( unsigned char data )
	@param cWire4Val : The offset data of 4-Wire. ( unsigned char data )
	@warning If you overwrite the offset value, you own risk.
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief 温度オフセットをROMに書き込む関数( 符号なしキャラクタ型)
	@param Id : デバイスID
	@param ch : チャネル番号
	@param cWire3Val : 三線式オフセットデータ( unsigned char型 )
	@param cWire4Val : 四線式オフセットデータ( unsigned char型 )
	@warning もしROMにオフセットの上書きを行った場合、自己責任になります。
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiWriteCalibrationOffsetToUChar( short Id, unsigned char ch, unsigned char cWire3Val, unsigned char cWire4Val )
{
	struct cpsssi_ioctl_arg	arg;

	arg.ch = ch + 1;
	arg.val = ( cWire3Val << 8 )  | cWire4Val;
	ioctl( Id, IOCTL_CPSSSI_WRITE_EEPROM_SSI, &arg);

	return SSI_ERR_SUCCESS;	
}

/**
	@~English
	@brief SSI Library ROM is written by temperature　offset.(double type )
	@param Id : Device ID
	@param ch : Channel Number
	@param wire3Value : The offset data of 3-Wire. ( double data )
	@param wire4Value : The offset data of 4-Wire. ( double data )
	@attention If rom data write before clear ROM data.(Call the ContecCpsSsiClearCalibrationData funtion)
	@warning If you write the offset value, you own risk.
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief 温度オフセットをROMに書き込む関数( 符号小数点型)
	@param Id : デバイスID
	@param ch : チャネル番号
	@param wire3Value : 三線式オフセットデータ( double型 )
	@param wire4Value : 四線式オフセットデータ( double型 )
	@attention 書き込みを行う場合、ContecCpsSsiClearCalibrationDataにてROMデータを消去する必要があります。
	@warning もしROMにオフセットの書き込みを行った場合、自己責任になります。
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiWriteCalibrationOffset( short Id, unsigned char ch, double wire3Value, double wire4Value )
{
	unsigned char cWire3Val = 0;
	unsigned char cWire4Val = 0;

	cWire3Val = _contec_cpsssi_4p_offset_double2uchar( wire3Value , 32.0 );
	cWire4Val = _contec_cpsssi_4p_offset_double2uchar( wire4Value , 32.0 );

	return ContecCpsSsiWriteCalibrationOffsetToUChar( Id, ch, cWire3Val, cWire4Val );
}

/**
	@~English
	@brief SSI Library the rom is read by temperature　calibration　gain. ( double type )
	@param Id : Device ID
	@param dblVal : The gain data. ( double data )
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief ROMに 温度ゲインを読み出す関数 ( 浮動小数点型）
	@param Id : デバイスID
	@param dblVal : ゲインデータ( 浮動小数点型 )
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiReadCalibrationGain( short Id, double *dblVal )
{
	int aisel = 0;
	struct cpsssi_ioctl_arg	arg;
	unsigned short value;
	arg.ch = 0;

	ioctl( Id, IOCTL_CPSSSI_READ_EEPROM_SSI, &arg);

	value = arg.val;

	*dblVal = _contec_cpsssi_4p_gain_ushort2double(value ,pow( 2.0, 10.0 ) );

	return SSI_ERR_SUCCESS;	

}

/**
	@~English
	@brief SSI Library ROM is read by temperature　offset.(　double type )
	@param Id : Device ID
	@param ch : Channel Number
	@param wire3Value : The offset data of 3-Wire. ( double data )
	@param wire4Value : The offset data of 4-Wire. ( double data )
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief 温度オフセットをROMに読み出す関数( 浮動小数点型)
	@param Id : デバイスID
	@param ch : チャネル番号
	@param wire3Value : 三線式オフセットデータ( double型 )
	@param wire4Value : 四線式オフセットデータ( double型 )
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiReadCalibrationOffset( short Id, unsigned char ch, double *wire3Value, double *wire4Value )
{
	unsigned char tmpVal;
	double *dblVal;
	unsigned int cnt;
	struct cpsssi_ioctl_arg	arg;

	arg.ch = ch + 1;

	ioctl( Id, IOCTL_CPSSSI_READ_EEPROM_SSI, &arg);

	for( cnt = 0; cnt < 2; cnt ++ ){

		switch( cnt ){
		case 0: dblVal = wire4Value; break;
		case 1: dblVal = wire3Value; break;
		}

		if( dblVal == NULL ) continue;

		tmpVal = (arg.val & (0xFF << (8 * cnt) ) ) >> (8 * cnt);
		
		*dblVal = _contec_cpsssi_4p_offset_uchar2double( tmpVal, 32.0 );
	}

	return SSI_ERR_SUCCESS;	

}

/**
	@~English
	@brief SSI Library ROM , FPGA calibrate data clear function.
	@param Id : Device ID
	@param iClear : clear flag
	@warning If you clear the ROM data, you own risk.
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief キャリブレーションデータを消去する関数
	@param Id : デバイスID
	@param iClear : クリア用フラグ
	@warning もしROMに保存していたデータをクリアする場合、自己責任になります。
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiClearCalibrationData( short Id, int iClear )
{
	int cnt;

	if( iClear & CPSSSI_CALIBRATION_CLEAR_RAM ){
		//all Clear
		ContecCpsSsiSetCalibrationGain( Id, 0.0 );

		for( cnt = 0; cnt < 4 ; cnt ++ ){
			ContecCpsSsiSetCalibrationOffset( Id, cnt , SSI_CHANNEL_3WIRE, 0.0);
			ContecCpsSsiSetCalibrationOffset( Id, cnt , SSI_CHANNEL_4WIRE, 0.0);
		}
	}
	if( iClear & CPSSSI_CALIBRATION_CLEAR_ROM ){
		//FPGA ROM CLEAR
		ioctl( Id, IOCTL_CPSSSI_CLEAR_EEPROM, NULL);
	}

	return SSI_ERR_SUCCESS;	

}

/* Direct Input / Output (Debug) */

/**
	@~English
	@brief SSI Library the register of command address read data.(1byte)
	@param Id : Device ID
	@param addr : Address
	@param value : Values
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief デバイスのCOMMANDアドレスレジスタを読み出す関数。(1byte)
	@param Id : デバイスID
	@param addr : アドレス
	@param value : 値
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiCommandInp( short Id, unsigned long addr, unsigned char *value )
{

	struct cpsssi_direct_command_arg arg;

	arg.addr = addr;
	ioctl( Id, IOCTL_CPSSSI_DIRECT_COMMAND_INPUT, arg );
	*value = (unsigned char)arg.val;

	return SSI_ERR_SUCCESS;
}


/**
	@~English
	@brief SSI Library the register of command address write data.(1byte)
	@param Id : Device ID
	@param addr : Address
	@param value : Values
	@return Success: SSI_ERR_SUCCESS
	@~Japanese
	@brief デバイスのCOMMANDアドレスレジスタへ書き出す関数。(1byte)
	@param Id : デバイスID
	@param addr : アドレス
	@param value : 値
	@return 成功: SSI_ERR_SUCCESS
**/
unsigned long ContecCpsSsiCommandOutp( short Id, unsigned long addr, unsigned char value )
{
	struct cpsssi_direct_command_arg arg;

	arg.addr = addr;
	arg.val = (unsigned long)value;
	ioctl( Id, IOCTL_CPSSSI_DIRECT_COMMAND_OUTPUT, arg );

	return 	SSI_ERR_SUCCESS;
}
