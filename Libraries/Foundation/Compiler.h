// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#if defined(SC_COMPILER_ENABLE_CONFIG)
#include "SCConfig.h"
#endif

//! @defgroup group_foundation_compiler_macros Compiler Macros
//! @ingroup group_foundation
//! Compiler Macros
/// Preprocessor macros to detect compiler and platform features.

//! @addtogroup group_foundation_compiler_macros
//! @{
#include "../Common/CompilerMacrosDebugBreak.h"
#include "../Common/CompilerMacrosExport.h"
#include "../Common/CompilerMacrosInline.h"
#include "../Common/CompilerMacrosLibraryPath.h"
#include "../Common/CompilerMacrosLifetimeBound.h"
#include "../Common/CompilerMacrosStdCpp.h"
#include "../Common/CompilerMacrosStdVersion.h"
#include "../Common/CompilerMacrosType.h"
#include "../Common/CompilerMacrosUnusedResult.h"
//! @}

#include "../Common/CompilerMinMax.h"
#include "../Common/CompilerMove.h"
#include "../Common/CompilerOffsetOf.h"
