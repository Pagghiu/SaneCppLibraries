# Sane Serial Port

## Overview

Use this guide for serial descriptor configuration and byte-stream transport. Keep it for port open settings, readback, and platform-specific caveats.

## Start Here

- Inspect `Documentation/Libraries/SerialPort.md`.
- Read `Tests/Libraries/SerialPort/SerialPortTest.cpp`.
- Check `Libraries/SerialPort/SerialPort.h`.

## Use It For

- Configure and open serial ports.
- Describe baud rate and transport settings through `SerialSettings`.
- Build opening options for common device workflows.
- Validate behavior with the built-in test coverage or platform-specific adapters.

## Prefer These Companions

- Use `file` if the serial workflow needs descriptor-level piping or inheritance.
- Use `process` only if the serial work is part of a broader external tool workflow.

## Pitfalls

- Do not treat it as a generic networking library.
- Do not assume all platform adapters behave identically.
- Do not ignore the optional Windows com0com test section when validating edge cases.

## References

- [Serial port reference](references/serial-port-configuration.md)
