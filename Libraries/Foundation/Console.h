// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Compiler.h"
#include "OS.h"
#include "StringView.h"
#include "Types.h"

namespace SC
{
struct String;
struct Console
{
    template <typename... Types>
    static bool print(StringView fmt, Types... args)
    {
        StringFormatOutput output;
        output.writeToStdout = true;
        return StringFormat<StringIteratorASCII>::format(output, fmt, args...);
    }

    static void print(const StringView str);
    static void print(const String& str);
    struct Internal;
};
} // namespace SC
