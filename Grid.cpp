
#include "Grid.h"

#include "random.h"

const int Room::MINLEN = 4;

Room::Room(size_t l, size_t r, size_t u, size_t d)
{
    left = l; right = r;
    up = u; down = d;
}

Room::Room( const Room& other )
{
    left = other.left; right = other.right;
    up = other.up; down = other.down;
}

#include <stdio.h>
int _split_pos( int min, int max, int len )
{
    min += len; max -= len;
    int range = max - min;
    
    // Make cuts closer to the center.
    if( range > 4 ) {
        min += range / 4;
        max -= range / 4;
    }

    return random( min, max );
}


static void print_area( Room r )
{
    printf( "<%d,%d,%d,%d>", r.left, r.right, r.up, r.down );
}

std::pair<Room,Room> hsplit( const Room& r, int len )
{
    int position = _split_pos( r.up, r.down, len );
    return std::make_pair ( 
        Room( r.left, r.right, r.up,       position-1 ),
        Room( r.left, r.right, position+1, r.down     )
    );
}

std::pair<Room,Room> vsplit( const Room& r, int len )
{
    int position = _split_pos( r.left, r.right, len );
    return std::make_pair (
        Room( r.left,     position-1, r.up, r.down ),
        Room( position+1, r.right,    r.up, r.down )
    );
}
