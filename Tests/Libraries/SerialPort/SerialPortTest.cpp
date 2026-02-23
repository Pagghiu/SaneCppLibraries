// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/SerialPort/SerialPort.h"
#include "Libraries/File/File.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Foundation/StringPath.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Process/Process.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Testing/Testing.h"

#include <string.h>

#if !SC_PLATFORM_WINDOWS
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#endif

namespace SC
{
struct SerialPortTest;
}

namespace
{
static SC::Result readExactDescriptor(SC::FileDescriptor& descriptor, SC::Span<char> destination)
{
    size_t totalRead = 0;
    while (totalRead < destination.sizeInBytes())
    {
        SC::Span<char> readData;
        SC_TRY(descriptor.read({destination.data() + totalRead, destination.sizeInBytes() - totalRead}, readData));
        SC_TRY_MSG(not readData.empty(), "readExactDescriptor - Unexpected EOF");
        totalRead += readData.sizeInBytes();
    }
    return SC::Result(true);
}

#if SC_PLATFORM_WINDOWS
struct WindowsCom0ComPorts
{
    enum class Status
    {
        NotConfigured,
        ConfiguredAndReady,
        ConfiguredButInvalid
    };

    Status              status  = Status::NotConfigured;
    const char*         message = nullptr;
    SC::SmallString<32> portA;
    SC::SmallString<32> portB;

    WindowsCom0ComPorts() : portA(SC::StringEncoding::Ascii), portB(SC::StringEncoding::Ascii) {}
};

static bool isComPortPath(SC::StringSpan path)
{
    SC::SmallString<32> asciiPath(SC::StringEncoding::Ascii);
    if (not asciiPath.assign(path))
    {
        return false;
    }
    SC::StringSpan asciiView = asciiPath.view();
    const char*    parsed    = asciiView.bytesWithoutTerminator();
    size_t         length    = asciiView.sizeInBytes();

    if (length == 0)
    {
        return false;
    }

    if (length >= 4 and parsed[0] == '\\' and parsed[1] == '\\' and parsed[2] == '.' and parsed[3] == '\\')
    {
        parsed += 4;
        length -= 4;
    }

    if (length < 4 or not((parsed[0] == 'C' or parsed[0] == 'c') and (parsed[1] == 'O' or parsed[1] == 'o') and
                          (parsed[2] == 'M' or parsed[2] == 'm')))
    {
        return false;
    }
    parsed += 3;
    length -= 3;
    if (length == 0)
    {
        return false;
    }

    for (size_t idx = 0; idx < length; ++idx)
    {
        const char c = parsed[idx];
        if (c < '0' or c > '9')
        {
            return false;
        }
    }
    return true;
}

static WindowsCom0ComPorts resolveWindowsCom0ComPorts()
{
    WindowsCom0ComPorts    ports;
    SC::ProcessEnvironment environment;
    SC::StringSpan         portAEnv;
    SC::StringSpan         portBEnv;
    const bool             hasPortA = environment.get("SC_TEST_COM0COM_PORT_A", portAEnv) and not portAEnv.isEmpty();
    const bool             hasPortB = environment.get("SC_TEST_COM0COM_PORT_B", portBEnv) and not portBEnv.isEmpty();

    if (not hasPortA and not hasPortB)
    {
        ports.status = WindowsCom0ComPorts::Status::NotConfigured;
        ports.message =
            "SerialPortTest - Skipping real COM loopback: set SC_TEST_COM0COM_PORT_A and SC_TEST_COM0COM_PORT_B";
        return ports;
    }

    if (hasPortA != hasPortB)
    {
        ports.status  = WindowsCom0ComPorts::Status::ConfiguredButInvalid;
        ports.message = "SerialPortTest - Both SC_TEST_COM0COM_PORT_A and SC_TEST_COM0COM_PORT_B must be set";
        return ports;
    }

    if (not isComPortPath(portAEnv) or not isComPortPath(portBEnv))
    {
        ports.status  = WindowsCom0ComPorts::Status::ConfiguredButInvalid;
        ports.message = "SerialPortTest - COM ports must be COMx or \\\\.\\COMx";
        return ports;
    }

    if (not ports.portA.assign(portAEnv) or not ports.portB.assign(portBEnv))
    {
        ports.status  = WindowsCom0ComPorts::Status::ConfiguredButInvalid;
        ports.message = "SerialPortTest - Invalid COM port path length";
        return ports;
    }

    ports.status  = WindowsCom0ComPorts::Status::ConfiguredAndReady;
    ports.message = "SerialPortTest - Running real COM loopback";
    return ports;
}
#endif

#if !SC_PLATFORM_WINDOWS
struct PosixPTYPair
{
    SC::FileDescriptor master;
    SC::StringPath     slavePath;

    SC::Result create()
    {
        int openFlags = O_RDWR | O_NOCTTY;
#ifdef O_CLOEXEC
        openFlags |= O_CLOEXEC;
#endif
        int masterFd;
        do
        {
            masterFd = ::posix_openpt(openFlags);
        } while (masterFd == -1 and errno == EINTR);
        SC_TRY_MSG(masterFd != -1, "PosixPTYPair - posix_openpt failed");

        if (::grantpt(masterFd) != 0 or ::unlockpt(masterFd) != 0)
        {
            (void)::close(masterFd);
            return SC::Result::Error("PosixPTYPair - grantpt/unlockpt failed");
        }
        const char* slaveName = ::ptsname(masterFd);
        if (slaveName == nullptr)
        {
            (void)::close(masterFd);
            return SC::Result::Error("PosixPTYPair - ptsname failed");
        }
        SC_TRY(master.assign(masterFd));
        SC_TRY(slavePath.assign(SC::StringSpan::fromNullTerminated(slaveName, SC::StringEncoding::Native)));
        return SC::Result(true);
    }
};
#endif
} // namespace

struct SC::SerialPortTest : public SC::TestCase
{
    SerialPortTest(SC::TestReport& report) : TestCase(report, "SerialPortTest")
    {
        if (test_section("invalid path and settings"))
        {
            invalidPathAndSettings();
        }
        if (test_section("non-serial handle contract"))
        {
            nonSerialHandleContract();
        }
#if !SC_PLATFORM_WINDOWS
        if (test_section("posix pty open/config/readback"))
        {
            posixPTYOpenConfigureReadback();
        }
#else
        if (test_section("windows com0com open/config/readback"))
        {
            windowsCom0ComOpenConfigureReadback();
        }
#endif
    }

    void invalidPathAndSettings();
    void nonSerialHandleContract();
#if !SC_PLATFORM_WINDOWS
    void posixPTYOpenConfigureReadback();
#else
    void windowsCom0ComOpenConfigureReadback();
#endif
};

void SC::SerialPortTest::invalidPathAndSettings()
{
    SerialDescriptor  serial;
    SerialOpenOptions options;
#if SC_PLATFORM_WINDOWS
    SC_TEST_EXPECT(not serial.open("COM9999", options));
#else
    SC_TEST_EXPECT(not serial.open("ttyS0", options));
    SC_TEST_EXPECT(not serial.open("/dev/this-device-should-not-exist-sc", options));
#endif

    SerialSettings invalidSettings;
    invalidSettings.baudRate = 0;
    SC_TEST_EXPECT(not serial.setSettings(invalidSettings));

    invalidSettings             = SerialSettings();
    invalidSettings.dataBits    = static_cast<SerialSettings::DataBits>(4);
    invalidSettings.parity      = SerialSettings::Parity::None;
    invalidSettings.stopBits    = SerialSettings::StopBits::One;
    invalidSettings.baudRate    = 9600;
    invalidSettings.flowControl = SerialSettings::FlowControl::None;
    SC_TEST_EXPECT(not serial.setSettings(invalidSettings));

    invalidSettings        = SerialSettings();
    invalidSettings.parity = static_cast<SerialSettings::Parity>(5);
    SC_TEST_EXPECT(not serial.setSettings(invalidSettings));

    invalidSettings          = SerialSettings();
    invalidSettings.stopBits = static_cast<SerialSettings::StopBits>(0);
    SC_TEST_EXPECT(not serial.setSettings(invalidSettings));

    invalidSettings             = SerialSettings();
    invalidSettings.flowControl = static_cast<SerialSettings::FlowControl>(9);
    SC_TEST_EXPECT(not serial.setSettings(invalidSettings));
}

void SC::SerialPortTest::nonSerialHandleContract()
{
    SmallStringNative<255> dirPath  = StringEncoding::Native;
    SmallStringNative<255> filePath = StringEncoding::Native;

    const StringView dirName  = "SerialPortTest";
    const StringView fileName = "non_serial_handle.bin";
    SC_TEST_EXPECT(Path::join(dirPath, {report.applicationRootDirectory.view(), dirName}));
    SC_TEST_EXPECT(Path::join(filePath, {dirPath.view(), fileName}));

    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
    SC_TEST_EXPECT(fs.makeDirectoryIfNotExists(dirName));

    FileDescriptor fileDescriptor;
    SC_TEST_EXPECT(fileDescriptor.open(filePath.view(), FileOpen::WriteRead));

    FileDescriptor::Handle nativeHandle = FileDescriptor::Invalid;
    SC_TEST_EXPECT(fileDescriptor.get(nativeHandle, Result::Error("nativeHandle")));
    fileDescriptor.detach();

    SerialDescriptor serial;
    SC_TEST_EXPECT(serial.assign(nativeHandle));
    SC_TEST_EXPECT(not serial.setSettings(SerialSettings()));

    SerialSettings currentSettings;
    SC_TEST_EXPECT(not serial.getSettings(currentSettings));
    SC_TEST_EXPECT(serial.close());

    SC_TEST_EXPECT(fs.changeDirectory(dirPath.view()));
    SC_TEST_EXPECT(fs.removeFile(fileName));
    SC_TEST_EXPECT(fs.changeDirectory(report.applicationRootDirectory.view()));
    SC_TEST_EXPECT(fs.removeEmptyDirectory(dirName));
}

#if !SC_PLATFORM_WINDOWS
void SC::SerialPortTest::posixPTYOpenConfigureReadback()
{
    PosixPTYPair pair;
    SC_TEST_EXPECT(pair.create());

    SerialOpenOptions options;
    options.blocking             = true;
    options.settings.baudRate    = 9600;
    options.settings.dataBits    = SerialSettings::DataBits::Bits8;
    options.settings.parity      = SerialSettings::Parity::None;
    options.settings.stopBits    = SerialSettings::StopBits::One;
    options.settings.flowControl = SerialSettings::FlowControl::None;

    SerialDescriptor serial;
    SC_TEST_EXPECT(serial.open(pair.slavePath.view(), options));

    SerialSettings readback;
    SC_TEST_EXPECT(serial.getSettings(readback));
    SC_TEST_EXPECT(readback.baudRate == options.settings.baudRate);
    SC_TEST_EXPECT(readback.dataBits == options.settings.dataBits);
    SC_TEST_EXPECT(readback.parity == options.settings.parity);
    SC_TEST_EXPECT(readback.stopBits == options.settings.stopBits);
    SC_TEST_EXPECT(readback.flowControl == options.settings.flowControl);

    SerialSettings updated = options.settings;
    updated.baudRate       = 19200;
    SC_TEST_EXPECT(serial.setSettings(updated));
    SC_TEST_EXPECT(serial.getSettings(readback));
    SC_TEST_EXPECT(readback.baudRate == updated.baudRate);
    SC_TEST_EXPECT(readback.dataBits == updated.dataBits);
    SC_TEST_EXPECT(readback.parity == updated.parity);
    SC_TEST_EXPECT(readback.stopBits == updated.stopBits);

    const char fromPeer[] = {'P', 'I', 'N', 'G'};
    SC_TEST_EXPECT(pair.master.write({fromPeer, sizeof(fromPeer)}));
    char receivedBySerial[4] = {0};
    SC_TEST_EXPECT(readExactDescriptor(serial, {receivedBySerial, sizeof(receivedBySerial)}));
    SC_TEST_EXPECT(::memcmp(receivedBySerial, fromPeer, sizeof(fromPeer)) == 0);

    const char fromSerial[] = {'P', 'O', 'N', 'G'};
    SC_TEST_EXPECT(serial.write({fromSerial, sizeof(fromSerial)}));
    char receivedByPeer[4] = {0};
    SC_TEST_EXPECT(readExactDescriptor(pair.master, {receivedByPeer, sizeof(receivedByPeer)}));
    SC_TEST_EXPECT(::memcmp(receivedByPeer, fromSerial, sizeof(fromSerial)) == 0);
}
#else
void SC::SerialPortTest::windowsCom0ComOpenConfigureReadback()
{
    const WindowsCom0ComPorts ports   = resolveWindowsCom0ComPorts();
    const StringSpan          message = StringSpan::fromNullTerminated(ports.message, StringEncoding::Ascii);
    switch (ports.status)
    {
    case WindowsCom0ComPorts::Status::NotConfigured: report.console.printLine(message); return;
    case WindowsCom0ComPorts::Status::ConfiguredButInvalid:
        SC_TEST_EXPECT(recordExpectation("windows com0com configuration", false, message));
        return;
    case WindowsCom0ComPorts::Status::ConfiguredAndReady: break;
    }
    report.console.printLine(message);

    SerialOpenOptions options;
    options.blocking             = true;
    options.settings.baudRate    = 115200;
    options.settings.dataBits    = SerialSettings::DataBits::Bits8;
    options.settings.parity      = SerialSettings::Parity::None;
    options.settings.stopBits    = SerialSettings::StopBits::One;
    options.settings.flowControl = SerialSettings::FlowControl::None;

    SerialDescriptor serialA;
    SerialDescriptor serialB;
    SC_TEST_EXPECT(serialA.open(ports.portA.view(), options));
    SC_TEST_EXPECT(serialB.open(ports.portB.view(), options));

    SerialSettings readback;
    SC_TEST_EXPECT(serialA.getSettings(readback));
    SC_TEST_EXPECT(readback.baudRate == options.settings.baudRate);
    SC_TEST_EXPECT(readback.dataBits == options.settings.dataBits);
    SC_TEST_EXPECT(readback.parity == options.settings.parity);
    SC_TEST_EXPECT(readback.stopBits == options.settings.stopBits);
    SC_TEST_EXPECT(readback.flowControl == options.settings.flowControl);

    SerialSettings updated = options.settings;
    updated.baudRate       = 57600;
    SC_TEST_EXPECT(serialA.setSettings(updated));
    SC_TEST_EXPECT(serialA.getSettings(readback));
    SC_TEST_EXPECT(readback.baudRate == updated.baudRate);

    const char fromA[] = {'P', 'I', 'N', 'G'};
    SC_TEST_EXPECT(serialA.write({fromA, sizeof(fromA)}));
    char receivedByB[4] = {};
    SC_TEST_EXPECT(readExactDescriptor(serialB, {receivedByB, sizeof(receivedByB)}));
    SC_TEST_EXPECT(::memcmp(receivedByB, fromA, sizeof(fromA)) == 0);

    const char fromB[] = {'P', 'O', 'N', 'G'};
    SC_TEST_EXPECT(serialB.write({fromB, sizeof(fromB)}));
    char receivedByA[4] = {};
    SC_TEST_EXPECT(readExactDescriptor(serialA, {receivedByA, sizeof(receivedByA)}));
    SC_TEST_EXPECT(::memcmp(receivedByA, fromB, sizeof(fromB)) == 0);
}
#endif

namespace SC
{
void runSerialPortTest(SC::TestReport& report) { SerialPortTest test(report); }

// clang-format off
Result snippetForSerialDescriptor()
{
//! [SerialDescriptorSnippet]
    SerialDescriptor serial;
    SerialOpenOptions options;
    options.settings.baudRate    = 115200;
    options.settings.dataBits    = SerialSettings::DataBits::Bits8;
    options.settings.parity      = SerialSettings::Parity::None;
    options.settings.stopBits    = SerialSettings::StopBits::One;
    options.settings.flowControl = SerialSettings::FlowControl::None;
#if SC_PLATFORM_WINDOWS
    StringView serialPath = "COM3";
#else
    StringView serialPath = "/dev/ttyUSB0";
#endif
    SC_TRY(serial.open(serialPath, options));

    SerialSettings current;
    SC_TRY(serial.getSettings(current));
    current.baudRate = 57600;
    SC_TRY(serial.setSettings(current));

    const char tx[] = {'P', 'I', 'N', 'G'};
    SC_TRY(serial.write({tx, sizeof(tx)}));

    char       rxBuffer[64] = {};
    Span<char> rxData;
    SC_TRY(serial.read({rxBuffer, sizeof(rxBuffer)}, rxData));
    // rxData is a slice of rxBuffer with the actual received bytes
//! [SerialDescriptorSnippet]
    return Result(true);
}
// clang-format on
} // namespace SC
