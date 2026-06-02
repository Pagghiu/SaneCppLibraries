// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#if defined(SC_FOUNDATION_PLATFORM_MACROS_DEBUG_DEFINITION_H)
#if SC_FOUNDATION_PLATFORM_MACROS_DEBUG_DEFINITION_H != 1
#error "PlatformMacrosDebug.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_PLATFORM_MACROS_DEBUG_DEFINITION_H 1 // Increment to indicate a new version of the file

#if defined(DEBUG) || defined(_DEBUG)
#define SC_CONFIGURATION_DEBUG   1 ///< True (1) if release configuration is active (_DEBUG==1 or DEBUG==1)
#define SC_CONFIGURATION_RELEASE 0 ///< True (1) if release configuration is active (!(_DEBUG==1 or DEBUG==1))
#else
#define SC_CONFIGURATION_DEBUG   0 ///< True (1) if debug configuration is active (_DEBUG==1 or DEBUG==1)
#define SC_CONFIGURATION_RELEASE 1 ///< True (1) if release configuration is active (!(_DEBUG==1 or DEBUG==1))
#endif

#endif // SC_FOUNDATION_PLATFORM_MACROS_DEBUG_DEFINITION_H
