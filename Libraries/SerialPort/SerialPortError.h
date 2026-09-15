// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Common/Result.h"

namespace SC
{
//! @addtogroup group_serial_port
//! @{

/// @brief Stable error codes returned by the SerialPort library.
enum class SerialPortError : uint32_t
{
    InvalidBaudRate = 1,
    InvalidDataBits,
    InvalidParity,
    InvalidStopBits,
    InvalidFlowControl,
    InvalidHandle,
    UnsupportedPathEncoding,
    InvalidPath,
    PathMustBeAbsolute,
    OpenFailed,
    ReadSettingsFailed,
    SetSettingsFailed,
    UnsupportedHardwareFlowControl,
    UnsupportedBaudRate,
    UnsupportedDataBits,
    UnsupportedParity,
    UnsupportedStopBits,
    PathTooLong,
};

/// @brief Stable lower-level and backend-specific context for a SerialPort error.
/// @details The platform is part of every OS-specific value so callers can use this context without relying on
/// platform headers. Detail values are diagnostic context and are deliberately not encoded in Result.
enum class SerialPortErrorDetail : uint32_t
{
    None = 0,
    PosixOpen,
    PosixReadDescriptorFlags,
    PosixSetDescriptorFlags,
    PosixReadSettings,
    PosixSetInputBaudRate,
    PosixSetOutputBaudRate,
    PosixSetSettings,
    WindowsCreateFile,
    WindowsSetTimeouts,
    WindowsReadSettings,
    WindowsSetSettings,
};

/// @brief Stable category assigned to errors owned by the SerialPort library.
static constexpr ResultCategory SerialPortResultCategory = ResultCategory(3);

/// @brief SerialPort result retaining optional lower-level and native error details.
/// @details nativeError is zero when unavailable or irrelevant. Converting to Result preserves the portable error
/// identity and deliberately discards both detail fields.
struct [[nodiscard]] ResultSerialPort
{
    Result                result;
    SerialPortErrorDetail detail      = SerialPortErrorDetail::None;
    uint32_t              nativeError = 0;

    explicit constexpr ResultSerialPort(bool valid = true) : result(valid) {}
    constexpr ResultSerialPort(SerialPortError error, SerialPortErrorDetail detail = SerialPortErrorDetail::None,
                               uint32_t nativeError = 0)
        : result(Result::Error(SerialPortResultCategory, error)), detail(detail), nativeError(nativeError)
    {}
    constexpr ResultSerialPort(Result result) : result(result) {}

    template <typename ResultLike>
    constexpr ResultSerialPort(const ResultLike& other) : result(other.toResult())
    {}

    explicit constexpr operator bool() const { return static_cast<bool>(result); }

    constexpr operator Result() const { return result; }

    constexpr Result toResult() const { return result; }
    constexpr bool   isError(SerialPortError error) const { return result.isError(SerialPortResultCategory, error); }
};

//! @}
} // namespace SC
