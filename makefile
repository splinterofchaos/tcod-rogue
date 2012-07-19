
CC = g++ -std=c++0x

LDFLAGS = -Llibtcod -ltcod -ltcodxx
CFLAGS  = -Wall -Wextra 

obj = .grid.o .random.o .message.o


rogue : main.cpp makefile Pure.h Vector.h libtcod ${obj}
	make -C mapgen/c++
	${CC} -o rogue main.cpp -Ilibtcod/include ${obj} ${CFLAGS} ${LDFLAGS}

.random.o : random.*
	${CC} -c -o .random.o random.cpp ${CFLAGS}

.grid.o : Grid.*
	${CC} -c -o .grid.o Grid.cpp ${CFLAGS} 

.message.o : Message.*
	${CC} -c -o .message.o Message.cpp -Ilibtcod/include ${CFLAGS}

libtcod : 
	hg clone https://bitbucket.org/jice/libtcod          
	cmake libtcod 
	mv Makefile libtcod
	make -f libtcod/Makefile
	make -f libtcod/Makefile install

clean :
	rm .*.o
