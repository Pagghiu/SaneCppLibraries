// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/Result.h"

namespace SC
{
//! @addtogroup group_threading
//! @{

/// @brief Stable error codes returned by the Threading library.
enum class ThreadingError : uint32_t
{
    InvalidThreadFunction = 1,
    ThreadAlreadyStarted,
    ThreadNotStarted,
    ThreadCreationFailed,
    ThreadJoinFailed,
    ThreadDetachFailed,
    ThreadPoolAlreadyCreated,
    InvalidWorkerThreadCount,
    ThreadPoolNotCreated,
    TaskAlreadyQueued,
    TaskInUseByAnotherThreadPool,
    ThreadPoolThreadCreationFailed,
};

/// @brief Stable detail identifying the native/backend stage of a Threading failure.
enum class ThreadingErrorDetail : uint32_t
{
    None = 0,
    PosixPthreadCreate,
    PosixPthreadJoin,
    PosixPthreadDetach,
    WindowsCreateThread,
    WindowsWaitForSingleObject,
    WindowsCloseHandle,
};

/// @brief Stable category assigned to errors owned by the Threading library.
static constexpr ResultCategory ThreadingResultCategory = ResultCategory(1);

/// @brief Threading result retaining optional native and backend-stage details.
/// @details nativeError is zero when unavailable or irrelevant. detail is None when no lower-level stage is relevant.
/// Converting to Result preserves the portable error identity and deliberately discards both details. The two detail
/// fields occupy eight bytes, keeping this type at 16 bytes.
struct [[nodiscard]] ResultThreading
{
    Result               result;
    uint32_t             nativeError = 0;
    ThreadingErrorDetail detail      = ThreadingErrorDetail::None;

    explicit constexpr ResultThreading(bool valid = true) : result(valid) {}
    constexpr ResultThreading(ThreadingError error, ThreadingErrorDetail detail = ThreadingErrorDetail::None,
                              uint32_t nativeError = 0)
        : result(Result::Error(ThreadingResultCategory, error)), nativeError(nativeError), detail(detail)
    {}
    constexpr ResultThreading(Result result) : result(result) {}

    template <typename ResultLike>
    constexpr ResultThreading(const ResultLike& other) : result(other.toResult())
    {}

    explicit constexpr operator bool() const { return static_cast<bool>(result); }

    constexpr operator Result() const { return result; }

    constexpr Result toResult() const { return result; }
    constexpr bool   isError(ThreadingError error) const { return result.isError(ThreadingResultCategory, error); }
};

//! @}
} // namespace SC
