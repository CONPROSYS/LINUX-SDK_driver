/*** cpscnt_devdata.h ******************************/

/*!
 @~English
 @name Address Register
 @brief CPS-CNT-3202I
 @~Japanese
 @name レジスタアドレス
 @brief デバイス： CPS-CNT-3202I
*/
/// @{
#define	OFFSET_CNT_ADDRESS_COMMAND				0x0	///< Command address register.
#define	OFFSET_CNT_ADDRESS_DATA				0x1	///< Data address register.
#define	OFFSET_CNT_COMMAND_DATALOCK				0x18	///< Device Data Lock register (Write Lock) Ver.0.9.3 added.

/// @}

/*!
 @~English
 @name CPS-CNT Command Address (define)
 @~Japanese
 @name CNT コマンド アドレス
*/
/// @{

#define	CPS_CNT_COMMAND_COUNT(n)				( n * 0x05 )
#define	CPS_CNT_COMMAND_MODE(n)				( n * 0x05 + 0x01 )
#define	CPS_CNT_COMMAND_Z_PHASE(n)				( n * 0x05 + 0x02 )
#define	CPS_CNT_COMMAND_COMPARE_REG(n)			( n * 0x05 + 0x03 )
#define	CPS_CNT_COMMAND_DIGITAL_FILTER(n)		( n * 0x05 + 0x04 )
#define	CPS_CNT_COMMAND_LATCH_COUNT			( 0x14 )
#define	CPS_CNT_COMMAND_INTERRUPT_MASK			( 0x15 )
#define	CPS_CNT_COMMAND_SENCE_PORT				( 0x16 )
#define	CPS_CNT_COMMAND_TIMER_DATA				( 0x17 )
#define	CPS_CNT_COMMAND_TIMER_START			( 0x18 )
#define	CPS_CNT_COMMAND_ONESHOT_PULSE_WIDTH		( 0x19 )
#define	CPS_CNT_COMMAND_SELECT_COMMON_INPUT		( 0x1A )

#define	CPS_CNT_COMMAND_STATUS(n)				( n * 0x05 + 0x01 )
#define	CPS_CNT_COMMAND_SENCE_READ				( 0x16 )

/// @}

/*!
 @~English
 @name CPS-CNT Write Lock /UnLock
 @brief Version.0.9.3 Added.
 @~Japanese
 @name CNT 書き込みロック / アンロック
 @brief Version.0.9.3 実装
*/
/// @{
#define CPS_CNT_DATA_LOCK			0x0			///<	データロック
#define CPS_CNT_DATA_UNLOCK			0xAA55		///<	データアンロック
/// @}
/*!
 @~English
 @name CPS-CNT Command Read or Write Flag (define)
 @~Japanese
 @name CNTコマンド 読込 /書込　フラグ
*/
/// @{
#define CPS_CNT_COMMAND_READ	0x01	///< Read
#define CPS_CNT_COMMAND_WRITE 0x02	///< Write
/// @}

/**************************************************/
