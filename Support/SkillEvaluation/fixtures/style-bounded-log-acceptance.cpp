#include "event_log.h"

#include <stdint.h>

static int fail(int code) { return code; }

int main()
{
    EventLog log;
    Event    events[3];
    char     bytes[4];
    if (not log.init(bytes, sizeof(bytes), events, 3))
        return fail(1);
    if (log.append(10, "abc", 3) != AppendResult::ok)
        return fail(2);
    if (log.append(11, "d", 1) != AppendResult::ok)
        return fail(3);
    if (log.append(12, "x", 1) != AppendResult::byte_capacity)
        return fail(4);
    if (log.size() != 2)
        return fail(5);
    EventView first = {};
    if (not log.get(0, first) or first.timestamp != 10 or first.length != 3)
        return fail(6);
    if (first.text[0] != 'a' or first.text[2] != 'c')
        return fail(7);
    log.reset();
    if (log.size() != 0)
        return fail(8);
    if (log.append(13, "xy", 2) != AppendResult::ok)
        return fail(9);

    EventLog eventBound;
    Event    oneEvent[1];
    char     largerBytes[8];
    if (not eventBound.init(largerBytes, sizeof(largerBytes), oneEvent, 1))
        return fail(10);
    if (eventBound.append(20, "a", 1) != AppendResult::ok)
        return fail(11);
    if (eventBound.append(21, "b", 1) != AppendResult::event_capacity)
        return fail(12);

    EventLog invalid;
    if (invalid.init(nullptr, 1, oneEvent, 1))
        return fail(13);
    return 0;
}
