
#include "libtcod.hpp"

#include "Vector.h"
#include "Pure.h"

typedef Vector<int,2> Vec;

int main()
{
    using Cons = TCODConsole;
    Cons::initRoot( 80, 60, "test rogue" );
    Cons::root->setDefaultBackground( TCODColor::black );
    Cons::root->setDefaultForeground( TCODColor::white );

    Vec pos(1,1);

    bool gameOver = false;
    while( not gameOver and not Cons::isWindowClosed() ) 
    {
        Cons::flush();
        TCOD_key_t key = Cons::checkForKeypress();
        switch( key.c ) {
          case 'q': gameOver = true; break;
          case 'h': case '5': pos.x() -= 1; break;
          case 'l': case '8': pos.x() += 1; break;
          case 'k': case '2': pos.y() -= 1; break;
          case 'j': case '6': pos.y() += 1; break;
          default: ;
        }

        Cons::root->clear();
        Cons::root->setChar( pos.x(), pos.y(), '@' );
    }
}

