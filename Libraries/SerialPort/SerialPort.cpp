// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../SerialPort/SerialPort.h"
#include "../Common/IGrowableBufferStringPath.h"

namespace SC
{
namespace detail
{
static ResultSerialPort validateSerialSettings(const SerialSettings& settings)
{
    if (settings.baudRate == 0)
        return ResultSerialPort(SerialPortError::InvalidBaudRate);
    switch (settings.dataBits)
    {
    case SerialSettings::DataBits::Bits5:
    case SerialSettings::DataBits::Bits6:
    case SerialSettings::DataBits::Bits7:
    case SerialSettings::DataBits::Bits8: break;
    default: return ResultSerialPort(SerialPortError::InvalidDataBits);
    }
    switch (settings.parity)
    {
    case SerialSettings::Parity::None:
    case SerialSettings::Parity::Odd:
    case SerialSettings::Parity::Even: break;
    default: return ResultSerialPort(SerialPortError::InvalidParity);
    }
    switch (settings.stopBits)
    {
    case SerialSettings::StopBits::One:
    case SerialSettings::StopBits::Two: break;
    default: return ResultSerialPort(SerialPortError::InvalidStopBits);
    }
    switch (settings.flowControl)
    {
    case SerialSettings::FlowControl::None:
    case SerialSettings::FlowControl::Software:
    case SerialSettings::FlowControl::Hardware: break;
    default: return ResultSerialPort(SerialPortError::InvalidFlowControl);
    }
    return ResultSerialPort(true);
}

ResultSerialPort openSerialHandle(StringSpan path, const SerialOpenOptions& options, FileDescriptor::Handle& outHandle);
ResultSerialPort setSerialSettings(FileDescriptor::Handle handle, const SerialSettings& settings);
ResultSerialPort getSerialSettings(FileDescriptor::Handle handle, SerialSettings& settings);

} // namespace detail
} // namespace SC

#if SC_PLATFORM_WINDOWS
#include "../SerialPort/Internal/SerialPortWindows.inl"
#else
#include "../SerialPort/Internal/SerialPortPosix.inl"
#endif

SC::ResultSerialPort SC::SerialDescriptor::open(StringSpan path, const SerialOpenOptions& options)
{
    SC_TRY(detail::validateSerialSettings(options.settings));
    FileDescriptor::Handle nativeHandle = FileDescriptor::Invalid;
    SC_TRY(detail::openSerialHandle(path, options, nativeHandle));
    SC_TRY(close());
    SC_TRY(assign(nativeHandle));
    const ResultSerialPort setRes = setSettings(options.settings);
    if (not setRes)
    {
        (void)close();
    }
    return setRes;
}

SC::ResultSerialPort SC::SerialDescriptor::setSettings(const SerialSettings& settings)
{
    SC_TRY(detail::validateSerialSettings(settings));
    FileDescriptor::Handle nativeHandle = FileDescriptor::Invalid;
    if (not get(nativeHandle, Result::Error("SerialDescriptor::setSettings - Invalid handle")))
        return ResultSerialPort(SerialPortError::InvalidHandle);
    return detail::setSerialSettings(nativeHandle, settings);
}

SC::ResultSerialPort SC::SerialDescriptor::getSettings(SerialSettings& settings) const
{
    FileDescriptor::Handle nativeHandle = FileDescriptor::Invalid;
    if (not get(nativeHandle, Result::Error("SerialDescriptor::getSettings - Invalid handle")))
        return ResultSerialPort(SerialPortError::InvalidHandle);
    return detail::getSerialSettings(nativeHandle, settings);
}
