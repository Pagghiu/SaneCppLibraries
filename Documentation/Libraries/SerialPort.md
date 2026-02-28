@page library_serial_port SerialPort

@brief 🟨 Synchronous serial port descriptor and configuration

[TOC]

[SaneCppSerialPort.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppSerialPort.h) is a library implementing cross-platform serial port open/configuration APIs.

# Dependencies
- Dependencies: [File](@ref library_file)
- All dependencies: [File](@ref library_file), [Foundation](@ref library_foundation)

![Dependency Graph](SerialPort.svg)


# Features
| Class                         | Description
|:------------------------------|:---------------------------------------------|
| SC::SerialDescriptor          | @copybrief SC::SerialDescriptor              |
| SC::SerialSettings            | @copybrief SC::SerialSettings                |
| SC::SerialOpenOptions         | @copybrief SC::SerialOpenOptions             |

# Status
🟨 MVP  
The v1 scope focuses on byte-stream descriptors and core serial settings.

# Blog

Some relevant blog posts are:

- [February 2026 Update](https://pagghiu.github.io/site/blog/2026-02-28-SaneCppLibrariesUpdate.html)

# Details

@copydetails group_serial_port

# Example
@snippet Tests/Libraries/SerialPort/SerialPortTest.cpp SerialDescriptorSnippet

# Optional Windows Real COM Test (com0com)
`SerialPortTest` includes an optional section named `windows com0com open/config/readback`.

- Set `SC_TEST_COM0COM_PORT_A` and `SC_TEST_COM0COM_PORT_B` to enable it.
- Accepted values are both `COMx` and `\\.\COMx`.
- If both variables are unset, the section prints a skip message and succeeds.
- If only one variable is set (or values are malformed), the section fails with a configuration error.

Example command:
`SC.bat build run SCTest Debug vs2022 -- --test "SerialPortTest" --test-section "windows com0com open/config/readback"`

# Roadmap

🟩 Usable
- Validate against more USB-to-serial adapters and edge-case drivers.

🟦 Complete Features:
- To be decided

💡 Unplanned Features:
- Port enumeration and hotplug monitoring

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 48			| 40		| 88	|
| Sources   | 591			| 76		| 667	|
| Sum       | 639			| 116		| 755	|
