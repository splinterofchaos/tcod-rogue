
#include "libtcod.hpp"

#include "Vector.h"
#include "Pure.h"

typedef Vector<int,2> Vec;

int main()
{
    TCODConsole::initRoot( 80, 60, "test rogue" );

    bool gameOver = false;
    while( not gameOver and not TCODConsole::isWindowClosed() ) 
    {
        Vec pos(1,1);

        TCODConsole::flush();
        TCOD_key_t key = TCODConsole::checkForKeypress();
        if( /*key.pressed*/ true ) 
        {
            switch( key.c ) {
              case 'q': gameOver = true; break;
              case 'h': case '5': pos.x() -= 1; break;
              case 'j': case '8': pos.x() += 1; break;
              case 'k': case '2': pos.y() -= 1; break;
              case 'l': case '6': pos.y() += 1; break;
              default: break;
            }
        }

        TCODConsole::setChar( pos.x(), pos.y(), '@' );
    }
}

