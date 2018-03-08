/*
 *  Header for cps-common-io.
 *
 *  Copyright (C) 2015 Syunsuke Okamoto.<okamoto@contec.jp>
 *
*/

#ifndef CPS_COMMON_IO_H
#define CPS_COMMON_IO_H

#include <linux/gpio.h>
#include <linux/ioctl.h>
#define CPS_FPGA_ACCESS_WORD		0	///< 16bit Access mode
#define CPS_FPGA_ACCESS_BYTE_HIGH	1	///< 8bit Access mode( high )
#define CPS_FPGA_ACCESS_BYTE_LOW	2	///< 8bit Access mode( low )
#define CPS_FPGA_ACCESS_RESERVED	3	///< Reserved

#ifndef GPIO_TO_PIN
  #define GPIO_TO_PIN( bank, gpio )	( 32 * (bank) + (gpio) )
#endif

#define CPS_FPGA_BYTE_LOW	GPIO_TO_PIN( 1, 31 )	///< CPS-MCS341 8bit Access (Low)
#define CPS_FPGA_BYTE_HIGH	GPIO_TO_PIN( 2, 5 )	///< CPS-MCS341 8bit Access (High)

/**
	@~English
	@brief FPGA initialize.
	@return success: 0 , failed: otherwise 0
 	@~Japanese
	@brief FPGAの初期化処理。
	@return 成功: 0 , 失敗: 0以外
**/
int cps_fpga_init(void)
{
	int ret;

	/* clear gpio free */
	gpio_free(CPS_FPGA_BYTE_LOW);
	gpio_free(CPS_FPGA_BYTE_HIGH);

	ret = gpio_request(CPS_FPGA_BYTE_LOW, "cps_fpga_byte_low");
	if( ret )	return ret;
	ret = gpio_request(CPS_FPGA_BYTE_HIGH, "cps_fpga_byte_high");

	return ret;

}

/**
	@~English
	@brief Set FPGA Access　Mode.
	@param mode : mode ( CPS_FPGA_ACCESS_WORD, CPS_FPGA_ACCESS_BYTE_HIGH and CPS_FPGA_ACCESS_BYTE_LOW )
	@return success: 0 , failed: otherwise 0
 	@~Japanese
	@brief この関数はFPGAのアクセスモードを設定します。
	@param mode : モード ( CPS_FPGA_ACCESS_WORD, CPS_FPGA_ACCESS_BYTE_HIGH もしくは CPS_FPGA_ACCESS_BYTE_LOW )
	@return 成功: 0 , 失敗: 0以外
**/
int cps_fpga_access( int mode )
{
	int ret = 0;

	switch( mode ){
	case CPS_FPGA_ACCESS_WORD : 
			ret = gpio_direction_output( CPS_FPGA_BYTE_LOW, 0 );
			ret = gpio_direction_output( CPS_FPGA_BYTE_HIGH, 0 );
			break;
	case CPS_FPGA_ACCESS_BYTE_LOW:
			ret = gpio_direction_output( CPS_FPGA_BYTE_LOW, 1 );
			ret = gpio_direction_output( CPS_FPGA_BYTE_HIGH, 0 );
			break;
	case CPS_FPGA_ACCESS_BYTE_HIGH:
			ret = gpio_direction_output( CPS_FPGA_BYTE_LOW, 0 );
			ret = gpio_direction_output( CPS_FPGA_BYTE_HIGH, 1 );
			break;
	}

	return ret;

}


/********************** Input / Output ****************************/
/**
	@~English
	@brief Read FPGA Access　8bit mode.
	@param addr : Address
	@param data : value
 	@~Japanese
	@brief この関数はFPGAのアドレスに8ビットアクセスしその値を読み出します。
	@param addr : アドレス
	@param data : データの値
**/
void cps_common_inpb(unsigned long addr, unsigned char *data )
{

	if( addr & 0x01 ) {
		cps_fpga_access(CPS_FPGA_ACCESS_BYTE_HIGH); // cps_common_io.h
	}else{
		cps_fpga_access(CPS_FPGA_ACCESS_BYTE_LOW); // cps_common_io.h
	}

	*data = readb( (unsigned char*) addr);

	cps_fpga_access(CPS_FPGA_ACCESS_WORD); // cps_common_io.h

}

/**
	@~English
	@brief Write FPGA Access　8bit mode.
	@param addr : Address
	@param data : value
 	@~Japanese
	@brief この関数はFPGAのアドレスに8ビットアクセスしその値を書き込みます。
	@param addr : アドレス
	@param data : データの値
**/
void cps_common_outb(unsigned long addr, unsigned char data )
{
	if( ( addr ) & 0x01 ) {
		cps_fpga_access(CPS_FPGA_ACCESS_BYTE_HIGH); // cps_common_io.h
	}else{
		cps_fpga_access(CPS_FPGA_ACCESS_BYTE_LOW); // cps_common_io.h
	}

	writeb(data, (unsigned char*)addr);

	cps_fpga_access(CPS_FPGA_ACCESS_WORD); // cps_common_io.h

}

/**
	@~English
	@brief Read FPGA Access　16bit mode.
	@param addr : Address
	@param data : value
 	@~Japanese
	@brief この関数はFPGAのアドレスに16ビットアクセスしその値を読み出します。
	@param addr : アドレス
	@param data : データの値
**/
void cps_common_inpw(unsigned long addr, unsigned short *data )
{

	cps_fpga_access(CPS_FPGA_ACCESS_WORD); // cps_common_io.h

	*data = readw( (unsigned char*)addr);

}

/**
	@~English
	@brief Write FPGA Access　16bit mode.
	@param addr : Address
	@param data : value
 	@~Japanese
	@brief この関数はFPGAのアドレスに16ビットアクセスしその値を書き込みます。
	@param addr : アドレス
	@param data : データの値
**/
void cps_common_outw(unsigned long addr, unsigned short data )
{

	cps_fpga_access(CPS_FPGA_ACCESS_WORD); // cps_common_io.h

	writew(data, (unsigned char*)addr);

}

/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
/*- Memory Allocate and Release                              -*/
/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/

#define CPS_COMMON_MEM_NONREGION	0 ///< リージョン確認なし
#define CPS_COMMON_MEM_REGION			1	///< リージョン確認あり

/**
	@~English
	@brief Allocate virtual memory mapping.
	@param baseMemory : mamory address
	@param areaSize : size
	@param drvName : Driver Name
	@param isRegion : Region ( CPS_COMMON_MEM_NONREGION or CPS_COMMON_MEM_REGION )
	@return success: Mapped Address , failed: less than 0
 	@~Japanese
	@brief この関数は仮想メモリのマッピングを生成します。
	@param baseMemory : メモリアドレス
	@param areaSize : サイズ
	@param drvName : ドライバ名
	@param isRegion : リージョン ( CPS_COMMON_MEM_NONREGION or CPS_COMMON_MEM_REGION )
	@return 成功: メモリアドレス , 失敗: 0未満
**/
static void __iomem *cps_common_mem_alloc( unsigned long baseMemory, unsigned int areaSize, char* drvName , unsigned int isRegion )
{
	int ret = 0;
	void __iomem *mappedAddress = NULL;
	if( isRegion ){
		if (!request_mem_region((resource_size_t)baseMemory, areaSize, drvName)) {
			ret = -EBUSY;
		}else{
			mappedAddress = ioremap_nocache((resource_size_t)baseMemory, areaSize );
			if ( !mappedAddress ) {
				release_mem_region(baseMemory, areaSize);
				ret = -ENOMEM;
			}
		}
	}
	else{
		mappedAddress = ioremap_nocache((resource_size_t)baseMemory, areaSize );
		if ( !mappedAddress ) ret = -ENOMEM;
	}

	return mappedAddress;
}

/**
	@~English
	@brief Free virtual memory mapping.
	@param baseMemory : mamory address
	@param areaSize : size
	@param mappedAddress : Virtual Memory Address
	@param isRegion : Region ( CPS_COMMON_MEM_NONREGION or CPS_COMMON_MEM_REGION )
	@return success: Mapped Address , failed: less than 0
 	@~Japanese
	@brief この関数は仮想メモリのマッピングを解放します。
	@param baseMemory : メモリアドレス
	@param areaSize : サイズ
	@param mappedAddress : 仮想メモリアドレス
	@param isRegion : リージョン ( CPS_COMMON_MEM_NONREGION or CPS_COMMON_MEM_REGION )
	@return 成功: メモリアドレス , 失敗: 0未満
**/
static int cps_common_mem_release( unsigned long baseMemory, unsigned int areaSize, unsigned char __iomem *mappedAddress ,unsigned int isRegion)
{
	int ret = 0;

	if ( mappedAddress ) {
		if ( isRegion ){
			release_mem_region(baseMemory, areaSize);
		}
		iounmap(mappedAddress);
	}

	return ret;
}

#endif

