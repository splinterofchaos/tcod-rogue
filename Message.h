
#include <string>

#include "libtcod.hpp"

/*
 * Messages are managed internally in Messages.cpp. This defines the interface
 * for interacting with the internal message list.
 */

struct Message
{
    enum Type {
        NORMAL,
        SPECIAL,
        COMBAT
    };

    static const int DURATION;

    std::string msg;
    int duration; // How many times this message should be printed.
    TCODColor fg, bg;

    /* The maximum size of msg. */
    static int width();

    Message( std::string, Type );

    float alpha() const;
    std::string::size_type size() const;
    const char* str() const;
};

/* Put new message into the internal log and stdout. */
void new_message( Message::Type type, const char* fmt, ... )
    __attribute__ ((format (printf, 2, 3)));

/* Apply f to every message. */
#include <functional>
void for_each_message( const std::function<void(const Message&)>& f );

/* Print a message to (0,0) on a console. */
std::string::size_type print( TCODConsole&, Message*);
