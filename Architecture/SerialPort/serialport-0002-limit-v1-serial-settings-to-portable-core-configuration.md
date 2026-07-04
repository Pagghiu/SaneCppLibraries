# SERIALPORT-0002 - Limit V1 Serial Settings to Portable Core Configuration

Status: Accepted
Date: 2026-07-04

## Context

Serial hardware and drivers vary widely across operating systems and adapters. Beyond basic byte-stream settings, features such as device enumeration, hotplug monitoring, modem control lines, custom baud handling, and adapter-specific behavior can expand the library quickly.

## Decision

SerialPort v1 exposes only the portable core configuration: baud rate, data bits, parity, stop bits, flow control, blocking mode, inheritability, and exclusive open. Port enumeration and hotplug monitoring remain out of scope unless a future decision expands the library.

## Consequences

The library can cover common open/configure/read/write workflows without becoming a hardware discovery framework. Some platform-specific serial features are deliberately unavailable. Tests should focus on validation, settings round trips, and representative loopback behavior rather than trying to model every device capability.

## Confirmation

A change preserves this decision when new public settings are portable or explicitly justified, enumeration and hotplug APIs are not added without a new ADR, unsupported settings fail through `Result`, and documentation continues to describe the v1 scope honestly.

## Related

- [SerialPort documentation](../../Documentation/Libraries/SerialPort.md)
- [SerialPort public API](../../Libraries/SerialPort/SerialPort.h)
- [SerialPort POSIX implementation](../../Libraries/SerialPort/Internal/SerialPortPosix.inl)
- [SerialPort Windows implementation](../../Libraries/SerialPort/Internal/SerialPortWindows.inl)
- [SC-0015 - Prefer maturing existing libraries over expanding scope](../Global/sc-0015-prefer-maturing-existing-libraries-over-expanding-scope.md)
- [SC-0017 - Publish Draft and MVP libraries deliberately](../Global/sc-0017-publish-draft-and-mvp-libraries-deliberately.md)
