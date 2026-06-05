// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Common/CompilerMacrosExport.h"

#ifndef SC_EXPORT_LIBRARY_HTTP
#define SC_EXPORT_LIBRARY_HTTP 0
#endif
#define SC_HTTP_EXPORT SC_COMPILER_LIBRARY_EXPORT(SC_EXPORT_LIBRARY_HTTP)

#include "../Common/Assert.h"

namespace SC
{
SC_DECLARE_ASSERT_PROVIDER(HttpAssert, SC_HTTP_EXPORT);
} // namespace SC

#define SC_HTTP_ASSERT_RELEASE(e)        SC_ASSERT_PROVIDER_RELEASE(SC::HttpAssert, e)
#define SC_HTTP_ASSERT_DEBUG(e)          SC_ASSERT_PROVIDER_DEBUG(SC::HttpAssert, e)
#define SC_HTTP_TRUST_RESULT(expression) SC_HTTP_ASSERT_RELEASE(expression)
