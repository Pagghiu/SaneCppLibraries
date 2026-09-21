// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Common/CompilerMacrosStdCpp.h"
#include "../Common/CompilerMacrosStdVersion.h"

#if SC_INCLUDE_STD_CPP && SC_LANGUAGE_CPP_AT_LEAST_20

#include "Await.h"

namespace SC
{
const char* AwaitCancellationMessage() { return "AwaitTask cancelled"; }

const char* AwaitWrongEventLoopMessage() { return "AwaitTask belongs to another AwaitEventLoop"; }
} // namespace SC

#endif
