// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#if !defined(SANE_CPP_LIBRARIES_HASHING)
#define SANE_CPP_LIBRARIES_HASHING 1
#include <stdbool.h> // bool
#include <stddef.h>  // size_t
#include <stdint.h>  // uint64_t

//! @defgroup group_c_bindings C Bindings
//! @brief C bindings for all libraries

//! @defgroup group_sc_hashing sc_hashing
//! @ingroup group_c_bindings

//! @addtogroup group_sc_hashing
//! @brief C bindings for @ref library_hashing library
///
/// Sample usage:
/// @code{.c}
/// sc_hashing_t ctx;
/// bool res;
/// res = sc_hashing_init(&ctx, type);
/// res = sc_hashing_add(&ctx, (sc_hashing_span_t){.data = "test", .length = strlen("test")});
/// res = sc_hashing_add(&ctx, (sc_hashing_span_t){.data = "data", .length = strlen("data")});
/// sc_hashing_result_t result;
/// sc_hashing_get(&ctx, &result);
/// //... use result.hash
/// sc_hashing_close(&ctx);
/// @endcode

//! @{

#if _MSC_VER
#define SANE_CPP_NO_DISCARD
#else
#define SANE_CPP_NO_DISCARD __attribute__((warn_unused_result))
#endif
// clang-format off
#ifdef __cplusplus
extern "C"
{
#endif

/// @brief Type of hashing algorithm to use
typedef enum
{
    SC_HASHING_TYPE_MD5,    ///< Computes MD5 Hash
    SC_HASHING_TYPE_SHA1,   ///< Computes SHA1 Hash
    SC_HASHING_TYPE_SHA256  ///< Computes SHA256 Hash
} sc_hashing_type;

/// @brief Opaque object holding state of hashing
typedef struct
{
    uint64_t opaque[14];
} sc_hashing_t;

/// @brief Hash result
typedef struct
{
    uint8_t hash[32];   ///< Contains the computed hash of length size
    size_t  size;       ///< Length of the computed hash
} sc_hashing_result_t;

/// @brief Just a generic data span
typedef struct
{
    const void* data;   ///< Pointer to data
    size_t      length; ///< Length of the data (in bytes)
} sc_hashing_span_t;

/// @brief Initializes OS objects to compute hash (call sc_hashing_close when done).
SANE_CPP_NO_DISCARD bool sc_hashing_init(sc_hashing_t* hashing, sc_hashing_type type);

/// @brief Releases os resources allocated by sc_hashing_init.
void sc_hashing_close(sc_hashing_t* hashing);

/// @brief Add data to hash computation. Can be called multiple times to hash data iteratively.
SANE_CPP_NO_DISCARD bool sc_hashing_add(sc_hashing_t* hashing, sc_hashing_span_t span);

/// @brief Obtain the actual hash of data added through sc_hashing_add.
SANE_CPP_NO_DISCARD bool sc_hashing_get(sc_hashing_t* hashing, sc_hashing_result_t* result);

#ifdef __cplusplus
} // extern "C"
#endif
// clang-format on

#undef SANE_CPP_NO_DISCARD
//! @}

#endif // SANE_CPP_LIBRARIES_HASHING
