// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Assert.h"
#include "Console.h"

void SC::printAssertion(const char_t* expression, const char_t* filename, const char_t* functionName, int lineNumber)
{
    Console::c_printf("Assertion failed: (%s), function %s, file %s, line %d\n", expression, filename, functionName,
                      lineNumber);
}
