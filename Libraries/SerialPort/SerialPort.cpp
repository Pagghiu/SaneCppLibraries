// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../SerialPort/SerialPort.h"
#include "../Foundation/StringPath.h"

namespace SC
{
namespace detail
{
static Result validateSerialSettings(const SerialSettings& settings)
{
    SC_TRY_MSG(settings.baudRate > 0, "SerialDescriptor::open - baudRate must be greater than zero");
    switch (settings.dataBits)
    {
    case SerialSettings::DataBits::Bits5:
    case SerialSettings::DataBits::Bits6:
    case SerialSettings::DataBits::Bits7:
    case SerialSettings::DataBits::Bits8: break;
    default: return Result::Error("SerialDescriptor::open - invalid dataBits");
    }
    switch (settings.parity)
    {
    case SerialSettings::Parity::None:
    case SerialSettings::Parity::Odd:
    case SerialSettings::Parity::Even: break;
    default: return Result::Error("SerialDescriptor::open - invalid parity");
    }
    switch (settings.stopBits)
    {
    case SerialSettings::StopBits::One:
    case SerialSettings::StopBits::Two: break;
    default: return Result::Error("SerialDescriptor::open - invalid stopBits");
    }
    switch (settings.flowControl)
    {
    case SerialSettings::FlowControl::None:
    case SerialSettings::FlowControl::Software:
    case SerialSettings::FlowControl::Hardware: break;
    default: return Result::Error("SerialDescriptor::open - invalid flowControl");
    }
    return Result(true);
}

Result openSerialHandle(StringSpan path, const SerialOpenOptions& options, FileDescriptor::Handle& outHandle);
Result setSerialSettings(FileDescriptor::Handle handle, const SerialSettings& settings);
Result getSerialSettings(FileDescriptor::Handle handle, SerialSettings& settings);

} // namespace detail
} // namespace SC

#if SC_PLATFORM_WINDOWS
#include "../SerialPort/Internal/SerialPortWindows.inl"
#else
#include "../SerialPort/Internal/SerialPortPosix.inl"
#endif

SC::Result SC::SerialDescriptor::open(StringSpan path, const SerialOpenOptions& options)
{
    SC_TRY(detail::validateSerialSettings(options.settings));
    FileDescriptor::Handle nativeHandle = FileDescriptor::Invalid;
    SC_TRY(detail::openSerialHandle(path, options, nativeHandle));
    SC_TRY(close());
    SC_TRY(assign(nativeHandle));
    const Result setRes = setSettings(options.settings);
    if (not setRes)
    {
        (void)close();
    }
    return setRes;
}

SC::Result SC::SerialDescriptor::setSettings(const SerialSettings& settings)
{
    SC_TRY(detail::validateSerialSettings(settings));
    FileDescriptor::Handle nativeHandle = FileDescriptor::Invalid;
    SC_TRY(get(nativeHandle, Result::Error("SerialDescriptor::setSettings - Invalid handle")));
    return detail::setSerialSettings(nativeHandle, settings);
}

SC::Result SC::SerialDescriptor::getSettings(SerialSettings& settings) const
{
    FileDescriptor::Handle nativeHandle = FileDescriptor::Invalid;
    SC_TRY(get(nativeHandle, Result::Error("SerialDescriptor::getSettings - Invalid handle")));
    return detail::getSerialSettings(nativeHandle, settings);
}
