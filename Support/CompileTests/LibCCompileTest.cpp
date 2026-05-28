// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Libraries/Foundation/PrimitiveTypes.h"
#include <string.h>

int main()
{
    char        buffer[16];
    const char* text = "hello";

    (void)::memcpy(buffer, text, 6);
    const int sameAfterCopy = ::memcmp(buffer, text, 6);

    (void)::memset(buffer + 6, 'A', 4);
    (void)::memmove(buffer + 1, buffer, 5);

    auto*       writeableMatch = ::memchr(buffer, 'e', sizeof(buffer));
    const auto* readOnlyMatch  = ::memchr(text, 'l', 5);
    const auto  textLength     = ::strlen(text);

    return sameAfterCopy == 0 && writeableMatch != nullptr && readOnlyMatch != nullptr && textLength == 5 ? 0 : 1;
}
