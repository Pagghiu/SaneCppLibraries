// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define SC_DEBUG   1
#define SC_RELEASE 0
#else
#define SC_DEBUG   0
#define SC_RELEASE 1
#endif

#define SC_PLATFORM_DARWIN  1
#define SC_PLATFORM_LINUX   0
#define SC_PLATFORM_WINDOWS 0
