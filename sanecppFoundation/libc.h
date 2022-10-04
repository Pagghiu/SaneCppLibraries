#pragma once
#include "types.h"

extern "C"
{
    // Memory
    void* memcpy(void* dst, const void* src, sanecpp::size_t n);
    int   memcmp(const void* s1, const void* s2, sanecpp::size_t n);
    void* memset(void* dst, sanecpp::int32_t c, sanecpp::size_t len);

    // system
    void exit(sanecpp::int32_t val) __attribute__((__noreturn__));

    // string
    sanecpp::int32_t atoi(const sanecpp::char_t* str);
    sanecpp::size_t  strlen(const sanecpp::char_t* str);
}
