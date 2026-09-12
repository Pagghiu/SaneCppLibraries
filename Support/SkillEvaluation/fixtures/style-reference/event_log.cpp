#include "event_log.h"

bool EventLog::init(void* bytes, size_t byteCapacity, Event* events, size_t eventCapacity)
{
    bytes_         = nullptr;
    byteCapacity_  = 0;
    usedBytes_     = 0;
    events_        = nullptr;
    eventCapacity_ = 0;
    count_         = 0;
    if (bytes == nullptr or events == nullptr or byteCapacity == 0 or eventCapacity == 0)
        return false;
    bytes_         = static_cast<char*>(bytes);
    byteCapacity_  = byteCapacity;
    events_        = events;
    eventCapacity_ = eventCapacity;
    return true;
}

AppendResult EventLog::append(uint64_t timestamp, const char* text, size_t length)
{
    if (count_ == eventCapacity_)
        return AppendResult::event_capacity;
    if (length > byteCapacity_ - usedBytes_)
        return AppendResult::byte_capacity;
    if (text == nullptr and length != 0)
        return AppendResult::byte_capacity;
    for (size_t index = 0; index < length; ++index)
        bytes_[usedBytes_ + index] = text[index];
    events_[count_] = {usedBytes_, length, timestamp};
    usedBytes_ += length;
    count_++;
    return AppendResult::ok;
}

bool EventLog::get(size_t index, EventView& out) const
{
    if (index >= count_)
        return false;
    const Event& event = events_[index];
    out                = {bytes_ + event.offset, event.length, event.timestamp};
    return true;
}

size_t EventLog::size() const { return count_; }

void EventLog::reset()
{
    usedBytes_ = 0;
    count_     = 0;
}
