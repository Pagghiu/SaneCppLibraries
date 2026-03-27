// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Foundation/Compiler.h"
#ifndef SC_EXPORT_LIBRARY_SERIAL_PORT
#define SC_EXPORT_LIBRARY_SERIAL_PORT 0
#endif
#define SC_SERIAL_PORT_EXPORT SC_COMPILER_LIBRARY_EXPORT(SC_EXPORT_LIBRARY_SERIAL_PORT)

#include "../File/File.h"

namespace SC
{

//! @defgroup group_serial_port SerialPort
//! @copybrief library_serial_port (see @ref library_serial_port for more details)

//! @addtogroup group_serial_port
//! @{

/// @brief Serial port settings.
struct SC_SERIAL_PORT_EXPORT SerialSettings
{
    enum class DataBits : uint8_t
    {
        Bits5 = 5,
        Bits6 = 6,
        Bits7 = 7,
        Bits8 = 8,
    };

    enum class Parity : uint8_t
    {
        None = 0,
        Odd,
        Even,
    };

    enum class StopBits : uint8_t
    {
        One = 1,
        Two = 2,
    };

    enum class FlowControl : uint8_t
    {
        None = 0,
        Software,
        Hardware,
    };

    uint32_t    baudRate    = 9600;
    DataBits    dataBits    = DataBits::Bits8;
    Parity      parity      = Parity::None;
    StopBits    stopBits    = StopBits::One;
    FlowControl flowControl = FlowControl::None;
};

/// @brief Open options for a serial descriptor.
struct SC_SERIAL_PORT_EXPORT SerialOpenOptions
{
    bool blocking    = true;
    bool inheritable = false;
    bool exclusive   = false;

    SerialSettings settings;
};

/// @brief Native serial port descriptor with configuration support.
/// \snippet Tests/Libraries/SerialPort/SerialPortTest.cpp SerialDescriptorSnippet
struct SC_SERIAL_PORT_EXPORT SerialDescriptor : public FileDescriptor
{
    /// @brief Opens a serial port and applies the requested settings.
    /// @param path Serial device path (`/dev/tty*` on Posix, `COM*` on Windows)
    /// @param options Open and configuration options.
    /// @return Valid Result if open/configuration succeeded.
    Result open(StringSpan path, const SerialOpenOptions& options = SerialOpenOptions());

    /// @brief Applies settings to an already opened serial descriptor.
    /// @param settings Settings to apply.
    /// @return Valid Result if settings have been applied.
    Result setSettings(const SerialSettings& settings);

    /// @brief Reads current settings from an opened serial descriptor.
    /// @param settings Output settings.
    /// @return Valid Result if settings have been read.
    Result getSettings(SerialSettings& settings) const;
};

//! @}
} // namespace SC
