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
    ReadDescriptorFlagsFailed,
    SetDescriptorFlagsFailed,
    ReadSettingsFailed,
    SetSettingsFailed,
    UnsupportedHardwareFlowControl,
    UnsupportedBaudRate,
    UnsupportedDataBits,
    UnsupportedParity,
    UnsupportedStopBits,
    PathTooLong,
    SetTimeoutsFailed,
};

/// @brief Stable category assigned to errors owned by the SerialPort library.
static constexpr ResultCategory SerialPortResultCategory = ResultCategory(3);

/// @brief SerialPort result retaining an optional native platform error value.
/// @details nativeError is zero when unavailable or irrelevant. Converting to Result preserves the portable error
/// identity and deliberately discards the native detail.
struct [[nodiscard]] ResultSerialPort
{
    Result   result;
    uint32_t nativeError = 0;

    explicit constexpr ResultSerialPort(bool valid = true) : result(valid) {}
    constexpr ResultSerialPort(SerialPortError error, uint32_t nativeError = 0)
        : result(Result::Error(SerialPortResultCategory, error)), nativeError(nativeError)
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
