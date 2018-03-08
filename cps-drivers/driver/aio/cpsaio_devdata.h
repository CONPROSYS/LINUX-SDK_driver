/*** cps_common_devdata.h ******************************/


/*!
 @~English
 @name Address Register
 @brief CPS-AI-1608LI and CPS-AO-1604LI
 @~Japanese
 @name レジスタアドレス
 @brief デバイス： CPS-AI-1608LI , CPS-AO-1604LI
*/
/// @{
#define	OFFSET_AIDATA_CPS_AIO	0x0	///< Analog input data register
#define	OFFSET_AISTATUS_CPS_AIO	0x4	///< Analog input status register
#define	OFFSET_AODATA_CPS_AIO 0x8	///< Analog output data register
#define	OFFSET_AOSTATUS_CPS_AIO	0xC	///< Analog output status register
#define	OFFSET_COMMAND_DATALOCK_AIO	0x18	///< Device Data Lock register (Write/Read Lock) Ver.1.0.7 added.
#define	OFFSET_MEMSTATUS_CPS_AIO	0x1C	///< Memory status register
#define	OFFSET_EXT_COMMAND_LOWER_CPS_AIO 0x20	///< Extension command address-lower register.
#define	OFFSET_EXT_COMMAND_UPPER_CPS_AIO 0x22	///< Extension command address-upper register.
#define	OFFSET_EXT_DATA_LOWER_CPS_AIO 0x24	///< Extension data-lower register.
#define	OFFSET_EXT_DATA_UPPER_CPS_AIO 0x26	///< Extension data-upper register.
#define	OFFSET_ECU_INTERRUPT_CHECK_CPS_AIO	0x28	///< ECU Interrupt check register.　（READ)
#define	OFFSET_INTERRUPT_LOWER_CPS_AIO	0x2C	///< Interrupt lower register.　（READ)
#define	OFFSET_INTERRUPT_UPPER_CPS_AIO	0x2E	///< Interrupt upper register.　（READ)
#define	OFFSET_ECU_COMMAND_LOWER_CPS_AIO	0x28	///< ECU command address-lower register.　（WRITE)
#define	OFFSET_ECU_COMMAND_UPPER_CPS_AIO	0x2A	///< ECU command address-upper register.
#define	OFFSET_ECU_DATA_LOWER_CPS_AIO 0x2C	///< ECU data-lower register.　（WRITE)
#define	OFFSET_ECU_DATA_UPPER_CPS_AIO 0x2E	///< ECU data-upper register.　（WRITE)

/// @}

/*!
 @~English
 @name CPS-AIO Write Lock /UnLock
 @brief Version.1.0.7 Added.
 @~Japanese
 @name AIO 書き込みロック / アンロック
 @brief Version.1.0.7 実装
*/
/// @{
#define CPS_AIO_DATA_LOCK			0x0			///<	データロック
#define CPS_AIO_DATA_UNLOCK			0xAA55		///<	データアンロック
/// @}

/*!
 @~English
 @name CPS-AIO Extension (define)
 @~Japanese
 @name AIO 拡張用フラグ
*/
/// @{
#define CPS_AIO_ADDRESS_MODE_ECU	0x01	///< ECU
#define CPS_AIO_ADDRESS_MODE_COMMAND	0x00	/// COMMAND
/// @}

/*!
 @~English
 @name CPS-AIO  Base Address of ECU and Command (define)
 @~Japanese
 @name ECU 及びコマンドの ベースアドレス
*/
/// @{
#define CPS_AIO_COMMAND_BASE_ECU		0x0000	///< ECU Base Address
#define CPS_AIO_COMMAND_BASE_AI			0x1000	///< AI Base Address
#define CPS_AIO_COMMAND_BASE_AO			0x2000	///< AO Base Address
//#define CPS_AIO_COMMAND_BASE_DI			0x3000	///< DI Base Address
//#define CPS_AIO_COMMAND_BASE_DO			0x4000	///< DO Base Address
//#define CPS_AIO_COMMAND_BASE_CNT		0x5000	///< CNT Base Address
#define CPS_AIO_COMMAND_BASE_MEM		0x6000	///< MEMory Base Address
/// @}
#define CPS_AIO_COMMAND_MASK	0xF000	///<  Base Address

/*!
 @~English
 @name CPS-AIO Command Read or Write or Call Flag (define)
 @~Japanese
 @name AIOコマンド 読込 /書込/ 呼出　フラグ
*/
/// @{
#define CPS_AIO_COMMAND_CALL 0x00		///< Call
#define CPS_AIO_COMMAND_READ	0x01	///< Read
#define CPS_AIO_COMMAND_WRITE 0x02	///< Write
/// @}


/*!
 @~English
 @name CPS-AIO ECU Command Address (define)
 @~Japanese
 @name ECU コマンド アドレス
*/
/// @{
#define CPS_AIO_COMMAND_ECU_INIT	( CPS_AIO_COMMAND_BASE_ECU | 0x00 )	///< Initialize ECU Register
#define CPS_AIO_COMMAND_ECU_SETIRQMASK_ENABLE	( CPS_AIO_COMMAND_BASE_ECU | 0x01 )	///< Set Ecu Irq Mask Enable Register
#define CPS_AIO_COMMAND_ECU_SETIRQMASK_DISABLE	( CPS_AIO_COMMAND_BASE_ECU | 0x02 )	///< Set Ecu Irq Mask Disable Register
#define CPS_AIO_COMMAND_ECU_SETINPUTSIGNAL	( CPS_AIO_COMMAND_BASE_ECU | 0x03 )	///< Set Ecu Input Signal Register
#define CPS_AIO_COMMAND_ECU_GENERAL_PULSE_COM0	( CPS_AIO_COMMAND_BASE_ECU | 0x05 )	///< Set Ecu General Common Pulse Register
#define CPS_AIO_COMMAND_ECU_AI_UNUSUAL_STOP	( CPS_AIO_COMMAND_BASE_ECU | 0x10 )	///< Set Ecu AI unusual stop Register
#define CPS_AIO_COMMAND_ECU_AO_UNUSUAL_STOP	( CPS_AIO_COMMAND_BASE_ECU | 0x11 )	///< Set Ecu AO unusual stop Register
#define CPS_AIO_COMMAND_ECU_AI_INTERRUPT_FLAG	( CPS_AIO_COMMAND_BASE_AI | 0x00 )	///< Set Ecu AI Interrupt flag Register
#define CPS_AIO_COMMAND_ECU_AO_INTERRUPT_FLAG	( CPS_AIO_COMMAND_BASE_AO | 0x00 )	///< Set Ecu AO Interrupt flag Register
#define CPS_AIO_COMMAND_ECU_MEM_INTERRUPT_FLAG	( CPS_AIO_COMMAND_BASE_MEM | 0x00 )	///< Set Ecu Memory Interrupt flag Register
/// @}
/*!
 @~English
 @name CPS-AIO AI Command Address (define)
 @~Japanese
 @name AI コマンド アドレス
*/
/// @{
#define CPS_AIO_COMMAND_AI_INIT ( CPS_AIO_COMMAND_BASE_AI | 0x00 )	///< AI Initialize Register
#define CPS_AIO_COMMAND_AI_GATE_OPEN	( CPS_AIO_COMMAND_BASE_AI | 0x01 )	///< AI Gate open Register
#define CPS_AIO_COMMAND_AI_FORCE_STOP	( CPS_AIO_COMMAND_BASE_AI | 0x02 )	///< AI Force stop Register
#define CPS_AIO_COMMAND_AI_SET_INTERNAL_CLOCK	( CPS_AIO_COMMAND_BASE_AI | 0x03 )	///< AI Set Internal Clock Register
#define CPS_AIO_COMMAND_AI_SET_CHANNELS ( CPS_AIO_COMMAND_BASE_AI | 0x05 )	///< AI Set Channel Register
#define CPS_AIO_COMMAND_AI_SET_BEFORE_TRIGGER_SAMPLING_NUMBER	( CPS_AIO_COMMAND_BASE_AI | 0x09 )	///< AI Set Before Trigger Sampling Register
#define CPS_AIO_COMMAND_AI_SET_EXCHANGE	( CPS_AIO_COMMAND_BASE_AI | 0x0C )	///< AI Set Exchange Register
#define CPS_AIO_COMMAND_AI_SET_CALIBRATION_TERMS	( CPS_AIO_COMMAND_BASE_AI | 0x20 )	///< AI Set Calibration Terms Register
#define CPS_AIO_COMMAND_AI_SET_CALIBRATION_EEPROM	( CPS_AIO_COMMAND_BASE_AI | 0x21 )	///< AI Set Calibration EEPROM Register
/// @}
/*!
 @~English
 @name CPS-AIO AO Command Address (define)
 @~Japanese
 @name AO コマンド アドレス
*/
/// @{
#define CPS_AIO_COMMAND_AO_INIT ( CPS_AIO_COMMAND_BASE_AO | 0x00 )	///< AO Initialize Register
#define CPS_AIO_COMMAND_AO_GATE_OPEN	( CPS_AIO_COMMAND_BASE_AO | 0x01 )	///< AO Gate open Register
#define CPS_AIO_COMMAND_AO_FORCE_STOP	( CPS_AIO_COMMAND_BASE_AO | 0x02 )	///< AO Force stop Register
#define CPS_AIO_COMMAND_AO_SET_INTERNAL_CLOCK	( CPS_AIO_COMMAND_BASE_AO | 0x03 )	///< AO Set Internal Clock Register
#define CPS_AIO_COMMAND_AO_SET_CHANNELS ( CPS_AIO_COMMAND_BASE_AO | 0x05 )	///< AO Set Channel Register
#define CPS_AIO_COMMAND_AO_SET_BEFORE_TRIGGER_SAMPLING_NUMBER	( CPS_AIO_COMMAND_BASE_AO | 0x09 )	///< AO Set Before Trigger Sampling Register
#define CPS_AIO_COMMAND_AO_SET_EXCHANGE	( CPS_AIO_COMMAND_BASE_AO | 0x0C )	///< AO Set Exchange Register
#define CPS_AIO_COMMAND_AO_SET_CALIBRATION_TERMS	( CPS_AIO_COMMAND_BASE_AO | 0x20 )	///< AO Set Calibration Terms Register
#define CPS_AIO_COMMAND_AO_SET_CALIBRATION_EEPROM	( CPS_AIO_COMMAND_BASE_AO | 0x21 )	///< AO Set Calibration EEPROM Register

/// @}
/*!
 @~English
 @name CPS-AIO Memory Command Address (define)
 @~Japanese
 @name メモリ コマンド アドレス
*/
/// @{
#define CPS_AIO_COMMAND_MEM_INIT ( CPS_AIO_COMMAND_BASE_MEM | 0x00 )///< MEM Initialize Register
#define CPS_AIO_COMMAND_MEM_AI_CLEAR	( CPS_AIO_COMMAND_BASE_MEM | 0x07 )///< MEM AI Clear Register
#define CPS_AIO_COMMAND_MEM_AO_CLEAR	( CPS_AIO_COMMAND_BASE_MEM | 0x08 )///< MEM AO Clear Register
#define CPS_AIO_COMMAND_MEM_AI_FIFO_COUNTER	( CPS_AIO_COMMAND_BASE_MEM | 0x0B )///< MEM AI FIFO Counter Register
#define CPS_AIO_COMMAND_MEM_AO_FIFO_COUNTER ( CPS_AIO_COMMAND_BASE_MEM | 0x0C )	///< MEM AO FIFO Counter Register
#define CPS_AIO_COMMAND_MEM_SET_AI_COMPARE_TYPE	( CPS_AIO_COMMAND_BASE_MEM | 0x10 )///< MEM Set AI compare type Register
#define CPS_AIO_COMMAND_MEM_SET_AI_COMPARE_DATA	( CPS_AIO_COMMAND_BASE_MEM | 0x11 )///< MEM Set AI compare data Register
#define CPS_AIO_COMMAND_MEM_SET_AO_COMPARE_TYPE	( CPS_AIO_COMMAND_BASE_MEM | 0x20 )///< MEM Set AO compare type Register
#define CPS_AIO_COMMAND_MEM_SET_AO_COMPARE_DATA	( CPS_AIO_COMMAND_BASE_MEM | 0x21 )///< MEM Set AO compare data Register

/// @}
/*!
 @~English
 @name CPS-AIO ECU Set Input Signal Values.(SET_INP_SIG)
 @~Japanese
 @name ECU 信号入力設定　(SET_INP_SIG)
*/
/// @{

/*!
 @name Destination.
*/
/// @{
#define CPS_AIO_ECU_SET_INP_SIG_DIST_AI_PARMIT_TRG	0x0000		///<  AI Parmit Trigger
#define CPS_AIO_ECU_SET_INP_SIG_DIST_AI_NOTPARMIT_TRG	0x0002	///< AI no Parmit Trigger
#define CPS_AIO_ECU_SET_INP_SIG_DIST_AI_SAMP_CLK	0x0004	///< AI Sampling Clock
#define CPS_AIO_ECU_SET_INP_SIG_DIST_AO_UPD_PARMIT_TRG	0x0020		///< AO Update Parmit Trigger
#define CPS_AIO_ECU_SET_INP_SIG_DIST_AO_UPD_NOTPARMIT_TRG	0x0022	///< AO Update no Parmit Trigger
#define CPS_AIO_ECU_SET_INP_SIG_DIST_AO_UPD_CLK	0x0024	///< AO Update Clock
/// @}
/*!
 @name Source.
*/
/// @{
#define CPS_AIO_ECU_SET_INP_SIG_SRC_NON_CONNECT		0x0000	///< Non Connect
#define CPS_AIO_ECU_SET_INP_SIG_SRC_AI_INTERNAL_CLK	0x0004 ///< AI INTERNAL CLOCK
#define CPS_AIO_ECU_SET_INP_SIG_SRC_AI_BEFTRG_NUMEND	0x0011 ///< AI BEFORE TRIGGER NUM END
#define CPS_AIO_ECU_SET_INP_SIG_SRC_AO_INTERNAL_CLK	0x0042 ///< AO INTERNAL CLOCK
#define CPS_AIO_ECU_SET_INP_SIG_SRC_AO_BEFTRG_NUMEND	0x0050 ///< AO BEFORE TRIGGER NUM END
#define CPS_AIO_ECU_SET_INP_SIG_SRC_MEM_AODATA_EMPTY	0x0160 ///< MEM AO Data Empty
#define CPS_AIO_ECU_SET_INP_SIG_SRC_ECU_GENERAL_COM0	0x0180	///< General command 0
/// @}
/// @}
/*!
 @~English
 @name CPS-AIO ECU AI UNUSUAL STOP VALUES.(AI_UNU_STP)
 @~Japanese
 @name CPS-AIO ECU AI UNUSUAL STOP VALUES.(AI_UNU_STP)
*/
/// @{
#define CPS_AIO_ECU_AI_UNU_STP_AI_CLK_ERR	0x0001	///< AI CLOCK ERROR
#define CPS_AIO_ECU_AI_UNU_STP_AI_OVERFLOW	0x0100	///< AI OVER FLOW
/// @}
/*!
 @~English
 @name CPS-AIO ECU AO UNUSUAL STOP VALUES (AO_UNU_STP)
 @~Japanese
 @name CPS-AIO ECU AO UNUSUAL STOP VALUES (AO_UNU_STP)
*/
/// @{
#define CPS_AIO_ECU_AO_UNU_STP_AO_CLK_ERR	0x0001	///< AO CLOCK ERROR
#define CPS_AIO_ECU_AO_UNU_STP_AO_EMPTY	0x0100	///< AO EMPTY
#define CPS_AIO_ECU_AO_UNU_STP_AO_EXT_STP	0x1000	///< AO EXTERNAL STOP
/// @}
/*!
 @~English
 @name CPS-AIO AI SET EXCHANGE VALUES (SET_EXC)
 @~Japanese
 @name CPS-AIO AI SET EXCHANGE VALUES (SET_EXC)
*/
/// @{
#define CPS_AIO_AI_SET_EXC_ENABLE_SINGLE	0x0000
#define CPS_AIO_AI_SET_EXC_ENABLE_MULTI	0x0001
/// @}
/*!
 @~English
 @name CPS-AIO AI SET CALIBRATION TERMS VALUES (SET_CAL_TRM )
 @~Japanese
 @name CPS-AIO AI SET CALIBRATION TERMS VALUES (SET_CAL_TRM )
*/
/// @{
#define CPS_AIO_AI_SET_CAL_TRM_SELECT_OFFSET	0x0000
#define CPS_AIO_AI_SET_CAL_TRM_SELECT_GAIN	0x0001
#define CPS_AIO_AI_SET_CAL_TRM_RANGE_PM10V	0x0000
/// @}
/*!
 @~English
 @name CPS-AIO AO SET EXCHANGE VALUES (SET_EXC)
 @~Japanese
 @name CPS-AIO AO SET EXCHANGE VALUES (SET_EXC)
*/
/// @{
/*  */
#define CPS_AIO_AO_SET_EXC_ENABLE_SINGLE	0x0000
#define CPS_AIO_AO_SET_EXC_ENABLE_MULTI	0x0001
/// @}
/*!
 @~English
 @name CPS-AIO AO SET CALIBRATION TERMS VALUES ( SET_CAL_TRM )
 @~Japanese
 @name CPS-AIO AO SET CALIBRATION TERMS VALUES ( SET_CAL_TRM )
*/
/// @{
#define CPS_AIO_AO_SET_CAL_TRM_SELECT_OFFSET	0x0000
#define CPS_AIO_AO_SET_CAL_TRM_SELECT_GAIN	0x0001
#define CPS_AIO_AO_SET_CAL_TRM_RANGE_PM10V	0x0000
/// @}
/**************************************************/
