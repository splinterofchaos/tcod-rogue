
#include "Vector.h"
#include "Pure.h"
#include "Grid.h"
#include "random.h"
#include "msg.h"

#include "Rogue.h"

#include "libtcod.hpp"

#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <memory>
#include <list>
#include <string>

Vec mapDims( 80, 60 );

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
    // The base stats added to every race.
    Stats base = {{ 10, 10, 10, 10, 10 }};

    // Racial stats.
    Stats human  = Stats{{ 10,  5,  5,  0,  5 }} + base;
    Stats kobold = Stats{{  0, -3, 10,  8,  5 }} + base;
    Stats bear   = Stats{{ 30, 10, -5, -5,  0 }} + base;
    
    // Item stats.
    Stats nothing = Stats{{ 0, 0,  0, 0, 0 }};
    Stats stick   = Stats{{ 0, 5,  0, 0, 3 }};

    // Gives extra health, but slows its wielder.
    Stats pillow  = Stats{{ 5, 1, -3, 0, 0 }};
                          
    // A special item that makes one super-quick and accurate.
    Stats thumbTack = Stats{{ 2, 0, 10, 10 }};
}

struct ThingData
{
    const char* name;
    char symbol; // Thing's image.
    TCODColor color;
    Stats stats;

    // At what levels this thing will spawn (inclusive).
    int minlvl, maxlvl; // {-1,-1} means never spawn naturally.
};   

/* 
 * Assume that two different things have different names and two things with
 * the same name have the same attributes.
 */
bool operator == ( const ThingData& r1, const ThingData& r2 )
{ return r1.name == r2.name; }
bool operator == ( const ThingData& r, const std::string& name )
{ return r.name == name; }
bool operator == ( const std::string& name, const ThingData& r )
{ return r == name; }

std::vector< ThingData > catalogue = {
    { "fist",    ' ', TCODColor::black,        stats::nothing,   -1, -1 },
    { "stick",   '/', TCODColor(200,150, 100), stats::stick,      0, 10 },
    { "pillow",  '-', TCODColor::white,        stats::pillow,     0, 10 },
    { "thumb tack", '-', TCODColor::green,     stats::thumbTack, -1, -1 }
};

std::vector< ThingData > races = {
    { "human",  '@', TCODColor(200,150, 50), stats::human,  0, 10 },
    { "kobold", 'K', TCODColor(100,200,100), stats::kobold, 0, 10 },
    { "bear",   'B', TCODColor(250,250,100), stats::bear,   0, 10 }
};

struct Item
{
    std::string name;
    char symbol;
    Stats stats;

    Item() { }
    Item( const ThingData& data )
        : name( data.name ), symbol( data.symbol ), stats( data.stats )
    {
    }
};

Item fist() { return Item(catalogue[0]); }

struct MapItem : Item
{
    Vec pos;

    MapItem() {}
    MapItem( const Item& item, const Vec& pos )
        : Item( item ), pos( pos ) { }
    MapItem( Item&& item, const Vec& pos )
        : Item( std::move(item) ), pos( pos ) { }
};

struct Actor
{
    std::string name;
    std::string race;
    Vec pos;
    Stats base;
    int hp;
    int nextMove;

    typedef std::vector<Item> Inventory;
    Inventory inventory;

    Item weapon;

    Actor()
    {
        nextMove = 0;
        weapon = fist();
    }

    Stats stats() const { return base + weapon.stats; }
};

typedef std::list<Actor> ActorList;
typedef std::list<MapItem> ItemList;
ActorList actors;
ItemList  items;

ActorList::iterator playeriter = std::end(actors);
std::string playerName;

/* Player's Field of Vision. */
TCODMap fov( grid.width, grid.height ); 
/* Distances from player. */
TCODDijkstra playerDistance( &fov );
TCODConsole& console = *TCODConsole::root;

/* 
 * Graphical overlay to draw UI. 
 * Painted over TCODConsole::root in render() offering no transparency.
 */
TCODConsole overlay( grid.width, grid.height );

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
        PICKUP,
        DROP,
        QUIT
    } type;

    // If type=MOVE/ATTACK, holds the destination.
    Vec pos;

    unsigned int inventoryIndex;

    Action() : type(WAIT) {} 

    Action( Type type, Vec pos=Vec(0,0) )
        : type( type ), pos( pos )
    {
    }

    Action( Type type, unsigned int ii )
        : type( type ), inventoryIndex( ii )
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

/* Drop actor->inventory[i], if exists. Returns true on success. */
bool drop( ActorList::iterator actor, unsigned int ii );
bool drop_weapon( ActorList::iterator actor );

/* Inventory Index to Char. */
char iitoc( unsigned int i ) { return 'a' + i; }
/* Char to Inventory Index. */
unsigned int ctoii( char c ) { return c - 'a'; }

ActorList::iterator actor_at( const Vec& pos )
{
    return pure::find_if ( 
        [&](const Actor& aptr) { return aptr.pos == pos; },
        actors
    );
}

ItemList::iterator item_at( const Vec& pos )
{
    return pure::find_if (
        [&](const MapItem& item){ return item.pos == pos; },
        items
    );
}

/* Expire: Drop all items. Remove from actors list. */
void expire( ActorList::iterator actor )
{
    while( actor->inventory.size() ) drop( actor, 0 );
    drop_weapon( actor );
    if( actor == playeriter ) playeriter = std::end( actors );
    actors.erase( actor );
}

bool walkable( const Vec& pos )
{
    return pos.x() > 0 and pos.y() > 0 
       and pos.x() < grid.width and pos.y() < grid.height 
       and grid.get( pos ).c == '.';
}

int clamp( int x, int min, int max )
{
    if( x < min )      x = min;
    else if( x > max ) x = max;
    return x;
}

template< class C/*ontainer*/ >
auto random_select( C&& c ) -> decltype( c[0] )
{ return c[ random(0, c.size()-1) ]; }

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
        do key = TCODConsole::waitForKeypress( false );
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
    render();

    msg::special( "%s has entered the game.", playerName.c_str() );

    int time = 0;

    while( actors.size() and not TCODConsole::isWindowClosed() )
    {
        if( playeriter == std::end(actors) )
            break;

        ActorList::iterator actor = pure::min (
            [](const Actor& a, const Actor& b)
            { return a.nextMove < b.nextMove; },
            actors
        );

        if( actor == std::end(actors) ) {
            msg::special( "NO MORE PLAYERS!" );
            break;
        }

        if( actor->hp <= 0 ) {
            msg::combat( "%s has mysteriously died.", actor->name.c_str() );
            actors.erase( actor );
            continue;
        }

        time = actor->nextMove;

        Action act;
        if( actor == playeriter ) {
            render();
            act = move_player( *actor );
        } else {
            act = move_monst( *actor );
        }

        if( act.type == Action::MOVE and walkable(act.pos) ) 
        {
            // Walk to act.pos or attack what's there.
            auto target = actor_at( act.pos );
            if( target != std::end(actors) ) 
            {
                bool killed = attack( *actor, *target );
                if( killed )
                    expire( target );
            }
            else
            {
                actor->pos = act.pos;
                if( actor == playeriter )
                    update_map( actor->pos );
            }

            actor->nextMove += 50 - actor->stats()[AGILITY];
        }

        if( act.type == Action::PICKUP ) 
        {
            auto item = item_at( actor->pos );
            if( item != std::end(items) ) 
            {
                actor->inventory.push_back( *item );

                if( actor == playeriter )
                    msg::normal( "Got %s.", item->name.c_str() );
                else if( grid.get(actor->pos).visible )
                    msg::normal( "You see %s grab a %s.", 
                                 actor->name.c_str(), item->name.c_str() );

                items.erase( item );
                actor->nextMove += 30 - actor->stats()[AGILITY];
            } 
            else if( actor == playeriter ) 
            {
                msg::normal( "Nothing here to pick up." );
            }
        }

        if( act.type == Action::DROP )
            drop( actor, act.inventoryIndex );

        if( act.type == Action::QUIT ) {
            printf( "QUIT received.\n" );
            return 0;
        }

        /* 
         * Give the player a chance to make a different move if the selected
         * choice isn't valid. Doing this for NPCs too would cause an infinite
         * loop. 
         */
        if( act.type == Action::MOVE and not walkable(act.pos) 
            and actor == playeriter ) 
        {
            msg::normal( "You cannot move there." );
            continue;
        }

        // Don't let a turn go on infinitely. 
        // NPC didn't move or actor is waiting.
        if( actor->nextMove == time )
            actor->nextMove += actor->stats()[AGILITY]/2;
    }

    if( playeriter == std::end(actors) )
        printf( "You, %s, have died. Have a nice day.\n", playerName.c_str() );
    if( actors.size() == 0 )
        printf( "Where did everyone go?\n" );
    if( TCODConsole::isWindowClosed() )
        printf( "Window closed.\n" );
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

    // Look for items available at this level.
    auto availableItems = pure::filter (
        []( const ThingData& item ) { return item.minlvl >= 0; },
        catalogue
    );

    // Read spawn points.
    char spawnpt[50];
    unsigned int nspawns = 10;
    while( nspawns-- and fgets(spawnpt, sizeof spawnpt, mapgen) ) {
        if( spawnpt[0] != 'X' )
            continue;

        actors.push_back( Actor() );
        Actor& actor = actors.back(); 

        sscanf( spawnpt, "X %u %u", &actor.pos.x(), &actor.pos.y() );

        if( actors.size() == 1 ) {
            // First actor! Initialize as the player.
            actor.name = playerName;
            actor.race = "human";
            playeriter = std::begin( actors );
        } else {
            actor.race = random_select(races).name;
            actor.name = "the " + actor.race;
            actor.weapon = random_select(availableItems);
        }

        auto raceIter = pure::find( actor.race, races );
        if( raceIter == std::end(races) )
            // This should never happen, but if it does...
            raceIter = std::begin( races );
        
        actor.base = raceIter->stats;
        actor.hp   = actor.stats()[HP];
    }

    if( actors.size() == 0 )
        die( "No spawn point!" );

    while( fgets(spawnpt, sizeof spawnpt, mapgen) ) {
        items.emplace_back( random_select(availableItems), Vec(0,0) );
        MapItem& item = items.back();
        sscanf( spawnpt, "X %u %u", &item.pos.x(), &item.pos.y() );
    }

    pclose( mapgen );

    // Initialize FOV.
    pure::for_ij ( [&]( int x, int y ) { 
             bool canWalk = walkable( Vec(x,y) );
             fov.setProperties( x, y, canWalk, canWalk ); 
        }, grid.width, grid.height 
    );

    if( playeriter != std::end(actors) )
        update_map( playeriter->pos );
}

void update_map( const Vec& pos )
{
    fov.computeFov( pos.x(), pos.y(), 10, true, FOV_PERMISSIVE_4 );
    playerDistance.compute( pos.x(), pos.y() );
}

bool drop( ActorList::iterator actor, unsigned int ii )
{
    Actor::Inventory& inv = actor->inventory;
    if( ii >= 0 and ii < inv.size() ) 
    {
        const auto item = std::begin(inv) + ii;

        if( fov.isInFov(actor->pos.x(), actor->pos.y()) )
            msg::normal ( 
                "%s dropped the %s", 
                actor == playeriter ? "You" : actor->name.c_str(),
                item->name.c_str() 
            );
        
        items.emplace_back( std::move(*item), actor->pos );
        inv.erase( item );

        return true;
    } 
    else if( actor == playeriter ) 
    {
        msg::normal( "You don't have that!" );
    }

    return false;
}

bool drop_weapon( ActorList::iterator actor )
{
    if( actor->weapon.name == "fist" )
        return false;
    
    if( actor != playeriter 
        and fov.isInFov(actor->pos.x(), actor->pos.y()) )
        msg::combat( "%s has dropped %s.", 
                     actor->name.c_str(), actor->weapon.name.c_str() );

    items.emplace_back( std::move(actor->weapon), actor->pos );
    actor->weapon = fist();

    return true;
}

void _look_loop( const Actor& player )
{
    Vec lpos = player.pos; // Look position.
    while( true )
    {
        playerDistance.setPath( lpos.x(), lpos.y() );

        Vec pos = lpos;
        do {
            grid.get(pos).highlight = true;
            playerDistance.walk( &pos.x(), &pos.y() );
        } while( playerDistance.size() > 0 );

        // Loop terminates before highlighting player's position.
        grid.get(player.pos).highlight = true;

        // Tell the player what they're looking at.
        if( grid.get(lpos).visible )
        {
            const int INFO_LEN = 20;
            std::string info;

            switch( grid.get(lpos).c ) {
              case '.': info = "A stone floor."; break;
              case '#': info = "A stone wall."; break;
            }

            ActorList::iterator actor;
            ItemList::iterator item;
            if( (actor = actor_at(lpos)) != std::end(actors) ) {
                if( actor == playeriter )
                    info = "It's you!";
                else
                    info = "You see %s." + actor->name;
            } else if( (item=item_at(lpos)) != std::end(items) ) {
                info = "You see a " + item->name;
            }

            static TCODConsole infobox(INFO_LEN,1);

            infobox.setDefaultForeground( TCODColor::green );
            infobox.print( 0, 0, info.c_str() );

            TCODConsole::blit (
                &infobox, 0, 0, info.size(), 1,
                &overlay, 
                // Draw it centered on the x-axis
                clamp( lpos.x()-info.size()/2, 1, grid.width-info.size() ), 
                // and just above or below on the y-axis.
                lpos.y() + (lpos.y() > 3 ? -2 : +2),
                1, 0.5f
            );
        }

        render();

        switch( next_pressed_key() ) {
          // Cardinal directions.
          case 'h': case '4': case TCODK_LEFT:  lpos.x() -= 1; break;
          case 'l': case '6': case TCODK_RIGHT: lpos.x() += 1; break;
          case 'k': case '8': case TCODK_UP:    lpos.y() -= 1; break;
          case 'j': case '2': case TCODK_DOWN:  lpos.y() += 1; break;

          // Diagonals.
          case 'y': case '7': lpos += Vec(-1,-1); break;
          case 'u': case '9': lpos += Vec(+1,-1); break;
          case 'b': case '1': lpos += Vec(-1,+1); break;
          case 'n': case '3': lpos += Vec(+1,+1); break;

          // Render one last time to unset the highlight path 
          // and erase the infobox.
          default: render(); return;
        }
    }
}

/* 
 * Render the inventory; wait for key press.
 * Returns an inventory index, assuming the player hit a key corresponding to a
 * held item.
 */
int _render_inventory( const Actor& player )
{
    if( not player.inventory.size() )
    {
        msg::normal( "You don't have anything." );
        render();
        return -1;
    }

    TCODConsole invcons( grid.width/2, player.inventory.size() + 1 );
    unsigned int y = 0;
    for( const Item& i : player.inventory ) 
    {
        char* row = nullptr;
        asprintf( &row, "%c - (%c)%s", iitoc(y), i.symbol, i.name.c_str() );
        invcons.print( 0, y++, row );
        if( row ) free( row );
    }

    invcons.setDefaultForeground( TCODColor::red );
    invcons.print( 0, y, "Press any key." );

    TCODConsole::blit (
        &invcons, 0, 0, invcons.getWidth(), invcons.getHeight(),
        &overlay,
        // Draw centered.
        grid.width  / 2 - invcons.getWidth()  / 2, 
        grid.height / 2 - invcons.getHeight() / 2
    );

    // Show the inventory (printed to overlay).
    render();
    // Wait for the player to finish reading.
    int k = next_pressed_key();
    // Erase the inventory.
    render();

    return ctoii( k );
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

      case 'L': _look_loop( player ); 
                return move_player(player);

      case 'i':  _render_inventory( player ); 
                 return move_player(player);

      case 'g': return Action::PICKUP;

      case 'd': // Drop
        {
            msg::special( "Pick an item." );
            // _render_inventory will display this message and return an
            // inventory index.
            int ii = _render_inventory( player );

            if( ii < player.inventory.size() and ii >= 0 ) {
                return Action( Action::DROP, ii );
            } else {
                msg::normal( "You don't have that!", ii );
                render();
                return move_player( player );
            }
        }

      case 'e': // Equip
        {
            msg::special( "Equip what?" );
            unsigned int ii = _render_inventory( player );
            if( ii < player.inventory.size() and ii >= 0 ) {
                player.weapon = std::move( player.inventory[ii] );
                player.inventory.erase( player.inventory.begin() + ii );
                msg::special( "Eqipped %s.", player.weapon.name.c_str() );
                render();
            }

            return move_player( player );
        }

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

    if( not fov.isInFov(x, y) )
        return Action( Action::WAIT );

    playerDistance.setPath( x, y );
    playerDistance.reverse();

    Vec pos;
    playerDistance.walk( &pos.x(), &pos.y() );
    return Action( Action::MOVE, pos );
}

bool attack( const Actor& aggressor, Actor& victim )
{
    Stats as = aggressor.stats();
    const Stats& vs = victim.stats();

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

    if( verb == DODGED ) {
        msg::combat( "%s dodged %s's %s.", 
                     victim.name.c_str(), aggressor.name.c_str(),
                     aggressor.weapon.name.c_str() );
    } else {
        msg::combat( "%s's %s %s %s%c", // "attacker's wpn (hit/missed) who(./!)"
                     aggressor.name.c_str(), aggressor.weapon.name.c_str(),
                     verb, 
                     victim.name.c_str(),
                     criticalHit ? '!' : '.' );
    }

    return verb == KILLED;
}

void render()
{
    // Draw the map onto root.
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

            if( fov.isInFov(x,y) )
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

            if( t.highlight ) {
                bg = bg * 1.5;
                fg = fg * 1.5;
                t.highlight = false;
            }

            TCODConsole::root->setCharForeground( x, y, fg );
            TCODConsole::root->setCharBackground( x, y, bg );
        }
    }

    for( auto& item : items ) {
        const Vec& pos = item.pos;
        if( not grid.get(pos).visible )
            continue;

        int symbol = 'X';
        TCODColor color = TCODColor::white;

        auto itemiter = pure::find( item.name, catalogue );
        if( itemiter != std::end(catalogue) ) {
            symbol = itemiter->symbol;
            color  = itemiter->color;
        }

        TCODConsole::root->setChar( pos.x(), pos.y(), symbol );
        TCODConsole::root->setCharForeground( pos.x(), pos.y(), color );
    }

    for( auto& actor : actors ) {

        const Vec& pos = actor.pos;
        if( not grid.get(pos).visible )
            continue;

        int symbol = 'X';
        TCODColor color = TCODColor::white;

        auto raceIter = pure::find( actor.race, races );
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
    const int SIZE = grid.width / 2; // Max size of message.
    static TCODConsole msgbox( SIZE, 1 );
    msgbox.setBackgroundFlag( TCOD_BKGND_SET );

    int y = 0;
    int x = playeriter != std::end(actors) and playeriter->pos.x() > SIZE ?  
        1 : SIZE;
    msg::for_each (
        [&]( const std::string& msg, 
             const TCODColor& fg, const TCODColor& bg, int duration )
        {
            msgbox.setDefaultForeground( fg );
            msgbox.setDefaultBackground( bg );
            msgbox.print( 0, 0, msg.c_str() );

            float alpha = float(duration) / msg::DURATION;
            TCODConsole::blit ( 
                &msgbox, 0, 0, msg.size(), 1, 
                &overlay, x, y++, 
                alpha, alpha
            );
        }
    );

    // Print a health bar.
    if( playeriter != std::end(actors) ) {
        int y = grid.height - 1; // y-position of health bar.

        overlay.setDefaultBackground( TCODColor::red );
        overlay.setDefaultForeground( TCODColor::white );
        unsigned int width = 
            (float(playeriter->hp)/playeriter->stats()[HP]) * (grid.width/2);
        overlay.hline( 0, y, width, TCOD_BKGND_SET );

        const char* healthFmt = width > sizeof "xx / xx" ? 
            "%u / %u" : "%u/%u";
        char* healthInfo;
        asprintf( &healthInfo, healthFmt, 
                  playeriter->hp, playeriter->stats()[HP] );
        if( healthInfo ) {
            TCOD_alignment_t allignment = strlen(healthInfo) < width ?
                TCOD_CENTER : TCOD_LEFT;
            overlay.setAlignment( allignment );
            overlay.print( width/2, y, healthInfo );
            free( healthInfo );
        }
    }

    // The overlay needs a blit-transparent key color, which cannot be black as
    // that may be used. Any uncommon color will do.
    const TCODColor KEY_COLOR(0.01f,0.01f,0.01f);
    overlay.setKeyColor( KEY_COLOR );

    TCODConsole::blit (
        &overlay, 0, 0, grid.width, grid.height,
        TCODConsole::root, 0, 0,
        1, 1
    );
    TCODConsole::flush();

    // Prepare for next call.
    TCODConsole::root->setDefaultForeground( TCODColor::white );
    TCODConsole::root->setDefaultBackground( TCODColor::black );
    TCODConsole::root->clear();
    
    overlay.setDefaultForeground( TCODColor::white );
    overlay.setDefaultBackground( KEY_COLOR );
    overlay.clear();
}

Vec keep_inside( const TCODConsole& cons, Vec v )
{
    v.x( clamp(v.x(), 1, cons.getWidth()-1) );
    v.y( clamp(v.y(), 1, cons.getWidth()-1) );
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
