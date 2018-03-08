/*** cps.h ******************************/

extern void contec_cps_micro_delay_sleep(unsigned long usec , unsigned int isUsedDelay);
extern void contec_cps_micro_sleep(unsigned long usec);
extern void contec_cps_micro_delay(unsigned long usec);

extern unsigned char contec_mcs341_controller_cpsDevicesInit(void);
extern unsigned char contec_mcs341_device_FindCategory( int *startIndex,int CategoryNum );
extern unsigned char contec_mcs341_device_IsCategory( int targetDevNum ,int CategoryNum );
extern unsigned char contec_mcs341_device_FindDevice( int *startIndex, int DeviceNum );
extern unsigned short contec_mcs341_device_productid_get( int dev );
extern unsigned short contec_mcs341_device_physical_id_get( int dev );
extern unsigned char contec_mcs341_device_mirror_get( int dev , int num );
extern unsigned char contec_mcs341_device_deviceNum_get( unsigned long baseAddr );
extern unsigned char contec_mcs341_device_serial_channel_get( unsigned long baseAddr );
extern unsigned char contec_mcs341_controller_setInterrupt( int GroupNum , int isEnable );
extern unsigned char contec_mcs341_controller_getInterrupt( int GroupNum );
extern unsigned char contec_mcs341_controller_getDeviceNum( void );
extern unsigned char contec_mcs341_device_logical_id_set( int dev, unsigned char valb );
extern unsigned char contec_mcs341_device_logical_id_get( int dev );
extern unsigned char contec_mcs341_device_logical_id_all_clear( int dev );
extern unsigned short contec_mcs341_device_extension_value_set( int dev, int cate, int num, unsigned short valw );
extern unsigned short contec_mcs341_device_extension_value_get( int dev, int cate, int num );
extern unsigned short contec_mcs341_device_extension_value_all_clear( int dev, int cate );

extern void contec_mcs341_device_outb(unsigned int dev, unsigned int offset, unsigned char valb );
extern void contec_mcs341_device_inpb(unsigned int dev, unsigned int offset, unsigned char *valb );
extern void contec_mcs341_device_outw(unsigned int dev, unsigned int offset, unsigned short valw );
extern void contec_mcs341_device_inpw(unsigned int dev, unsigned int offset, unsigned short *valw );
extern unsigned char contec_mcs341_device_board_version_get( int dev );
extern unsigned char contec_mcs341_device_fpga_version_get( int dev );


extern unsigned int contec_mcs341_controller_cpsChildUnitInit(unsigned int childType);
/*********************************************************/
