
CC = g++ -std=c++0x

LDFLAGS = -Llibtcod -ltcod{,xx} 
CFLAGS = -Ilibtcod/include -Wall

rogue : main.cpp makefile Pure.h Vector.h libtcod
	${CC} -o rogue main.cpp ${CFLAGS} ${LDFLAGS}

libtcod : 
	hg clone https://bitbucket.org/jice/libtcod          
	cmake libtcod 
	mv Makefile libtcod
	make -f libtcod/Makefile
	make -f libtcod/Makefile install

