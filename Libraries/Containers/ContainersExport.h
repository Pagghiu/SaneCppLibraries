// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Common/CompilerMacrosExport.h"

#ifndef SC_EXPORT_LIBRARY_CONTAINERS
#define SC_EXPORT_LIBRARY_CONTAINERS 0
#endif
#define SC_CONTAINERS_EXPORT SC_COMPILER_LIBRARY_EXPORT(SC_EXPORT_LIBRARY_CONTAINERS)

#include "../Common/Assert.h"

namespace SC
{
SC_DECLARE_ASSERT_PROVIDER(ContainersAssert, SC_CONTAINERS_EXPORT);
} // namespace SC

#define SC_CONTAINERS_ASSERT_RELEASE(e)        SC_ASSERT_PROVIDER_RELEASE(SC::ContainersAssert, e)
#define SC_CONTAINERS_ASSERT_DEBUG(e)          SC_ASSERT_PROVIDER_DEBUG(SC::ContainersAssert, e)
#define SC_CONTAINERS_TRUST_RESULT(expression) SC_CONTAINERS_ASSERT_RELEASE(expression)
