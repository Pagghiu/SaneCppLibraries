// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.

#include <WinSock2.h>
// Order is important
#include <MSWSock.h> // AcceptEx, LPFN_CONNECTEX
//
#include <Ws2tcpip.h> // sockadd_in6

#include "EventLoop.h"
#include "EventLoopWindows.h"

#include "EventLoopInternalWindowsAPI.h"

#include "../System/System.h" // SystemFunctions

#if SC_MSVC
// Not sure why on MSVC we don't get on Level 4 warnings for missing switch cases
#pragma warning(default : 4062)
#endif

SC::ReturnCode SC::EventLoopWinWaitTraits::releaseHandle(Handle& waitHandle)
{
    if (waitHandle != INVALID_HANDLE_VALUE)
    {
        BOOL res   = ::UnregisterWaitEx(waitHandle, INVALID_HANDLE_VALUE);
        waitHandle = INVALID_HANDLE_VALUE;
        if (res == FALSE)
        {
            return "UnregisterWaitEx failed"_a8;
        }
    }
    return true;
}

struct SC::EventLoop::Internal
{
    FileDescriptor         loopFd;
    AsyncWakeUp            wakeUpAsync;
    NTSetInformationFile   pNtSetInformationFile = nullptr;
    LPFN_CONNECTEX         pConnectEx            = nullptr;
    LPFN_ACCEPTEX          pAcceptEx             = nullptr;
    LPFN_DISCONNECTEX      pDisconnectEx         = nullptr;
    EventLoopWinOverlapped wakeUpOverlapped;

    Internal()
    {
        HMODULE ntdll         = GetModuleHandleA("ntdll.dll");
        pNtSetInformationFile = reinterpret_cast<NTSetInformationFile>(GetProcAddress(ntdll, "NtSetInformationFile"));

        wakeUpOverlapped.userData = &wakeUpAsync;
    }

    [[nodiscard]] ReturnCode ensureConnectFunction(SocketDescriptor::Handle sock)
    {
        if (pConnectEx == nullptr)
        {

            DWORD dwBytes;
            GUID  guid = WSAID_CONNECTEX;
            int   rc   = WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &pConnectEx,
                                  sizeof(pConnectEx), &dwBytes, NULL, NULL);
            if (rc != 0)
                return "WSAIoctl failed"_a8;
        }
        return true;
    }

    [[nodiscard]] ReturnCode ensureAcceptFunction(SocketDescriptor::Handle sock)
    {
        if (pAcceptEx == nullptr)
        {
            DWORD dwBytes;
            GUID  guid = WSAID_ACCEPTEX;
            int   rc   = WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &pAcceptEx,
                                  sizeof(pAcceptEx), &dwBytes, NULL, NULL);
            if (rc != 0)
                return "WSAIoctl failed"_a8;
        }
        if (pDisconnectEx == nullptr)
        {
            DWORD dwBytes;
            GUID  guid = WSAID_DISCONNECTEX;
            int   rc   = WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &pDisconnectEx,
                                  sizeof(pDisconnectEx), &dwBytes, NULL, NULL);
            if (rc != 0)
                return "WSAIoctl failed"_a8;
        }
        return true;
    }

    ~Internal() { SC_TRUST_RESULT(close()); }

    [[nodiscard]] ReturnCode close() { return loopFd.close(); }

    [[nodiscard]] ReturnCode createEventLoop()
    {
        HANDLE newQueue = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
        if (newQueue == INVALID_HANDLE_VALUE)
        {
            // TODO: Better CreateIoCompletionPort error handling
            return "EventLoop::Internal::createEventLoop() - CreateIoCompletionPort"_a8;
        }
        SC_TRY_IF(loopFd.assign(newQueue));
        return true;
    }

    [[nodiscard]] ReturnCode createWakeup(EventLoop& loop)
    {
        // No need to register it with EventLoop as we're calling PostQueuedCompletionStatus manually
        // As a consequence we don't need to do loop.decreseActiveCount()
        wakeUpAsync.eventLoop = &loop;
        return true;
    }

    [[nodiscard]] static Async* getAsync(OVERLAPPED_ENTRY& event)
    {
        return EventLoopWinOverlapped::getUserDataFromOverlapped<Async>(event.lpOverlapped);
    }

    [[nodiscard]] static void* getUserData(OVERLAPPED_ENTRY& event)
    {
        return reinterpret_cast<void*>(event.lpCompletionKey);
    }

    [[nodiscard]] static ReturnCode checkWSAResult(SOCKET handle, OVERLAPPED& overlapped, size_t* size = nullptr)
    {
        DWORD transferred = 0;
        DWORD flags       = 0;
        BOOL  res         = ::WSAGetOverlappedResult(handle, &overlapped, &transferred, FALSE, &flags);
        if (res == FALSE)
        {
            // TODO: report error
            return "WSAGetOverlappedResult error"_a8;
        }
        if (size)
        {
            *size = static_cast<size_t>(transferred);
        }
        // TODO: should we reset also completion port?
        return true;
    }
};

SC::ReturnCode SC::EventLoop::wakeUpFromExternalThread()
{
    Internal&              self = internal.get();
    FileDescriptor::Handle loopNativeDescriptor;
    SC_TRY_IF(self.loopFd.get(loopNativeDescriptor, "watchInputs - Invalid Handle"_a8));

    if (PostQueuedCompletionStatus(loopNativeDescriptor, 0, 0, &self.wakeUpOverlapped.overlapped) == FALSE)
    {
        return "EventLoop::wakeUpFromExternalThread() - PostQueuedCompletionStatus"_a8;
    }
    return true;
}

SC::ReturnCode SC::EventLoop::associateExternallyCreatedTCPSocket(SocketDescriptor& outDescriptor)
{
    HANDLE loopHandle;
    SC_TRY_IF(internal.get().loopFd.get(loopHandle, "loop handle"_a8));
    SOCKET socket;
    SC_TRY_IF(outDescriptor.get(socket, "Invalid handle"_a8));
    HANDLE iocp = ::CreateIoCompletionPort(reinterpret_cast<HANDLE>(socket), loopHandle, 0, 0);
    SC_TRY_MSG(iocp == loopHandle, "associateExternallyCreatedTCPSocket CreateIoCompletionPort failed"_a8);
    return true;
}

SC::ReturnCode SC::EventLoop::associateExternallyCreatedFileDescriptor(FileDescriptor& outDescriptor)
{
    HANDLE loopHandle;
    SC_TRY_IF(internal.get().loopFd.get(loopHandle, "loop handle"_a8));
    HANDLE handle;
    SC_TRY_IF(outDescriptor.get(handle, "Invalid handle"_a8));
    HANDLE iocp = ::CreateIoCompletionPort(handle, loopHandle, 0, 0);
    SC_TRY_MSG(iocp == loopHandle, "associateExternallyCreatedFileDescriptor CreateIoCompletionPort failed"_a8);
    return true;
}

struct SC::EventLoop::KernelQueue
{
    static constexpr int totalNumEvents = 128;
    OVERLAPPED_ENTRY     events[totalNumEvents];
    ULONG                newEvents = 0;

    [[nodiscard]] ReturnCode pushStagedAsync(Async& async)
    {
        if (async.state != Async::State::Active)
        {
            async.eventLoop->addActiveHandle(async);
        }
        return true;
    }

    // POLL
    [[nodiscard]] ReturnCode pollAsync(EventLoop& self, PollMode pollMode)
    {
        const TimeCounter*     nextTimer = self.findEarliestTimer();
        FileDescriptor::Handle loopNativeDescriptor;
        SC_TRY_IF(
            self.internal.get().loopFd.get(loopNativeDescriptor, "EventLoop::Internal::poll() - Invalid Handle"_a8));
        IntegerMilliseconds timeout;
        if (nextTimer)
        {
            if (nextTimer->isLaterThanOrEqualTo(self.loopTime))
            {
                timeout = nextTimer->subtractApproximate(self.loopTime).inRoundedUpperMilliseconds();
            }
        }
        const BOOL success = GetQueuedCompletionStatusEx(
            loopNativeDescriptor, events, static_cast<ULONG>(SizeOfArray(events)), &newEvents,
            nextTimer or pollMode == PollMode::NoWait ? static_cast<ULONG>(timeout.ms) : INFINITE, FALSE);
        if (not success and GetLastError() != WAIT_TIMEOUT)
        {
            // TODO: GetQueuedCompletionStatusEx error handling
            return "KernelQueue::poll() - GetQueuedCompletionStatusEx error"_a8;
        }
        if (nextTimer)
        {
            self.executeTimers(*this, *nextTimer);
        }
        return true;
    }

    [[nodiscard]] static bool validateEvent(const OVERLAPPED_ENTRY&, bool&) { return true; }

    // TIMEOUT
    [[nodiscard]] static bool completeAsync(AsyncResult::Timeout&)
    {
        // This is used for overlapped notifications (like FileSystemWatcher)
        // It would be probably better to create a dedicated enum type for Overlapped Notifications
        return true;
    }

    // WAKEUP
    [[nodiscard]] static bool completeAsync(AsyncResult::WakeUp& result)
    {
        result.async.eventLoop->executeWakeUps(result);
        return true;
    }

    // ACCEPT
    [[nodiscard]] static ReturnCode setupAsync(Async::Accept& async)
    {
        SC_TRY_IF(SystemFunctions::isNetworkingInited());
        async.overlapped.get().userData = &async;
        return true;
    }

    [[nodiscard]] static ReturnCode activateAsync(Async::Accept& operation)
    {
        EventLoop& eventLoop    = *operation.eventLoop;
        SOCKET     clientSocket = ::WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0,
                                               WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
        SC_TRY_MSG(clientSocket != INVALID_SOCKET, "WSASocketW failed"_a8);
        auto deferDeleteSocket = MakeDeferred([&] { closesocket(clientSocket); });
        static_assert(sizeof(AsyncAccept::acceptBuffer) == sizeof(struct sockaddr_storage) * 2 + 32,
                      "Check acceptBuffer size");

        EventLoopWinOverlapped& overlapped      = operation.overlapped.get();
        DWORD                   sync_bytes_read = 0;

        SC_TRY_IF(eventLoop.internal.get().ensureAcceptFunction(operation.handle));
        BOOL res;
        res = eventLoop.internal.get().pAcceptEx(
            operation.handle, clientSocket, operation.acceptBuffer, 0, sizeof(struct sockaddr_storage) + 16,
            sizeof(struct sockaddr_storage) + 16, &sync_bytes_read, &overlapped.overlapped);
        if (res == FALSE and WSAGetLastError() != WSA_IO_PENDING)
        {
            // TODO: Check AcceptEx WSA error codes
            return "AcceptEx failed"_a8;
        }
        // TODO: Handle synchronous success
        deferDeleteSocket.disarm();
        return operation.clientSocket.assign(clientSocket);
    }

    [[nodiscard]] static ReturnCode completeAsync(AsyncResult::Accept& result)
    {
        Async::Accept& operation = *result.async.asAccept();
        SC_TRY_IF(Internal::checkWSAResult(operation.handle, operation.overlapped.get().overlapped));
        SOCKET clientSocket;
        SC_TRY_IF(operation.clientSocket.get(clientSocket, "clientSocket error"_a8));
        const int socketOpRes = ::setsockopt(clientSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                                             reinterpret_cast<char*>(&operation.handle), sizeof(operation.handle));
        SC_TRY_MSG(socketOpRes == 0, "setsockopt SO_UPDATE_ACCEPT_CONTEXT failed"_a8);
        HANDLE loopHandle;
        SC_TRY_IF(result.async.eventLoop->internal.get().loopFd.get(loopHandle, "loop handle"_a8));
        HANDLE iocp = ::CreateIoCompletionPort(reinterpret_cast<HANDLE>(clientSocket), loopHandle, 0, 0);
        SC_TRY_MSG(iocp == loopHandle, "completeAsync ACCEPT CreateIoCompletionPort failed"_a8);
        return result.acceptedClient.assign(move(operation.clientSocket));
    }

    [[nodiscard]] ReturnCode stopAsync(Async::Accept& asyncAccept)
    {
        HANDLE listenHandle = reinterpret_cast<HANDLE>(asyncAccept.handle);
        // This will cause one more event loop run with GetOverlappedIO failing
        SC_TRY_IF(asyncAccept.clientSocket.close());
        struct FILE_COMPLETION_INFORMATION file_completion_info;
        file_completion_info.Key  = NULL;
        file_completion_info.Port = NULL;
        struct IO_STATUS_BLOCK status_block;
        memset(&status_block, 0, sizeof(status_block));

        EventLoop& eventLoop = *asyncAccept.eventLoop;
        NTSTATUS   status    = eventLoop.internal.get().pNtSetInformationFile(
            listenHandle, &status_block, &file_completion_info, sizeof(file_completion_info),
            FileReplaceCompletionInformation);
        if (status != STATUS_SUCCESS)
        {
            // This will fail if we have a pending AcceptEx call.
            // I've tried ::CancelIoEx, ::shutdown and of course ::closesocket but nothing works
            // return "FileReplaceCompletionInformation failed"_a8;
            return true;
        }
        return true;
    }

    // CONNECT
    [[nodiscard]] static ReturnCode setupAsync(Async::Connect& async)
    {
        SC_TRY_IF(SystemFunctions::isNetworkingInited());
        async.overlapped.get().userData = &async;
        return true;
    }

    [[nodiscard]] static ReturnCode activateAsync(Async::Connect& asyncConnect)
    {
        EventLoop& eventLoop  = *asyncConnect.eventLoop;
        auto&      overlapped = asyncConnect.overlapped.get();
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
            return "bind failed"_a8;
        }
        SC_TRY_IF(eventLoop.internal.get().ensureConnectFunction(asyncConnect.handle));

        const struct sockaddr* sockAddr    = &asyncConnect.ipAddress.handle.reinterpret_as<const struct sockaddr>();
        const int              sockAddrLen = asyncConnect.ipAddress.sizeOfHandle();
        DWORD                  dummyTransferred;
        BOOL connectRes = eventLoop.internal.get().pConnectEx(asyncConnect.handle, sockAddr, sockAddrLen, nullptr, 0,
                                                              &dummyTransferred, &overlapped.overlapped);
        if (connectRes == FALSE and WSAGetLastError() != WSA_IO_PENDING)
        {
            return "ConnectEx failed"_a8;
        }
        ::setsockopt(asyncConnect.handle, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0);
        return true;
    }

    [[nodiscard]] static ReturnCode completeAsync(AsyncResult::Connect& result)
    {
        Async::Connect& operation = *result.async.asConnect();
        SC_TRY_IF(Internal::checkWSAResult(operation.handle, operation.overlapped.get().overlapped));
        return true;
    }

    [[nodiscard]] static ReturnCode stopAsync(Async::Connect& async)
    {
        SC_UNUSED(async);
        return true;
    }

    // SEND
    [[nodiscard]] static ReturnCode setupAsync(Async::Send& async)
    {
        SC_TRY_IF(SystemFunctions::isNetworkingInited());
        async.overlapped.get().userData = &async;
        return true;
    }

    [[nodiscard]] static ReturnCode activateAsync(Async::Send& async)
    {
        auto&  overlapped = async.overlapped.get();
        WSABUF buffer;
        // this const_cast is caused by WSABUF being used for both send and receive
        buffer.buf = const_cast<CHAR*>(async.data.data());
        buffer.len = static_cast<ULONG>(async.data.sizeInBytes());
        DWORD     transferred;
        const int res = ::WSASend(async.handle, &buffer, 1, &transferred, 0, &overlapped.overlapped, nullptr);
        SC_TRY_MSG(res != SOCKET_ERROR or WSAGetLastError() == WSA_IO_PENDING, "WSASend failed"_a8);
        // TODO: when res == 0 we could avoid the additional GetOverlappedResult syscall
        return true;
    }

    [[nodiscard]] static ReturnCode completeAsync(AsyncResult::Send& result)
    {
        Async::Send& operation   = *result.async.asSend();
        size_t       transferred = 0;
        SC_TRY_IF(Internal::checkWSAResult(operation.handle, operation.overlapped.get().overlapped, &transferred));
        return true;
    }

    [[nodiscard]] ReturnCode stopAsync(Async::Send& async)
    {
        SC_UNUSED(async);
        return true;
    }

    // RECEIVE
    [[nodiscard]] static ReturnCode setupAsync(Async::Receive& async)
    {
        SC_TRY_IF(SystemFunctions::isNetworkingInited());
        async.overlapped.get().userData = &async;
        return true;
    }

    [[nodiscard]] static ReturnCode activateAsync(Async::Receive& async)
    {
        auto&  overlapped = async.overlapped.get();
        WSABUF buffer;
        buffer.buf = async.data.data();
        buffer.len = static_cast<ULONG>(async.data.sizeInBytes());
        DWORD     transferred;
        DWORD     flags = 0;
        const int res   = ::WSARecv(async.handle, &buffer, 1, &transferred, &flags, &overlapped.overlapped, nullptr);
        SC_TRY_MSG(res != SOCKET_ERROR or WSAGetLastError() == WSA_IO_PENDING, "WSARecv failed"_a8);
        // TODO: when res == 0 we could avoid the additional GetOverlappedResult syscall
        return true;
    }

    [[nodiscard]] static ReturnCode completeAsync(AsyncResult::Receive& result)
    {
        Async::Receive& operation   = *result.async.asReceive();
        size_t          transferred = 0;
        SC_TRY_IF(Internal::checkWSAResult(operation.handle, operation.overlapped.get().overlapped, &transferred));
        SC_TRY_IF(operation.data.sliceStartLength(0, transferred, result.readData));
        return true;
    }

    [[nodiscard]] ReturnCode stopAsync(Async::Receive& async)
    {
        SC_UNUSED(async);
        return true;
    }

    // READ
    [[nodiscard]] static ReturnCode setupAsync(Async::Read& async)
    {
        async.overlapped.get().userData = &async;
        return true;
    }

    [[nodiscard]] static ReturnCode activateAsync(Async::Read& operation)
    {
        auto& overlapped                 = operation.overlapped.get();
        overlapped.overlapped.Offset     = static_cast<DWORD>(operation.offset & 0xffffffff);
        overlapped.overlapped.OffsetHigh = static_cast<DWORD>((operation.offset >> 32) & 0xffffffff);
        const DWORD numAvalableBuffer    = static_cast<DWORD>(operation.readBuffer.sizeInBytes());
        DWORD       numBytes;
        BOOL res = ::ReadFile(operation.fileDescriptor, operation.readBuffer.data(), numAvalableBuffer, &numBytes,
                              &overlapped.overlapped);
        if (res == FALSE and GetLastError() != ERROR_IO_PENDING)
        {
            return "ReadFile failed"_a8;
        }
        return true;
    }

    [[nodiscard]] static ReturnCode completeAsync(AsyncResult::Read& result)
    {
        Async::Read& operation   = *result.async.asRead();
        OVERLAPPED&  overlapped  = operation.overlapped.get().overlapped;
        DWORD        transferred = 0;
        BOOL         res         = ::GetOverlappedResult(operation.fileDescriptor, &overlapped, &transferred, FALSE);
        if (res == FALSE)
        {
            // TODO: report error
            return "GetOverlappedResult error"_a8;
        }
        SC_TRY_IF(result.async.readBuffer.sliceStartLength(0, static_cast<size_t>(transferred), result.readData));
        return true;
    }

    [[nodiscard]] static ReturnCode stopAsync(Async::Read& async)
    {
        SC_UNUSED(async);
        return true;
    }

    // WRITE
    [[nodiscard]] static ReturnCode setupAsync(Async::Write& async)
    {
        async.overlapped.get().userData = &async;
        return true;
    }

    [[nodiscard]] static ReturnCode activateAsync(Async::Write& async)
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
            return "WriteFile failed"_a8;
        }
        return true;
    }

    [[nodiscard]] static ReturnCode completeAsync(AsyncResult::Write& result)
    {
        Async::Write& operation   = *result.async.asWrite();
        OVERLAPPED&   overlapped  = operation.overlapped.get().overlapped;
        DWORD         transferred = 0;
        BOOL          res         = ::GetOverlappedResult(operation.fileDescriptor, &overlapped, &transferred, FALSE);
        if (res == FALSE)
        {
            // TODO: report error
            return "GetOverlappedResult error"_a8;
        }
        result.writtenBytes = transferred;
        return true;
    }

    [[nodiscard]] static ReturnCode stopAsync(Async::Write& async)
    {
        SC_UNUSED(async);
        return true;
    }

    // PROCESS
    [[nodiscard]] static ReturnCode setupAsync(Async::ProcessExit& async)
    {
        async.overlapped.get().userData = &async;
        return true;
    }

    // This is executed on windows thread pool
    static void CALLBACK processExitCallback(void* data, BOOLEAN timeoutOccurred)
    {
        SC_UNUSED(timeoutOccurred);
        Async&                 async = *static_cast<Async*>(data);
        FileDescriptor::Handle loopNativeDescriptor;
        SC_TRUST_RESULT(async.eventLoop->getLoopFileDescriptor(loopNativeDescriptor));

        if (PostQueuedCompletionStatus(loopNativeDescriptor, 0, 0,
                                       &async.asProcessExit()->overlapped.get().overlapped) == FALSE)
        {
            // TODO: Report error?
            // return "EventLoop::wakeUpFromExternalThread() - PostQueuedCompletionStatus"_a8;
        }
    }

    [[nodiscard]] static ReturnCode activateAsync(Async::ProcessExit& async)
    {
        const ProcessDescriptor::Handle processHandle = async.handle;

        HANDLE waitHandle;
        BOOL result = RegisterWaitForSingleObject(&waitHandle, processHandle, KernelQueue::processExitCallback, &async,
                                                  INFINITE, WT_EXECUTEINWAITTHREAD | WT_EXECUTEONLYONCE);
        if (result == FALSE)
        {
            return "RegisterWaitForSingleObject failed"_a8;
        }
        return async.asProcessExit()->waitHandle.assign(waitHandle);
    }

    [[nodiscard]] static ReturnCode completeAsync(AsyncResult::ProcessExit& result)
    {
        Async::ProcessExit& processExit = *result.async.asProcessExit();
        SC_TRY_IF(processExit.waitHandle.close());
        DWORD processStatus;
        if (GetExitCodeProcess(processExit.handle, &processStatus) == FALSE)
        {
            return "GetExitCodeProcess failed"_a8;
        }
        result.exitStatus.status = static_cast<int32_t>(processStatus);
        return true;
    }

    [[nodiscard]] static ReturnCode stopAsync(Async::ProcessExit& async) { return async.waitHandle.close(); }
};

template <>
void SC::OpaqueFuncs<SC::EventLoopWinOverlappedTraits>::construct(Handle& buffer)
{
    new (&buffer.reinterpret_as<Object>(), PlacementNew()) Object();
}
template <>
void SC::OpaqueFuncs<SC::EventLoopWinOverlappedTraits>::destruct(Object& obj)
{
    obj.~Object();
}
template <>
void SC::OpaqueFuncs<SC::EventLoopWinOverlappedTraits>::moveConstruct(Handle& buffer, Object&& obj)
{
    new (&buffer.reinterpret_as<Object>(), PlacementNew()) Object(move(obj));
}
template <>
void SC::OpaqueFuncs<SC::EventLoopWinOverlappedTraits>::moveAssign(Object& pthis, Object&& obj)
{
    pthis = move(obj);
}
