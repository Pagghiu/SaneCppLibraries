# Serial Port Configuration

## Start Here

- `Documentation/Libraries/SerialPort.md`
- `Libraries/SerialPort/SerialPort.h`
- `Tests/Libraries/SerialPort/SerialPortTest.cpp`

## Use `sane-serial-port` For

- Opening a serial port.
- Setting baud and transport options.
- Describing a serial connection through `SerialSettings` and `SerialOpenOptions`.
- Reading back or validating serial configuration in tests and examples.

## Key Types

- `SC::SerialDescriptor`
- `SC::SerialSettings`
- `SC::SerialOpenOptions`

## Common Patterns

- Keep the serial configuration separate from whatever higher-level protocol sits on top of it.
- Use the library as a byte-stream transport rather than a protocol parser.
- Validate platform-specific behavior with the existing test coverage.

## Hand Off To Other Skills

- Use `sane-file` when you need descriptor-level piping or inheritance around the serial connection.
- Use `sane-process` if the serial workflow is only one piece of a broader device-tool command flow.

## Pitfalls

- Do not call it a networking library.
- Do not assume every USB adapter behaves the same.
- Do not forget the optional Windows com0com test scenario when checking regression coverage.
