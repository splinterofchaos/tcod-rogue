
#include "libtcod.hpp"

#include "Vector.h"
#include "Pure.h"

typedef Vector<int,2> Vec;

Vec mapDims( 80, 60 );

struct OffCons // Off-screen Console
{
    TCODConsole cons;
    Vec offset;
    Vec dims;

    OffCons( Vec dims, Vec off=Vec(0,0) ) 
        : cons( dims.x(), dims.y() ), 
          offset( off ),
          dims( dims )
    {
    }

    OffCons( int x, int y ) : OffCons(Vec(x,y)) {}
    OffCons( int x, int y, int offx, int offy ) 
        : OffCons( Vec(x,y), Vec(offx,offy) ) 
    {
    }

    void clear() { cons.clear(); };
    void put( Vec v, char c ) { cons.setChar( v.x(), v.y(), c ); }
};

struct Actor
{
    Vec pos;
};

// Adjusts v to keep in on screen. (Based on mapDims.)
Vec keep_inside( const OffCons&, Vec );

// Blit an off-screen console to the root.
void blit_root( const OffCons& oc );

// Waits for the next pressed key. 
// Does not return on key lift.
// Converts number/keypad input to '0'-'9'.
int next_pressed_key();

int main()
{
    using Cons = TCODConsole;

    Cons::initRoot( mapDims.x(), mapDims.y(), "test rogue" );
    Cons::root->setDefaultBackground( TCODColor::black );
    Cons::root->setDefaultForeground( TCODColor::white );
    Cons::disableKeyboardRepeat();

    OffCons dungeonCons( 80-2, 60-2, 0, 0 );
    dungeonCons.cons.setDefaultBackground( TCODColor::black );
    dungeonCons.cons.setDefaultForeground( TCODColor::white );
    dungeonCons.cons.clear();

    Vec pos(0,0);

    bool gameOver = false;
    while( not gameOver and not Cons::isWindowClosed() ) 
    {

        dungeonCons.clear();
        dungeonCons.put( pos, '@' );

        Cons::root->clear();
        blit_root( dungeonCons );

        Cons::flush();

        switch( next_pressed_key() ) {
          case 'q': gameOver = true; break;

          // Cardinal directions.
          case 'h': case '4': case TCODK_LEFT:  pos.x() -= 1; break;
          case 'l': case '6': case TCODK_RIGHT: pos.x() += 1; break;
          case 'k': case '8': case TCODK_UP:    pos.y() -= 1; break;
          case 'j': case '2': case TCODK_DOWN:  pos.y() += 1; break;

          // Diagonals.
          case 'y': case '7': pos += Vec(-1,-1); break;
          case 'u': case '9': pos += Vec(+1,-1); break;
          case 'b': case '1': pos += Vec(-1,+1); break;
          case 'n': case '3': pos += Vec(+1,+1); break;

          default: ;
        }

        pos = keep_inside( dungeonCons, pos );
    }
}

int _clamp_range( int x, int min, int max )
{
    if( x < min )      x = min;
    else if( x > max ) x = max;
    return x;
}

Vec keep_inside( const OffCons& oc, Vec v )
{
    v.x( _clamp_range(v.x(), 0, oc.dims.x()-1) );
    v.y( _clamp_range(v.y(), 0, oc.dims.y()-1) );
    return v;
}

void blit_root( const OffCons& oc )
{
    TCODConsole::blit ( 
        &oc.cons, 0, 0, oc.dims.x(), oc.dims.y(),
        TCODConsole::root, oc.offset.x(), oc.offset.y()
    );
}


int next_pressed_key()
{
    TCOD_key_t key;
        
    do key = TCODConsole::waitForKeypress(false);
    while( not key.pressed );

    int k = key.vk == TCODK_CHAR ? key.c : key.vk;

    if( k >= TCODK_0 and k <= TCODK_9 )
        k = '0' + (k - TCODK_0);
    if( k >= TCODK_KP0 and k <= TCODK_KP9 )
        k = '0' + (k - TCODK_KP0);

    return k;
}
