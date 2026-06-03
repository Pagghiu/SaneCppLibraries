// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Compiler.h"

#ifndef SC_EXPORT_LIBRARY_STRINGS
#define SC_EXPORT_LIBRARY_STRINGS 0
#endif
#define SC_STRINGS_EXPORT SC_COMPILER_LIBRARY_EXPORT(SC_EXPORT_LIBRARY_STRINGS)

#include "../Common/Assert.h"

namespace SC
{
SC_DECLARE_ASSERT_PROVIDER(StringsAssert, SC_STRINGS_EXPORT);
} // namespace SC

#define SC_STRINGS_ASSERT_RELEASE(e)        SC_ASSERT_PROVIDER_RELEASE(SC::StringsAssert, e)
#define SC_STRINGS_ASSERT_DEBUG(e)          SC_ASSERT_PROVIDER_DEBUG(SC::StringsAssert, e)
#define SC_STRINGS_TRUST_RESULT(expression) SC_STRINGS_ASSERT_RELEASE(expression)
