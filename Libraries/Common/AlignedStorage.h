// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#if defined(SC_FOUNDATION_ALIGNED_STORAGE_DEFINITION_H)
#if SC_FOUNDATION_ALIGNED_STORAGE_DEFINITION_H != 1
#error "AlignedStorage.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_ALIGNED_STORAGE_DEFINITION_H 1 // Increment to indicate a new version of the file

#include "CompilerMacrosExport.h" // SC_FOUNDATION_EXPORT

namespace SC
{
#if !DOXYGEN
template <typename T, decltype(sizeof(0)) E, decltype(sizeof(0)) R = sizeof(T)>
void static_assert_size()
{
    static_assert(R <= E, "Size mismatch");
}
#endif
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow="
#endif
//! @addtogroup group_foundation_utility
//! @{

/// @brief  A buffer of bytes with given alignment.
///         Typically used in PIMPL or similar mechanisms to hide OS Specific system includes.
///         For example it's used used to wrap SocketIPAddress, a Mutex and ConditionVariable.
/// @tparam N Size in bytes of the Operating System Handle
/// @tparam Alignment Alignment in Bytes of the operating system Handle
template <int N, int Alignment = alignof(void*)>
struct SC_FOUNDATION_EXPORT AlignedStorage
{
    /// @brief  Access wanted OS Handle with it's actual type.
    ///         This is typically done in a .cpp file where the concrete type of the Handle is known
    ///         For example it is used inside  Mutex class like:
    /// @code
    /// pthread_mutex_t& myMutes = handle.reinterpret_as<pthread_mutex_t>()
    /// @endcode
    /// @tparam T Type of the handle. It will statically check size and alignment of requested type.
    /// @return A reference to actual OS handle
    template <typename T>
    T& reinterpret_as()
    {
        static_assert_size<T, N>();
        static_assert(alignof(T) <= Alignment, "Increase Alignment of AlignedStorage");
        return *reinterpret_cast<T*>(bytes);
    }

    /// @brief  Access wanted OS Handle with it's actual type.
    ///         This is typically done in a .cpp file where the concrete type of the Handle is known
    ///         For example it is used inside  Mutex class like:
    /// @code
    /// pthread_mutex_t& myMutes = handle.reinterpret_as<pthread_mutex_t>()
    /// @endcode
    /// @tparam T Type of the handle. It will statically check size and alignment of requested type.
    /// @return A reference to actual OS handle
    template <typename T>
    const T& reinterpret_as() const
    {
        static_assert_size<T, N>();
        static_assert(alignof(T) <= Alignment, "Increase Alignment of AlignedStorage");
        return *reinterpret_cast<const T*>(bytes);
    }

  private:
    alignas(Alignment) char bytes[N] = {0};
};

//! @}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
} // namespace SC

#endif