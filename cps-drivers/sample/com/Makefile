#CC=gcc
CC=arm-linux-gnueabi-gcc

all: comread comsend comtest 

comread:
	${CC} -o comread comread.c

comsend:
	${CC} -o comsend comsend.c

comtest:
	${CC} -o comtest comtest.c


clean:
	rm -f comread comsend  comtest
