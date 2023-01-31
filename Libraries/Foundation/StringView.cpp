// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "StringView.h"

bool SC::StringView::parseInt32(int32_t* value) const
{
    if (isIntegerNumber<StringIteratorASCII>())
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

template <typename StringIterator>
bool SC::StringView::isIntegerNumber() const
{
    if (text.sizeInBytes() == 0)
        return false;
    StringIteratorASCII it = getIterator<StringIterator>();
    if (it.matchesAny({'-', '+'}))
    {
        (void)it.skipNext();
    }

    // From here, first is either a sign (and size > 1) or a digit
    // We just look for non-digits
    bool matchedAtLeastOneDigit = false;
    do
    {
        if (not it.matchesAny({'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'}))
        {
            return false;
        }
        matchedAtLeastOneDigit = true;
    } while (it.skipNext());
    return matchedAtLeastOneDigit;
}
