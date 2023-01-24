// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "StringView.h"
#include "StringUtility.h"

// TODO: Use StringIterator
bool SC::StringView::parseInt32(int32_t* value) const
{
    if (isIntegerNumber(text))
    {
        if (hasNullTerm)
        {
            *value = atoi(text.data());
        }
        else
        {
            char_t buffer[12]; // 10 digits + sign + nullterm
            memcpy(buffer, text.data(), text.sizeInBytes());
            buffer[text.sizeInBytes()] = 0;

            *value = atoi(buffer);
        }
        return true;
    }
    else
    {
        return false;
    }
}
