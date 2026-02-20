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

# Details

@copydetails group_serial_port

# Example
@snippet Tests/Libraries/SerialPort/SerialPortTest.cpp SerialDescriptorSnippet

# Roadmap

🟩 Usable
- Validate against more USB-to-serial adapters and edge-case drivers.

🟦 Complete Features:
- To be decided

💡 Unplanned Features:
- Port enumeration and hotplug monitoring
