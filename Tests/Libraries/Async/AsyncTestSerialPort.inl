// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncTest.h"
#include "Libraries/Process/Process.h"
#include "Libraries/SerialPort/SerialPort.h"

#include <string.h>

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wchar.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#endif

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
        ports.status  = WindowsCom0ComPorts::Status::NotConfigured;
        ports.message = "AsyncTest - Skipping real COM loopback: set SC_TEST_COM0COM_PORT_A and SC_TEST_COM0COM_PORT_B";
        return ports;
    }

    if (hasPortA != hasPortB)
    {
        ports.status  = WindowsCom0ComPorts::Status::ConfiguredButInvalid;
        ports.message = "AsyncTest - Both SC_TEST_COM0COM_PORT_A and SC_TEST_COM0COM_PORT_B must be set";
        return ports;
    }

    if (not isComPortPath(portAEnv) or not isComPortPath(portBEnv))
    {
        ports.status  = WindowsCom0ComPorts::Status::ConfiguredButInvalid;
        ports.message = "AsyncTest - COM ports must be COMx or \\\\.\\COMx";
        return ports;
    }

    if (not ports.portA.assign(portAEnv) or not ports.portB.assign(portBEnv))
    {
        ports.status  = WindowsCom0ComPorts::Status::ConfiguredButInvalid;
        ports.message = "AsyncTest - Invalid COM port path length";
        return ports;
    }

    ports.status  = WindowsCom0ComPorts::Status::ConfiguredAndReady;
    ports.message = "AsyncTest - Running real COM loopback";
    return ports;
}
#endif

struct AsyncSerialVirtualEndpoint
{
    SC::SerialDescriptor serial;
    SC::FileDescriptor   peer;

    SC::Result create()
    {
#if SC_PLATFORM_WINDOWS
        wchar_t   pipeName[128] = {};
        const int charsWritten  = ::swprintf(
            pipeName, sizeof(pipeName) / sizeof(pipeName[0]), L"\\\\.\\pipe\\SCAsyncSerial_%lu_%llu",
            static_cast<unsigned long>(::GetCurrentProcessId()), static_cast<unsigned long long>(::GetTickCount64()));
        SC_TRY_MSG(charsWritten > 0, "AsyncSerialVirtualEndpoint - Invalid pipe name");

        HANDLE serialSide =
            ::CreateNamedPipeW(pipeName, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                               PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, nullptr);
        SC_TRY_MSG(serialSide != INVALID_HANDLE_VALUE, "AsyncSerialVirtualEndpoint - CreateNamedPipeW failed");

        HANDLE peerSide = ::CreateFileW(pipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                        FILE_ATTRIBUTE_NORMAL, nullptr);
        if (peerSide == INVALID_HANDLE_VALUE)
        {
            (void)::CloseHandle(serialSide);
            return SC::Result::Error("AsyncSerialVirtualEndpoint - CreateFileW failed");
        }

        if (::ConnectNamedPipe(serialSide, nullptr) == FALSE and ::GetLastError() != ERROR_PIPE_CONNECTED)
        {
            (void)::CloseHandle(serialSide);
            (void)::CloseHandle(peerSide);
            return SC::Result::Error("AsyncSerialVirtualEndpoint - ConnectNamedPipe failed");
        }

        const SC::Result serialAssigned = serial.assign(serialSide);
        if (not serialAssigned)
        {
            (void)::CloseHandle(serialSide);
            (void)::CloseHandle(peerSide);
            return serialAssigned;
        }
        const SC::Result peerAssigned = peer.assign(peerSide);
        if (not peerAssigned)
        {
            (void)serial.close();
            (void)::CloseHandle(peerSide);
            return peerAssigned;
        }
#else
        int openFlags = O_RDWR | O_NOCTTY;
#ifdef O_CLOEXEC
        openFlags |= O_CLOEXEC;
#endif
        int masterFd;
        do
        {
            masterFd = ::posix_openpt(openFlags);
        } while (masterFd == -1 and errno == EINTR);
        SC_TRY_MSG(masterFd != -1, "AsyncSerialVirtualEndpoint - posix_openpt failed");

        if (::grantpt(masterFd) != 0 or ::unlockpt(masterFd) != 0)
        {
            (void)::close(masterFd);
            return SC::Result::Error("AsyncSerialVirtualEndpoint - grantpt/unlockpt failed");
        }
        const char* slavePath = ::ptsname(masterFd);
        if (slavePath == nullptr)
        {
            (void)::close(masterFd);
            return SC::Result::Error("AsyncSerialVirtualEndpoint - ptsname failed");
        }

        SC_TRY(peer.assign(masterFd));
        SC::SerialOpenOptions options;
        options.blocking = false;
        SC_TRY(serial.open(SC::StringSpan::fromNullTerminated(slavePath, SC::StringEncoding::Native), options));
#endif
        return SC::Result(true);
    }
};
} // namespace

void SC::AsyncTest::serialReadWrite()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    AsyncSerialVirtualEndpoint endpoint;
    SC_TEST_EXPECT(endpoint.create());
    SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(endpoint.serial));

    struct ReadContext
    {
        size_t bytesRead = 0;
        int    callbacks = 0;
        char   data[4]   = {0};
    } readContext;

    AsyncFileRead readRequest;
    char          readBuffer[4] = {0};
    readRequest.callback        = [&](AsyncFileRead::Result& res)
    {
        Span<char> data;
        SC_TEST_EXPECT(res.get(data));
        SC_TEST_EXPECT(data.sizeInBytes() > 0);
        SC_TEST_EXPECT(readContext.bytesRead + data.sizeInBytes() <= sizeof(readContext.data));
        ::memcpy(readContext.data + readContext.bytesRead, data.data(), data.sizeInBytes());
        readContext.bytesRead += data.sizeInBytes();
        readContext.callbacks++;
        if (readContext.bytesRead < sizeof(readContext.data))
        {
            res.reactivateRequest(true);
        }
    };
    SC_TEST_EXPECT(readRequest.start(eventLoop, endpoint.serial, {readBuffer, sizeof(readBuffer)}));

    const char ping[] = {'P', 'I', 'N', 'G'};
    SC_TEST_EXPECT(endpoint.peer.write({ping, sizeof(ping)}));
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(readContext.bytesRead == sizeof(readContext.data));
    SC_TEST_EXPECT(readContext.callbacks >= 1);
    SC_TEST_EXPECT(::memcmp(readContext.data, ping, sizeof(ping)) == 0);

    AsyncFileWrite writeRequest;

    int writeCallbacks    = 0;
    writeRequest.callback = [&](AsyncFileWrite::Result& res)
    {
        size_t bytesWritten = 0;
        SC_TEST_EXPECT(res.get(bytesWritten));
        SC_TEST_EXPECT(bytesWritten == 4);
        writeCallbacks++;
    };
    const char       pong[]  = {'P', 'O', 'N', 'G'};
    Span<const char> toWrite = {pong, sizeof(pong)};
    SC_TEST_EXPECT(writeRequest.start(eventLoop, endpoint.serial, toWrite));
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(writeCallbacks == 1);

    char receivedByPeer[4] = {0};
    SC_TEST_EXPECT(readExactDescriptor(endpoint.peer, {receivedByPeer, sizeof(receivedByPeer)}));
    SC_TEST_EXPECT(::memcmp(receivedByPeer, pong, sizeof(pong)) == 0);
}

void SC::AsyncTest::serialStop()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    AsyncSerialVirtualEndpoint endpoint;
    SC_TEST_EXPECT(endpoint.create());
    SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(endpoint.serial));

    AsyncFileRead readRequest;

    char readBuffer[8]   = {0};
    int  readCallbacks   = 0;
    readRequest.callback = [&](AsyncFileRead::Result&) { readCallbacks++; };
    SC_TEST_EXPECT(readRequest.start(eventLoop, endpoint.serial, {readBuffer, sizeof(readBuffer)}));

    int afterStopCalled = 0;

    Function<void(AsyncResult&)> afterStopped = [&](AsyncResult&) { afterStopCalled++; };
    SC_TEST_EXPECT(readRequest.stop(eventLoop, &afterStopped));
    SC_TEST_EXPECT(afterStopCalled == 0);
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(afterStopCalled == 1);
    SC_TEST_EXPECT(readCallbacks == 0);
}

void SC::AsyncTest::serialSequenceOrdering()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    AsyncSerialVirtualEndpoint endpoint;
    SC_TEST_EXPECT(endpoint.create());
    SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(endpoint.serial));

    AsyncSequence  sequence;
    AsyncFileWrite writeFirst;
    AsyncFileWrite writeSecond;
    writeFirst.executeOn(sequence);
    writeSecond.executeOn(sequence);

    int callbackOrder   = 0;
    writeFirst.callback = [&](AsyncFileWrite::Result& res)
    {
        size_t bytesWritten = 0;
        SC_TEST_EXPECT(res.get(bytesWritten));
        SC_TEST_EXPECT(bytesWritten == 2);
        SC_TEST_EXPECT(callbackOrder == 0);
        callbackOrder = 1;
    };
    writeSecond.callback = [&](AsyncFileWrite::Result& res)
    {
        size_t bytesWritten = 0;
        SC_TEST_EXPECT(res.get(bytesWritten));
        SC_TEST_EXPECT(bytesWritten == 2);
        SC_TEST_EXPECT(callbackOrder == 1);
        callbackOrder = 2;
    };

    const char firstChunk[]  = {'A', 'B'};
    const char secondChunk[] = {'C', 'D'};
    SC_TEST_EXPECT(writeFirst.start(eventLoop, endpoint.serial, {firstChunk, sizeof(firstChunk)}));
    SC_TEST_EXPECT(writeSecond.start(eventLoop, endpoint.serial, {secondChunk, sizeof(secondChunk)}));
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(callbackOrder == 2);

    char receivedByPeer[4] = {0};
    SC_TEST_EXPECT(readExactDescriptor(endpoint.peer, {receivedByPeer, sizeof(receivedByPeer)}));
    const char expected[] = {'A', 'B', 'C', 'D'};
    SC_TEST_EXPECT(::memcmp(receivedByPeer, expected, sizeof(expected)) == 0);
}

#if SC_PLATFORM_WINDOWS
void SC::AsyncTest::serialCom0ComReadWrite()
{
    const WindowsCom0ComPorts ports   = resolveWindowsCom0ComPorts();
    const StringSpan          message = StringSpan::fromNullTerminated(ports.message, StringEncoding::Ascii);
    switch (ports.status)
    {
    case WindowsCom0ComPorts::Status::NotConfigured: report.console.printLine(message); return;
    case WindowsCom0ComPorts::Status::ConfiguredButInvalid:
        SC_TEST_EXPECT(recordExpectation("serial com0com configuration", false, message));
        return;
    case WindowsCom0ComPorts::Status::ConfiguredAndReady: break;
    }
    report.console.printLine(message);

    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    SerialOpenOptions asyncOptions;
    asyncOptions.blocking             = false;
    asyncOptions.settings.baudRate    = 115200;
    asyncOptions.settings.dataBits    = SerialSettings::DataBits::Bits8;
    asyncOptions.settings.parity      = SerialSettings::Parity::None;
    asyncOptions.settings.stopBits    = SerialSettings::StopBits::One;
    asyncOptions.settings.flowControl = SerialSettings::FlowControl::None;

    SerialOpenOptions peerOptions = asyncOptions;
    peerOptions.blocking          = true;

    SerialDescriptor asyncSerial;
    SerialDescriptor peerSerial;
    SC_TEST_EXPECT(asyncSerial.open(ports.portA.view(), asyncOptions));
    SC_TEST_EXPECT(peerSerial.open(ports.portB.view(), peerOptions));
    SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(asyncSerial));

    struct ReadContext
    {
        size_t bytesRead = 0;
        int    callbacks = 0;
        char   data[4]   = {0};
    } readContext;

    AsyncFileRead readRequest;
    char          readBuffer[4] = {};
    readRequest.callback        = [&](AsyncFileRead::Result& res)
    {
        Span<char> data;
        SC_TEST_EXPECT(res.get(data));
        SC_TEST_EXPECT(data.sizeInBytes() > 0);
        SC_TEST_EXPECT(readContext.bytesRead + data.sizeInBytes() <= sizeof(readContext.data));
        ::memcpy(readContext.data + readContext.bytesRead, data.data(), data.sizeInBytes());
        readContext.bytesRead += data.sizeInBytes();
        readContext.callbacks++;
        if (readContext.bytesRead < sizeof(readContext.data))
        {
            res.reactivateRequest(true);
        }
    };
    SC_TEST_EXPECT(readRequest.start(eventLoop, asyncSerial, {readBuffer, sizeof(readBuffer)}));

    const char ping[] = {'P', 'I', 'N', 'G'};
    SC_TEST_EXPECT(peerSerial.write({ping, sizeof(ping)}));
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(readContext.bytesRead == sizeof(readContext.data));
    SC_TEST_EXPECT(readContext.callbacks >= 1);
    SC_TEST_EXPECT(::memcmp(readContext.data, ping, sizeof(ping)) == 0);

    AsyncFileWrite writeRequest;
    int            writeCallbacks = 0;
    writeRequest.callback         = [&](AsyncFileWrite::Result& res)
    {
        size_t bytesWritten = 0;
        SC_TEST_EXPECT(res.get(bytesWritten));
        SC_TEST_EXPECT(bytesWritten == 4);
        writeCallbacks++;
    };

    const char       pong[]  = {'P', 'O', 'N', 'G'};
    Span<const char> toWrite = {pong, sizeof(pong)};
    SC_TEST_EXPECT(writeRequest.start(eventLoop, asyncSerial, toWrite));
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(writeCallbacks == 1);

    char receivedByPeer[4] = {};
    SC_TEST_EXPECT(readExactDescriptor(peerSerial, {receivedByPeer, sizeof(receivedByPeer)}));
    SC_TEST_EXPECT(::memcmp(receivedByPeer, pong, sizeof(pong)) == 0);
}
#endif
