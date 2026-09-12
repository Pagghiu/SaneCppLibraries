#include "event_log.h"

int main()
{
    EventLog log;
    Event    events[1];
    char     bytes[1];
    return log.init(bytes, 1, events, 1) ? 0 : 1;
}
