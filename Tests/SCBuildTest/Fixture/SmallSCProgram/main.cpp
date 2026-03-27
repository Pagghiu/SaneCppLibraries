// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "Libraries/Memory/String.h"
#include "Libraries/Strings/StringBuilder.h"
#include <stdio.h>

int main()
{
    SC::SmallString<32> output(SC::StringEncoding::Ascii);
    if (not SC::StringBuilder::format(output, "small-{}", 42))
    {
        return 1;
    }
    if (output.view() != "small-42")
    {
        return 2;
    }

    puts(output.view().bytesIncludingTerminator());
    return 0;
}
