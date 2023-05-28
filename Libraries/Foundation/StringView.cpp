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
        value = atoi(text.data());
    }
    else
    {
        char_t buffer[12]; // 10 digits + sign + nullterm
        if (text.sizeInBytes() >= sizeof(buffer))
            return false;
        memcpy(buffer, text.data(), text.sizeInBytes());
        buffer[text.sizeInBytes()] = 0;

        value = atoi(buffer);
    }
    if (value == 0)
    {
        // atoi returns 0 on failed parsing...
        StringIteratorASCII it = getIterator<StringIteratorASCII>();
        (void)it.advanceIfMatchesAny({'-', '+'}); // optional
        if (it.isEmpty())
        {
            return false;
        }
        it.advanceUntilDifferentFrom('0'); // any number of 0s
        return it.isEmpty();
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
        value = atof(text.data());
    }
    else
    {
        char         buffer[255];
        const size_t bufferSize = min(text.sizeInBytes(), sizeof(buffer) - 1);
        memcpy(buffer, text.data(), bufferSize);
        buffer[bufferSize] = 0;
        value              = atof(buffer);
    }
    if (value == 0.0f)
    {
        // atof returns 0 on failed parsing...
        // TODO: Handle float scientific notation
        StringIteratorASCII it = getIterator<StringIteratorASCII>();
        (void)it.advanceIfMatchesAny({'-', '+'}); // optional
        if (it.isEmpty())                         // we now require something
        {
            return false;
        }
        if (it.advanceIfMatches('.')) // optional
        {
            if (it.isEmpty()) // but if it exists now we need at least a number
            {
                return false;
            }
        }
        it.advanceUntilDifferentFrom('0'); // any number of 0s
        return it.isEmpty();               // if they where all zeroes
    }
    return true;
}

SC::StringComparison SC::StringView::compareASCII(StringView other) const
{
    const int res = memcmp(text.data(), other.text.data(), min(text.sizeInBytes(), other.text.sizeInBytes()));
    if (res < 0)
        return StringComparison::Smaller;
    else if (res == 0)
        return StringComparison::Equals;
    else
        return StringComparison::Bigger;
}

bool SC::StringView::startsWith(const StringView str) const
{
    if (encoding == str.encoding && str.text.sizeInBytes() <= text.sizeInBytes())
    {
        const StringView ours(text.data(), str.text.sizeInBytes(), false, encoding);
        return str == ours;
    }
    return false;
}
bool SC::StringView::endsWith(const StringView str) const
{
    if (hasCompatibleEncoding(str) && str.sizeInBytes() <= sizeInBytes())
    {
        const StringView ours(text.data() + text.sizeInBytes() - str.text.sizeInBytes(), str.text.sizeInBytes(), false,
                              encoding);
        return str == ours;
    }
    return false;
}
