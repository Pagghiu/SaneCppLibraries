// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Assert.h"
#include "Console.h"

#include <stdio.h>

void SC::printAssertion(const char_t* expression, const char_t* filename, const char_t* functionName, int lineNumber)
{
    // Here we're explicitly avoiding usage of StringFormat to avoid synamic allocation
    Console::print("Assertion failed: (");
    Console::print(StringView(expression, strlen(expression), true, StringEncoding::Utf8));
    Console::print(")\nFile: ");
    Console::print(StringView(filename, strlen(filename), true, StringEncoding::Utf8));
    Console::print("\nFunction: ");
    Console::print(StringView(functionName, strlen(functionName), true, StringEncoding::Utf8));
    Console::print("\nLine: ");
    char_t    buffer[50];
    const int numCharsExcludingTerminator = snprintf(buffer, sizeof(buffer), "%d", lineNumber);
    Console::print(StringView(buffer, numCharsExcludingTerminator, false, StringEncoding::Utf8));
    Console::print("\n");
}
