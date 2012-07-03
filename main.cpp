
#include "libtcod.hpp"

#include "Vector.h"
#include "Pure.h"

typedef Vector<int,2> Vec;

Vec mapDims( 80, 60 );

int clamp_range( int x, int min, int max )
{
    if( x < min )      x = min;
    else if( x > max ) x = max;
    return x;
}

Vec clamp_map( Vec v )
{
    v.x( clamp_range(v.x(), 1, mapDims.x()-1) );
    v.y( clamp_range(v.y(), 1, mapDims.y()-1) );
    return v;
}

int main()
{
    using Cons = TCODConsole;
    Cons::initRoot( mapDims.x(), mapDims.y(), "test rogue" );
    Cons::root->setDefaultBackground( TCODColor::black );
    Cons::root->setDefaultForeground( TCODColor::white );

    Cons::disableKeyboardRepeat();

    Vec pos(1,1);

    bool gameOver = false;
    while( not gameOver and not Cons::isWindowClosed() ) 
    {
        Cons::flush();

        TCOD_key_t key = Cons::waitForKeypress(true);
        if( key.pressed ) switch( key.c ) {
          case 'q': gameOver = true; break;
          case 'h': case '5': pos.x() -= 1; break;
          case 'l': case '8': pos.x() += 1; break;
          case 'k': case '2': pos.y() -= 1; break;
          case 'j': case '6': pos.y() += 1; break;
          default: ;
        }
        pos = clamp_map( pos );

        Cons::root->clear();
        Cons::root->setChar( pos.x(), pos.y(), '@' );
    }
}

