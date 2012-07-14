
CC = g++ -std=c++0x

LDFLAGS = -Llibtcod -ltcod -ltcodxx
CFLAGS = -Ilibtcod/include -Wall

obj = .grid.o .random.o


rogue : main.cpp makefile Pure.h Vector.h libtcod ${obj}
	make -C mapgen/c++
	${CC} -o rogue main.cpp ${obj} ${CFLAGS} ${LDFLAGS}

.random.o : random.*
	${CC} -c -o .random.o random.cpp ${CFLAGS}

.grid.o : Grid.*
	${CC} -c -o .grid.o Grid.cpp ${CFLAGS} 

libtcod : 
	hg clone https://bitbucket.org/jice/libtcod          
	cmake libtcod 
	mv Makefile libtcod
	make -f libtcod/Makefile
	make -f libtcod/Makefile install
