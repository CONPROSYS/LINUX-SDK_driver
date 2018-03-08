/*
 *  Lib for CONTEC CONPROSYS Digital I/O (CPS-CNT) Series.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>

#include "cpscnt.h"

#ifdef CONFIG_CONPROSYS_SDK
 #include "../include/libcpscnt.h"
#else
 #include "libcpscnt.h"
#endif

#define CONTEC_CPSCNT_LIB_VERSION	"0.9.5"



typedef struct __contec_cps_cnt_int_callback__
{
	PCONTEC_CPS_CNT_INT_CALLBACK func;
	CONTEC_CPS_CNT_INT_CALLBACK_DATA data;
}CONTEC_CPS_CNT_INT_CALLBACK_LIST, *PCONTEC_CPS_CNT_INT_CALLBACK_LIST;

CONTEC_CPS_CNT_INT_CALLBACK_LIST contec_cps_cnt_cb_list[CPS_DEVICE_MAX_NUM];




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
	if( signo == SIGUSR2 ){
		for( cnt = 0; cnt < CPS_DEVICE_MAX_NUM ; cnt ++){
			if( contec_cps_cnt_cb_list[cnt].func != (PCONTEC_CPS_CNT_INT_CALLBACK)NULL ){
				
				contec_cps_cnt_cb_list[cnt].func(
					contec_cps_cnt_cb_list[cnt].data.id,
					CNTM_INTERRUPT,
					contec_cps_cnt_cb_list[cnt].data.wParam,
					contec_cps_cnt_cb_list[cnt].data.lParam,
					contec_cps_cnt_cb_list[cnt].data.Param
				);
			}
		}
	}
}

/**
	@~English
	@brief CNT Library Initialize.
	@param DeviceName : Device node name ( cpscntX )
	@param Id : Device Access Id
	@return Success: CNT_ERR_SUCCESS, Failed: otherwise CNT_ERR_SUCCESS
	@~Japanese
	@brief 初期化関数.
	@param DeviceName : デバイスノード名  ( cpscntX )
	@param Id : デバイスID
	@return 成功: CNT_ERR_SUCCESS, 失敗: CNT_ERR_SUCCESS 以外

**/
unsigned long ContecCpsCntInit( char *DeviceName, short *Id )
{
	// open
	char Name[32];

	strcpy(Name, "/dev/");
	strcat(Name, DeviceName);

	*Id = open( Name, O_RDWR );

	if( *Id <= 0 ) return CNT_ERR_DLL_CREATE_FILE;

	return CNT_ERR_SUCCESS;

}

/**
	@~English
	@brief CNT Library Exit.
	@param Id : Device ID
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief 終了関数.
	@param Id : デバイスID
	@return 成功:  CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntExit( short Id )
{
	// close
	close( Id );
	return CNT_ERR_SUCCESS;

}

/**
	@~English
	@brief CNT Library output from ErrorNumber to ErrorString.
	@param code : Error Code
	@param Str : Error String
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief エラー文字列出力関数（未実装）
	@param code : エラーコード
	@param Str : エラー文字列
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntGetErrorStrings( unsigned long code, char *Str )
{

	return CNT_ERR_SUCCESS;
}

/**
	@~English
	@brief CNT Library query Device.
	@param Id : Device ID
	@param DeviceName : Device Node Name ( cpscntX )
	@param Device : Device Name ( CPS-CNT-3202I , etc )
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief クエリデバイス関数
	@param Id : デバイスID
	@param DeviceName : デバイスノード名 ( cpscntX )
	@param Device : デバイス型式名 (  CPS-CNT-3202Iなど )
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntQueryDeviceName( short Index, char *DeviceName, char *Device )
{
	struct cpscnt_ioctl_string_arg	arg;
	int len;

	char tmpDevName[16];
	char baseDeviceName[16]="cpscnt";
	char strNum[2]={0};
	int findNum=0, cnt, ret;

	short tmpId = 0;

	for(cnt = 0;cnt < CPS_DEVICE_MAX_NUM ; cnt ++ ){
		sprintf(tmpDevName,"%s%x",baseDeviceName, cnt);
		ret = ContecCpsCntInit(tmpDevName, &tmpId);

		if( ret == 0 ){
			ioctl(tmpId, IOCTL_CPSCNT_GET_DEVICE_NAME, &arg);
			ContecCpsCntExit(tmpId);

			if(findNum == Index){
				sprintf(DeviceName,"%s",tmpDevName);
				sprintf(Device,"%s", arg.str);
				return CNT_ERR_SUCCESS;
			}else{
				findNum ++;
			}
			memset(&tmpDevName,0x00, 16);
			memset(&arg.str, 0x00, sizeof(arg.str)/sizeof(arg.str[0]));

		}
	}

	return CNT_ERR_INFO_NOT_FIND_DEVICE;
}

/**
	@~English
	@brief CNT Library get maximum channels.
	@param Id : Device ID
	@param maxChannel : Maximum Channel Number.
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief デバイスの最大のチャネル数を取得します
	@param Id : デバイスID
	@param maxChannel :最大チャネル数
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntGetMaxChannels( short Id, short *maxChannel )
{

	struct cpscnt_ioctl_arg	arg;

	ioctl( Id, IOCTL_CPSCNT_GET_MAX_CHANNELS, &arg );

	*maxChannel = (short)( arg.val );

	return CNT_ERR_SUCCESS;
}


/**** Single Functions ****/
/**
	@~English
	@brief CNT Library set Z-Phase Mode.
	@param Id : Device ID
	@param ChNo : Channel Number
	@param Mode : Z-Phase mode
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief Z相のモードを指定します
	@param Id : デバイスID
	@param ChNo : チャネル番号
	@param Mode : Z相モード
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntSetZMode( short Id, short ChNo, short Mode )
{
	struct cpscnt_ioctl_arg	arg;

	arg.ch = ChNo;
	arg.val = Mode;

	ioctl( Id, IOCTL_CPSCNT_SET_Z_PHASE, &arg );


	return CNT_ERR_SUCCESS;
}

/**
	@~English
	@brief CNT Library set Z-Logic.
	@param Id : Device ID
	@param ChNo : Channel Number
	@param Logic : Z-Logic
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief Z相のロジックを指定します
	@param Id : デバイスID
	@param ChNo : チャネル番号
	@param Logic : Z相ロジック
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntSetZLogic( short Id, short ChNo, short Logic )
{
	struct cpscnt_ioctl_arg	arg;

	arg.ch = ChNo;
	arg.val = Logic;

	ioctl( Id, IOCTL_CPSCNT_SET_Z_LOGIC, &arg );

	return CNT_ERR_SUCCESS;
}

/**
	@~English
	@brief CNT Library set Select Channel Signal.
	@param Id : Device ID
	@param ChNo : Channel Number
	@param SigType : Signal Type
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief Z相のモードを指定します
	@param Id : デバイスID
	@param ChNo : チャネル番号
	@param SigType : Z相モード
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntSelectChannelSignal( short Id, short ChNo, short SigType )
{
	struct cpscnt_ioctl_arg	arg;

	arg.ch = ChNo;
	arg.val = SigType;

	ioctl( Id, IOCTL_CPSCNT_SET_SELECT_COMMON_INPUT, &arg );

	return CNT_ERR_SUCCESS;
}

/**
	@~English
	@brief CNT Library set Select Count Direction.
	@param Id : Device ID
	@param ChNo : Channel Number
	@param Dir : Direction
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief カウンタのアップカウント、ダウンカウントを設定します
	@param Id : デバイスID
	@param ChNo : チャネル番号
	@param Dir : 方向
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntSetCountDirection( short Id, short ChNo, short Dir )
{
	struct cpscnt_ioctl_arg	arg;

	arg.ch = ChNo;
	arg.val = Dir;

	ioctl( Id, IOCTL_CPSCNT_SET_DIRECTION, &arg );

	return CNT_ERR_SUCCESS;
}

/**
	@~English
	@brief CNT Library set Operation Mode.
	@param Id : Device ID
	@param ChNo : Channel Number
	@param Phase : Phase
	@param Mul : Multiple
	@param SyncDir : Sync Direction
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief オペレーション・モードを設定します
	@param Id : デバイスID
	@param ChNo : チャネル番号
	@param Phase : 相設定　1相、2相、ゲートコントロール
	@param Mul : 逓倍設定
	@param SyncDir :同期、非同期設定
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntSetOperationMode( short Id, short ChNo, short Phase, short Mul, short SyncDir )
{
	struct cpscnt_ioctl_arg	arg;
	unsigned char valb;

	switch ( Phase ){
	case CNT_MODE_GATECONTROL:
			switch ( Mul ){
			case CNT_MUL_X1	:	valb = 0x03; break;
			case CNT_MUL_X2	:	valb = 0x07;	break;
			}
			break;
	case CNT_MODE_1PHASE :
			valb = 0x13;
			break;
	case CNT_MODE_2PHASE :
			valb = ( ( !SyncDir & 0x01 ) << 2 ) | ( Mul & 0x03 );
			break;
	}

	arg.ch = ChNo;
	arg.val = valb;

	ioctl( Id, IOCTL_CPSCNT_SET_MODE, &arg );

	return CNT_ERR_SUCCESS;
}

/**
	@~English
	@brief CNT Library set Digital Filter.
	@param Id : Device ID
	@param ChNo : Channel Number
	@param FilterValue : Filter Value
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief デジタルフィルタを設定します
	@param Id : デバイスID
	@param ChNo : チャネル番号
	@param FilterValue :　フィルタ係数
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntSetDigitalFilter( short Id, short ChNo, short FilterValue )
{
	struct cpscnt_ioctl_arg	arg;


	arg.ch = ChNo;
	arg.val = FilterValue;

	ioctl( Id, IOCTL_CPSCNT_SET_FILTER, &arg );

	return CNT_ERR_SUCCESS;
}

/**
	@~English
	@brief CNT Library set Pulse Width.
	@param Id : Device ID
	@param ChNo : Channel Number
	@param PlsWidth : Pulse Width
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief 出力するパルス幅を指定します
	@param Id : デバイスID
	@param ChNo : チャネル番号
	@param PlsWidth :　パルス幅
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntSetPulseWidth( short Id, short ChNo, short PlsWidth )
{
	struct cpscnt_ioctl_arg	arg;

	arg.ch = ChNo;
	arg.val = PlsWidth;

	ioctl( Id, IOCTL_CPSCNT_SET_ONESHOT_PULSE_WIDTH, &arg );

	return CNT_ERR_SUCCESS;
}

/**
	@~English
	@brief CNT Library get Z-Phase Mode.
	@param Id : Device ID
	@param ChNo : Channel Number
	@param Mode : Z-Phase mode
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief Z相のモードを指定します
	@param Id : デバイスID
	@param ChNo : チャネル番号
	@param Mode : Z相モード
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntGetZMode( short Id, short ChNo, short *Mode )
{
	struct cpscnt_ioctl_arg	arg;

	arg.ch = ChNo;

	ioctl( Id, IOCTL_CPSCNT_GET_Z_PHASE, &arg );

	*Mode = arg.val;

	return CNT_ERR_SUCCESS;
}

/**
	@~English
	@brief CNT Library get Z-Logic.
	@param Id : Device ID
	@param ChNo : Channel Number
	@param Logic : Z-Logic
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief Z相のロジックを取得します
	@param Id : デバイスID
	@param ChNo : チャネル番号
	@param Logic : Z相ロジック
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntGetZLogic( short Id, short ChNo, short *Logic )
{
	struct cpscnt_ioctl_arg	arg;

	arg.ch = ChNo;

////////////////////// Ver.0.9.2
	ioctl( Id, IOCTL_CPSCNT_GET_Z_LOGIC, &arg );
////////////////////// Ver.0.9.2

	*Logic = arg.val;

	return CNT_ERR_SUCCESS;
}

/**
	@~English
	@brief CNT Library get Select Channel Signal.
	@param Id : Device ID
	@param ChNo : Channel Number
	@param SigType : Signal Type
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief 信号タイプを取得します
	@param Id : デバイスID
	@param ChNo : チャネル番号
	@param SigType : 信号タイプ
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntGetChannelSignal( short Id, short ChNo, short *SigType )
{
	struct cpscnt_ioctl_arg	arg;

	arg.ch = ChNo;
	

	ioctl( Id, IOCTL_CPSCNT_GET_SELECT_COMMON_INPUT, &arg );

	*SigType = arg.val;

	return CNT_ERR_SUCCESS;
}

/**
	@~English
	@brief CNT Library get Select Count Direction.
	@param Id : Device ID
	@param ChNo : Channel Number
	@param Dir : Direction
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief カウンタのアップカウント、ダウンカウントを取得します
	@param Id : デバイスID
	@param ChNo : チャネル番号
	@param Dir : 方向
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntGetCountDirection( short Id, short ChNo, short *Dir )
{
	struct cpscnt_ioctl_arg	arg;

	arg.ch = ChNo;


	ioctl( Id, IOCTL_CPSCNT_SET_DIRECTION, &arg );

	*Dir = arg.val;

	return CNT_ERR_SUCCESS;
}

/**
	@~English
	@brief CNT Library Get Operation Mode.
	@param Id : Device ID
	@param ChNo : Channel Number
	@param Phase : Phase
	@param Mul : Multiple
	@param SyncDir : Sync Direction
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief オペレーション・モードを取得します
	@param Id : デバイスID
	@param ChNo : チャネル番号
	@param Phase : 相設定　1相、2相、ゲートコントロール
	@param Mul : 逓倍設定
	@param SyncDir :同期、非同期設定
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntGetOperationMode( short Id, short ChNo, short *Phase, short *Mul, short *SyncDir )
{
	struct cpscnt_ioctl_arg	arg;
	unsigned char valb;

	arg.ch = ChNo;

	ioctl( Id, IOCTL_CPSCNT_GET_MODE, &arg );

	switch ( arg.val ){
	case 0x03 :
	case 0x07 :
		*Phase = CNT_MODE_GATECONTROL;
		*SyncDir = CNT_CLR_ASYNC;
		if( arg.val == 0x03 )		*Mul = CNT_MUL_X1;
		else *Mul = CNT_MUL_X2;
		break;
	
	case 0x13 :
		*Phase = CNT_MODE_1PHASE;
		*SyncDir = CNT_CLR_ASYNC;
		*Mul = CNT_MUL_X1;
		break;		
	default :
		*Phase = CNT_MODE_2PHASE;
		*SyncDir = ~(arg.val) & 0x04;
		*Mul = arg.val & 0x03 ;
		break;
	}

	return CNT_ERR_SUCCESS;
}


/**
	@~English
	@brief CNT Library get Digital Filter.
	@param Id : Device ID
	@param ChNo : Channel Number
	@param FilterValue : Filter Value
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief デジタルフィルタを取得します
	@param Id : デバイスID
	@param ChNo : チャネル番号
	@param FilterValue : フィルタ係数
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntGetDigitalFilter( short Id, short ChNo, short *FilterValue )
{
	struct cpscnt_ioctl_arg	arg;


	arg.ch = ChNo;

	ioctl( Id, IOCTL_CPSCNT_GET_FILTER, &arg );

	*FilterValue = arg.val;

	return CNT_ERR_SUCCESS;
}


/**
	@~English
	@brief CNT Library get Pulse Width.
	@param Id : Device ID
	@param ChNo : Channel Number
	@param PlsWidth : Pulse Width
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief 出力するパルス幅を取得します。
	@param Id : デバイスID
	@param ChNo : チャネル番号
	@param PlsWidth :パルス幅
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntGetPulseWidth( short Id, short ChNo, short *PlsWidth )
{
	struct cpscnt_ioctl_arg	arg;

	arg.ch = ChNo;

	ioctl( Id, IOCTL_CPSCNT_GET_ONESHOT_PULSE_WIDTH, &arg );

	*PlsWidth = arg.val;

	return CNT_ERR_SUCCESS;
}



/**
	@~English
	@brief CNT Library start count.
	@param Id : Device ID
	@param ChNo : Start of Channels ( Array )
	@param chNum :Channel Number
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief 指定されたチャネルのカウントをスタートします。
	@param Id : デバイスID
	@param ChNo :　チャネル配列
	@param chNum :　チャネル数
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntStartCount( short Id, short ChNo[], short chNum )
{

	struct cpscnt_ioctl_arg	arg;
	int cnt;

	// arg.val <- enable Ch

	for( cnt = 0, arg.val = 0 ;cnt < chNum; cnt ++ ){
		arg.val |= 1 << (ChNo[cnt] );
	}

	ioctl( Id, IOCTL_CPSCNT_START_COUNT, &arg );

	return CNT_ERR_SUCCESS;
}

/**
	@~English
	@brief CNT Library stop count.
	@param Id : Device ID
	@param ChNo : Stop of Channels ( Array )
	@param chNum : Channel Number
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief 指定されたチャネルのカウントを停止します
	@param Id : デバイスID
	@param ChNo :　チャネル配列
	@param chNum :　チャネル数
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntStopCount( short Id, short ChNo[], short chNum )
{

	struct cpscnt_ioctl_arg	arg;
	int cnt;

	// arg.val <- enable Ch

	for( cnt = 0, arg.val = 0 ;cnt < chNum; cnt ++ ){
		arg.val |= 1 << (ChNo[cnt] );
	}

	ioctl( Id, IOCTL_CPSCNT_STOP_COUNT, &arg );

	return CNT_ERR_SUCCESS;
}

/**
	@~English
	@brief CNT Library preset count.
	@param Id : Device ID
	@param ChNo : Preset of Channels ( Array )
	@param chNum : Chennel Number
	@param PresetData : Preset Data ( Array )
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief 指定したチャネルにデータをプリセットします
	@param Id : デバイスID
	@param ChNo :　チャネル配列
	@param chNum :チャネル数
	@param PresetData : プリセットデータ配列
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntPreset( short Id, short ChNo[], short chNum, unsigned long PresetData[] )
{

	struct cpscnt_ioctl_arg	arg;
	int cnt;

	//
	for( cnt = 0 , arg.val = 0 ;cnt < chNum; cnt ++ ) {
		arg.ch = ChNo[cnt];
		arg.val = PresetData[cnt];
		ioctl( Id, IOCTL_CPSCNT_INIT_COUNT, &arg );
	}

	return CNT_ERR_SUCCESS;
}


/**
	@~English
	@brief CNT Library read count.
	@param Id : Device ID
	@param ChNo : Read of Channels ( Array )
	@param chNum :　Channel Number
	@param CntDat : Data of channels ( Array )
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief カウンタデバイスのカウント値を読み出します。
	@param Id : デバイスID
	@param ChNo : リードするチャネル配列
	@param chNum :チャネル数
	@param CntDat : データ 配列
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntReadCount( short Id, short ChNo[], short chNum, unsigned long CntDat[] )
{

	struct cpscnt_ioctl_arg	arg;
	int cnt;

	// arg.val <- latch

	for( cnt = 0, arg.val = 0 ;cnt < chNum; cnt ++ ){
		arg.val |= 1 << (ChNo[cnt] );
	}

	ioctl( Id, IOCTL_CPSCNT_SET_COUNT_LATCH, &arg );

	//
	for( cnt = 0 , arg.val = 0 ;cnt < chNum; cnt ++ ) {
		arg.ch = ChNo[cnt];
		ioctl( Id, IOCTL_CPSCNT_READ_COUNT, &arg );
		CntDat[cnt] = arg.val;
	}

	return CNT_ERR_SUCCESS;
}
////////////////////////////////////////// Ver.0.9.2
/**
	@~English
	@brief CNT Library read status.
	@param Id : Device ID
	@param ChNo : Channels
	@param Status :　Status
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief カウンタデバイスのステータスを読み出します。
	@param Id : デバイスID
	@param ChNo : チャネル番号
	@param Status : ステータス
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntReadStatus( short Id, short ChNo, short *Status )
{
	struct cpscnt_ioctl_arg	arg;

	arg.ch = ChNo;
	ioctl( Id, IOCTL_CPSCNT_GET_STATUS, &arg );

	*Status = arg.val;

	return CNT_ERR_SUCCESS;
}
////////////////////////////////////////// Ver.0.9.3

/**
	@~English
	@brief CNT Library read Digital Input.
	@param Id : Device ID
	@param ChNo : Channels
	@param InData :　Digital Input Bit
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief カウンタデバイスの入力を読み出します。
	@param Id : デバイスID
	@param ChNo : チャネル番号
	@param InData : デジタル入力ビット
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntInputDIBit( short Id, short ChNo, short *InData)
{
	short Status;

	ContecCpsCntReadStatus( Id, ChNo, &Status );

	*InData = ( Status & CNT_STATUS_BIT_U );

	return CNT_ERR_SUCCESS;
}

/**
	@~English
	@brief CNT Library sets the compare register of matching count.
	@param Id : Device ID
	@param RegNo : compare register
	@param Count : match Count
	@param hWnd :　Reserved.
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief カウンタデバイスのカウント一致による比較レジスタの設定を行ないます。
	@param Id : デバイスID
	@param ChNo : チャネル番号
	@param RegNo : 比較レジスタ番号
	@param Count :　一致さ競るためのカウント
	@param hWnd :　ウィンドウハンドル( 未実装 )
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntNotifyCountUp( short Id , short ChNo , short RegNo , unsigned long Count , int hWnd )
{

	struct cpscnt_ioctl_arg	arg;

	arg.ch = ChNo;
	arg.val = Count;
	ioctl( Id, IOCTL_CPSCNT_SET_COMPARE_REG, &arg );

	return CNT_ERR_SUCCESS;

}

/**
	@~English
	@brief CNT Library gets driver and library version.
	@param Id : Device ID
	@param libVer : library version
	@param drvVer : driver version
	@return Success: CNT_ERR_SUCCESS
	@~Japanese
	@brief カウンタデバイスのカウント一致による比較レジスタの設定を行ないます。
	@param Id : デバイスID
	@param libVer : ライブラリバージョン
	@param drvVer : ドライババージョン
	@note Ver.0.9.4 Change from cpsaio_ioctl_arg to cpsaio_ioctl_string_arg.
	@return 成功: CNT_ERR_SUCCESS
**/
unsigned long ContecCpsCntGetVersion( short Id , unsigned char libVer[] , unsigned char drvVer[] )
{

	struct cpscnt_ioctl_string_arg	arg;
	int len;


	ioctl( Id, IOCTL_CPSCNT_GET_DRIVER_VERSION, &arg );

	len = sizeof( arg.str ) / sizeof( arg.str[0] );
	memcpy(drvVer, arg.str, len);
//	strcpy_s(drvVer, arg.str);

	len = sizeof( CONTEC_CPSCNT_LIB_VERSION ) /sizeof( unsigned char );
	memcpy(libVer, CONTEC_CPSCNT_LIB_VERSION, len);	
//	strcpy_s(libVer, CONTEC_CPSCNT_LIB_VERSION);

	return CNT_ERR_SUCCESS;

}

////////////////////////////////////////// Ver.0.9.3


