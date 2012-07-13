
#include "libtcod.hpp"

#include "Vector.h"
#include "Pure.h"
#include "Grid.h"

#include <cstdlib>
#include <cstdio>
#include <algorithm>

typedef Vector<int,2> Vec;

Vec mapDims( 80, 60 );

Grid<char> grid( 80, 60, '#' );

struct Actor
{
    Vec pos;
};

#include <cstdarg>
void die( const char* fmt, ... )
{
    va_list vl;
    va_start( vl, fmt );
    vfprintf( stderr, fmt, vl );
    va_end( vl );
    exit( 1 );
}

void die_perror( const char* msg )
{
    perror( msg );
    exit( 1 );
}

// Adjusts v to keep in on screen. (Based on mapDims.)
Vec keep_inside( const TCODConsole&, Vec );

// Waits for the next pressed key. 
// Does not return on key lift.
// Converts number/keypad input to '0'-'9'.
int next_pressed_key();

int main()
{
    FILE* mapgen = popen( "./mapgen/c++/mapgen", "r" );

    if( not mapgen ) 
        die_perror( "Error running mapgen" );

    // Read the map in, line by line.
    for( unsigned int y=0; y < grid.height; y++ ) {
        char line[500];
        fgets( line, sizeof line, mapgen );
        
        if( line[0] != '#' ) 
            die( "mapgen: Too few rows. Expected %d, got %d.\n", 
                 grid.height, y );
        if( line[grid.width-1] != '#' )
            die( "mapgen: Wrong number of columns. Expected %d, got (N\\A).\n",
                 grid.width );

        std::copy_n( line, grid.width, grid.row_begin(y) );
    }

    char spawnpt[50];
    if( not fgets(spawnpt, sizeof spawnpt, mapgen) or spawnpt[0] != 'X' )
        die( "No spawn point!" );

    Vec pos(0,0);
    sscanf( spawnpt, "X %u %u", &pos.x(), &pos.y() );

    pclose( mapgen );
    

    using Cons = TCODConsole;

    Cons::initRoot( mapDims.x(), mapDims.y(), "test rogue" );
    Cons::root->setDefaultBackground( TCODColor::black );
    Cons::root->setDefaultForeground( TCODColor::white );
    Cons::disableKeyboardRepeat();

    bool gameOver = false;
    while( not gameOver and not Cons::isWindowClosed() ) 
    {
        Cons::root->clear();
        for( unsigned int x=0; x < grid.width; x++ )
            for( unsigned int y=0; y < grid.height; y++ ) {
                char c = grid.get( x, y );
                Cons::root->setChar( x, y, c );

                using C = TCODColor;
                C fg = C::white;
                C bg = C::black;
                if( c == '#' ) {
                    fg = C::darkestAzure;
                } else if( c == '.' ) {
                    fg = C::darkestHan;
                    bg = C::darkestGrey;
                }
                
                Cons::root->setCharForeground( x, y, fg );
                Cons::root->setCharBackground( x, y, bg );
            }

        Cons::root->setChar( pos.x(), pos.y(), '@' );
        Cons::root->setCharForeground( pos.x(), pos.y(), TCODColor::white );
        // Leave background as is.

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

        pos = keep_inside( *Cons::root, pos );
    }
}

int _clamp_range( int x, int min, int max )
{
    if( x < min )      x = min;
    else if( x > max ) x = max;
    return x;
}

Vec keep_inside( const TCODConsole& cons, Vec v )
{
    v.x( _clamp_range(v.x(), 1, cons.getWidth()-1) );
    v.y( _clamp_range(v.y(), 1, cons.getWidth()-1) );
    return v;
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
