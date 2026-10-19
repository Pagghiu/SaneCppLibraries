// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Base64.h"
#if !SC_ASYNC_EXECUTION_AVAILABLE
#error "Set SC_ASYNC_EXECUTION_STDEXEC_INCLUDE to the stdexec include directory."
#endif
#include "Libraries/Memory/Globals.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SC::Result runAsyncExecutionLifetimeDemo(bool stall, bool cancel, uint16_t port, unsigned frameMilliseconds);

namespace
{
using namespace SC;
namespace ex = AsyncExecutionProtocol;
constexpr ResultCategory DemoCategory{0x80000020u};
enum class DemoError : uint32_t
{
    Mismatch = 1
};
constexpr char Message[] = "hello from cpp day!\n";

// Independent test peer: decode the Base64 stream and compare every byte with the original file pattern.
struct LoopbackPeer
{
    SocketDescriptor listener;
    Thread           thread;
    Result           status{true};
    size_t           received = 0;
    size_t           decoded  = 0;
    size_t           expected;
    uint16_t         port;
    explicit LoopbackPeer(uint16_t port, size_t expected) : expected(expected), port(port) {}
    static int digit(char c)
    {
        if (c >= 'A' and c <= 'Z')
            return c - 'A';
        if (c >= 'a' and c <= 'z')
            return c - 'a' + 26;
        if (c >= '0' and c <= '9')
            return c - '0' + 52;
        if (c == '+')
            return 62;
        if (c == '/')
            return 63;
        return -1;
    }
    Result serve()
    {
        SocketDescriptor peer;
        SC_TRY(SocketServer(listener).accept(peer));
        char   storage[4096];
        char   quartet[4];
        size_t used   = 0;
        bool   padded = false;
        while (true)
        {
            Span<char> data;
            SC_TRY(SocketClient(peer).readWithTimeout(storage, data, 2000));
            if (data.empty())
                break;
            for (char byte : data)
            {
                if (padded)
                    return Result::Error(DemoCategory, DemoError::Mismatch);
                quartet[used++] = byte;
                ++received;
                if (used != 4)
                    continue;
                used        = 0;
                const int a = digit(quartet[0]), b = digit(quartet[1]);
                const int c = quartet[2] == '=' ? 0 : digit(quartet[2]);
                const int d = quartet[3] == '=' ? 0 : digit(quartet[3]);
                if (a < 0 or b < 0 or c < 0 or d < 0 or (quartet[2] == '=' and quartet[3] != '='))
                    return Result::Error(DemoCategory, DemoError::Mismatch);
                const char   bytes[3] = {static_cast<char>((a << 2) | (b >> 4)), static_cast<char>((b << 4) | (c >> 2)),
                                         static_cast<char>((c << 6) | d)};
                const size_t count    = quartet[2] == '=' ? 1 : quartet[3] == '=' ? 2 : 3;
                for (size_t i = 0; i < count; ++i)
                {
                    if (decoded >= expected or bytes[i] != Message[decoded % (sizeof(Message) - 1)])
                        return Result::Error(DemoCategory, DemoError::Mismatch);
                    ++decoded;
                }
                padded = count != 3;
            }
        }
        return decoded == expected and used == 0 ? Result(true) : Result::Error(DemoCategory, DemoError::Mismatch);
    }
    Result start()
    {
        SocketIPAddress address;
        SC_TRY(address.fromAddressPort("127.0.0.1", port));
        SC_TRY(listener.create(address.getAddressFamily()));
        SC_TRY(SocketServer(listener).bind(address));
        SC_TRY(SocketServer(listener).listen(2));
        return thread.start([this](Thread&) { status = serve(); });
    }
    Result join()
    {
        // Unblock accept too if a failing pipeline was cancelled before connecting.
        SocketDescriptor wake;
        SC_TRY(wake.create(SocketFlags::AddressFamilyIPV4));
        (void)SocketClient(wake).connect("127.0.0.1", port);
        (void)wake.close();
        return thread.join();
    }
};

Result runDemo(bool failFile, uint16_t port)
{
    AsyncExecution app;
    SC_TRY(app.create(2));
#if SC_PLATFORM_WINDOWS
    const StringSpan first  = "_Build/AsyncExecutionDemo-Windows-first.txt";
    const StringSpan second = "_Build/AsyncExecutionDemo-Windows-second.txt";
#elif SC_PLATFORM_APPLE
    const StringSpan first  = "_Build/AsyncExecutionDemo-macOS-first.txt";
    const StringSpan second = "_Build/AsyncExecutionDemo-macOS-second.txt";
#else
    const StringSpan first  = "_Build/AsyncExecutionDemo-Linux-first.txt";
    const StringSpan second = "_Build/AsyncExecutionDemo-Linux-second.txt";
#endif
    AsyncExecutionBuffer input;
    SC_TRY(input.resize(32768));
    for (size_t i = 0; i < input.size(); ++i)
        input.bytes()[i] = Message[i % (sizeof(Message) - 1)];
    SC_TRY(app.run(app.writeFile(first, move(input))));
    SC_TRY(input.resize(32767));
    for (size_t i = 0; i < input.size(); ++i)
        input.bytes()[i] = Message[i % (sizeof(Message) - 1)];
    SC_TRY(app.run(app.writeFile(second, move(input))));
    LoopbackPeer firstPeer(port, 32768), secondPeer(static_cast<uint16_t>(port + 1), 32767);
    SC_TRY(firstPeer.start());
    const Result secondStarted = secondPeer.start();
    if (not secondStarted)
    {
        (void)firstPeer.join();
        return secondStarted;
    }

    const auto  ownerThread       = Thread::CurrentThreadID();
    Atomic<int> encodersOnWorkers = 0;
    auto        encode = [&](AsyncExecutionBuffer data) noexcept -> AsyncExecutionValueResult<AsyncExecutionBuffer>
    {
        if (Thread::CurrentThreadID() != ownerThread)
            encodersOnWorkers.fetch_add(1);
        return base64Encode(move(data));
    };

    static_assert(std::is_nothrow_move_constructible_v<AsyncExecutionConnection>);
    static_assert(std::is_nothrow_move_constructible_v<AsyncExecutionValueResult<AsyncExecutionBuffer>>);
    // Presentation snippet: each transfer is sequential; when_all runs the two transfers concurrently.
    auto sendEncoded = [&](StringSpan file, uint16_t destination)
    {
        return app.readFile(file) | ex::on(app.getPoolScheduler(), SC::tryThen(encode)) |
               ex::let_value([&, destination](auto& encoded) noexcept
                             { return app.sendTCP("127.0.0.1", destination, move(encoded)); });
    };
    const StringSpan missing = "_Build/AsyncExecutionDemo-does-not-exist.txt";
    const Result     result  = app.run(ex::when_all(sendEncoded(failFile ? missing : first, port),
                                                    sendEncoded(second, static_cast<uint16_t>(port + 1))));

    SC_TRY(firstPeer.join());
    SC_TRY(secondPeer.join());
    StringPath absolute;
    SC_TRY(detail::AsyncExecutionResolvePath(first, absolute));
    SC_TRY(FileSystem::Operations::removeFile(absolute.view()));
    SC_TRY(detail::AsyncExecutionResolvePath(second, absolute));
    SC_TRY(FileSystem::Operations::removeFile(absolute.view()));
    if (failFile)
    {
        SC_TRY(not result);
        printf("Expected file error propagated; when_all joined cancellation of the sibling transfer.\n");
        return Result(true);
    }
    SC_TRY(result);
    SC_TRY(firstPeer.status);
    SC_TRY(secondPeer.status);
    if (encodersOnWorkers.load() != 2)
        return Result::Error(DemoCategory, DemoError::Mismatch);
    printf("when_all: 2 file -> Base64 (pool) -> TCP transfers completed.\n");
    printf("Decoded and verified %zu + %zu source bytes; received %zu + %zu Base64 bytes.\n", firstPeer.decoded,
           secondPeer.decoded, firstPeer.received, secondPeer.received);
    printf("Both encoders ran off the event-loop thread; sends <= 16 KiB, receives <= 4 KiB.\n");
    return Result(true);
}
} // namespace

int main(int argc, const char* argv[])
{
    SC::Globals::init(SC::Globals::Global);
    bool     fail     = false;
    bool     lifetime = false, stall = false, cancel = false;
    uint16_t port              = 19092;
    unsigned frameMilliseconds = 1;
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--lifetime") == 0)
            lifetime = true;
        else if (strcmp(argv[i], "--stall") == 0)
            stall = true;
        else if (strcmp(argv[i], "--cancel") == 0)
            cancel = true;
        else if (strcmp(argv[i], "--fail") == 0)
            fail = true;
        else if (strcmp(argv[i], "--frame-ms") == 0 and i + 1 < argc)
        {
            const int value = atoi(argv[++i]);
            if (value < 1 or value > 1000)
                return 2;
            frameMilliseconds = static_cast<unsigned>(value);
        }
        else if (strcmp(argv[i], "--port") == 0 and i + 1 < argc)
            port = static_cast<uint16_t>(atoi(argv[++i]));
    }
    const SC::Result result =
        lifetime ? runAsyncExecutionLifetimeDemo(stall, cancel, port, frameMilliseconds) : runDemo(fail, port);
    if (not result)
        fprintf(stderr, "Demo failed: category %u, code %u\n", result.category().value, result.errorValue());
    return result ? 0 : 1;
}
