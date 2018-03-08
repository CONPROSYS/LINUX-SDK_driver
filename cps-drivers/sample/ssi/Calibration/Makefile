#CC = gcc
CC = arm-linux-gnueabi-gcc

INC = -I../../../include/

SRC_NAME001=Calibration
SRC_NAME002=Linerity
#SRC_NAME003=
#SRC_NAME004=

all:${SRC_NAME001}.o ${SRC_NAME002}.o

${SRC_NAME001}.o:
	${CC} ${INC} -o ${SRC_NAME001} ${SRC_NAME001}.c -lCpsSsi -lm

${SRC_NAME002}.o:
	${CC} ${INC} -o ${SRC_NAME002} ${SRC_NAME002}.c -lCpsSsi -lm

#${SRC_NAME003}.o:
#	${CC} ${INC} -o ${SRC_NAME003} ${SRC_NAME003}.c -lCpsSsi -lm

#${SRC_NAME004}.o:
#	${CC} ${INC} -o ${SRC_NAME004} ${SRC_NAME004}.c -lCpsSsi -lm

clean:
	rm -f ${SRC_NAME001} ${SRC_NAME002} 
