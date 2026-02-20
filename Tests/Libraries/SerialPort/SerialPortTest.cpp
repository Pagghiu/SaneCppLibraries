// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/SerialPort/SerialPort.h"
#include "Libraries/File/File.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Foundation/StringPath.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Testing/Testing.h"

#include <string.h>

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
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
#if !SC_PLATFORM_WINDOWS
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
#endif
    }

    void invalidPathAndSettings();
    void nonSerialHandleContract();
#if !SC_PLATFORM_WINDOWS
    void posixPTYOpenConfigureReadback();
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
