#CC = gcc
CC = arm-linux-gnueabi-gcc

INC = -I../../../include/

SRC_NAME001=queryDeviceName

all:${SRC_NAME001}.o 

${SRC_NAME001}.o:
	${CC} ${INC} -o ${SRC_NAME001} ${SRC_NAME001}.c -lCpsSsi -lm

clean:
	rm -f ${SRC_NAME001} 
