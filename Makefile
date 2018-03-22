CC ?= gcc
CFLAGS ?= -O2 -Wall -std=c99

smon: main.o system.o util.o
	${CC} main.o system.o util.o -o smon

main.o: main.c system.h cpu.h disk.h interface.h
	${CC} ${CFLAGS} -c main.c

system.o: system.c system.h util.h cpu.h disk.h interface.h
	${CC} ${CFLAGS} -c system.c

util.o: util.c util.h
	${CC} ${CFLAGS} -c util.c

clean:
	rm -f *.o smon
