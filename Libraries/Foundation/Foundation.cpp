// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Foundation/Assert.h"
#include "../Foundation/HeapBuffer.h"
#include "../Foundation/Limits.h"
#include "../Foundation/Memory.h"

// system includes
#include <float.h>  // FLT_MAX / DBL_MAX
#include <stdint.h> // Linux *_{MAX | MIN}
#include <stdio.h>  // stdout
#include <stdlib.h> // malloc, free, *_MAX (integer)
#include <string.h> // strlen

#if SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL
#include <BaseTsd.h>
using ssize_t = SSIZE_T;
#endif

#if SC_PLATFORM_EMSCRIPTEN
#include <emscripten.h>
#elif SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else

#if SC_PLATFORM_APPLE
#include <TargetConditionals.h>
#endif

#include <execinfo.h> // backtrace
#include <unistd.h>   // _exit
#endif

SC::OperatingSystem::Type SC::OperatingSystem::getHostOS()
{
#if SC_PLATFORM_WINDOWS
    return Windows;
#elif SC_PLATFORM_LINUX
    return Linux;
#elif SC_PLATFORM_EMSCRIPTEN
    return emscripten;
#elif SC_PLATFORM_APPLE
#if TARGET_OS_IPHONE
    return iOS;
#else
    return macOS;
#endif
#endif
}

//--------------------------------------------------------------------
// Assert
//--------------------------------------------------------------------
#if SC_PLATFORM_EMSCRIPTEN
void SC::Assert::exit(int code) { ::emscripten_force_exit(code); }
#else
void SC::Assert::exit(int code) { ::_exit(code); }
#endif

void SC::Assert::printAscii(const char* str)
{
    if (str == nullptr)
        return;

#if SC_PLATFORM_WINDOWS
    ::WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), str, static_cast<DWORD>(::strlen(str)), nullptr, nullptr);
    // TODO: We should limit the string sent to OutputDebugStringA to numBytes
    ::OutputDebugStringA(str);
#else
    ::fwrite(str, sizeof(char), ::strlen(str), stdout);
#endif
}

#if SC_PLATFORM_EMSCRIPTEN or SC_PLATFORM_WINDOWS

bool SC::Assert::printBacktrace() { return true; }

bool SC::Assert::printBacktrace(void** backtraceBuffer, size_t backtraceBufferSizeInBytes)
{
    SC_COMPILER_UNUSED(backtraceBufferSizeInBytes);
    if (!backtraceBuffer)
        return false;
    return true;
}

SC::size_t SC::Assert::captureBacktrace(size_t framesToSkip, void** backtraceBuffer, size_t backtraceBufferSizeInBytes,
                                        uint32_t* hash)
{
    SC_COMPILER_UNUSED(framesToSkip);
    SC_COMPILER_UNUSED(backtraceBufferSizeInBytes);
    if (hash)
        *hash = 1;
    if (backtraceBuffer == nullptr)
        return 0;
    return 1;
}

#else

bool SC::Assert::printBacktrace()
{
    void* backtraceBuffer[100];
    return printBacktrace(backtraceBuffer, sizeof(backtraceBuffer));
}

bool SC::Assert::printBacktrace(void** backtraceBuffer, size_t backtraceBufferSizeInBytes)
{
    const size_t numFrames = captureBacktrace(2, backtraceBuffer, backtraceBufferSizeInBytes, nullptr);
    if (numFrames == 0)
    {
        return false;
    }
    char** strs = ::backtrace_symbols(backtraceBuffer, static_cast<int>(numFrames));
    for (size_t i = 0; i < numFrames; ++i)
    {
        printAscii(strs[i]);
        printAscii("\n");
    }
    // TODO: Fix Backtrace line numbers
    // https://stackoverflow.com/questions/8278691/how-to-fix-backtrace-line-number-error-in-c
    ::free(strs);
    return true;
}

SC::size_t SC::Assert::captureBacktrace(size_t framesToSkip, void** backtraceBuffer, size_t backtraceBufferSizeInBytes,
                                        uint32_t* hash)
{
    const size_t framesToCapture = backtraceBufferSizeInBytes / sizeof(void*);
    constexpr auto maxVal = static_cast<size_t>(static_cast<int>(MaxValue()));
    if (framesToCapture > maxVal || (backtraceBuffer == nullptr))
    {
        return 0;
    }
    // This signature maps 1 to 1 with windows CaptureStackBackTrace, at some
    // point we will allow framesToSkip > 0 and compute has
    int numFrames = ::backtrace(backtraceBuffer, static_cast<int>(framesToCapture));
    if (framesToSkip > static_cast<size_t>(numFrames))
        return 0;
    numFrames -= framesToSkip;
    if (framesToSkip > 0)
    {
        for (int frame = 0; frame < numFrames; ++frame)
        {
            backtraceBuffer[frame] = backtraceBuffer[static_cast<size_t>(frame) + framesToSkip];
        }
    }
    if (hash)
    {
        uint32_t computedHash = 0;
        // TODO: Compute a proper hash
        for (int i = 0; i < numFrames; ++i)
        {
            uint32_t value;
            ::memcpy(&value, backtraceBuffer[i], sizeof(uint32_t));
            computedHash ^= value;
        }
        *hash = computedHash;
    }
    return static_cast<size_t>(numFrames);
}

#endif

void SC::Assert::print(const char* expression, const char* filename, const char* functionName, int lineNumber)
{
    // Here we're explicitly avoiding usage of StringFormat to avoid dynamic allocation
    printAscii("Assertion failed: (");
    printAscii(expression);
    printAscii(")\nFile: ");
    printAscii(filename);
    printAscii("\nFunction: ");
    printAscii(functionName);
    printAscii("\nLine: ");
    char buffer[50];
    ::snprintf(buffer, sizeof(buffer), "%d", lineNumber);
    printAscii(buffer);
    printAscii("\n");
}

//--------------------------------------------------------------------
// Memory
//--------------------------------------------------------------------
void* SC::Memory::reallocate(void* memory, size_t numBytes) { return ::realloc(memory, numBytes); }
void* SC::Memory::allocate(size_t numBytes) { return ::malloc(numBytes); }
void  SC::Memory::release(void* allocatedMemory) { return ::free(allocatedMemory); }

//--------------------------------------------------------------------
// HeapBuffer
//--------------------------------------------------------------------

SC::HeapBuffer::~HeapBuffer() { Memory::release(data.data()); }

SC::HeapBuffer::HeapBuffer() = default;

SC::HeapBuffer::HeapBuffer(HeapBuffer&& other)
{
    data       = other.data;
    other.data = {};
}

SC::HeapBuffer& SC::HeapBuffer::operator=(HeapBuffer&& other)
{
    data       = other.data;
    other.data = {};
    return *this;
}

bool SC::HeapBuffer::allocate(size_t numBytes)
{
    Memory::release(data.data());
    char* memory = reinterpret_cast<char*>(Memory::allocate(numBytes));
    if (memory != nullptr)
    {
        data = {memory, numBytes};
        return true;
    }
    else
    {
        data = {};
        return false;
    }
}

bool SC::HeapBuffer::reallocate(size_t numBytes)
{
    char* memory = reinterpret_cast<char*>(Memory::reallocate(data.data(), numBytes));
    if (memory != nullptr)
    {
        data = {memory, numBytes};
        return true;
    }
    else
    {
        data = {};
        return false;
    }
}

void SC::HeapBuffer::release()
{
    Memory::release(data.data());
    data = {};
}
//--------------------------------------------------------------------
// Standard C++ Library support
//--------------------------------------------------------------------
#if not SC_COMPILER_ENABLE_STD_CPP and (not SC_COMPILER_MSVC or not SC_COMPILER_CLANG_CL)
#if SC_COMPILER_ASAN == 0
void operator delete(void* p) noexcept
{
    if (p != 0)
        SC_LANGUAGE_LIKELY { ::free(p); }
}
void operator delete[](void* p) noexcept
{
    if (p != 0)
        SC_LANGUAGE_LIKELY { ::free(p); }
}
void operator delete(void* p, size_t) noexcept
{
    if (p != 0)
        SC_LANGUAGE_LIKELY { ::free(p); }
}
void operator delete[](void* p, size_t) noexcept
{
    if (p != 0)
        SC_LANGUAGE_LIKELY { ::free(p); }
}
#endif

#if SC_COMPILER_ASAN == 0
void* operator new(size_t len) { return ::malloc(len); }
void* operator new[](size_t len) { return ::malloc(len); }
#endif

void* __cxa_pure_virtual   = 0;
void* __gxx_personality_v0 = 0;

// We're stripping away all locking from static initialization because if one relies on multithreaded order of
// static initialization for your program to function properly, it's a good idea to encourage it to fail/crash
//// so one can fix it with some sane code.
extern "C" int __cxa_guard_acquire(uint64_t* guard_object)
{
    if (*reinterpret_cast<const uint8_t*>(guard_object) != 0)
        return 0;
    return 1;
}

extern "C" void __cxa_guard_release(uint64_t* guard_object) { *reinterpret_cast<uint8_t*>(guard_object) = 1; }
extern "C" void __cxa_guard_abort(uint64_t* guard_object) { SC_COMPILER_UNUSED(guard_object); }
#endif

//--------------------------------------------------------------------
// Limits
//--------------------------------------------------------------------
namespace SC
{
template <typename T, typename U>
struct CheckSameType
{
    static constexpr bool value = false;
};
template <typename T>
struct CheckSameType<T, T>
{
    static constexpr bool value = true;
};

static_assert(static_cast<float>(MaxValue()) == FLT_MAX, "static_cast<float>(MaxValue)");
static_assert(static_cast<double>(MaxValue()) == DBL_MAX, "static_cast<double>(MaxValue)");

static_assert(static_cast<uint8_t>(MaxValue()) == UINT8_MAX, "static_cast<uint8_t>(MaxValue)");
static_assert(static_cast<uint16_t>(MaxValue()) == UINT16_MAX, "static_cast<uint16_t>(MaxValue)");
static_assert(static_cast<uint32_t>(MaxValue()) == UINT32_MAX, "static_cast<uint32_t>(MaxValue)");
static_assert(static_cast<uint64_t>(MaxValue()) == UINT64_MAX, "static_cast<uint64_t>(MaxValue)");

static_assert(static_cast<int8_t>(MaxValue()) == INT8_MAX, "static_cast<int8_t>(MaxValue)");
static_assert(static_cast<int16_t>(MaxValue()) == INT16_MAX, "static_cast<int16_t>(MaxValue)");
static_assert(static_cast<int32_t>(MaxValue()) == INT32_MAX, "static_cast<int32_t>(MaxValue)");
static_assert(static_cast<int64_t>(MaxValue()) == INT64_MAX, "static_cast<int64_t>(MaxValue)");

static_assert(CheckSameType<uint8_t, ::uint8_t>::value, "uint8_t");
static_assert(CheckSameType<uint16_t, ::uint16_t>::value, "uint16_t");
static_assert(CheckSameType<uint32_t, ::uint32_t>::value, "uint32_t");
static_assert(CheckSameType<uint64_t, ::uint64_t>::value, "uint64_t");

static_assert(CheckSameType<int8_t, ::int8_t>::value, "int8_t");
static_assert(CheckSameType<int16_t, ::int16_t>::value, "int16_t");
static_assert(CheckSameType<int32_t, ::int32_t>::value, "int32_t");
static_assert(CheckSameType<int64_t, ::int64_t>::value, "int64_t");

static_assert(CheckSameType<size_t, ::size_t>::value, "size_t");
static_assert(CheckSameType<ssize_t, ::ssize_t>::value, "size_t");
} // namespace SC
