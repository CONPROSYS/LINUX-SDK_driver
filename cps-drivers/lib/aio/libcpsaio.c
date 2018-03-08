/*
 *  Lib for CONTEC CONPROSYS Analog I/O (CPS-AIO) Series.
 *
 *  Copyright (C) 2015 Syunsuke Okamoto.<okamoto@contec.jp>
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
   <http://www.gnu.org/licenses/>.  
*
*
* Change Log:
*  2016-01-20 : <Ver.1.0.1> 
                (1) Add ContecCpsAioSetAiCalibrationData function.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <math.h> //Ver.1.0.2
#include <malloc.h> // Ver.1.0.2
#include "cpsaio.h"

#ifdef CONFIG_CONPROSYS_SDK
 #include "../include/libcpsaio.h"
#else
 #include "libcpsaio.h"
#endif

#define CONTEC_CPSAIO_LIB_VERSION	"1.0.9"

typedef struct __contec_cps_aio_int_callback__
{
	PCONTEC_CPS_AIO_INT_CALLBACK func;
	CONTEC_CPS_AIO_INT_CALLBACK_DATA data;
}CONTEC_CPS_AIO_INT_CALLBACK_LIST, *PCONTEC_CPS_AIO_INT_CALLBACK_LIST;

CONTEC_CPS_AIO_INT_CALLBACK_LIST contec_cps_aio_cb_list[CPS_DEVICE_MAX_NUM];

/**
	@~English
	@brief callback process function.(The running process is called to receive user's signal.)
	@param signo : signal number
	@~Japanese
	@param signo : シグナルナンバー
	@brief コールバック内部関数　（ユーザシグナル受信で動作）
**/
void _contec_cpsaio_signal_proc( int signo )
{
	int cnt;
	if( signo == SIGUSR2 ){
		for( cnt = 0; cnt < CPS_DEVICE_MAX_NUM ; cnt ++){
			if( contec_cps_aio_cb_list[cnt].func != (PCONTEC_CPS_AIO_INT_CALLBACK)NULL ){
				
				contec_cps_aio_cb_list[cnt].func(
					contec_cps_aio_cb_list[cnt].data.id,
					AIOM_INTERRUPT,
					contec_cps_aio_cb_list[cnt].data.wParam,
					contec_cps_aio_cb_list[cnt].data.lParam,
					contec_cps_aio_cb_list[cnt].data.Param
				);
			}
		}
	}
}
/**
	@~English
	@brief set exchange function.
	@param Id : Device ID
	@param isOutput : "Analog Input" or "Analog Output" Flag
	@param isMulti : Get data type "Single" or "Multi" Channel.
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief AI or AOのデータを単数チャネルで取得するか　複数チャネル分取得するかを設定する関数.
	@param Id : デバイスID
	@param isOutput : "アナログ入力"か"アナログ出力"か
	@param isMulti : "Single"チャネルか "Multi"チャネルか
	@return 成功:  AIO_ERR_SUCCESS
**/
unsigned long _contec_cpsaio_set_exchange( short Id, unsigned char isOutput, unsigned char isMulti )
{
	struct cpsaio_ioctl_arg	arg;

	/* Single or Multi */
	if( isMulti )	arg.val = 1;
	else	arg.val = 0;

	/* Ai or Ao */
	if( isOutput )
		ioctl( Id, IOCTL_CPSAIO_SETTRANSFER_MODE_AO, &arg );
	else
		ioctl( Id, IOCTL_CPSAIO_SETTRANSFER_MODE_AI, &arg );

	return AIO_ERR_SUCCESS;

}

/**
	@~English
	@brief Check Memory Flag function.
	@param Id : Device ID
	@param isCheckMemFlag : Memory Status Bit Flag
	@note Added Version 1.0.8.
	@par This is internal function.
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief メモリのフラグを確認する関数
	@param Id : デバイスID
	@param isCheckMemFlag : メモリStatusビット確認用フラグ
	@note Ver1.0.8より追加
	@par この関数は内部関数です。
	@return 成功:  AIO_ERR_SUCCESS
**/
unsigned long _contec_cpsaio_check_memstatus( short Id, unsigned char isCheckMemFlag )
{
	struct cpsaio_ioctl_arg	arg;
	unsigned long count;

	count = 0;
	do{
		usleep( 1 );
		ioctl( Id, IOCTL_CPSAIO_GETMEMSTATUS , &arg);
		if( count >= 1000 ) return 1;
		count ++;
	}while(!( arg.val & isCheckMemFlag ) );

	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library Initialize.
	@param DeviceName : Device node name ( cpsaioX )
	@param Id : Device Access Id
	@return Success: AIO_ERR_SUCCESS, Failed: otherwise AIO_ERR_SUCCESS
	@~Japanese
	@brief 初期化関数.
	@param DeviceName : デバイスノード名  ( cpsaioX )
	@param Id : デバイスID
	@return 成功: AIO_ERR_SUCCESS, 失敗: AIO_ERR_SUCCESS 以外

**/
unsigned long ContecCpsAioInit( char *DeviceName, short *Id )
{
	// open
	char Name[32];
	struct cpsaio_ioctl_arg	arg;
	unsigned char g = 0, o = 0;
	strcpy(Name, "/dev/");
	strcat(Name, DeviceName);

	*Id = open( Name, O_RDWR );

	if( *Id <= 0 ) return AIO_ERR_DLL_CREATE_FILE;

	ioctl( *Id, IOCTL_CPSAIO_INIT, &arg);

	ContecCpsAioSetEcuSignal(*Id, AIOECU_DEST_AI_CLK, AIOECU_SRC_AI_CLK );
	ContecCpsAioSetEcuSignal(*Id, AIOECU_DEST_AI_START, AIOECU_SRC_START );
	ContecCpsAioSetEcuSignal(*Id, AIOECU_DEST_AI_STOP, AIOECU_SRC_AI_STOP );

	ContecCpsAioSetEcuSignal(*Id, AIOECU_DEST_AO_CLK, AIOECU_SRC_AO_CLK );
	ContecCpsAioSetEcuSignal(*Id, AIOECU_DEST_AO_START, AIOECU_SRC_START );
	ContecCpsAioSetEcuSignal(*Id, AIOECU_DEST_AO_STOP, AIOECU_SRC_AO_STOP_RING );


//	ContecCpsAioReadAiCalibrationData(*Id, 0, &g, &o);
//	ContecCpsAioSetAiCalibrationData(*Id, CPSAIO_AI_CALIBRATION_SELECT_OFFSET, 0, CPSAIO_AI_CALIBRATION_RANGE_PM10,  o);
//	ContecCpsAioSetAiCalibrationData(*Id, CPSAIO_AI_CALIBRATION_SELECT_GAIN, 0, CPSAIO_AI_CALIBRATION_RANGE_PM10,  g);
	return AIO_ERR_SUCCESS;

}

/**
	@~English
	@brief AIO Library Exit.
	@param Id : Device ID
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief 終了関数.
	@param Id : デバイスID
	@return 成功:  AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioExit( short Id )
{
	struct cpsaio_ioctl_arg	arg;
	arg.val = 0;

	ioctl( Id, IOCTL_CPSAIO_EXIT, &arg );
	// close
	close( Id );
	return AIO_ERR_SUCCESS;

}

/**
	@~English
	@brief AIO Library output from ErrorNumber to ErrorString.
	@param code : Error Code
	@param Str : Error String
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief エラー文字列出力関数（未実装）
	@param code : エラーコード
	@param Str : エラー文字列
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioGetErrorStrings( unsigned long code, char *Str )
{

	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library query Device.
	@param Index : Device ID
	@param DeviceName : Device Node Name ( cpsaioX )
	@param Device : Device Name ( CPS-AI-1608LI , etc )
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief クエリデバイス関数
	@param Index : デバイスID
	@param DeviceName : デバイスノード名 ( cpsaioX )
	@param Device : デバイス型式名 (  CPS-AIO-1608LIなど )
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioQueryDeviceName( short Index, char *DeviceName, char *Device )
{
	struct cpsaio_ioctl_string_arg	arg;
	int len;

	char tmpDevName[16];
	char baseDeviceName[16]="cpsaio";
	char strNum[2]={0};
	int findNum=0, cnt, ret;

	short tmpId = 0;

	for(cnt = 0;cnt < CPS_DEVICE_MAX_NUM ; cnt ++ ){
		sprintf(tmpDevName,"%s%x",baseDeviceName, cnt);
		ret = ContecCpsAioInit(tmpDevName, &tmpId);

		if( ret == 0 ){
			ioctl(tmpId, IOCTL_CPSAIO_GET_DEVICE_NAME, &arg);
			ContecCpsAioExit(tmpId);

			if(findNum == Index){
				sprintf(DeviceName,"%s",tmpDevName);
				sprintf(Device,"%s", arg.str);
				return AIO_ERR_SUCCESS;
			}else{
				findNum ++;
			}
			memset(&tmpDevName,0x00, 16);
			memset(&arg.str, 0x00, sizeof(arg.str)/sizeof(arg.str[0]));

		}
	}
	
	return AIO_ERR_INFO_NOT_FIND_DEVICE;
}

//---- Ai/Ao Get Resolution function ------------------
/**
	@~English
	@brief DIO Library get analog input resolution.
	@param Id : Device ID
	@param AiResolution : Resolution of AnalogInput.
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ入力の分解能を取得します
	@param Id : デバイスID
	@param AiResolution : アナログ入力の分解能
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioGetAiResolution( short Id, unsigned short *AiResolution )
{
	struct cpsaio_ioctl_arg	arg;

	/* Get resolution */
	arg.inout = CPS_AIO_INOUT_AI;
	ioctl( Id, IOCTL_CPSAIO_GETRESOLUTION, &arg );

	*AiResolution = (unsigned short)arg.val;

	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief DIO Library get analog output resolution.
	@param Id : Device ID
	@param AoResolution : Resolution of Analog Output.
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ出力の分解能を取得します
	@param Id : デバイスID
	@param AoResolution : アナログ出力の分解能
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioGetAoResolution( short Id, unsigned short *AoResolution )
{
	struct cpsaio_ioctl_arg	arg;

	/* Get resolution */
	arg.inout = CPS_AIO_INOUT_AO;
	ioctl( Id, IOCTL_CPSAIO_GETRESOLUTION, &arg );

	*AoResolution = (unsigned short)arg.val;

	return AIO_ERR_SUCCESS;
}

//---- Ai Channel function ----------------------------
/**
	@~English
	@brief AIO Library get maximum analog input channels.
	@param Id : Device ID
	@param AiMaxChannels : analog input max channel number.
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスの最大の入力チャネル数を取得します
	@param Id : デバイスID
	@param AiMaxChannels :アナログ入力の最大チャネル数
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioGetAiMaxChannels( short Id, short *AiMaxChannels ){
	struct cpsaio_ioctl_arg	arg;

	arg.inout = CPS_AIO_INOUT_AI;
	ioctl( Id, IOCTL_CPSAIO_GETMAXCHANNEL, &arg );
	*AiMaxChannels = arg.val;

	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library gets driver and library version.
	@param Id : Device ID
	@param libVer : library version
	@param drvVer : driver version
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログデバイスのドライバとライブラリのバージョン文字列を取得します。
	@param Id : デバイスID
	@param libVer : ライブラリバージョン
	@param drvVer : ドライババージョン
	@note Ver.1.0.8 Change from cpsaio_ioctl_arg to cpsaio_ioctl_string_arg.
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioGetVersion( short Id , unsigned char libVer[] , unsigned char drvVer[] )
{

	struct cpsaio_ioctl_string_arg	arg;
	int len;


	ioctl( Id, IOCTL_CPSAIO_GET_DRIVER_VERSION, &arg );

	len = sizeof( arg.str ) / sizeof( arg.str[0] );
	memcpy(drvVer, arg.str, len);
//	strcpy_s(drvVer, arg.str);

	len = sizeof( CONTEC_CPSAIO_LIB_VERSION ) /sizeof( unsigned char );
	memcpy(libVer, CONTEC_CPSAIO_LIB_VERSION, len);

	return AIO_ERR_SUCCESS;

}

/**
	@~English
	@brief AIO Library set analog input channel.
	@param Id : Device ID
	@param AiChannels : analog input channel number.
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスを計測する入力チャネルを設定します
	@param Id : デバイスID
	@param AiChannels :　アナログ入力のチャネル番号
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioSetAiChannels( short Id, short AiChannels )
{
	struct cpsaio_ioctl_arg	arg;

	_contec_cpsaio_set_exchange( Id, 0, 1 );

	/* Set Channel */
	arg.val = AiChannels - 1;
	ioctl( Id, IOCTL_CPSAIO_SETCHANNEL_AI, &arg );

	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library get analog input channel.
	@param Id : Device ID
	@param AiChannels : analog input channel number.
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスを計測する入力チャネルを取得します
	@param Id : デバイスID
	@param AiChannels :　アナログ入力のチャネル番号
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioGetAiChannels( short Id, short *AiChannels )
{
	struct cpsaio_ioctl_arg	arg;

	/* Get Channel */
	ioctl( Id, IOCTL_CPSAIO_GETCHANNEL_AI, &arg );

	*AiChannels = (short)(arg.val + 1);

	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library set analog input sampling clock.( set time micro second order.)
	@param Id : Device ID
	@param AiSamplingClock : analog input channel number.
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ入力のサンプリング時間を設定します。単位は usecです。
	@param Id : デバイスID
	@param AiSamplingClock :　サンプリング時間
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioSetAiSamplingClock( short Id, double AiSamplingClock )
{
	struct cpsaio_ioctl_arg	arg;

	/* Set Channel */
	arg.val = (unsigned long) ( (AiSamplingClock * 1000.0 / 25.0) - 1 );
	ioctl( Id, IOCTL_CPSAIO_SET_CLOCK_AI, &arg );

	return AIO_ERR_SUCCESS;

}

/**
	@~English
	@brief AIO Library get analog input sampling clock.( set time micro second order.)
	@param Id : Device ID
	@param AiSamplingClock : analog input sampling clock.
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ入力のサンプリング時間を取得します。単位は usecです。
	@param Id : デバイスID
	@param AiSamplingClock :　サンプリング時間
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioGetAiSamplingClock( short Id, double *AiSamplingClock )
{
	struct cpsaio_ioctl_arg	arg;

	/* Get Channel */
	ioctl( Id, IOCTL_CPSAIO_GET_CLOCK_AI, &arg );

	*AiSamplingClock = (double) ( (arg.val + 1.0 ) * 25.0 ) / 1000.0;

	return AIO_ERR_SUCCESS;

}

/**
	@~English
	@brief AIO Library set analog input sampling number.
	@param Id : Device ID
	@param AiSamplingTimes : analog input sample number.
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief 入力サンプリング数の設定を行います。
	@param Id : デバイスID
	@param AiSamplingTimes :　入力サンプリング数
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioSetAiEventSamplingTimes( short Id, unsigned long AiSamplingTimes )
{
	struct cpsaio_ioctl_arg	arg;

	/* Set Sampling number( before Trigger ) */
	arg.val = AiSamplingTimes - 1;
	ioctl( Id, IOCTL_CPSAIO_SET_SAMPNUM_AI, &arg );

	return AIO_ERR_SUCCESS;

}
/**
	@~English
	@brief AIO Library get analog input sampling number.
	@param Id : Device ID
	@param AiSamplingTimes : analog input sample number.
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief サンプリング数の取得を行います。
	@param Id : デバイスID
	@param AiSamplingTimes :　サンプリング数
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioGetAiEventSamplingTimes( short Id, unsigned long *AiSamplingTimes )
{
	struct cpsaio_ioctl_arg	arg;

	/* Get Sampling number( before Trigger ) */
	ioctl( Id, IOCTL_CPSAIO_GET_SAMPNUM_AI, &arg );
	*AiSamplingTimes = arg.val + 1;

	return AIO_ERR_SUCCESS;

}

//--- Running Functions ------------------------
/**
	@~English
	@brief AIO Library start analog input sampling.
	@param Id : Device ID
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ入力サンプリングの開始。
	@param Id : デバイスID
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioStartAi( short Id )
{
	struct cpsaio_ioctl_arg	arg;
	unsigned long count = 0;

	ioctl( Id, IOCTL_CPSAIO_START_AI, 0 );

	do{
		usleep( 1 );
		ioctl( Id, IOCTL_CPSAIO_INSTATUS, &arg );
		if( count >= 1000 ) return 1;
		count ++;
	}while( arg.val & CPS_AIO_AI_STATUS_START_DISABLE );

	//softtrigger only out pulse start
	ioctl( Id, IOCTL_CPSAIO_SET_OUTPULSE0, 0 );

	count = 0;
	do{
		usleep( 1 );
		ioctl( Id, IOCTL_CPSAIO_GET_INTERRUPT_FLAG_AI , &arg);
		if( count >= 1000 ) return 2;
		count ++;
	}while(!( arg.val & CPS_AIO_AI_STATUS_MOTION_END ) );

////////////////////// Ver.1.0.6
	if( arg.val & CPS_AIO_AI_STATUS_MOTION_END ) {
		arg.val = CPS_AIO_AI_STATUS_MOTION_END;
		ioctl( Id, IOCTL_CPSAIO_SET_INTERRUPT_FLAG_AI , &arg);
	}
////////////////////// Ver.1.0.6


	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library stop analog input sampling.
	@param Id : Device ID
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ入力サンプリングの停止。
	@param Id : デバイスID
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioStopAi( short Id )
{
	ioctl( Id, IOCTL_CPSAIO_STOP_AI, 0 );
	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library get analog input status.
	@param Id : Device ID
	@param AiStatus : status of analog input
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ入力デバイスの状態の取得。
	@param Id : デバイスID
	@param AiStatus : アナログ入力のステータス
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioGetAiStatus( short Id, long *AiStatus )
{
	struct cpsaio_ioctl_arg	arg;

	ioctl( Id, IOCTL_CPSAIO_INSTATUS, &arg );

	*AiStatus = (long)arg.val;

	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library get analog input sampling data.
	@param Id : Device ID
	@param AiSamplingTimes : set the Getting Data length
	@param AiData : get Data of analog input
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ入力デバイスのサンプリングデータを取得。(16bit)
	@param Id : デバイスID
	@param AiSamplingTimes : サンプリング数
	@param AiData : アナログ入力データ配列
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioGetAiSamplingData( short Id, short AiSamplingTimes, long AiData[] )
{

	unsigned char *tmpAiData;
	int cnt;

	tmpAiData = (unsigned char * )malloc( sizeof(unsigned char)* AiSamplingTimes * 2 );

	read( Id, tmpAiData , (size_t)(AiSamplingTimes * 2) );

	for( cnt = 0;cnt < AiSamplingTimes * 2; cnt += 2 ){
		AiData[cnt/2] = (long) ( ( tmpAiData[cnt+1] << 8 ) | tmpAiData[cnt]);
	}

	free(tmpAiData);

	return AIO_ERR_SUCCESS;
}
/**
	@~English
	@brief AIO Library get analog input sampling data.( double type )
	@param Id : Device ID
	@param AiSamplingTimes : set the Getting Data length
	@param AiData : get Data of analog input
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ入力デバイスのサンプリングデータを取得。(浮動小数点型)
	@param Id : デバイスID
	@param AiSamplingTimes : サンプリング数
	@param AiData : アナログ入力データ配列
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioGetAiSamplingDataEx( short Id, short AiSamplingTimes, double AiData[] )
{

	long *tmpAiData;
	unsigned short AiResolution = 0;
	int cnt;
	double dblMin, dblMax;

	tmpAiData = (long*)malloc( sizeof(long) * AiSamplingTimes );

	ContecCpsAioGetAiSamplingData( Id, AiSamplingTimes, tmpAiData );

	ContecCpsAioGetAiResolution( Id, &AiResolution );

/*
	ContecCpsAioGetAiRange( Id, &AiRange ); 
	switch( AiRange ){
		case PM10 :
*/
			dblMax = 10.0; dblMin = -10.0;
/*
			break;
	}
*/	
	for( cnt = 0;cnt < AiSamplingTimes; cnt ++){
		AiData[cnt] =  (double)( tmpAiData[cnt] / pow(2.0,AiResolution) ) *(dblMax - dblMin) + dblMin;
	}
	free(tmpAiData);

}

/**
	@~English
	@brief AIO Library get channel of analog input device sampling one data.( unsigned short type )
	@param Id : Device ID
	@param AiChannel : set the Data Channel
	@param AiData : get Data of analog input
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ入力デバイスの指定したチャネルのサンプリングデータを一回取得。(16bit)
	@param Id : デバイスID
	@param AiChannel : チャネル番号
	@param AiData : アナログ入力データ
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioSingleAi( short Id, short AiChannel, long *AiData )
{

	struct cpsaio_ioctl_arg	arg;

	// Exchange Transfer Mode Single Ai
	_contec_cpsaio_set_exchange( Id, 0, 0 );

	// Set Ai Channel
	arg.val = AiChannel;
	ioctl( Id, IOCTL_CPSAIO_SETCHANNEL_AI, &arg );

	// Set Sampling Number
	ContecCpsAioSetAiEventSamplingTimes( Id, 1 );

	// Ai Start
	ContecCpsAioStartAi( Id );

	// Single Ai の場合、DREフラグをチェックする
	_contec_cpsaio_check_memstatus( Id, CPU_AIO_MEMSTATUS_DRE );

	ioctl( Id, IOCTL_CPSAIO_INDATA, &arg );
	*AiData = (long)( arg.val );

	ContecCpsAioStopAi( Id );

	return AIO_ERR_SUCCESS;
}
/**
	@~English
	@brief AIO Library get channel of analog input device sampling one data.( double type )
	@param Id : Device ID
	@param AiChannel : set the Data Channel
	@param AiData : get Data of analog input
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ入力デバイスの指定したチャネルのサンプリングデータを一回取得。(浮動小数点型)
	@param Id : デバイスID
	@param AiChannel : チャネル番号
	@param AiData : アナログ入力データ
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioSingleAiEx( short Id, short AiChannel, double *AiData )
{

	long tmpAiData = 0;
	unsigned short AiResolution = 0;

	double dblMin, dblMax;

	ContecCpsAioSingleAi( Id, AiChannel, &tmpAiData );

	ContecCpsAioGetAiResolution( Id, &AiResolution );

/*
	ContecCpsAioGetAiRange( Id, &AiRange ); 
	switch( AiRange ){
		case PM10 :
*/
			dblMax = 10.0; dblMin = -10.0;
/*
			break;
	}
*/	
	*AiData =  (double)( tmpAiData / pow(2.0,AiResolution) ) *(dblMax - dblMin) + dblMin;

	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library get multiple channels of analog input device sampling one data.( unsigned short type )
	@param Id : Device ID
	@param AiChannels : set the Data Channel
	@param AiData : get Data array of analog input
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ入力デバイスの複数チャネルのサンプリングデータを一回取得。(16bit)
	@param Id : デバイスID
	@param AiChannels : チャネル数
	@param AiData : アナログ入力データ配列
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioMultiAi( short Id, short AiChannels, long AiData[] )
{

	struct cpsaio_ioctl_arg	arg;
	int cnt;

	ContecCpsAioSetAiChannels(Id, AiChannels );

	// Set Sampling Number
	ContecCpsAioSetAiEventSamplingTimes( Id, 1 );

	// Ai Start
	ContecCpsAioStartAi( Id );

	// Multi Ai の場合、MDREフラグをチェックする
	_contec_cpsaio_check_memstatus( Id, CPU_AIO_MEMSTATUS_MDRE );

	for( cnt = 0;cnt < AiChannels; cnt ++ ){
		ioctl( Id, IOCTL_CPSAIO_INDATA, &arg );
		AiData[cnt] = (long)( arg.val );
	}

	// Ai Stop
	ContecCpsAioStopAi( Id );

	return AIO_ERR_SUCCESS;
}
/**
	@~English
	@brief AIO Library get multiple channels of analog input device sampling one data.( double type )
	@param Id : Device ID
	@param AiChannels : set the Data Channel
	@param AiData : get Data array of analog input
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ入力デバイスの複数チャネルのサンプリングデータを一回取得。(浮動小数点型)
	@param Id : デバイスID
	@param AiChannels : チャネル数
	@param AiData : アナログ入力データ配列
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioMultiAiEx( short Id, short AiChannels, double AiData[] )
{

	long *tmpAiData;
	unsigned short AiResolution = 0;
	int cnt;
	double dblMin, dblMax;

	tmpAiData = (long*)malloc( sizeof(long) * AiChannels );

	ContecCpsAioMultiAi( Id, AiChannels, tmpAiData );

	ContecCpsAioGetAiResolution( Id, &AiResolution );

/*
	ContecCpsAioGetAiRange( Id, &AiRange ); 
	switch( AiRange ){
		case PM10 :
*/
			dblMax = 10.0; dblMin = -10.0;
/*
			break;
	}
*/	
	for( cnt = 0;cnt < AiChannels; cnt ++){
		AiData[cnt] =  (double)( tmpAiData[cnt] / pow(2.0,AiResolution) ) *(dblMax - dblMin) + dblMin;
	}
	free(tmpAiData);

	return AIO_ERR_SUCCESS;
}

/* Ao Channel */
/**
	@~English
	@brief AIO Library get maximum analog output channels.
	@param Id : Device ID
	@param AoMaxChannels : analog output maximum channel number.
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスの最大の出力チャネル数を取得します
	@param Id : デバイスID
	@param AoMaxChannels :アナログ出力の最大チャネル数
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioGetAoMaxChannels( short Id, short *AoMaxChannels ){
	struct cpsaio_ioctl_arg	arg;

	arg.inout = CPS_AIO_INOUT_AO;
	ioctl( Id, IOCTL_CPSAIO_GETMAXCHANNEL, &arg );
	*AoMaxChannels = arg.val;

	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library set analog output channel.
	@param Id : Device ID
	@param AoChannels : analog output channel number.
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスを計測する出力チャネルを設定します
	@param Id : デバイスID
	@param AoChannels :　アナログ出力のチャネル番号
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioSetAoChannels( short Id, short AoChannels )
{
	struct cpsaio_ioctl_arg	arg;

	/* Multi only*/
	_contec_cpsaio_set_exchange( Id, 1, 1 );

	/* Set Channel */
	arg.val = AoChannels - 1;
	ioctl( Id, IOCTL_CPSAIO_SETCHANNEL_AO, &arg );

	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library set analog output sampling number.
	@param Id : Device ID
	@param AoSamplingTimes : analog output sample number.
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief 出力サンプリング数の設定を行います。
	@param Id : デバイスID
	@param AoSamplingTimes :　出力サンプリング数
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioSetAoEventSamplingTimes( short Id, unsigned long AoSamplingTimes )
{
	struct cpsaio_ioctl_arg	arg;

	/* Set Channel */
	arg.val = AoSamplingTimes - 1 ;
	ioctl( Id, IOCTL_CPSAIO_SET_SAMPNUM_AO, &arg );

	return AIO_ERR_SUCCESS;

}


//----- Running Functions ------
/**
	@~English
	@brief AIO Library start analog output sampling.
	@param Id : Device ID
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ出力サンプリングの開始。
	@param Id : デバイスID
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioStartAo( short Id )
{

	struct cpsaio_ioctl_arg	arg;

	ioctl( Id, IOCTL_CPSAIO_START_AO, NULL );

//	do{
//		ioctl( Id, IOCTL_CPSAIO_OUTSTATUS, &arg );;
//	}while( arg.val & 0x10 );

	//softtrigger only out pulse start
	ioctl( Id, IOCTL_CPSAIO_SET_OUTPULSE0, 0 );

	return AIO_ERR_SUCCESS;

}

/**
	@~English
	@brief AIO Library stop analog output sampling.
	@param Id : Device ID
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ出力サンプリングの停止。
	@param Id : デバイスID
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioStopAo( short Id )
{
	ioctl( Id, IOCTL_CPSAIO_STOP_AO, NULL );
	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library get analog output status.
	@param Id : Device ID
	@param AoStatus : status of analog output
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ出力デバイスの状態の取得。
	@param Id : デバイスID
	@param AoStatus : アナログ出力のステータス
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioGetAoStatus( short Id, long *AoStatus )
{
	struct cpsaio_ioctl_arg	arg;

	ioctl( Id, IOCTL_CPSAIO_OUTSTATUS, &arg );

	*AoStatus = (long)arg.val;

	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library set analog output sampling clock.( set time micro second order.)
	@param Id : Device ID
	@param AoSamplingClock : analog output channel number.
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ出力のサンプリング時間を設定します。単位は usecです。
	@param Id : デバイスID
	@param AoSamplingClock :　サンプリング時間
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioSetAoSamplingClock( short Id, double AoSamplingClock )
{
	struct cpsaio_ioctl_arg	arg;

	/* Set Clock */
	arg.val = (unsigned long) (AoSamplingClock * 1000.0 / 25.0) - 1 ;
	ioctl( Id, IOCTL_CPSAIO_SET_CLOCK_AO, &arg );

	return AIO_ERR_SUCCESS;

}

/**
	@~English
	@brief AIO Library get channel of analog output device sampling one data.( unsigned short type )
	@param Id : Device ID
	@param AoChannel : set the Data Channel
	@param AoData : get Data of analog output
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ出力デバイスの指定したチャネルのサンプリングデータを一回取得。(16bit)
	@param Id : デバイスID
	@param AoChannel : チャネル番号
	@param AoData : アナログ出力データ
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioSingleAo( short Id, short AoChannel, long AoData )
{

	struct cpsaio_ioctl_arg	arg;
	long AoStatus = 0;

	// Exchange Transfer Mode Single Ao
	_contec_cpsaio_set_exchange( Id, 1, 0 );

	// Set Ao Channel
	arg.val = AoChannel;
	ioctl( Id, IOCTL_CPSAIO_SETCHANNEL_AO, &arg );

	// Set Sampling Number
	ContecCpsAioSetAoEventSamplingTimes( Id, 1 );

	arg.val = AoData;
	ioctl( Id, IOCTL_CPSAIO_OUTDATA, &arg );

	// Ao Start
	ContecCpsAioStartAo( Id );



	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library get channel of analog output device sampling one data.( double type )
	@param Id : Device ID
	@param AoChannel : set the Data Channel
	@param AoData : get Data of analog output
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ出力デバイスの指定したチャネルのサンプリングデータを一回取得。(浮動小数点型)
	@param Id : デバイスID
	@param AoChannel : チャネル番号
	@param AoData : アナログ出力データ
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioSingleAoEx( short Id, short AoChannel, double AoData )
{

	long tmpAoData = 0;
	unsigned short AoResolution = 0;

	double dblMin, dblMax;

	ContecCpsAioGetAoResolution( Id, &AoResolution );

/*
	ContecCpsAioGetAoRange( Id, &AoRange ); 
	switch( AoRange ){
		case PM10 :
*/
			dblMax = 20.0; dblMin = 0.0;
/*
			break;
	}
*/

	tmpAoData = (long)( (AoData - dblMin) * pow(2.0, AoResolution) / (dblMax - dblMin) );
	
	ContecCpsAioSingleAo( Id, AoChannel, tmpAoData );

	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library get multiple channels of analog input device sampling one data.( unsigned short type )
	@param Id : Device ID
	@param AoChannels : set the Data Channel
	@param AoData : get Data array of analog input
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ出力デバイスの複数チャネルのサンプリングデータを一回取得。(16bit)
	@param Id : デバイスID
	@param AoChannels : チャネル数
	@param AoData : アナログ出力データ配列
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioMultiAo( short Id, short AoChannels, long AoData[] )
{

	struct cpsaio_ioctl_arg	arg;
	int cnt;

	ContecCpsAioSetAoChannels(Id, AoChannels );

	// Set Sampling Number
	ContecCpsAioSetAoEventSamplingTimes( Id, 1 );

	// Ai Start
	ContecCpsAioStartAo( Id );

	for( cnt = 0;cnt < AoChannels; cnt ++ ){
		arg.val = (long)AoData[cnt];
		ioctl( Id, IOCTL_CPSAIO_OUTDATA, &arg );
	}

	// Ai Stop
	ContecCpsAioStopAo( Id );

	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library get multiple channels of analog output device sampling one data.( double type )
	@param Id : Device ID
	@param AoChannels : set the Data Channel
	@param AoData : get Data array of analog output
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ出力デバイスの複数チャネルのサンプリングデータを一回取得。(浮動小数点型)
	@param Id : デバイスID
	@param AoChannels : チャネル数
	@param AoData : アナログ出力データ配列
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioMultiAoEx( short Id, short AoChannels, double AoData[] )
{

	long *tmpAoData;
	unsigned short AoResolution = 0;
	int cnt;
	double dblMin, dblMax;

	tmpAoData = (long*)malloc( sizeof(long) * AoChannels );

	ContecCpsAioGetAoResolution( Id, &AoResolution );

/*
	ContecCpsAioGetAoRange( Id, &AoRange ); 
	switch( AoRange ){
		case PM10 :
*/
			dblMax = 20.0; dblMin = 0.0;
/*
			break;
	}
*/	
	for( cnt = 0;cnt < AoChannels; cnt ++){
		tmpAoData[cnt] = (long)( (AoData[cnt] - dblMin) * pow(2.0, AoResolution) / (dblMax - dblMin) );
	}

	ContecCpsAioMultiAo( Id, AoChannels, tmpAoData );

	free(tmpAoData);

	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library set destination and source signals by E Control unit.
	@param Id : Device ID
	@param dest : Destination Signal
	@param src : Source Signal
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief ECUに接続元の信号と接続先の信号を設定する関数。
	@param Id : デバイスID
	@param dest : 接続先の信号
	@param src : 接続元の信号
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioSetEcuSignal( short Id, unsigned short dest, unsigned short src )
{
	struct cpsaio_ioctl_arg	arg;

	arg.val = CPSAIO_ECU_DESTSRC_SIGNAL(dest, src);
	ioctl( Id, IOCTL_CPSAIO_SETECU_SIGNAL, &arg);

	return AIO_ERR_SUCCESS;
}

// 2016-01-20 (1) 
/**
	@~English
	@brief AIO Library set Calibration data by analog input.
	@param Id : Device ID
	@param select : select offset or gain
	@param ch : channel
	@param range : Range
	@param data : 16bit data
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ入力の補正データを設定する関数。
	@param Id : デバイスID
	@param select : オフセットかゲインか
	@param ch : チャネル番号
	@param range : レンジ
	@param data : Data( 16bit )
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioSetAiCalibrationData( short Id, unsigned char select, unsigned char ch, unsigned char range, unsigned short data )
{
	int aisel = 0;
	struct cpsaio_ioctl_arg	arg;


	arg.ch = ch;
	arg.val = CPSAIO_AI_CALIBRATION_DATA(select, ch, range, aisel, data);
	ioctl( Id, IOCTL_CPSAIO_SET_CALIBRATION_AI, &arg);

	return AIO_ERR_SUCCESS;

} 
/**
	@~English
	@brief AIO Library get Calibration data by analog input.
	@param Id : Device ID
	@param select : select offset or gain
	@param ch : channel
	@param range : Range
	@param data : 16bit data
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ入力の補正データを取得する関数。
	@param Id : デバイスID
	@param select : オフセットかゲインか
	@param ch : チャネル番号
	@param range : レンジ
	@param data : Data( 16bit )
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioGetAiCalibrationData( short Id, unsigned char *select, unsigned char *ch, unsigned char *range, unsigned short *data )
{
	struct cpsaio_ioctl_arg	arg;

	ioctl( Id, IOCTL_CPSAIO_GET_CALIBRATION_AI, &arg);

	*select = CPSAIO_AI_CALIBRATION_GETSELECT( arg.val );
	*range = CPSAIO_AI_CALIBRATION_GETRANGE( arg.val );	
	*data = CPSAIO_AI_CALIBRATION_GETDATA( arg.val );

	return AIO_ERR_SUCCESS;
} 
/**
	@~English
	@brief AIO Library write analog input calibration data to ROM.
	@param Id : Device ID
	@param ch : channel
	@param gain : gain value (8bit)
	@param offset : offset value (8bit)
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ入力の補正データをROMへ書き込む関数。
	@param Id : デバイスID
	@param ch : チャネル番号
	@param gain : ゲインの値 (  8bit )
	@param offset : オフセットの値(  8bit )
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioWriteAiCalibrationData( short Id, unsigned char ch, unsigned char gain, unsigned char offset )
{
	int aisel = 0;
	struct cpsaio_ioctl_arg	arg;

	arg.val = ( gain << 8 )  | offset;
	ioctl( Id, IOCTL_CPSAIO_WRITE_EEPROM_AI, &arg);

	return AIO_ERR_SUCCESS;	

}

/**
	@~English
	@brief AIO Library read analog input calibration data to ROM.
	@param Id : Device ID
	@param ch : channel
	@param gain : gain value (8bit)
	@param offset : offset value (8bit)
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ入力の補正データをROMから読み出す関数。
	@param Id : デバイスID
	@param ch : チャネル番号
	@param gain : ゲインの値 (  8bit )
	@param offset : オフセットの値(  8bit )
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioReadAiCalibrationData( short Id, unsigned char ch, unsigned char *gain, unsigned char *offset )
{
	int aisel = 0;
	struct cpsaio_ioctl_arg	arg;

	ioctl( Id, IOCTL_CPSAIO_READ_EEPROM_AI, &arg);

	*gain = 	(arg.val & 0xFF00 ) >> 8;

	*offset = (arg.val & 0xFF );

	return AIO_ERR_SUCCESS;	

}
/**
	@~English
	@brief AIO Library　clear analog input calibration data from ROM( or RAM).
	@param Id : Device ID
	@param iClear : ROM , RAM Flags
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスのROM/RAMからアナログ入力補正データを消去する関数。
	@param Id : デバイスID
	@param iClear : ROM もしくは　RAM
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioClearAiCalibrationData( short Id, int iClear )
{

	if( iClear & CPSAIO_AI_CALIBRATION_CLEAR_RAM ){
		//FPGA all Clear
		ContecCpsAioSetAiCalibrationData( Id, CPSAIO_AI_CALIBRATION_SELECT_OFFSET, 0, CPSAIO_AI_CALIBRATION_RANGE_PM10, 0 );
		ContecCpsAioSetAiCalibrationData( Id, CPSAIO_AI_CALIBRATION_SELECT_GAIN, 0, CPSAIO_AI_CALIBRATION_RANGE_PM10, 0 );
	}

	if( iClear & CPSAIO_AI_CALIBRATION_CLEAR_ROM ){
		//FPGA ROM CLEAR
		ioctl( Id, IOCTL_CPSAIO_CLEAR_EEPROM, NULL);
	}

	return AIO_ERR_SUCCESS;	

}

// 2016-01-20 (1) 
/**
	@~English
	@brief AIO Library set Calibration data by analog output.
	@param Id : Device ID
	@param select : select offset or gain
	@param ch : channel
	@param range : Range
	@param data : 16bit data
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ出力の補正データを設定する関数。
	@param Id : デバイスID
	@param select : オフセットかゲインか
	@param ch : チャネル番号
	@param range : レンジ
	@param data : Data( 16bit )
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioSetAoCalibrationData( short Id, unsigned char select, unsigned char ch, unsigned char range, unsigned short data )
{
	int aisel = 0;
	struct cpsaio_ioctl_arg	arg;

	arg.val = CPSAIO_AO_CALIBRATION_DATA(select, ch, range, aisel, data);
	ioctl( Id, IOCTL_CPSAIO_SET_CALIBRATION_AO, &arg);

	return AIO_ERR_SUCCESS;

} 

/**
	@~English
	@brief AIO Library get Calibration data by analog output.
	@param Id : Device ID
	@param select : select offset or gain
	@param ch : channel
	@param range : Range
	@param data : 16bit data
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ出力の補正データを取得する関数。
	@param Id : デバイスID
	@param select : オフセットかゲインか
	@param ch : チャネル番号
	@param range : レンジ
	@param data : Data( 16bit )
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioGetAoCalibrationData( short Id, unsigned char *select, unsigned char *ch, unsigned char *range, unsigned short *data )
{
	struct cpsaio_ioctl_arg	arg;


	ioctl( Id, IOCTL_CPSAIO_GET_CALIBRATION_AO, &arg);

	*select = CPSAIO_AO_CALIBRATION_GETSELECT( arg.val );
	*range = CPSAIO_AO_CALIBRATION_GETRANGE( arg.val );	
	*data = CPSAIO_AO_CALIBRATION_GETDATA( arg.val );

	return AIO_ERR_SUCCESS;
} 

/**
	@~English
	@brief AIO Library write Calibration analog output data to ROM.
	@param Id : Device ID
	@param ch : channel
	@param gain : gain value (8bit)
	@param offset : offset value (8bit)
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ出力の補正データをROMへ書き込む関数。
	@param Id : デバイスID
	@param ch : チャネル番号
	@param gain : ゲインの値 (  8bit )
	@param offset : オフセットの値(  8bit )
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioWriteAoCalibrationData( short Id, unsigned char ch, unsigned char gain, unsigned char offset )
{
	int aisel = 0;
	struct cpsaio_ioctl_arg	arg;

	arg.ch = ch;
	arg.val = ( gain << 8 )  | offset;
	ioctl( Id, IOCTL_CPSAIO_WRITE_EEPROM_AO, &arg);

	return AIO_ERR_SUCCESS;	

}
/**
	@~English
	@brief AIO Library read analog output calibration data to ROM.
	@param Id : Device ID
	@param ch : channel
	@param gain : gain value (8bit)
	@param offset : offset value (8bit)
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief アナログ出力の補正データをROMから読み出す関数。
	@param Id : デバイスID
	@param ch : チャネル番号
	@param gain : ゲインの値 (  8bit )
	@param offset : オフセットの値(  8bit )
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioReadAoCalibrationData( short Id, unsigned char ch, unsigned char *gain, unsigned char *offset )
{
	int aisel = 0;
	struct cpsaio_ioctl_arg	arg;

	arg.ch = ch;

	ioctl( Id, IOCTL_CPSAIO_READ_EEPROM_AO, &arg);

	*gain = 	(arg.val & 0xFF00 ) >> 8;

	*offset = (arg.val & 0xFF );

	return AIO_ERR_SUCCESS;	

}

/**
	@~English
	@brief AIO Library　clear analog output calibration data from ROM( or RAM).
	@param Id : Device ID
	@param iClear : ROM , RAM Flags
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスのROM/RAMからアナログ出力補正データを消去する関数。
	@param Id : デバイスID
	@param iClear : ROM もしくは　RAM
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioClearAoCalibrationData( short Id, int iClear )
{
	int cnt;

	if( iClear & CPSAIO_AO_CALIBRATION_CLEAR_RAM ){
		//FPGA all Clear
		for( cnt = 0;cnt < 4 ;cnt ++ ){
			ContecCpsAioSetAoCalibrationData( Id, CPSAIO_AO_CALIBRATION_SELECT_OFFSET, cnt, CPSAIO_AO_CALIBRATION_RANGE_P20MA, 0 );
			ContecCpsAioSetAoCalibrationData( Id, CPSAIO_AO_CALIBRATION_SELECT_GAIN, cnt, CPSAIO_AO_CALIBRATION_RANGE_P20MA, 0 );
		}
	}
	if( iClear & CPSAIO_AO_CALIBRATION_CLEAR_ROM ){
		//FPGA ROM CLEAR
		ioctl( Id, IOCTL_CPSAIO_CLEAR_EEPROM, NULL);
	}

	return AIO_ERR_SUCCESS;	

}

/* Direct Input / Output (Debug) */

/**
	@~English
	@brief AIO Library the register of address read data.(1byte)
	@param Id : Device ID
	@param addr : Address
	@param value : Values
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスのアドレスレジスタを読み出す関数。(1byte)
	@param Id : デバイスID
	@param addr : アドレス
	@param value : 値
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioInp( short Id, unsigned long addr, unsigned char *value )
{
	struct cpsaio_direct_arg arg;

	arg.addr = addr;
	ioctl( Id, IOCTL_CPSAIO_DIRECT_INPUT, arg );
	*value = (unsigned char)arg.val;

	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library the register of address read data.(2byte)
	@param Id : Device ID
	@param addr : Address
	@param value : Values
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスのアドレスレジスタを読み出す関数。(2byte)
	@param Id : デバイスID
	@param addr : アドレス
	@param value : 値
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioInpW( short Id, unsigned long addr, unsigned short *value )
{
	struct cpsaio_direct_arg arg;
	
	arg.addr = addr;
	ioctl( Id, IOCTL_CPSAIO_DIRECT_INPUT, arg );
	*value = (unsigned short)arg.val;
	
	return AIO_ERR_SUCCESS;

}

/**
	@~English
	@brief AIO Library the register of address read data.(4byte)
	@param Id : Device ID
	@param addr : Address
	@param value : Values
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスのアドレスレジスタを読み出す関数。(4byte)
	@param Id : デバイスID
	@param addr : アドレス
	@param value : 値
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioInpD( short Id, unsigned long addr, unsigned long *value )
{
	struct cpsaio_direct_arg arg;
	
	arg.addr = addr;
	ioctl( Id, IOCTL_CPSAIO_DIRECT_INPUT, arg );
	*value = (unsigned long)arg.val;
	
	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library the register of address write data.(1byte)
	@param Id : Device ID
	@param addr : Address
	@param value : Values
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスのアドレスレジスタへ書き出す関数。(1byte)
	@param Id : デバイスID
	@param addr : アドレス
	@param value : 値
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioOutp( short Id, unsigned long addr, unsigned char value )
{
	struct cpsaio_direct_arg arg;
	
	arg.addr = addr;
	arg.val = (unsigned long)value;
	ioctl( Id, IOCTL_CPSAIO_DIRECT_OUTPUT, arg );
	
	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library the register of address write data.(2byte)
	@param Id : Device ID
	@param addr : Address
	@param value : Values
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスのアドレスレジスタへ書き出す関数。(2byte)
	@param Id : デバイスID
	@param addr : アドレス
	@param value : 値
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioOutpW( short Id, unsigned long addr, unsigned short value )
{
	struct cpsaio_direct_arg arg;
	
	arg.addr = addr;
	arg.val = (unsigned long)value;
	ioctl( Id, IOCTL_CPSAIO_DIRECT_OUTPUT, arg );
	
	return AIO_ERR_SUCCESS;

}

/**
	@~English
	@brief AIO Library the register of address write data.(4byte)
	@param Id : Device ID
	@param addr : Address
	@param value : Values
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスのアドレスレジスタへ書き出す関数。(4byte)
	@param Id : デバイスID
	@param addr : アドレス
	@param value : 値
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioOutpD( short Id, unsigned long addr, unsigned long value )
{
	struct cpsaio_direct_arg arg;
	
	arg.addr = addr;
	arg.val = (unsigned long)value;
	ioctl( Id, IOCTL_CPSAIO_DIRECT_OUTPUT, arg );
	
	return AIO_ERR_SUCCESS;

}

/**
	@~English
	@brief AIO Library the register of ecu address read data.(1byte)
	@param Id : Device ID
	@param addr : Address
	@param value : Values
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスのECUアドレスレジスタを読み出す関数。(1byte)
	@param Id : デバイスID
	@param addr : アドレス
	@param value : 値
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioEcuInp( short Id, unsigned long addr, unsigned char *value )
{
	struct cpsaio_direct_command_arg arg;

	arg.addr = addr;
	arg.isEcu = 1;
	arg.size = 1;
	ioctl( Id, IOCTL_CPSAIO_DIRECT_COMMAND_INPUT, arg );
	*value = (unsigned char)arg.val;

	return AIO_ERR_SUCCESS;

}

/**
	@~English
	@brief AIO Library the register of ecu address read data.(2byte)
	@param Id : Device ID
	@param addr : Address
	@param value : Values
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスのECUアドレスレジスタを読み出す関数。(2byte)
	@param Id : デバイスID
	@param addr : アドレス
	@param value : 値
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioEcuInpW( short Id, unsigned long addr, unsigned short *value )
{

	struct cpsaio_direct_command_arg arg;

	arg.addr = addr;
	arg.isEcu = 1;
	arg.size = 2;
	ioctl( Id, IOCTL_CPSAIO_DIRECT_COMMAND_INPUT, arg );
	*value = (unsigned short)arg.val;

	return 	AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library the register of ecu address read data.(4byte)
	@param Id : Device ID
	@param addr : Address
	@param value : Values
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスのECUアドレスレジスタを読み出す関数。(4byte)
	@param Id : デバイスID
	@param addr : アドレス
	@param value : 値
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioEcuInpD( short Id, unsigned long addr, unsigned long *value )
{

	struct cpsaio_direct_command_arg arg;

	arg.addr = addr;
	arg.isEcu = 1;
	arg.size = 4;
	ioctl( Id, IOCTL_CPSAIO_DIRECT_COMMAND_INPUT, arg );
	*value = (unsigned long)arg.val;

	return 	AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library the register of ecu address write data.(1byte)
	@param Id : Device ID
	@param addr : Address
	@param value : Values
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスのECUアドレスレジスタへ書き出す関数。(1byte)
	@param Id : デバイスID
	@param addr : アドレス
	@param value : 値
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioEcuOutp( short Id, unsigned long addr, unsigned char value )
{
	struct cpsaio_direct_command_arg arg;

	arg.addr = addr;
	arg.isEcu = 1;
	arg.size = 1;
	arg.val = value;
	ioctl( Id, IOCTL_CPSAIO_DIRECT_COMMAND_OUTPUT, arg );

	return 	AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library the register of ecu address write data.(2byte)
	@param Id : Device ID
	@param addr : Address
	@param value : Values
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスのECUアドレスレジスタへ書き出す関数。(2byte)
	@param Id : デバイスID
	@param addr : アドレス
	@param value : 値
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioEcuOutpW( short Id, unsigned long addr, unsigned short value )
{

	struct cpsaio_direct_command_arg arg;

	arg.addr = addr;
	arg.isEcu = 1;
	arg.size = 2;
	arg.val = (unsigned long)value;

	ioctl( Id, IOCTL_CPSAIO_DIRECT_COMMAND_OUTPUT, arg );

	return 	AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library the register of ecu address write data.(4byte)
	@param Id : Device ID
	@param addr : Address
	@param value : Values
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスのECUアドレスレジスタへ書き出す関数。(4byte)
	@param Id : デバイスID
	@param addr : アドレス
	@param value : 値
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioEcuOutpD( short Id, unsigned long addr, unsigned long value )
{

	struct cpsaio_direct_command_arg arg;

	arg.addr = addr;
	arg.isEcu = 1;
	arg.size = 4;
	arg.val = value;

	ioctl( Id, IOCTL_CPSAIO_DIRECT_COMMAND_OUTPUT, arg );

	return 	AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library the register of command address read data.(1byte)
	@param Id : Device ID
	@param addr : Address
	@param value : Values
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスのCOMMANDアドレスレジスタを読み出す関数。(1byte)
	@param Id : デバイスID
	@param addr : アドレス
	@param value : 値
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioCommandInp( short Id, unsigned long addr, unsigned char *value )
{

	struct cpsaio_direct_command_arg arg;

	arg.addr = addr;
	arg.isEcu = 0;
	arg.size = 1;
	ioctl( Id, IOCTL_CPSAIO_DIRECT_COMMAND_INPUT, arg );
	*value = (unsigned char)arg.val;

	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library the register of command address read data.(2byte)
	@param Id : Device ID
	@param addr : Address
	@param value : Values
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスのCOMMANDアドレスレジスタを読み出す関数。(2byte)
	@param Id : デバイスID
	@param addr : アドレス
	@param value : 値
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioCommandInpW( short Id, unsigned long addr, unsigned short *value )
{

	struct cpsaio_direct_command_arg arg;

	arg.addr = addr;
	arg.isEcu = 0;
	arg.size = 2;
	ioctl( Id, IOCTL_CPSAIO_DIRECT_COMMAND_INPUT, arg );
	*value = (unsigned short)arg.val;

	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library the register of command address read data.(4byte)
	@param Id : Device ID
	@param addr : Address
	@param value : Values
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスのCOMMANDアドレスレジスタを読み出す関数。(4byte)
	@param Id : デバイスID
	@param addr : アドレス
	@param value : 値
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioCommandInpD( short Id, unsigned long addr, unsigned long *value )
{

	struct cpsaio_direct_command_arg arg;

	arg.addr = addr;
	arg.isEcu = 0;
	arg.size = 4;
	ioctl( Id, IOCTL_CPSAIO_DIRECT_COMMAND_INPUT, arg );
	*value = (unsigned long)arg.val;

	return AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library the register of command address write data.(1byte)
	@param Id : Device ID
	@param addr : Address
	@param value : Values
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスのCOMMANDアドレスレジスタへ書き出す関数。(1byte)
	@param Id : デバイスID
	@param addr : アドレス
	@param value : 値
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioCommandOutp( short Id, unsigned long addr, unsigned char value )
{
	struct cpsaio_direct_command_arg arg;

	arg.addr = addr;
	arg.isEcu = 0;
	arg.size = 1;
	arg.val = (unsigned long)value;
	ioctl( Id, IOCTL_CPSAIO_DIRECT_COMMAND_OUTPUT, arg );

	return 	AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library the register of command address write data.(2byte)
	@param Id : Device ID
	@param addr : Address
	@param value : Values
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスのCOMMANDアドレスレジスタへ書き出す関数。(2byte)
	@param Id : デバイスID
	@param addr : アドレス
	@param value : 値
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioCommandOutpW( short Id, unsigned long addr, unsigned short value )
{
	struct cpsaio_direct_command_arg arg;

	arg.addr = addr;
	arg.isEcu = 0;
	arg.size = 2;
	arg.val = (unsigned long)value;
	ioctl( Id, IOCTL_CPSAIO_DIRECT_COMMAND_OUTPUT, arg );

	return 	AIO_ERR_SUCCESS;
}

/**
	@~English
	@brief AIO Library the register of command address write data.(4byte)
	@param Id : Device ID
	@param addr : Address
	@param value : Values
	@return Success: AIO_ERR_SUCCESS
	@~Japanese
	@brief デバイスのCOMMANDアドレスレジスタへ書き出す関数。(4byte)
	@param Id : デバイスID
	@param addr : アドレス
	@param value : 値
	@return 成功: AIO_ERR_SUCCESS
**/
unsigned long ContecCpsAioCommandOutpD( short Id, unsigned long addr, unsigned long value )
{
	struct cpsaio_direct_command_arg arg;

	arg.addr = addr;
	arg.isEcu = 0;
	arg.size = 4;
	arg.val = value;
	ioctl( Id, IOCTL_CPSAIO_DIRECT_COMMAND_OUTPUT, arg );

	return 	AIO_ERR_SUCCESS;
}

