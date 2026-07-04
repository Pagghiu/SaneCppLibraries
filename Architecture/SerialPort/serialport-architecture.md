# SerialPort Architecture

## Purpose

SerialPort is the synchronous serial device open/configuration library. It must expose portable serial settings and a descriptor-shaped byte stream while reusing File for ordinary descriptor lifetime and read/write behavior.

## Architectural Shape

The public interface is `SerialDescriptor`, `SerialOpenOptions`, and `SerialSettings`. `SerialDescriptor` derives from `FileDescriptor`, so open, close, assign, read, and write behavior remains descriptor-based. SerialPort owns validation, native serial open, and settings get/set behavior.

Platform-specific termios and Windows COM details belong in internal implementation files.

## Boundaries

SerialPort owns opening serial devices and applying or reading portable serial settings. It intentionally does not own port enumeration, hotplug monitoring, async I/O, named-pipe behavior, terminal emulation, or adapter-specific feature discovery.

The File dependency is part of the public shape, not an accidental dependency.

## Similarities With Other Libraries

SerialPort follows the same native-platform wrapper style as Process and Socket: small public Sane types, explicit `Result` failures, and private platform implementations. It shares Process's intentional File dependency for descriptor operations.

## Differences From Other Libraries

Unlike Socket, SerialPort is descriptor-based through File. Unlike Async, SerialPort does not own event-loop registration or overlapped I/O completion policy. Unlike File, SerialPort has device-specific configuration semantics and must reject non-serial handles for serial settings operations.

## Inspirations

The evidenced inspirations are native POSIX termios and Windows COM APIs, plus the project's existing File descriptor abstraction. Tests use POSIX PTYs and optional Windows com0com coverage to validate representative serial behavior.

## Anti-Inspirations

Explicit anti-inspirations include port enumeration and hotplug monitoring, which the documentation marks as unplanned. Inferred anti-inspirations include serial frameworks that combine discovery, monitoring, async transport, and protocol parsing in one library.

## Architectural Choices

- Keep serial descriptors as File descriptors with extra configuration.
- Keep v1 settings limited to portable core serial options.
- Validate settings before applying native configuration.
- Keep real hardware-style Windows coverage opt-in.
- Keep async serial use as composition with File/Async, not a SerialPort dependency.

## Explicitly Excluded Targets

- Port enumeration and hotplug monitoring.
- Serial protocol parsing.
- Terminal emulator behavior.
- Async runtime ownership.
- Adapter-specific control-line feature expansion without a new decision.

## Sources

- [SerialPort documentation](../../Documentation/Libraries/SerialPort.md)
- [SerialPort public API](../../Libraries/SerialPort/SerialPort.h)
- [SerialPort implementation](../../Libraries/SerialPort/SerialPort.cpp)
- [SerialPort POSIX implementation](../../Libraries/SerialPort/Internal/SerialPortPosix.inl)
- [SerialPort Windows implementation](../../Libraries/SerialPort/Internal/SerialPortWindows.inl)
- [SerialPort tests](../../Tests/Libraries/SerialPort/SerialPortTest.cpp)
- [SERIALPORT-0001 - Build SerialPort as a FileDescriptor-based byte stream](serialport-0001-build-serialport-as-a-filedescriptor-based-byte-stream.md)
- [SERIALPORT-0002 - Limit V1 serial settings to portable core configuration](serialport-0002-limit-v1-serial-settings-to-portable-core-configuration.md)
- [SC-0015 - Prefer maturing existing libraries over expanding scope](../Global/sc-0015-prefer-maturing-existing-libraries-over-expanding-scope.md)

## Decision Log

- [SERIALPORT-0001 - Build SerialPort as a FileDescriptor-based byte stream](serialport-0001-build-serialport-as-a-filedescriptor-based-byte-stream.md)
- [SERIALPORT-0002 - Limit V1 serial settings to portable core configuration](serialport-0002-limit-v1-serial-settings-to-portable-core-configuration.md)
