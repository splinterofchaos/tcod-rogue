
#include "random.h"

#include <ctime> // To seed random number.
#include <cstdlib>

int seed = 0;

int random( int max )
{
    return random( 0, max );
}

int random( int min, int max )
{
    if( not seed ) {
        seed = std::time(0);
        srand( seed );
    }

    if( min > max )
        return random( max, min );
    return rand() % (max-min+1) + min;
}
