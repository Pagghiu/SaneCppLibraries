#include "console.h"
#include "limits.h"
#include "platform.h"

#include <stdarg.h> // va_list
#include <stdio.h>  // printf

//#define XSTR(x) STR(x)
//#define STR(x) #x
//#pragma message "The value of DOUBLE_MAX: " XSTR(DBL_MAX)
int sanecpp::printf(const char_t* format, ...)
{
    va_list args;
    va_start(args, format);
    int res = ::vprintf(format, args);
    va_end(args);
    return res;
}
