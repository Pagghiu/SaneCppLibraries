#include "Console.h"
#include "Limits.h"
#include "Platform.h"
#include "String.h"

#include <stdarg.h> // va_list
#include <stdio.h>  // printf

int SC::Console::c_printf(const char_t* format, ...)
{
    va_list args;
    va_start(args, format);
    int res = ::vprintf(format, args);
    va_end(args);
    return res;
}

void SC::Console::printUTF8(const StringView str)
{
    SC_DEBUG_ASSERT(str.sizeInBytesWithoutTerminator() < static_cast<int>(MaxValue()));
    printf("%.*s", static_cast<int>(str.sizeInBytesWithoutTerminator()), str.bytesWithoutTerminator());
}

void SC::Console::printUTF8(const String& str)
{
    const size_t length = str.sizeInBytesIncludingTerminator();
    SC_DEBUG_ASSERT(length < static_cast<int>(MaxValue()));
    if (length > 1)
    {
        // TODO: On windows this is not UTF8 for sure
        printf("%.*s", static_cast<int>(length), str.bytesIncludingTerminator());
    }
}
