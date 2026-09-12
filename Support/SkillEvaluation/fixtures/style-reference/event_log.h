#pragma once

#include <stddef.h>
#include <stdint.h>

enum class AppendResult
{
    ok,
    event_capacity,
    byte_capacity,
};

struct Event
{
    size_t   offset;
    size_t   length;
    uint64_t timestamp;
};

struct EventView
{
    const char* text;
    size_t      length;
    uint64_t    timestamp;
};

class EventLog
{
  public:
    bool         init(void* bytes, size_t byteCapacity, Event* events, size_t eventCapacity);
    AppendResult append(uint64_t timestamp, const char* text, size_t length);
    bool         get(size_t index, EventView& out) const;
    size_t       size() const;
    void         reset();

  private:
    char*  bytes_         = nullptr;
    size_t byteCapacity_  = 0;
    size_t usedBytes_     = 0;
    Event* events_        = nullptr;
    size_t eventCapacity_ = 0;
    size_t count_         = 0;
};
