
CC = g++ -std=c++0x

LDFLAGS = -L/home/soc/src/rogue/libtcod -ltcod{,xx} 
CFLAGS = -I/home/soc/src/rogue/libtcod/include

rogue : main.cpp makefile Pure.h Vector.h
	${CC} -o rogue main.cpp ${CFLAGS} ${LDFLAGS}
