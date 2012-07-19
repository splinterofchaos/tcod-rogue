
#include "Message.h"
#include "Rogue.h" // For grid.width in Message::width.
#include <list>

const int Message::DURATION = 4;

std::list< Message > messageList;

int Message::width()
{
    return grid.width / 2;
}

Message::Message( std::string msg, Type type )
    : msg( std::move(msg) ), duration( DURATION )
{ 
    typedef TCODColor C;
    switch( type ) {
      case SPECIAL: fg=C::lightestYellow; bg=C::black; break;
      case COMBAT: fg=C::lightestFlame; bg=C::desaturatedYellow; break;

      default:
      case NORMAL: fg=C::white; bg=C::black; break;
    }
}

float Message::alpha() const
{
    // Called after print, which decrements duration.
    return float(duration) / DURATION;
}

std::string::size_type Message::size() const
{
    return msg.size();
}

const char* Message::str() const
{
    return msg.c_str();
}

#include <cstdarg>
void new_message( Message::Type type, const char* fmt, ... )
{
    va_list vl;
    va_start( vl, fmt );
    
    char* msg;
    vasprintf( &msg, fmt, vl );
    if( msg ) {
        messageList.push_front( Message(msg,type) );
        printf( "%s\n", msg );
        free( msg );
    }

    va_end( vl );
}

void for_each_message( const std::function<void(const Message&)>& f )
{
    typedef std::list<Message>::iterator I;
    for( I it = std::begin(messageList); it != std::end(messageList); it++ )
    {
        if( it->duration <= 0 ) {
            messageList.erase( it, std::end(messageList) );
            break;
        }

        f( *it );
        it->duration--;
    }
}
