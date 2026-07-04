# SERIALPORT-0001 - Build SerialPort as a FileDescriptor-Based Byte Stream

Status: Accepted
Date: 2026-07-04

## Context

Serial ports are byte-stream OS handles with extra open and configuration rules. The project already has File descriptors, read/write behavior, and async integrations over descriptors. Reimplementing that surface in SerialPort would duplicate behavior and increase dependency pressure.

## Decision

`SerialDescriptor` derives from `FileDescriptor`. SerialPort owns serial-specific open and settings APIs, while ordinary byte reads, writes, close, assignment, and async composition reuse the File descriptor abstraction. The File dependency is intentional and documented.

## Consequences

SerialPort stays small and focused on device opening and configuration. Existing File and Async code can work with serial descriptors without a separate serial stream abstraction. SerialPort users must understand that a serial descriptor is also a file descriptor and that non-serial handles fail serial settings operations.

## Confirmation

A change preserves this decision when SerialPort keeps File as its only library dependency, serial read/write behavior continues to come from `FileDescriptor`, non-serial descriptor settings fail cleanly, and async serial behavior is built by composing File/Async layers rather than adding an Async dependency to SerialPort.

## Related

- [SerialPort documentation](../../Documentation/Libraries/SerialPort.md)
- [SerialPort public API](../../Libraries/SerialPort/SerialPort.h)
- [SerialPort implementation](../../Libraries/SerialPort/SerialPort.cpp)
- [SerialPort tests](../../Tests/Libraries/SerialPort/SerialPortTest.cpp)
- [SC-0003 - Keep libraries independently consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
