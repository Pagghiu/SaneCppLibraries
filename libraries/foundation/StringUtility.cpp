#include "StringUtility.h"

bool SC::isIntegerNumber(Span<const char_t> text)
{
    if (text.size == 0)
        return false;
    if (isSign(text.data[0]))
    {
        // If we have sign in first char, but we have no more chars, then error
        if (text.size == 1)
            return false;
    }
    else if (!isDigit(text.data[0]))
    {
        // First char is not a sign and not a digit
        return false;
    }
    // From here, first is either a sign (and size > 1) or a digit
    // We just look for non-digits
    for (size_t idx = 1; idx < text.size; ++idx)
    {
        if (!isDigit(text.data[idx]))
        {
            return false;
        }
    }
    return true;
}
