#CC = gcc
CC = arm-linux-gnueabi-gcc

INC = -I../../../include/

all:CalibWrite.o CalibRead.o CalibClear.o

CalibWrite.o:
	${CC} ${INC} -o CalibWrite CalibWrite.c -lCpsSsi -lm

CalibRead.o:
	${CC} ${INC} -o CalibRead CalibRead.c -lCpsSsi -lm

CalibClear.o:
	${CC} ${INC} -o CalibClear CalibClear.c -lCpsSsi -lm

clean:
	rm -f ssi CalibWrite CalibRead CalibClear
