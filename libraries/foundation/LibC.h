#pragma once
#include "Types.h"

extern "C"
{
    // Memory
    void* memcpy(void* dst, const void* src, SC::size_t n);
    int   memcmp(const void* s1, const void* s2, SC::size_t n);
    void* memset(void* dst, SC::int32_t c, SC::size_t len);
    void* memchr(const void* ptr, SC::int32_t c, SC::size_t count);

    // system
    void exit(SC::int32_t val) __attribute__((__noreturn__));

    // string
    SC::int32_t atoi(const SC::char_t* str);
    SC::size_t  strlen(const SC::char_t* str);
}
