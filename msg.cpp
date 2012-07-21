
#include "msg.h"
#include "Rogue.h" // For grid.width in Message::width.
#include <list>
#include <cstdarg>

namespace msg
{

const int DURATION = 4;

struct Message
{
    std::string msg;
    TCODColor fg, bg;
    int duration; // How many times this message should be printed.
};

::std::list< Message > messageList;

void _push_msg( const char* fmt, va_list vl, 
                const TCODColor& fg, const TCODColor& bg )
{
    char* msg;
    if( vasprintf(&msg,fmt,vl) > 0 ) {
        messageList.push_front ( {msg, fg, bg, DURATION} );
        printf( "%s\n", msg );
        free( msg );
    }
}

void combat( const char* fmt, ... )
{
    va_list vl;
    va_start( vl, fmt );

    _push_msg( fmt, vl, 
               TCODColor::lightestFlame, TCODColor::desaturatedYellow );
    
    va_end( vl );
}

void special( const char* fmt, ... )
{
    va_list vl;
    va_start( vl, fmt );

    _push_msg( fmt, vl, TCODColor::lightestYellow, TCODColor::black );
    
    va_end( vl );
}

void normal( const char* fmt, ... )
{
    va_list vl;
    va_start( vl, fmt );

    _push_msg( fmt, vl, TCODColor::white, TCODColor::black );
    
    va_end( vl );
}

void for_each( const Fn& f )
{
    typedef std::list<Message>::iterator I;
    for( I it = std::begin(messageList); it != std::end(messageList); it++ )
    {
        if( it->duration <= 0 ) {
            messageList.erase( it, std::end(messageList) );
            break;
        }

        f( it->msg, it->fg, it->bg, it->duration );
        it->duration--;
    }
}

} // namespace msg
