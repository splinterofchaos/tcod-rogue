
#include "libtcod.hpp"

#include "Vector.h"
#include "Pure.h"
#include "Grid.h"
#include "random.h"

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

enum StatType {
    HP,
    STRENGTH,
    AGILITY,
    DEXTERITY,
    ACCURACY,
    N_STATS
};

typedef std::array<int,N_STATS> Stats;

Stats operator+( const Stats& a, const Stats& b )
{ return pure::zip_with( std::plus<int>(), a, b ); }
Stats operator-( const Stats& a, const Stats& b )
{ return pure::zip_with( std::minus<int>(), a, b ); }
Stats operator*( const Stats& a, const Stats& b )
{ return pure::zip_with( std::multiplies<int>(), a, b ); }
Stats operator/( const Stats& a, const Stats& b )
{ return pure::zip_with( std::divides<int>(), a, b ); }

namespace stats
{
    // The base stats added to everything.
    Stats base = { 10, 10, 10, 10, 10 };

    // Racial stats.
    Stats human  = Stats{ 10,  5,  5,  0,  5 } + base;
    Stats kobold = Stats{  0, -3, 10,  8,  5 } + base;
    Stats bear   = Stats{ 30, 10, -5, -5,  0 } + base;
}

struct Race
{
    const char* const name;
    char symbol; // Race's image.
    TCODColor color;
    Stats stats;
};   

/* 
 * Racial equality!
 * Assume that two different races have different names and two races with the
 * same name have the same attributes.
 */
bool operator == ( const Race& r1, const Race& r2 )
{ return r1.name == r2.name; }
bool operator == ( const Race& r, const std::string& name )
{ return r.name == name; }
bool operator == ( const std::string& name, const Race& r )
{ return r == name; }

std::vector< Race > races = {
    { "human",  '@', TCODColor(200,150, 50), stats::human  },
    { "kobold", 'K', TCODColor(100,200,100), stats::kobold },
    { "bear",   'B', TCODColor(250,250,100), stats::bear   }
};

struct Actor
{
    std::string name;
    std::string race;
    Vec pos;
    Stats stats;
    int hp;
};

std::string playerName;

typedef std::shared_ptr<Actor> ActorPtr;
typedef std::weak_ptr<Actor> WeakActorPtr;
typedef std::list<ActorPtr> ActorList;

ActorList actors;
WeakActorPtr wplayer;

// Used by render() and move_monst().
std::unique_ptr<TCODMap> fov; // Field of vision.
std::unique_ptr<TCODDijkstra> playerDistance;

/* 
 * Run mapgen.
 * Initialize grid and actors with mapgen output.
 */
void generate_grid();

/* Update fov and playerDistance. */
void update_map( const Vec& pos );

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

struct Action
{
    enum Type {
        MOVE,
        WAIT,
        ATTACK,
        QUIT
    } type;

    // If type=MOVE/ATTACK, holds the destination.
    Vec pos;

    Action() : type(WAIT) {} 

    Action( Type type, Vec pos=Vec(0,0) )
        : type( type ), pos( pos )
    {
    }
};

/* Handle keyboard input on player's turn. */
Action move_player( Actor& );

/*
 * Move monster. 
 * If visible by player, move towards and attack player.
 * Otherwise, sit tight.
 */
Action move_monst( Actor& );

/* Simulate attack and print a message. Return true on kill. */ 
bool attack( const Actor& aggressor, Actor& victim );

ActorList::iterator actor_at( const Vec& pos )
{
    return pure::find_if ( 
        [&](const ActorPtr& aptr) { return aptr->pos == pos; },
        actors
    );
}

bool walkable( const Vec& pos )
{
    return grid.get( pos ).c == '.';
}

// Data for a message like "you step on some grass" or "you hit it".
struct Message
{
    enum Type {
        NORMAL,
        SPECIAL,
        COMBAT
    };

    static const int DURATION = 4;
    std::string msg;
    int duration; // How many times this message should be printed.
    Type type;

    Message( std::string msg, Type type ) 
        : msg( std::move(msg) ), duration( DURATION ), type( type )
    { 
    }
    Message( Message&& other, Type type ) 
        : msg( std::move(other.msg) ), duration( other.duration ),
          type( type )
    { 
    }
};

std::list< Message > messages;

/* Put new message into messages and stdout. */
void new_message( Message::Type type, const char* fmt, ... )
    __attribute__ ((format (printf, 2, 3)));

int main()
{
    TCODConsole::initRoot( mapDims.x(), mapDims.y(), "test rogue" );
    TCODConsole::root->setDefaultBackground( TCODColor::black );
    TCODConsole::root->setDefaultForeground( TCODColor::white );
    TCODConsole::disableKeyboardRepeat();

    // A little intro screen. Just asks for the player's name.
    while( true )
    {
        TCODConsole::root->setAlignment( TCOD_CENTER );
        TCODConsole::root->print( 40, 5, "Welcome to this WIP roguelike. " );
        TCODConsole::root->print( 40, 10, "You may notice sone hitches," );

        TCODConsole::root->setAlignment( TCOD_LEFT );
        TCODConsole::root->print( 30, 20, "Please enter in your name: " );
        TCODConsole::root->print( 30, 23, playerName.c_str() );

        TCODConsole::root->flush();
        TCODConsole::root->clear();

        TCOD_key_t key;
        do key = TCODConsole::waitForKeypress( true );
        while( not key.pressed );

        if( key.vk == TCODK_ENTER ) {
            // Don't leave without a name, 
            // but don't add the newline char to playerName either.
            if( playerName.size() > 0 ) 
                break;
            else {
                TCODConsole::root->print ( 
                    30, 25, "Your name must be at least one character long." 
                );
                continue;
            }
        }

        if( key.c )
            playerName.push_back( key.c );
    }

    TCODConsole::root->setDefaultForeground( TCODColor::white );

    generate_grid();


    new_message( Message::SPECIAL, "%s has entered the game.", 
                 playerName.c_str() );

    while( not TCODConsole::isWindowClosed() ) 
    {
        ActorPtr player = wplayer.lock();
        if( not player )
            break;

        render();

        for( ActorPtr& actorptr : actors )
        {
            Actor& actor = *actorptr;

            if( actor.hp <= 0 )
                // Dead man standing! Will be cleaned up by next render().
                continue;

            Action act;

            if( actorptr != player )
                act = move_monst( actor );
            else
                act = move_player( actor );

            if( act.type == Action::MOVE and walkable(act.pos) ) 
            {
                // Walk to act.pos or attack what's there.
                auto targetIter = actor_at( act.pos );
                if( targetIter != std::end(actors) ) 
                {
                    if(  attack(actor, *(*targetIter)) )
                        actors.erase( targetIter );
                } 
                else
                {
                    actor.pos = act.pos;
                    if( actorptr == player )
                        update_map( player->pos );
                }
            }

            if( act.type == Action::MOVE and not walkable(act.pos) )
                new_message( Message::NORMAL, "You cannot move there." );

            if( act.type == Action::QUIT )
                return 0;
        }
    }

    if( not wplayer.lock() )
        printf( "You, %s, have died. Have a nice day.\n", playerName.c_str() );
}

void generate_grid()
{
    FILE* mapgen = popen( "./mapgen/c++/mapgen -n 5 -X 15", "r" );

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

    // Read spawn points.
    char spawnpt[50];
    while( fgets(spawnpt, sizeof spawnpt, mapgen) ) {
        if( spawnpt[0] != 'X' )
            continue;

        ActorPtr actor( new Actor ); 
        sscanf( spawnpt, "X %u %u", &actor->pos.x(), &actor->pos.y() );

        if( not actors.size() ) {
            // First actor! Initialize as the player.
            actor->name = playerName;
            actor->race = "human";
            wplayer = actor;
        } else {
            actor->race = races[ random(0, races.size()-1) ].name;
            actor->name = "the " + actor->race;
        }

        auto raceIter = pure::find( actor->race, races );
        if( raceIter == std::end(races) )
            // This should never happen, but if it does...
            raceIter = std::begin( races );
        
        actor->stats = raceIter->stats;
        actor->hp    = actor->stats[HP];

        actors.push_back( actor );
    }

    if( actors.size() == 0 )
        die( "No spawn point!" );

    pclose( mapgen );
}

void update_map( const Vec& pos )
{
    fov->computeFov( pos.x(), pos.y(), 10, true, FOV_PERMISSIVE_4 );
    playerDistance->compute( pos.x(), pos.y() );
}

Action move_player( Actor& player )
{
    Vec pos( 0, 0 );
    switch( next_pressed_key() ) {
      case 'q': return Action::QUIT;

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

      case '.': case '5': return Action::WAIT;

      default: ;
    }

    if( pos.x() or pos.y() )
        return Action( Action::MOVE, pos + player.pos );

    // The player has not yet moved (or we would have returned already).
    return move_player( player );
}

Action move_monst( Actor& monst )
{
    int& x = monst.pos.x();
    int& y = monst.pos.y();

    if( not fov->isInFov(x, y) )
        return Action( Action::WAIT );

    playerDistance->setPath( x, y );
    playerDistance->reverse();

    Vec pos;
    playerDistance->walk( &pos.x(), &pos.y() );
    return Action( Action::MOVE, pos );
}

bool attack( const Actor& aggressor, Actor& victim )
{
    const Stats& as = aggressor.stats;
    const Stats& vs = victim.stats;

    const char* const HIT    = "hit";
    const char* const DODGED = "dodged";
    const char* const KILLED = "killed";
    const char* const MISSED = "missed";
    const char* const CRITICAL = "critically hit";

    const char* verb = MISSED;
    bool criticalHit = false;
    
    // Victim can move out of the way before before aggressor attacks.
    if( random(1, as[AGILITY]*as[ACCURACY]) < as[AGILITY]+as[DEXTERITY] ) 
    { 
        verb = MISSED;
    }
    else
    {
        // Victim can dodge aggressor's attack.
        if( random(1, vs[AGILITY]+vs[DEXTERITY]) > as[DEXTERITY] ) {
            verb = DODGED;
        } else {
            verb = HIT;

            int dmg = random( as[STRENGTH]/2, as[STRENGTH]+1 );
            if( dmg >= as[STRENGTH] ) {
                dmg *= 1.5f;
                verb = CRITICAL;
                criticalHit = true;
            }

            victim.hp -= dmg;
            if( victim.hp < 1 )
                verb = KILLED;
        }
    } 

    if( verb != DODGED ) {
        new_message (
            Message::COMBAT, "%s %s %s%c", 
            aggressor.name.c_str(), verb, victim.name.c_str(),
            criticalHit ? '!' : '.' 
        );
    } else {
        // "the aggressor dodged the victim" doesn't make sense, 
        // so do something different for dodging.
        new_message (
            Message::COMBAT, "%s dodged %s's attack.", 
            victim.name.c_str(), aggressor.name.c_str()
        );
    }

    return verb == KILLED;
}

void render()
{
    if( not fov or fov->getWidth()  != (int)grid.width 
                or fov->getHeight() != (int)grid.height ) 
    {
        fov.reset( new TCODMap(grid.width, grid.height) );
        for( unsigned int x=0; x < grid.width; x++ )
            for( unsigned int y=0; y < grid.height; y++ ) {
                bool walkable = (grid.get(x,y).c == '.');
                fov->setProperties( x, y, walkable, walkable );
            }

        playerDistance.reset( new TCODDijkstra(fov.get()) );

        ActorPtr player = wplayer.lock();
        if( player )
            update_map( player->pos );
    }

    TCODConsole::root->setDefaultForeground( TCODColor::white );
    TCODConsole::root->setDefaultBackground( TCODColor::black );
    TCODConsole::root->clear();

    // Draw onto root.
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

            typedef TCODColor C;

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

        const Vec& pos = actorIter->pos;
        if( not grid.get(pos).visible )
            continue;

        int symbol = 'X';
        TCODColor color = TCODColor::white;

        auto raceIter = pure::find( actorIter->race, races );
        if( raceIter != std::end(races) ) {
            symbol = raceIter->symbol;
            color = raceIter->color;
        }

        TCODConsole::root->setChar( pos.x(), pos.y(), symbol );
        TCODConsole::root->setCharForeground( pos.x(), pos.y(), color );

        // Draw background as a function of vitality.
        //float vitality = 20.f / actorIter->hp * (255/20.f);
        //TCODColor c( 255.f, vitality, vitality );
        //TCODConsole::root->setCharBackground( pos.x(), pos.y(), c );
    }

    // Print messages.
    int y = 0;
    TCODConsole::root->setAlignment( TCOD_LEFT );
    for( auto it=std::begin(messages); it!=std::end(messages); it++ )
    {
        Message& msg = *it;
        if( not msg.duration ) {
            messages.erase( it, std::end(messages) );
            break;
        }

        TCODColor fg, bg;
        if( msg.type == Message::SPECIAL ) {
            fg = TCODColor::lightestYellow;
            bg = TCODColor::black;
        } else if( msg.type == Message::COMBAT ) {
            fg = TCODColor::lightestFlame;
            bg = TCODColor::desaturatedYellow;
        } else /* msg.type == NORMAL */ { 
            fg = TCODColor::white;
            bg = TCODColor::black;
        }

        static TCODConsole msgCons( 40, 1 );
        msgCons.clear();
        msgCons.setDefaultForeground( fg );
        msgCons.setDefaultBackground( bg );
        msgCons.print( 0, 0, msg.msg.c_str() );

        float alpha = float(msg.duration--) / Message::DURATION;
        TCODConsole::blit ( 
            &msgCons, 0, 0, msgCons.getWidth(), msgCons.getHeight(), 
            TCODConsole::root, 1, y++, 
            alpha, alpha
        );
    }

    // Print a health bar.
    ActorPtr player = wplayer.lock();
    if( player ) {
        int y = grid.height - 1; // y-position of health bar.

        TCODConsole::root->setDefaultBackground( TCODColor::red );
        TCODConsole::root->setDefaultForeground( TCODColor::white );
        unsigned int width = 
            (float(player->hp)/player->stats[HP]) * (grid.width/2);
        TCODConsole::root->hline( 0, y, width, TCOD_BKGND_SET );

        const char* healthFmt = width > sizeof "xx / xx" ? 
            "%u / %u" : "%u/%u";
        char* healthInfo;
        asprintf( &healthInfo, healthFmt, player->hp, player->stats[HP] );
        if( healthInfo ) {
            TCOD_alignment_t allignment = strlen(healthInfo) < width ?
                TCOD_CENTER : TCOD_LEFT;
            TCODConsole::root->setAlignment( allignment );
            TCODConsole::root->print( width/2, y, healthInfo );
            free( healthInfo );
        }
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

    int k = key.vk == TCODK_CHAR ? key.c : (int)key.vk;

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
    return actor_at(pos) != std::end(actors);
}

#include <cstdarg>
void new_message( Message::Type type, const char* fmt, ... )
{
    va_list vl;
    va_start( vl, fmt );
    
    char* msg;
    vasprintf( &msg, fmt, vl );
    if( msg ) {
        messages.push_front( Message(msg,type) );
        printf( "%s\n", msg );
        free( msg );
    }

    va_end( vl );
}

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
