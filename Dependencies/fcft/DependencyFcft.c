#if _MSC_VER
#pragma warning (push)
#pragma warning (disable:4244 4005)
#elif __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#elif __GCC__
#endif

#include "../nanosvg/_nanosvg/src/nanosvg.h"
#include "../nanosvg/_nanosvg/src/nanosvgrast.h"
#include <stdint.h>
#include "_fcft/svg-backend-nanosvg.c"

#if _MSC_VER
#pragma warning (pop)
#elif __clang__
#pragma clang diagnostic pop
#else

#endif
