# Standalone bounded event log library

Create a small C++ library with no dependency on Sane C++ Libraries. It receives timestamped event text and lets callers iterate the retained events in insertion order. The library must have a fixed caller-selected storage budget in bytes and a fixed caller-selected event-slot count. It may reject a new event when either capacity is exhausted; report the reason distinctly. It must not allocate internally.

Provide `event_log.h`, `event_log.cpp`, `event_log_test.cpp`, and `build.sh`; the build script must place its test executable in the directory named by `SC_SKILL_EVAL_BUILD_DIR`. The public API must include an `EventLog` type with `bool init(void* bytes, size_t byteCapacity, Event* events, size_t eventCapacity)`, `AppendResult append(uint64_t timestamp, const char* text, size_t length)`, `bool get(size_t index, EventView& out) const`, `size_t size() const`, and `void reset()`. `AppendResult` must distinguish `ok`, `event_capacity`, and `byte_capacity`. `EventView` must expose timestamp, pointer, and length. Document whether event text is copied or borrowed, what happens to an iterator/view when the log is reset, and how failed initialization leaves the object. Make it compile with warnings enabled and without exceptions/RTTI, but do not use Sane types or APIs.

Do not read this benchmark's acceptance or review files while implementing.
