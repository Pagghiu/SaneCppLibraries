#include "SCConfig.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#if _MSC_VER
#pragma warning (push)
#pragma warning (disable:4244)
#elif __clang__
#pragma clang diagnostic push
#elif __GCC__
#endif

#define NANOSVG_IMPLEMENTATION
#include "_nanosvg/src/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "_nanosvg/src/nanosvgrast.h"
#if _MSC_VER
#pragma warning (pop)
#elif __clang__
#pragma clang diagnostic pop
#else

#endif
