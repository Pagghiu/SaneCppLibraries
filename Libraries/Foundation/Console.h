// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "StringFormat.h"
#include "StringView.h"
#include "Vector.h"

namespace SC
{
struct String;
struct Console
{
    Console(Vector<char>& encodingConversionBuffer) : encodingConversionBuffer(encodingConversionBuffer) {}

    template <typename... Types>
    bool print(StringView fmt, Types&&... args)
    {
        StringFormatOutput output(fmt.getEncoding());
        output.redirectToConsole(*this);
        if (fmt.getEncoding() == StringEncoding::Ascii || fmt.getEncoding() == StringEncoding::Utf8)
        {
            // It's ok parsing format string '{' and '}' both for utf8 and ascii with StringIteratorASCII
            // because on a valid UTF8 string, these chars are unambiguously recognizable
            return StringFormat<StringIteratorASCII>::format(output, fmt, forward<Types>(args)...);
        }
        return false; // UTF16/32 format strings are not supported
    }

    void print(const StringView str);
    void printLine(const StringView str);

    static void printNullTerminatedASCII(const StringView str);

  private:
    Vector<char>& encodingConversionBuffer;
};
} // namespace SC
