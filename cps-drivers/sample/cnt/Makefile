#CC = gcc
CC = arm-linux-gnueabi-gcc

INC = -I../../include/

SRC_NAME01=cnt
SRC_NAME02=check_cnts
SRC_NAME03=queryDeviceName

all:${SRC_NAME01}.o ${SRC_NAME02}.o ${SRC_NAME03}.o



${SRC_NAME01}.o :	
	${CC} ${INC} -o ${SRC_NAME01} ${SRC_NAME01}.c -lCpsCnt

${SRC_NAME02}.o :	
	${CC} ${INC} -o ${SRC_NAME02} ${SRC_NAME02}.c -lCpsCnt

${SRC_NAME03}.o :	
	${CC} ${INC} -o ${SRC_NAME03} ${SRC_NAME03}.c -lCpsCnt


clean:
	rm -f ${SRC_NAME01} ${SRC_NAME02} ${SRC_NAME03}

