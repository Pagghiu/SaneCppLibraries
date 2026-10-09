// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/SerialPort/SerialPort.h"
#include "Libraries/Common/IGrowableBufferStringPath.h"
#include "Libraries/File/File.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Process/Process.h"
#include "Libraries/SerialPort/SerialPortErrorFormatter.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Testing/Testing.h"
#include "Tests/Support/SerialPortFixture.h"

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
        SC_TRY(SC::Result(not readData.empty()));
        totalRead += readData.sizeInBytes();
    }
    return SC::Result(true);
}

#if SC_PLATFORM_WINDOWS
using SerialPortTestHelpers::resolveWindowsCom0ComPorts;
using SerialPortTestHelpers::WindowsCom0ComPorts;
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
        SC_TRY(SC::Result(masterFd != -1));

        if (::grantpt(masterFd) != 0 or ::unlockpt(masterFd) != 0)
        {
            (void)::close(masterFd);
            return SC::Result(false);
        }
        const char* slaveName = ::ptsname(masterFd);
        if (slaveName == nullptr)
        {
            (void)::close(masterFd);
            return SC::Result(false);
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
        if (test_section("structured errors and formatter"))
        {
            structuredErrorsAndFormatter();
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
    void structuredErrorsAndFormatter();
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
    const ResultSerialPort openResult = serial.open("COM9999", options);
    SC_TEST_EXPECT(openResult.isError(SerialPortError::OpenFailed));
    SC_TEST_EXPECT(openResult.detail == SerialPortErrorDetail::WindowsCreateFile);
    SC_TEST_EXPECT(openResult.nativeError != 0);
#else
    const ResultSerialPort relativePathResult = serial.open("ttyS0", options);
    SC_TEST_EXPECT(relativePathResult.isError(SerialPortError::PathMustBeAbsolute));

    const ResultSerialPort openResult = serial.open("/dev/this-device-should-not-exist-sc", options);
    SC_TEST_EXPECT(openResult.isError(SerialPortError::OpenFailed));
    SC_TEST_EXPECT(openResult.detail == SerialPortErrorDetail::PosixOpen);
    SC_TEST_EXPECT(openResult.nativeError == static_cast<uint32_t>(ENOENT));
#endif

    SerialDescriptor unopened;
    SC_TEST_EXPECT(unopened.setSettings(SerialSettings()).isError(SerialPortError::InvalidHandle));
    SerialSettings unopenedSettings;
    SC_TEST_EXPECT(unopened.getSettings(unopenedSettings).isError(SerialPortError::InvalidHandle));

    SerialSettings invalidSettings;
    invalidSettings.baudRate = 0;
    SC_TEST_EXPECT(serial.setSettings(invalidSettings).isError(SerialPortError::InvalidBaudRate));

    invalidSettings             = SerialSettings();
    invalidSettings.dataBits    = static_cast<SerialSettings::DataBits>(4);
    invalidSettings.parity      = SerialSettings::Parity::None;
    invalidSettings.stopBits    = SerialSettings::StopBits::One;
    invalidSettings.baudRate    = 9600;
    invalidSettings.flowControl = SerialSettings::FlowControl::None;
    SC_TEST_EXPECT(serial.setSettings(invalidSettings).isError(SerialPortError::InvalidDataBits));

    invalidSettings        = SerialSettings();
    invalidSettings.parity = static_cast<SerialSettings::Parity>(5);
    SC_TEST_EXPECT(serial.setSettings(invalidSettings).isError(SerialPortError::InvalidParity));

    invalidSettings          = SerialSettings();
    invalidSettings.stopBits = static_cast<SerialSettings::StopBits>(0);
    SC_TEST_EXPECT(serial.setSettings(invalidSettings).isError(SerialPortError::InvalidStopBits));

    invalidSettings             = SerialSettings();
    invalidSettings.flowControl = static_cast<SerialSettings::FlowControl>(9);
    SC_TEST_EXPECT(serial.setSettings(invalidSettings).isError(SerialPortError::InvalidFlowControl));
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
    SC_TEST_EXPECT(fileDescriptor.get(nativeHandle, Result::Error(FileResultCategory, FileError::InvalidHandle)));
    fileDescriptor.detach();

    SerialDescriptor serial;
    SC_TEST_EXPECT(serial.assign(nativeHandle));
#if SC_PLATFORM_WINDOWS
    constexpr SerialPortErrorDetail readSettingsDetail = SerialPortErrorDetail::WindowsReadSettings;
#else
    constexpr SerialPortErrorDetail readSettingsDetail = SerialPortErrorDetail::PosixReadSettings;
#endif
    const ResultSerialPort setResult = serial.setSettings(SerialSettings());
    SC_TEST_EXPECT(setResult.isError(SerialPortError::ReadSettingsFailed));
    SC_TEST_EXPECT(setResult.detail == readSettingsDetail);
    SC_TEST_EXPECT(setResult.nativeError != 0);

    SerialSettings         currentSettings;
    const ResultSerialPort getResult = serial.getSettings(currentSettings);
    SC_TEST_EXPECT(getResult.isError(SerialPortError::ReadSettingsFailed));
    SC_TEST_EXPECT(getResult.detail == readSettingsDetail);
    SC_TEST_EXPECT(getResult.nativeError != 0);
    SC_TEST_EXPECT(serial.close());

    SC_TEST_EXPECT(fs.changeDirectory(dirPath.view()));
    SC_TEST_EXPECT(fs.removeFile(fileName));
    SC_TEST_EXPECT(fs.changeDirectory(report.applicationRootDirectory.view()));
    SC_TEST_EXPECT(fs.removeEmptyDirectory(dirName));
}

void SC::SerialPortTest::structuredErrorsAndFormatter()
{
    static_assert(sizeof(ResultSerialPort) == 16, "ResultSerialPort must remain 16 bytes");
    static_assert(__is_standard_layout(ResultSerialPort), "ResultSerialPort must remain standard-layout");
    static_assert(TypeTraits::IsTriviallyCopyable<ResultSerialPort>::value,
                  "ResultSerialPort must remain trivially copyable");

    ResultErrorFormat formatted = formatSerialPortError(SerialPortError::InvalidHandle, {});
    SC_TEST_EXPECT(formatted.status == ResultErrorFormatStatus::InsufficientCapacity);
    SC_TEST_EXPECT(formatted.requiredCapacity > 1 and formatted.requiredCapacity <= 64);

    char         exact[64];
    const size_t exactCapacity =
        formatted.requiredCapacity <= sizeof(exact) ? formatted.requiredCapacity : sizeof(exact);
    formatted = formatSerialPortError(Result::Error(SerialPortResultCategory, SerialPortError::InvalidHandle),
                                      Span<char>{exact, exactCapacity});
    SC_TEST_EXPECT(formatted);
    if (exactCapacity > 0)
        SC_TEST_EXPECT(exact[exactCapacity - 1] == '\0');

    ResultSerialPort detailed(SerialPortError::OpenFailed, SerialPortErrorDetail::PosixOpen, 12345);
    char             nativeMessage[128];
    formatted = formatSerialPortError(detailed, nativeMessage);
    SC_TEST_EXPECT(formatted);
    SC_TEST_EXPECT(formatted.requiredCapacity <= sizeof(nativeMessage));
    SC_TEST_EXPECT(StringView::fromNullTerminated(nativeMessage, StringEncoding::Ascii).containsString("12345"));

    const Result plain = detailed;
    SC_TEST_EXPECT(plain.isError(SerialPortResultCategory, SerialPortError::OpenFailed));
    SC_TEST_EXPECT(detailed.detail == SerialPortErrorDetail::PosixOpen);
    SC_TEST_EXPECT(detailed.nativeError == 12345);
    SC_TEST_EXPECT(plain.errorValue() == static_cast<uint32_t>(SerialPortError::OpenFailed));

    ResultSerialPort detailOnly(SerialPortError::ReadSettingsFailed, SerialPortErrorDetail::WindowsReadSettings);
    char             detailMessage[128];
    formatted = formatSerialPortError(detailOnly, detailMessage);
    SC_TEST_EXPECT(formatted);
    SC_TEST_EXPECT(formatted.requiredCapacity <= sizeof(detailMessage));
    SC_TEST_EXPECT(not StringView::fromNullTerminated(detailMessage, StringEncoding::Ascii).containsString("12345"));

    const ResultSerialPort foreign(Result::Error(ResultCategory(1234), 1));
    SC_TEST_EXPECT(not foreign);
    SC_TEST_EXPECT(foreign.nativeError == 0);
    SC_TEST_EXPECT(foreign.detail == SerialPortErrorDetail::None);
    SC_TEST_EXPECT(foreign.result.isError(ResultCategory(1234), 1));
    formatted = formatSerialPortError(foreign, nativeMessage);
    SC_TEST_EXPECT(formatted.status == ResultErrorFormatStatus::ForeignCategory);

    formatted = formatSerialPortError(Result(true), nativeMessage);
    SC_TEST_EXPECT(formatted.status == ResultErrorFormatStatus::NotAnError);

    formatted = formatSerialPortError(Result::Error(ResultCategory(1234), 1), nativeMessage);
    SC_TEST_EXPECT(formatted.status == ResultErrorFormatStatus::ForeignCategory);

    formatted = formatSerialPortError(Result::Error(SerialPortResultCategory, 9999), nativeMessage);
    SC_TEST_EXPECT(formatted.status == ResultErrorFormatStatus::UnknownError);
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
    case WindowsCom0ComPorts::Status::NotConfigured: report.console.print("SerialPortTest - {}\n", message); return;
    case WindowsCom0ComPorts::Status::ConfiguredButInvalid:
        SC_TEST_EXPECT(recordExpectation("windows com0com configuration", false, message));
        return;
    case WindowsCom0ComPorts::Status::ConfiguredAndReady: break;
    }
    report.console.print("SerialPortTest - {}\n", message);

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
