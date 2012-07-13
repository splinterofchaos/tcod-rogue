
#include "libtcod.hpp"

#include "Vector.h"
#include "Pure.h"
#include "Grid.h"

#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <memory>

typedef Vector<int,2> Vec;

Vec mapDims( 80, 60 );

Vec pos(0,0); // The player's position.

struct Tile
{
    bool seen, visible;
    char c;

    Tile() : seen(false), visible(false), c(' ') {}

    // Allow explicit construction.
    Tile( char c ) : seen(false), visible(false), c(c) {}
};

Grid<Tile> grid( 80, 60, '#' );
void initialize_grid();

struct Actor
{
    Vec pos;
};

void die( const char* fmt, ... );
void die_perror( const char* msg );

/* Adjust vec to keep it on screen. */
Vec keep_inside( const TCODConsole&, Vec );

/*
 * Wait for the next pressed key. 
 * Do not return on key lift. Convert number/keypad input to '0'-'9'.
 */
int next_pressed_key();

/* 
 * Do all rendering. 
 * Calculate FOV based on player's position, draw all discovered tiles,
 * colorize, and print to libtcod's root.
 */
void render();

int main()
{
    initialize_grid();

    TCODConsole::initRoot( mapDims.x(), mapDims.y(), "test rogue" );
    TCODConsole::root->setDefaultBackground( TCODColor::black );
    TCODConsole::root->setDefaultForeground( TCODColor::white );
    TCODConsole::disableKeyboardRepeat();

    bool gameOver = false;
    while( not gameOver and not TCODConsole::isWindowClosed() ) 
    {
        render();

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

        pos = keep_inside( *TCODConsole::root, pos );
    }
}

void initialize_grid()
{
    FILE* mapgen = popen( "./mapgen/c++/mapgen", "r" );

    // Read the map in, line by line.
    for( unsigned int y=0; y < grid.height; y++ ) {
        char line[500];
        fgets( line, sizeof line, mapgen );
        
        // Will fail here on first iteration if mapgen failed to open (got 0).
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

    sscanf( spawnpt, "X %u %u", &pos.x(), &pos.y() );

    pclose( mapgen );
}

void render()
{
    static std::unique_ptr<TCODMap> fov;

    if( not fov or fov->getWidth()  != grid.width 
                or fov->getHeight() != grid.height ) 
    {
        fov.reset( new TCODMap(grid.width, grid.height) );
        for( unsigned int x=0; x < grid.width; x++ )
            for( unsigned int y=0; y < grid.height; y++ ) {
                bool walkable = (grid.get(x,y).c == '.');
                fov->setProperties( x, y, walkable, not walkable );
            }
    }

    // Update FOV if the player has moved.
    static Vec lastPos(-1,-1);
    if( pos != lastPos ) {
        fov->computeFov( pos.x(), pos.y(), 0, true, FOV_PERMISSIVE_4 );
        lastPos = pos;
    }

    TCODConsole::root->clear();

    // Draw onto root (r).
    for( unsigned int x=0; x < grid.width; x++ )
    {
        for( unsigned int y=0; y < grid.height; y++ ) 
        {
            /*
             * Draw any tile, except those the player hasn't discovered, but
             * color them according to whether they can be seen now, or have
             * been seen.
             */
            Tile& t = grid.get( x, y );

            if( fov->isInFov(x,y) )
                t.seen = t.visible = true;
            else
                t.visible = false;

            if( not t.seen )
                // Not in view, nor discovered.
                continue;

            TCODConsole::root->setChar( x, y, t.c );

            using C = TCODColor;
            C fg = C::white;
            C bg = C::black;
            if( t.c == '#' ) {
                if( t.visible ) {
                    bg = C::darkGrey;
                    fg = C::darkAzure;
                }
                else {
                    fg = C::darkestAzure;
                }
            } else if( t.c == '.' ) {
                if( t.visible ) {
                    bg = C::grey;
                    fg = C::darkestHan;
                } else {
                    bg = C::darkestGrey;
                    fg = C::lightBlue;
                }
            }

            TCODConsole::root->setCharForeground( x, y, fg );
            TCODConsole::root->setCharBackground( x, y, bg );
        }
    }

    TCODConsole::root->setChar( pos.x(), pos.y(), '@' );
    TCODConsole::root->setCharForeground( pos.x(), pos.y(), TCODColor::white );
    // Leave background as is.

    TCODConsole::flush();
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
