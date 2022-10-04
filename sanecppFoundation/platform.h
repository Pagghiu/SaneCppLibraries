#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define SANECPP_DEBUG   1
#define SANECPP_RELEASE 0
#else
#define SANECPP_DEBUG   0
#define SANECPP_RELEASE 1
#endif

#define SANECPP_PLATFORM_DARWIN  1
#define SANECPP_PLATFORM_LINUX   0
#define SANECPP_PLATFORM_WINDOWS 0
