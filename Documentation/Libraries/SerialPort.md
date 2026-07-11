@page library_serial_port SerialPort

@brief 🟨 Synchronous serial-port configuration and byte I/O

[SaneCppSerialPort.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppSerialPort.h) opens and configures native serial devices behind the same owned descriptor abstraction used by the File library.

[TOC]

# Dependencies
- Dependencies: [File](@ref library_file)
- All dependencies: [File](@ref library_file)

![Dependency Graph](SerialPort.svg)


# When SerialPort fits

Use SerialPort when an application already knows the device path and needs a small, portable layer for opening a serial
connection, applying the usual line settings, and exchanging bytes. The library covers baud rate, data bits, parity, stop
bits, software or hardware flow control, blocking mode, handle inheritance, and exclusive opening.

It deliberately does not discover ports, monitor USB hotplug events, frame messages, buffer a protocol, or interpret the
bytes. Those policies are device- and application-specific. Code that needs a list of friendly device names, automatic
reconnection, modem-control lines, break signalling, or a protocol such as Modbus needs another layer around this one.

# A serial port is a configured file descriptor

SC::SerialDescriptor derives from SC::FileDescriptor. Opening it first opens the native device for reading and writing,
then applies SC::SerialOpenOptions::settings. If applying those line settings fails, the newly opened descriptor is closed
rather than returned in a partly configured state.

The descriptor owns the native handle and is move-only through the File library's handle abstraction. Destruction or
SC::FileDescriptor::close releases it; moving transfers that responsibility. The path and settings passed to
SC::SerialDescriptor::open are consumed during the call and are not retained. SerialPort does not allocate an I/O buffer:
reads fill caller-provided spans, writes borrow caller-provided bytes for the duration of the call, and failures are
reported as SC::Result.

This representative example is compiled as part of `SerialPortTest`:

\snippet Tests/Libraries/SerialPort/SerialPortTest.cpp SerialDescriptorSnippet

The resulting stream has no message boundaries. A successful read reports the slice actually filled and may produce
fewer bytes than requested; the caller must accumulate bytes according to its own framing rules. Likewise, a higher-level
protocol must decide how to handle partial messages, timeouts, checksums, retries, and reconnects.

## Settings describe intent, drivers decide what is possible

SC::SerialSettings defaults to 9600 baud, 8 data bits, no parity, one stop bit, and no flow control. `open` applies those
defaults unless the caller supplies different options. SC::SerialDescriptor::getSettings reads back the settings the
driver exposes, while SC::SerialDescriptor::setSettings changes an open port without reopening it.

The portable shape of the settings does not guarantee that every combination works on every adapter or operating system.
POSIX builds accept only baud constants available on the target platform, and hardware flow control requires native
`CRTSCTS` support. Windows passes the requested positive baud rate to the communications driver. Drivers can still reject
unsupported data-bit, parity, stop-bit, or flow-control combinations. Treat a successful `Result`—and, when it matters,
`getSettings` readback—as the capability check for the attached device.

POSIX paths must be absolute native/UTF-8 or ASCII device paths such as `/dev/ttyUSB0` or `/dev/tty.usbserial-*`. Windows
accepts `COMx` names and transparently retries them with the `\\.\` prefix, which also allows higher-numbered COM ports.
SerialPort does not choose between similarly named devices or provide stable hardware identity.

# Blocking, Async, and ownership boundaries

SC::SerialOpenOptions::blocking selects the native mode used when the handle is opened. A blocking descriptor is suitable
for straightforward synchronous code or a worker thread, but device and driver behavior still controls when reads return.
Setting `blocking` to `false` prepares the native descriptor for asynchronous use; it does not turn
SC::FileDescriptor::read or `write` into an event-driven API by itself.

On Windows, SerialPort installs fixed `COMMTIMEOUTS`: blocking reads use a 50 ms interval timeout plus a size-dependent
total timeout, while non-blocking opens use the native immediate-return pattern. These values are not caller-configurable.
On POSIX, raw-mode `VMIN` and `VTIME` are both zero, so framing and wait policy still belong above this layer.

For event-loop integration, open the serial descriptor as non-blocking and pass it to
SC::AsyncEventLoop::associateExternallyCreatedFileDescriptor from the [Async](@ref library_async) library. The descriptor
continues to own the handle, so it must remain alive while requests or event-loop registrations can refer to it. SerialPort
does not depend on Async; applications that only need synchronous access pay for no event-loop layer.

The `inheritable` option controls whether child processes may inherit the native handle. It defaults to false, which is
the safer ownership boundary for most applications. On Windows, `exclusive` disables handle sharing. POSIX currently
passes `O_EXCL` to the device open, which may not enforce serial-port exclusivity on the target OS or driver. Do not treat
the option as a portable cross-process locking protocol.

# Boundaries and tradeoffs

SerialPort is intentionally a thin specialization of [File](@ref library_file): configuration lives here, while byte
reads, writes, closing, native-handle ownership, and span-based memory behavior come from SC::FileDescriptor. This keeps
the library small and lets the same descriptor participate in Async, but it also means there is no serial-specific read
queue, caller-configurable timeout policy, drain/flush operation, or packet API.

That boundary is a good fit when the application already has a device protocol and wants direct control over storage and
lifetime. It is a poor fit when the desired abstraction is “find this USB product and maintain a reliable framed session”;
enumeration, identity, reconnection, buffering, and protocol state must currently be built above the library.

For the complete API surface and option fields, see the [SerialPort module](@ref group_serial_port).

# Status
🟨 MVP
The current scope is usable for byte-stream descriptors and core serial settings, with automated pseudo-terminal coverage
on POSIX and an optional real `com0com` loopback test on Windows. Broader adapter and edge-case driver coverage is still
needed.

# Testing a real Windows COM pair

The `windows com0com open/config/readback` section of `SerialPortTest` can exercise a configured virtual COM pair. Set both
`SC_TEST_COM0COM_PORT_A` and `SC_TEST_COM0COM_PORT_B` to `COMx` or `\\.\COMx` values. With neither variable set, the test
reports a skip; a missing partner or malformed value is a configuration failure.

For example:

`SC.bat build run SCTest Debug vs2022 -- --test "SerialPortTest" --test-section "windows com0com open/config/readback"`

# Blog

Some relevant blog posts are:

- [February 2026 Update](https://pagghiu.github.io/site/blog/2026-02-28-SaneCppLibrariesUpdate.html)

# Roadmap

🟩 Usable
- Validate against more USB-to-serial adapters and edge-case drivers.

🟦 Complete Features:
- To be decided

💡 Unplanned Features:
- Port enumeration and hotplug monitoring

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/SerialPort`.
Single File counts
`SaneCppSerialPort.h`.
Standalone counts `SaneCppSerialPortStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 55		| 599		| 654	|
| Single File | 86		| 1428		| 1514	|
| Standalone  | 1289		| 3243		| 4532	|
