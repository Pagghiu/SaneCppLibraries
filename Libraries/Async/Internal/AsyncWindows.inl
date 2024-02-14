// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include <WinSock2.h>
// Order is important
#include <MSWSock.h> // AcceptEx, LPFN_CONNECTEX
//
#include <Ws2tcpip.h> // sockadd_in6

#include "../../Foundation/Deferred.h"
#include "../Async.h"

#include "AsyncWindows.h"

#include "AsyncWindowsAPI.h"

#if SC_COMPILER_MSVC
// Not sure why on MSVC we don't get on Level 4 warnings for missing switch cases
#pragma warning(default : 4062)
#endif

SC::Result SC::detail::AsyncWinWaitDefinition::releaseHandle(Handle& waitHandle)
{
    if (waitHandle != INVALID_HANDLE_VALUE)
    {
        BOOL res   = ::UnregisterWaitEx(waitHandle, INVALID_HANDLE_VALUE);
        waitHandle = INVALID_HANDLE_VALUE;
        if (res == FALSE)
        {
            return Result::Error("UnregisterWaitEx failed");
        }
    }
    return Result(true);
}

struct SC::AsyncEventLoop::Internal
{
    FileDescriptor             loopFd;
    AsyncLoopWakeUp            wakeUpAsync;
    SC_NtSetInformationFile    pNtSetInformationFile = nullptr;
    LPFN_CONNECTEX             pConnectEx            = nullptr;
    LPFN_ACCEPTEX              pAcceptEx             = nullptr;
    LPFN_DISCONNECTEX          pDisconnectEx         = nullptr;
    detail::AsyncWinOverlapped wakeUpOverlapped;

    Internal()
    {
        HMODULE ntdll = GetModuleHandleA("ntdll.dll");
        pNtSetInformationFile =
            reinterpret_cast<SC_NtSetInformationFile>(GetProcAddress(ntdll, "NtSetInformationFile"));

        wakeUpOverlapped.userData = &wakeUpAsync;
    }

    [[nodiscard]] Result associateExternallyCreatedTCPSocket(SocketDescriptor& outDescriptor)
    {
        HANDLE loopHandle;
        SC_TRY(loopFd.get(loopHandle, Result::Error("loop handle")));
        SOCKET socket;
        SC_TRY(outDescriptor.get(socket, Result::Error("Invalid handle")));
        HANDLE iocp = ::CreateIoCompletionPort(reinterpret_cast<HANDLE>(socket), loopHandle, 0, 0);
        SC_TRY_MSG(iocp == loopHandle, "associateExternallyCreatedTCPSocket CreateIoCompletionPort failed");
        return Result(true);
    }

    [[nodiscard]] Result associateExternallyCreatedFileDescriptor(FileDescriptor& outDescriptor)
    {
        HANDLE loopHandle;
        SC_TRY(loopFd.get(loopHandle, Result::Error("loop handle")));
        HANDLE handle;
        SC_TRY(outDescriptor.get(handle, Result::Error("Invalid handle")));
        HANDLE iocp = ::CreateIoCompletionPort(handle, loopHandle, 0, 0);
        SC_TRY_MSG(iocp == loopHandle, "associateExternallyCreatedFileDescriptor CreateIoCompletionPort failed");
        return Result(true);
    }

    [[nodiscard]] Result ensureConnectFunction(SocketDescriptor::Handle sock)
    {
        if (pConnectEx == nullptr)
        {

            DWORD dwBytes;
            GUID  guid = WSAID_CONNECTEX;
            int   rc   = WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &pConnectEx,
                                  sizeof(pConnectEx), &dwBytes, NULL, NULL);
            if (rc != 0)
                return Result::Error("WSAIoctl failed");
        }
        return Result(true);
    }

    [[nodiscard]] Result ensureAcceptFunction(SocketDescriptor::Handle sock)
    {
        if (pAcceptEx == nullptr)
        {
            DWORD dwBytes;
            GUID  guid = WSAID_ACCEPTEX;
            int   rc   = WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &pAcceptEx,
                                  sizeof(pAcceptEx), &dwBytes, NULL, NULL);
            if (rc != 0)
                return Result::Error("WSAIoctl failed");
        }
        if (pDisconnectEx == nullptr)
        {
            DWORD dwBytes;
            GUID  guid = WSAID_DISCONNECTEX;
            int   rc   = WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &pDisconnectEx,
                                  sizeof(pDisconnectEx), &dwBytes, NULL, NULL);
            if (rc != 0)
                return Result::Error("WSAIoctl failed");
        }
        return Result(true);
    }

    ~Internal() { SC_TRUST_RESULT(close()); }

    [[nodiscard]] Result close() { return loopFd.close(); }

    [[nodiscard]] Result createEventLoop(AsyncEventLoop::Options options = AsyncEventLoop::Options())
    {
        if (options.apiType != AsyncEventLoop::Options::ApiType::Automatic)
        {
            return Result::Error("createEventLoop only accepts ApiType::Automatic");
        }
        HANDLE newQueue = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
        if (newQueue == INVALID_HANDLE_VALUE)
        {
            // TODO: Better CreateIoCompletionPort error handling
            return Result::Error("AsyncEventLoop::Internal::createEventLoop() - CreateIoCompletionPort");
        }
        SC_TRY(loopFd.assign(newQueue));
        return Result(true);
    }

    [[nodiscard]] Result createSharedWatchers(AsyncEventLoop& loop)
    {
        // No need to register it with AsyncEventLoop as we're calling PostQueuedCompletionStatus manually
        // As a consequence we don't need to do loop.decreaseActiveCount()
        wakeUpAsync.eventLoop = &loop;
        wakeUpAsync.state     = AsyncRequest::State::Active;
        return Result(true);
    }

    [[nodiscard]] static Result checkWSAResult(SOCKET handle, OVERLAPPED& overlapped, size_t* size = nullptr)
    {
        DWORD transferred = 0;
        DWORD flags       = 0;
        BOOL  res         = ::WSAGetOverlappedResult(handle, &overlapped, &transferred, FALSE, &flags);
        if (res == FALSE)
        {
            // TODO: report error
            return Result::Error("WSAGetOverlappedResult error");
        }
        if (size)
        {
            *size = static_cast<size_t>(transferred);
        }
        // TODO: should we reset also completion port?
        return Result(true);
    }

    Result wakeUpFromExternalThread()
    {
        FileDescriptor::Handle loopNativeDescriptor;
        SC_TRY(loopFd.get(loopNativeDescriptor, Result::Error("watchInputs - Invalid Handle")));

        if (PostQueuedCompletionStatus(loopNativeDescriptor, 0, 0, &wakeUpOverlapped.overlapped) == FALSE)
        {
            return Result::Error("AsyncEventLoop::wakeUpFromExternalThread() - PostQueuedCompletionStatus");
        }
        return Result(true);
    }
};

struct SC::AsyncEventLoop::KernelQueue
{
    static constexpr int totalNumEvents = 128;
    OVERLAPPED_ENTRY     events[totalNumEvents];
    ULONG                newEvents = 0;

    KernelQueue(Internal&) {}

    uint32_t getNumEvents() const { return static_cast<uint32_t>(newEvents); }

    [[nodiscard]] AsyncRequest* getAsyncRequest(uint32_t index)
    {
        OVERLAPPED_ENTRY& event = events[index];
        return detail::AsyncWinOverlapped::getUserDataFromOverlapped<AsyncRequest>(event.lpOverlapped);
    }

    // SYNC WITH KERNEL
    [[nodiscard]] Result syncWithKernel(AsyncEventLoop& self, SyncMode syncMode)
    {
        const Time::HighResolutionCounter* nextTimer = self.findEarliestTimer();

        FileDescriptor::Handle loopNativeDescriptor;
        SC_TRY(self.internal.get().loopFd.get(loopNativeDescriptor,
                                              Result::Error("AsyncEventLoop::Internal::poll() - Invalid Handle")));
        Time::Milliseconds timeout;
        if (nextTimer)
        {
            if (nextTimer->isLaterThanOrEqualTo(self.loopTime))
            {
                timeout = nextTimer->subtractApproximate(self.loopTime).inRoundedUpperMilliseconds();
            }
        }
        const BOOL success = GetQueuedCompletionStatusEx(
            loopNativeDescriptor, events, static_cast<ULONG>(TypeTraits::SizeOfArray(events)), &newEvents,
            nextTimer or syncMode == SyncMode::NoWait ? static_cast<ULONG>(timeout.ms) : INFINITE, FALSE);
        if (not success and GetLastError() != WAIT_TIMEOUT)
        {
            // TODO: GetQueuedCompletionStatusEx error handling
            return Result::Error("KernelQueue::poll() - GetQueuedCompletionStatusEx error");
        }
        if (nextTimer)
        {
            self.executeTimers(*this, *nextTimer);
        }
        return Result(true);
    }

    [[nodiscard]] static bool validateEvent(uint32_t, bool&) { return Result(true); }

    // TIMEOUT
    [[nodiscard]] static bool setupAsync(AsyncLoopTimeout&) { return true; }

    // Using activateAsync / cancelAsync so that AsyncEventLoop::cancelAsync() can add the async to submission queue
    [[nodiscard]] static bool activateAsync(AsyncLoopTimeout& async)
    {
        async.eventLoop->activeTimers.queueBack(async);
        return Result(true);
    }

    [[nodiscard]] static bool cancelAsync(AsyncLoopTimeout& async)
    {
        async.eventLoop->activeTimers.remove(async);
        return Result(true);
    }

    // WAKEUP

    [[nodiscard]] static bool setupAsync(AsyncLoopWakeUp&) { return true; }

    // Using activateAsync / cancelAsync so that AsyncEventLoop::cancelAsync() can add the async to submission queue
    [[nodiscard]] static bool activateAsync(AsyncLoopWakeUp& async)
    {
        async.eventLoop->activeWakeUps.queueBack(async);
        return Result(true);
    }

    [[nodiscard]] static bool completeAsync(AsyncLoopWakeUp::Result& result)
    {
        result.async.eventLoop->executeWakeUps(result);
        return Result(true);
    }

    [[nodiscard]] static bool cancelAsync(AsyncLoopWakeUp& async)
    {
        async.eventLoop->activeWakeUps.remove(async);
        return Result(true);
    }

    // Socket ACCEPT

    [[nodiscard]] static Result activateAsync(AsyncSocketAccept& operation)
    {
        SC_TRY(SocketNetworking::isNetworkingInited());
        AsyncEventLoop& eventLoop = *operation.eventLoop;

        SOCKET clientSocket = ::WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0,
                                           WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
        SC_TRY_MSG(clientSocket != INVALID_SOCKET, "WSASocketW failed");
        auto deferDeleteSocket = MakeDeferred([&] { closesocket(clientSocket); });
        static_assert(sizeof(AsyncSocketAccept::acceptBuffer) == sizeof(struct sockaddr_storage) * 2 + 32,
                      "Check acceptBuffer size");

        detail::AsyncWinOverlapped& overlapped = operation.overlapped.get();

        DWORD sync_bytes_read = 0;

        SC_TRY(eventLoop.internal.get().ensureAcceptFunction(operation.handle));
        BOOL res;
        res = eventLoop.internal.get().pAcceptEx(
            operation.handle, clientSocket, operation.acceptBuffer, 0, sizeof(struct sockaddr_storage) + 16,
            sizeof(struct sockaddr_storage) + 16, &sync_bytes_read, &overlapped.overlapped);
        if (res == FALSE and WSAGetLastError() != WSA_IO_PENDING)
        {
            // TODO: Check AcceptEx WSA error codes
            return Result::Error("AcceptEx failed");
        }
        // TODO: Handle synchronous success
        deferDeleteSocket.disarm();
        return operation.clientSocket.assign(clientSocket);
    }

    [[nodiscard]] static Result completeAsync(AsyncSocketAccept::Result& result)
    {
        AsyncSocketAccept& operation = result.async;
        SC_TRY(Internal::checkWSAResult(operation.handle, operation.overlapped.get().overlapped));
        SOCKET clientSocket;
        SC_TRY(operation.clientSocket.get(clientSocket, Result::Error("clientSocket error")));
        const int socketOpRes = ::setsockopt(clientSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                                             reinterpret_cast<char*>(&operation.handle), sizeof(operation.handle));
        SC_TRY_MSG(socketOpRes == 0, "setsockopt SO_UPDATE_ACCEPT_CONTEXT failed");
        HANDLE loopHandle;
        SC_TRY(result.async.eventLoop->internal.get().loopFd.get(loopHandle, Result::Error("loop handle")));
        HANDLE iocp = ::CreateIoCompletionPort(reinterpret_cast<HANDLE>(clientSocket), loopHandle, 0, 0);
        SC_TRY_MSG(iocp == loopHandle, "completeAsync ACCEPT CreateIoCompletionPort failed");
        return result.acceptedClient.assign(move(operation.clientSocket));
    }

    [[nodiscard]] Result cancelAsync(AsyncSocketAccept& asyncAccept)
    {
        HANDLE listenHandle = reinterpret_cast<HANDLE>(asyncAccept.handle);
        // This will cause one more event loop run with GetOverlappedIO failing
        SC_TRY(asyncAccept.clientSocket.close());
        struct SC_FILE_COMPLETION_INFORMATION file_completion_info;
        file_completion_info.Key  = NULL;
        file_completion_info.Port = NULL;
        struct SC_IO_STATUS_BLOCK status_block;
        memset(&status_block, 0, sizeof(status_block));

        AsyncEventLoop& eventLoop = *asyncAccept.eventLoop;
        NTSTATUS        status    = eventLoop.internal.get().pNtSetInformationFile(
            listenHandle, &status_block, &file_completion_info, sizeof(file_completion_info),
            FileReplaceCompletionInformation);
        if (status != STATUS_SUCCESS)
        {
            // This will fail if we have a pending AcceptEx call.
            // I've tried ::CancelIoEx, ::shutdown and of course ::closesocket but nothing works
            // return Result::Error("FileReplaceCompletionInformation failed");
            return Result(true);
        }
        return Result(true);
    }

    // Socket CONNECT

    [[nodiscard]] static Result activateAsync(AsyncSocketConnect& asyncConnect)
    {
        SC_TRY(SocketNetworking::isNetworkingInited());
        AsyncEventLoop& eventLoop  = *asyncConnect.eventLoop;
        auto&           overlapped = asyncConnect.overlapped.get();
        // To allow loading connect function we must first bind the socket
        int bindRes;
        if (asyncConnect.ipAddress.getAddressFamily() == SocketFlags::AddressFamilyIPV4)
        {
            struct sockaddr_in addr;
            ZeroMemory(&addr, sizeof(addr));
            addr.sin_family      = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port        = 0;
            bindRes              = ::bind(asyncConnect.handle, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
        }
        else
        {
            struct sockaddr_in6 addr;
            ZeroMemory(&addr, sizeof(addr));
            addr.sin6_family = AF_INET6;
            addr.sin6_port   = 0;
            bindRes          = ::bind(asyncConnect.handle, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
        }
        if (bindRes == SOCKET_ERROR)
        {
            return Result::Error("bind failed");
        }
        SC_TRY(eventLoop.internal.get().ensureConnectFunction(asyncConnect.handle));

        const struct sockaddr* sockAddr = &asyncConnect.ipAddress.handle.reinterpret_as<const struct sockaddr>();

        const int sockAddrLen = asyncConnect.ipAddress.sizeOfHandle();

        DWORD dummyTransferred;
        BOOL  connectRes = eventLoop.internal.get().pConnectEx(asyncConnect.handle, sockAddr, sockAddrLen, nullptr, 0,
                                                               &dummyTransferred, &overlapped.overlapped);
        if (connectRes == FALSE and WSAGetLastError() != WSA_IO_PENDING)
        {
            return Result::Error("ConnectEx failed");
        }
        ::setsockopt(asyncConnect.handle, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0);
        return Result(true);
    }

    [[nodiscard]] static Result completeAsync(AsyncSocketConnect::Result& result)
    {
        AsyncSocketConnect& operation = result.async;
        SC_TRY(Internal::checkWSAResult(operation.handle, operation.overlapped.get().overlapped));
        return Result(true);
    }

    // Socket SEND

    [[nodiscard]] static Result activateAsync(AsyncSocketSend& async)
    {
        auto&  overlapped = async.overlapped.get();
        WSABUF buffer;
        // this const_cast is caused by WSABUF being used for both send and receive
        buffer.buf = const_cast<CHAR*>(async.data.data());
        buffer.len = static_cast<ULONG>(async.data.sizeInBytes());
        DWORD     transferred;
        const int res = ::WSASend(async.handle, &buffer, 1, &transferred, 0, &overlapped.overlapped, nullptr);
        SC_TRY_MSG(res != SOCKET_ERROR or WSAGetLastError() == WSA_IO_PENDING, "WSASend failed");
        // TODO: when res == 0 we could avoid the additional GetOverlappedResult syscall
        return Result(true);
    }

    [[nodiscard]] static Result completeAsync(AsyncSocketSend::Result& result)
    {
        AsyncSocketSend& operation   = result.async;
        size_t           transferred = 0;
        SC_TRY(Internal::checkWSAResult(operation.handle, operation.overlapped.get().overlapped, &transferred));
        return Result(true);
    }

    // Socket RECEIVE

    [[nodiscard]] static Result activateAsync(AsyncSocketReceive& async)
    {
        auto&  overlapped = async.overlapped.get();
        WSABUF buffer;
        buffer.buf = async.data.data();
        buffer.len = static_cast<ULONG>(async.data.sizeInBytes());
        DWORD     transferred;
        DWORD     flags = 0;
        const int res   = ::WSARecv(async.handle, &buffer, 1, &transferred, &flags, &overlapped.overlapped, nullptr);
        SC_TRY_MSG(res != SOCKET_ERROR or WSAGetLastError() == WSA_IO_PENDING, "WSARecv failed");
        // TODO: when res == 0 we could avoid the additional GetOverlappedResult syscall
        return Result(true);
    }

    [[nodiscard]] static Result completeAsync(AsyncSocketReceive::Result& result)
    {
        AsyncSocketReceive& operation   = result.async;
        size_t              transferred = 0;
        SC_TRY(Internal::checkWSAResult(operation.handle, operation.overlapped.get().overlapped, &transferred));
        SC_TRY(operation.data.sliceStartLength(0, transferred, result.readData));
        return Result(true);
    }

    // Socket Close
    [[nodiscard]] static Result setupAsync(AsyncSocketClose& async)
    {
        async.eventLoop->scheduleManualCompletion(async);
        async.code = ::closesocket(async.handle);
        SC_TRY_MSG(async.code == 0, "Close returned error");
        return Result(true);
    }

    // File READ

    [[nodiscard]] static Result activateAsync(AsyncFileRead& operation)
    {
        auto& overlapped                 = operation.overlapped.get();
        overlapped.overlapped.Offset     = static_cast<DWORD>(operation.offset & 0xffffffff);
        overlapped.overlapped.OffsetHigh = static_cast<DWORD>((operation.offset >> 32) & 0xffffffff);
        const DWORD numAvailableBuffer   = static_cast<DWORD>(operation.readBuffer.sizeInBytes());
        DWORD       numBytes;
        BOOL res = ::ReadFile(operation.fileDescriptor, operation.readBuffer.data(), numAvailableBuffer, &numBytes,
                              &overlapped.overlapped);
        if (res == FALSE and GetLastError() != ERROR_IO_PENDING)
        {
            return Result::Error("ReadFile failed");
        }
        return Result(true);
    }

    [[nodiscard]] static Result completeAsync(AsyncFileRead::Result& result)
    {
        AsyncFileRead& operation   = result.async;
        OVERLAPPED&    overlapped  = operation.overlapped.get().overlapped;
        DWORD          transferred = 0;
        BOOL           res         = ::GetOverlappedResult(operation.fileDescriptor, &overlapped, &transferred, FALSE);
        if (res == FALSE)
        {
            // TODO: report error
            return Result::Error("GetOverlappedResult error");
        }
        SC_TRY(result.async.readBuffer.sliceStartLength(0, static_cast<size_t>(transferred), result.readData));
        return Result(true);
    }

    // File WRITE

    [[nodiscard]] static Result activateAsync(AsyncFileWrite& async)
    {
        auto& overlapped                 = async.overlapped.get();
        overlapped.overlapped.Offset     = static_cast<DWORD>(async.offset & 0xffffffff);
        overlapped.overlapped.OffsetHigh = static_cast<DWORD>((async.offset >> 32) & 0xffffffff);

        const DWORD numBytesToWrite = static_cast<DWORD>(async.writeBuffer.sizeInBytes());
        DWORD       numBytes;
        BOOL        res = ::WriteFile(async.fileDescriptor, async.writeBuffer.data(), numBytesToWrite, &numBytes,
                                      &overlapped.overlapped);
        if (res == FALSE and GetLastError() != ERROR_IO_PENDING)
        {
            return Result::Error("WriteFile failed");
        }
        return Result(true);
    }

    [[nodiscard]] static Result completeAsync(AsyncFileWrite::Result& result)
    {
        AsyncFileWrite& operation   = result.async;
        OVERLAPPED&     overlapped  = operation.overlapped.get().overlapped;
        DWORD           transferred = 0;
        BOOL            res         = ::GetOverlappedResult(operation.fileDescriptor, &overlapped, &transferred, FALSE);
        if (res == FALSE)
        {
            // TODO: report error
            return Result::Error("GetOverlappedResult error");
        }
        result.writtenBytes = transferred;
        return Result(true);
    }

    // File CLOSE
    [[nodiscard]] Result setupAsync(AsyncFileClose& async)
    {
        async.eventLoop->scheduleManualCompletion(async);
        async.code = ::CloseHandle(async.fileDescriptor) == FALSE ? -1 : 0;
        SC_TRY_MSG(async.code == 0, "Close returned error");
        return Result(true);
    }

    // PROCESS

    // This is executed on windows thread pool
    static void CALLBACK processExitCallback(void* data, BOOLEAN timeoutOccurred)
    {
        SC_COMPILER_UNUSED(timeoutOccurred);
        AsyncProcessExit&      async = *static_cast<AsyncProcessExit*>(data);
        FileDescriptor::Handle loopNativeDescriptor;
        SC_TRUST_RESULT(async.eventLoop->internal.get().loopFd.get(loopNativeDescriptor, Result::Error("loopFd")));

        if (PostQueuedCompletionStatus(loopNativeDescriptor, 0, 0, &async.overlapped.get().overlapped) == FALSE)
        {
            // TODO: Report error?
            // return Result::Error("AsyncEventLoop::wakeUpFromExternalThread() - PostQueuedCompletionStatus");
        }
    }

    [[nodiscard]] static Result activateAsync(AsyncProcessExit& async)
    {
        const ProcessDescriptor::Handle processHandle = async.handle;

        HANDLE waitHandle;
        BOOL result = RegisterWaitForSingleObject(&waitHandle, processHandle, KernelQueue::processExitCallback, &async,
                                                  INFINITE, WT_EXECUTEINWAITTHREAD | WT_EXECUTEONLYONCE);
        if (result == FALSE)
        {
            return Result::Error("RegisterWaitForSingleObject failed");
        }
        return async.waitHandle.assign(waitHandle);
    }

    [[nodiscard]] static Result completeAsync(AsyncProcessExit::Result& result)
    {
        AsyncProcessExit& processExit = result.async;
        SC_TRY(processExit.waitHandle.close());
        DWORD processStatus;
        if (GetExitCodeProcess(processExit.handle, &processStatus) == FALSE)
        {
            return Result::Error("GetExitCodeProcess failed");
        }
        result.exitStatus.status = static_cast<int32_t>(processStatus);
        return Result(true);
    }

    [[nodiscard]] static Result cancelAsync(AsyncProcessExit& async) { return async.waitHandle.close(); }

    // Template
    template <typename T>
    [[nodiscard]] static bool setupAsync(T& async)
    {
        async.overlapped.get().userData = &async;
        return true;
    }

    // clang-format off
    template <typename T> [[nodiscard]] bool teardownAsync(T&)  { return true; }
    template <typename T> [[nodiscard]] bool activateAsync(T&)  { return true; }
    template <typename T> [[nodiscard]] bool completeAsync(T&)  { return true; }
    template <typename T> [[nodiscard]] bool cancelAsync(T&)    { return true; }
    // clang-format on
};

template <>
void SC::detail::WinOverlappedOpaque::construct(Handle& buffer)
{
    placementNew(buffer.reinterpret_as<Object>());
}
template <>
void SC::detail::WinOverlappedOpaque::destruct(Object& obj)
{
    obj.~Object();
}
template <>
void SC::detail::WinOverlappedOpaque::moveConstruct(Handle& buffer, Object&& obj)
{
    placementNew(buffer.reinterpret_as<Object>(), move(obj));
}
template <>
void SC::detail::WinOverlappedOpaque::moveAssign(Object& selfObject, Object&& obj)
{
    selfObject = move(obj);
}
