// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Assert.h"
#include "Console.h"

#include <stdio.h>

void SC::printAssertion(const char_t* expression, const char_t* filename, const char_t* functionName, int lineNumber)
{
    // Here we're explicitly avoiding usage of StringFormat to avoid synamic allocation
    Console::printNullTerminatedASCII("Assertion failed: ("_a8);
    Console::printNullTerminatedASCII(StringView(expression, strlen(expression), true, StringEncoding::Ascii));
    Console::printNullTerminatedASCII(")\nFile: "_a8);
    Console::printNullTerminatedASCII(StringView(filename, strlen(filename), true, StringEncoding::Ascii));
    Console::printNullTerminatedASCII("\nFunction: "_a8);
    Console::printNullTerminatedASCII(StringView(functionName, strlen(functionName), true, StringEncoding::Ascii));
    Console::printNullTerminatedASCII("\nLine: "_a8);
    char_t    buffer[50];
    const int numCharsExcludingTerminator = snprintf(buffer, sizeof(buffer), "%d", lineNumber);
    Console::printNullTerminatedASCII(StringView(buffer, numCharsExcludingTerminator, true, StringEncoding::Ascii));
    Console::printNullTerminatedASCII("\n"_a8);
}
