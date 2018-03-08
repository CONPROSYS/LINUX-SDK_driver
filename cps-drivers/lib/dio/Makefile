#CC = gcc
#LD = ld
CC := arm-linux-gnueabi-gcc
LD := arm-linux-gnueabi-ld
TARGET=libCpsDio.so
OBJ=libcpsdio.o
SRC=libcpsdio.c
CFLAGS= -g -Wall

VERSION =1.0.6

# CPS_SDK_MACRO_CHECK ( without CONPROSYS SDK )
ifneq "$(CPS_SDK_INSTALL_FULLDIR)" ""
TARGET_ROOTFS   := ${CPS_SDK_INSTALL_FULLDIR}/rootfs
endif

ifneq "$(CPS_SDK_ROOTDIR)" ""
INCLUDE= -I$(CPS_SDK_ROOTDIR)/driver/cps-drivers/include
EXTRA_CFLAGS	+= -DCONFIG_CONPROSYS_SDK
CPS_LIB_REELASE_DIR:=../../release
else
INCLUDE= -I../../include/
CPS_LIB_REELASE_DIR:=../release
endif


all:$(OBJ) ${TARGET}

$(OBJ):	
	${CC} ${INCLUDE} ${LD_FLAGS} $(SRC) -c -fPIC -o $(OBJ)

$(TARGET):
	${CC} ${INCLUDE} ${LD_FLAGS}  -shared -O2 -Wl,-soname,$(TARGET) -o $(TARGET) $(OBJ)

install:
	cp -p $(TARGET) $(TARGET_ROOTFS)/usr/local/lib/$(TARGET).$(VERSION)
	@if [ ! -f $(TARGET_ROOTFS)/usr/local/lib/$(TARGET) ]; then \
	( cd $(TARGET_ROOTFS)/usr/local/lib/ && ln -s $(TARGET).$(VERSION) $(TARGET) ) \
	fi

release_copy:
	cp -p ./$(TARGET) $(CPS_LIB_REELASE_DIR)/$(TARGET).$(VERSION)

clean:
	rm -f $(OBJ) $(TARGET)
