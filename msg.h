
#include <string>
#include <functional>

#include "libtcod.hpp"

namespace msg
{

extern const int DURATION; // How long a message will last.

/* 
 * Message printing functions.
 * Each function will result in a message of a different color.
 */
void combat(  const char* fmt, ... ) __attribute__((format (printf, 1, 2)));
void special( const char* fmt, ... ) __attribute__((format (printf, 1, 2)));
void normal(  const char* fmt, ... ) __attribute__((format (printf, 1, 2)));

/* 
 * For each message, do f and decrement the duration.
 * Used to print each message to the screen.
 */
typedef std::function< void(const std::string&,
                            const TCODColor&,const TCODColor&, int) > Fn; 
void for_each( const Fn& f );

}
