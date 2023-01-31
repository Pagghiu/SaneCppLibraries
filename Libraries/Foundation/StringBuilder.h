// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "String.h"
#include "StringFormat.h"

namespace SC
{
struct StringBuilder
{
    StringBuilder(StringEncoding encoding) : encoding(encoding) {}
    template <typename... Types>
    [[nodiscard]] bool append(StringView fmt, Types... args)
    {
        if (not data.isEmpty())
            SC_TRY_IF(data.pop_back());
        StringFormatOutput sos;
        sos.encoding   = encoding;
        sos.data       = move(data);
        const bool res = StringFormat<StringIteratorASCII>::format(sos, fmt, args...);
        data           = move(sos.data);
        return res;
    }

    [[nodiscard]] bool append(StringView str)
    {
        if (not data.isEmpty())
            SC_TRY_IF(data.pop_back());
        if (str.isNullTerminated())
        {
            return data.appendCopy(str.bytesIncludingTerminator(), str.sizeInBytesIncludingTerminator());
        }
        else
        {
            SC_TRY_IF(data.appendCopy(str.bytesWithoutTerminator(), str.sizeInBytes()));
            SC_TRY_IF(data.push_back(0));
            return true;
        }
    }

    [[nodiscard]] bool append(const String& str)
    {
        if (not data.isEmpty())
            SC_TRY_IF(data.pop_back());
        return data.appendCopy(str.bytesIncludingTerminator(), str.sizeInBytesIncludingTerminator());
    }

    [[nodiscard]] StringView view()
    {
        String     s(move(data), encoding);
        StringView sv = s.view();
        data          = move(s.data);
        return sv;
    }

    void clear() { (void)data.resizeWithoutInitializing(0); }

    [[nodiscard]] String releaseString() { return String(move(data), encoding); }

  private:
    StringEncoding encoding;
    Vector<char_t> data;
};
} // namespace SC
