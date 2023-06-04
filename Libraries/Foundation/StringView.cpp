// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "StringView.h"

#include <stdlib.h> //atoi

bool SC::StringView::parseInt32(int32_t& value) const
{
    if (encoding != StringEncoding::Ascii and encoding != StringEncoding::Utf8)
    {
        return false;
    }

    if (hasNullTerm)
    {
        value = atoi(textUtf8);
    }
    else
    {
        char_t buffer[12]; // 10 digits + sign + nullterm
        if (textSizeInBytes >= sizeof(buffer))
            return false;
        memcpy(buffer, textUtf8, textSizeInBytes);
        buffer[textSizeInBytes] = 0;

        value = atoi(buffer);
    }
    if (value == 0)
    {
        // atoi returns 0 on failed parsing...
        StringIteratorASCII it = getIterator<StringIteratorASCII>();
        (void)it.advanceIfMatchesAny({'-', '+'}); // optional
        if (it.isAtEnd())
        {
            return false;
        }
        it.advanceUntilDifferentFrom('0'); // any number of 0s
        return it.isAtEnd();
    }
    return true;
}

bool SC::StringView::parseFloat(float& value) const
{
    double dValue;
    if (parseDouble(dValue))
    {
        value = static_cast<float>(dValue);
        return true;
    }
    return false;
}

bool SC::StringView::parseDouble(double& value) const
{
    if (hasNullTerm)
    {
        value = atof(textUtf8);
    }
    else
    {
        char         buffer[255];
        const size_t bufferSize = min(textSizeInBytes, static_cast<decltype(textSizeInBytes)>(sizeof(buffer) - 1));
        memcpy(buffer, textUtf8, bufferSize);
        buffer[bufferSize] = 0;
        value              = atof(buffer);
    }
    if (value == 0.0f)
    {
        // atof returns 0 on failed parsing...
        // TODO: Handle float scientific notation
        StringIteratorASCII it = getIterator<StringIteratorASCII>();
        (void)it.advanceIfMatchesAny({'-', '+'}); // optional
        if (it.isAtEnd())                         // we now require something
        {
            return false;
        }
        if (it.advanceIfMatches('.')) // optional
        {
            if (it.isAtEnd()) // but if it exists now we need at least a number
            {
                return false;
            }
        }
        it.advanceUntilDifferentFrom('0'); // any number of 0s
        return it.isAtEnd();               // if they where all zeroes
    }
    return true;
}

SC::StringComparison SC::StringView::compareASCII(StringView other) const
{
    const int res = memcmp(textUtf8, other.textUtf8, min(textSizeInBytes, other.textSizeInBytes));
    if (res < 0)
        return StringComparison::Smaller;
    else if (res == 0)
        return StringComparison::Equals;
    else
        return StringComparison::Bigger;
}

bool SC::StringView::startsWith(const StringView str) const
{
    if (hasCompatibleEncoding(str))
    {
        if (str.textSizeInBytes <= textSizeInBytes)
        {
            const StringView ours(textUtf8, str.textSizeInBytes, false, encoding);
            return str == ours;
        }
        return false;
    }
    return withIterator([str](auto it1) { return str.withIterator([it1](auto it2) { return it1.startsWith(it2); }); });
}

bool SC::StringView::endsWith(const StringView str) const
{
    if (hasCompatibleEncoding(str))
    {
        if (str.sizeInBytes() <= sizeInBytes())
        {
            const StringView ours(textUtf8 + textSizeInBytes - str.textSizeInBytes, str.textSizeInBytes, false,
                                  encoding);
            return str == ours;
        }
        return false;
    }
    return withIterator([str](auto it1) { return str.withIterator([it1](auto it2) { return it1.endsWith(it2); }); });
}

[[nodiscard]] bool SC::StringView::containsString(const StringView str) const
{
    SC_RELEASE_ASSERT(hasCompatibleEncoding(str));
    return withIterator([str](auto it) { return it.advanceAfterFinding(str.getIterator<decltype(it)>()); });
}
