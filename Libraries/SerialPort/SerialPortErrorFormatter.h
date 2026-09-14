// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Common/ResultErrorFormatter.h"
#include "SerialPortError.h"

namespace SC
{
//! @addtogroup group_serial_port
//! @{

namespace detail
{
inline ResultErrorFormat formatSerialPortErrorWithNativeCode(SerialPortError error, uint32_t nativeError,
                                                             Span<char> output)
{
    ResultErrorFormatter formatter(output);
    switch (error)
    {
    case SerialPortError::InvalidBaudRate: formatter.append("Baud rate must be greater than zero"); break;
    case SerialPortError::InvalidDataBits: formatter.append("Data bits value is invalid"); break;
    case SerialPortError::InvalidParity: formatter.append("Parity value is invalid"); break;
    case SerialPortError::InvalidStopBits: formatter.append("Stop bits value is invalid"); break;
    case SerialPortError::InvalidFlowControl: formatter.append("Flow control value is invalid"); break;
    case SerialPortError::InvalidHandle: formatter.append("Serial descriptor is invalid"); break;
    case SerialPortError::UnsupportedPathEncoding: formatter.append("Serial path encoding is not supported"); break;
    case SerialPortError::InvalidPath: formatter.append("Serial path is invalid"); break;
    case SerialPortError::PathMustBeAbsolute: formatter.append("Serial path must be absolute"); break;
    case SerialPortError::OpenFailed: formatter.append("Failed to open serial port"); break;
    case SerialPortError::ReadDescriptorFlagsFailed: formatter.append("Failed to read descriptor flags"); break;
    case SerialPortError::SetDescriptorFlagsFailed: formatter.append("Failed to set descriptor flags"); break;
    case SerialPortError::ReadSettingsFailed: formatter.append("Failed to read serial settings"); break;
    case SerialPortError::SetSettingsFailed: formatter.append("Failed to set serial settings"); break;
    case SerialPortError::UnsupportedHardwareFlowControl:
        formatter.append("Hardware flow control is not supported");
        break;
    case SerialPortError::UnsupportedBaudRate: formatter.append("Baud rate is not supported"); break;
    case SerialPortError::UnsupportedDataBits: formatter.append("Data bits value is not supported"); break;
    case SerialPortError::UnsupportedParity: formatter.append("Parity value is not supported"); break;
    case SerialPortError::UnsupportedStopBits: formatter.append("Stop bits value is not supported"); break;
    case SerialPortError::PathTooLong: formatter.append("Serial path is too long"); break;
    case SerialPortError::SetTimeoutsFailed: formatter.append("Failed to set serial timeouts"); break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }

    if (nativeError != 0 and
        (error == SerialPortError::OpenFailed or error == SerialPortError::ReadDescriptorFlagsFailed or
         error == SerialPortError::SetDescriptorFlagsFailed or error == SerialPortError::ReadSettingsFailed or
         error == SerialPortError::SetSettingsFailed or error == SerialPortError::SetTimeoutsFailed))
    {
        formatter.append(" (native error: ");
        formatter.append(static_cast<uint64_t>(nativeError));
        formatter.append(")");
    }
    return formatter.finish();
}
} // namespace detail

/// @brief Formats a canonical English SerialPort diagnostic into caller-owned storage.
/// @details This opt-in header owns the canonical strings. Applications can instead switch on SerialPortError and use
/// ResultErrorFormatter with their own translated text.
inline ResultErrorFormat formatSerialPortError(SerialPortError error, Span<char> output)
{
    return detail::formatSerialPortErrorWithNativeCode(error, 0, output);
}

/// @brief Formats a plain Result when it contains a SerialPort error.
inline ResultErrorFormat formatSerialPortError(Result result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.category() != SerialPortResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return formatSerialPortError(static_cast<SerialPortError>(result.errorValue()), output);
}

/// @brief Formats a SerialPort result, including its native error value when available.
inline ResultErrorFormat formatSerialPortError(ResultSerialPort result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.result.category() != SerialPortResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return detail::formatSerialPortErrorWithNativeCode(static_cast<SerialPortError>(result.result.errorValue()),
                                                       result.nativeError, output);
}

//! @}
} // namespace SC
