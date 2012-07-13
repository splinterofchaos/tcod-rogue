
#include "libtcod.hpp"

#include "Vector.h"
#include "Pure.h"
#include "Grid.h"

#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <memory>
#include <list>
#include <string>

typedef Vector<int,2> Vec;

Vec mapDims( 80, 60 );

struct Tile
{
    bool seen, visible;
    char c;

    Tile() : seen(false), visible(false), c(' ') {}

    // Allow implicit construction.
    Tile( char c ) : seen(false), visible(false), c(c) {}
};

Grid<Tile> grid( 80, 60, '#' );

struct Actor
{
    std::string name;
    Vec pos;
};

std::string playerName;

typedef std::unique_ptr<Actor> ActorPtr;
typedef std::list<ActorPtr> ActorList;

ActorList actors;

/* 
 * Run mapgen.
 * Initialize grid and actors with mapgen output.
 */
void generate_map();

/* Exit gracefully. */
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

/* True if the tile at pos blocks movement. */
bool blocked( const Vec& pos );

int main()
{
    generate_map();

    TCODConsole::initRoot( mapDims.x(), mapDims.y(), "test rogue" );
    TCODConsole::root->setDefaultBackground( TCODColor::black );
    TCODConsole::root->setDefaultForeground( TCODColor::white );
    TCODConsole::disableKeyboardRepeat();

    bool gameOver = false;
    while( not gameOver and not TCODConsole::isWindowClosed() ) 
    {
        render();

        for( ActorPtr& actorptr : actors )
        {
            Actor& actor = *actorptr;

            // Just for now, we're player-only.
            if( actor.name != playerName )
                continue;

            bool turnOver = true;

turn_start:
            Vec pos( 0, 0 );
            switch( next_pressed_key() ) {
              case 'q': gameOver = true; break;

              // Cardinal directions.
              case 'h': case '4': case TCODK_LEFT:  pos.x() -= 1; break;
              case 'l': case '6': case TCODK_RIGHT: pos.x() += 1; break;
              case 'k': case '8': case TCODK_UP:    pos.y() -= 1; break;
              case 'j': case '2': case TCODK_DOWN:  pos.y() += 1; break;

              // Diagonals.
              case 'y': case '7': pos = Vec(-1,-1); break;
              case 'u': case '9': pos = Vec(+1,-1); break;
              case 'b': case '1': pos = Vec(-1,+1); break;
              case 'n': case '3': pos = Vec(+1,+1); break;

              default: turnOver = false;
            }

            pos += actor.pos;
            if( pos.x() and pos.y() ) {
                if( not blocked(pos) )
                    actor.pos = keep_inside( *TCODConsole::root, pos );
                else
                    turnOver = false;
            }

            if( not turnOver )
                goto turn_start;

        }
    }
}

void generate_map()
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
    while( fgets(spawnpt, sizeof spawnpt, mapgen) ) {
        if( spawnpt[0] != 'X' )
            continue;

        unsigned int x, y;
        sscanf( spawnpt, "X %u %u", &x, &y );
        Vec pos = { x, y };
        std::string name;

        if( not actors.size() ) {
            // First actor! Initialize as the player.
            playerName = "player";
            name = playerName;
        } else {
            name = "monst";
        }

        actors.push_back( ActorPtr(new Actor{name,pos}) );
    }
    if( actors.size() == 0 )
        die( "No spawn point!" );

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
    auto playerIter = pure::find_if(
        actors, [](const ActorPtr& aptr){return aptr->name == playerName;} 
    );
    if( playerIter != std::end(actors) ) {
        static Vec lastPos(-1,-1);
        Vec& pos = (*playerIter)->pos; 
        if( pos != lastPos ) {
            fov->computeFov( pos.x(), pos.y(), 0, true, FOV_PERMISSIVE_4 );
            lastPos = pos;
        }
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

    for( auto& actorIter : actors ) {

        Vec& pos = actorIter->pos;

        if( not grid.get(pos).visible )
            continue;

        TCODConsole::root->setChar( pos.x(), pos.y(), '@' );
        TCODConsole::root->setCharForeground( pos.x(), pos.y(), TCODColor::white );
        // Leave background as is.
    }

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

bool blocked( const Vec& pos )
{
    if( grid.get(pos).c == '#' )
        return true;
    return pure::find_if (
        actors, [&](const ActorPtr& aptr){return aptr->pos == pos;}
    ) != std::end(actors);
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
