// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.

#include <WinSock2.h>
// Order is important
#include <MSWSock.h> // AcceptEx, LPFN_CONNECTEX
// #pragma comment(lib, "Mswsock.lib")
#include <Ws2tcpip.h> // sockadd_in6

#include "EventLoop.h"
#include "EventLoopWindows.h"

#include "EventLoopInternalWindowsAPI.h"

#include "../System/System.h" // SystemFunctions

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

    [[nodiscard]] Async* getAsync(OVERLAPPED_ENTRY& event) const
    {
        return EventLoopWinOverlapped::getUserDataFromOverlapped<Async>(event.lpOverlapped);
    }

    [[nodiscard]] void* getUserData(OVERLAPPED_ENTRY& event) const
    {
        return reinterpret_cast<void*>(event.lpCompletionKey);
    }

    [[nodiscard]] ReturnCode canRunCompletionFor(Async&, const OVERLAPPED_ENTRY&) { return true; }

    [[nodiscard]] ReturnCode runCompletionFor(AsyncResult::Timeout&, const OVERLAPPED_ENTRY&)
    {
        // This is used for overlapped notifications (like FileSystemWatcher)
        // It would be probably better to create a dedicated enum type for Overlapped Notifications
        return true;
    }

    [[nodiscard]] ReturnCode runCompletionFor(AsyncResult::WakeUp& result, const OVERLAPPED_ENTRY&)
    {
        if (&result.async == &wakeUpAsync)
        {
            result.async.eventLoop->executeWakeUps();
        }
        return true;
        // Do not unregister async
    }

    [[nodiscard]] ReturnCode runCompletionFor(AsyncResult::ProcessExit& result, const OVERLAPPED_ENTRY&)
    {
        Async::ProcessExit& processExit = *result.async.asProcessExit();
        SC_TRY_IF(processExit.waitHandle.close());
        DWORD processStatus;
        if (GetExitCodeProcess(processExit.handle, &processStatus) == FALSE)
        {
            return "GetExitCodeProcess failed"_a8;
        }
        result.exitStatus.status = static_cast<int32_t>(processStatus);
        result.async.eventLoop->numberOfActiveHandles -= 1;
        result.async.eventLoop->activeHandles.remove(result.async);
        return true;
    }

    [[nodiscard]] ReturnCode runCompletionFor(AsyncResult::Accept& result, const OVERLAPPED_ENTRY&)
    {
        Async::Accept& operation = *result.async.asAccept();
        SC_TRY_IF(checkWSAResult(operation.handle, operation.overlapped.get().overlapped));
        SOCKET clientSocket;
        SC_TRY_IF(operation.clientSocket.get(clientSocket, "clientSocket error"_a8));
        const int socketOpRes = ::setsockopt(clientSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                                             reinterpret_cast<char*>(&operation.handle), sizeof(operation.handle));
        SC_TRY_MSG(socketOpRes == 0, "setsockopt SO_UPDATE_ACCEPT_CONTEXT failed"_a8);
        return result.acceptedClient.assign(move(operation.clientSocket));
        // Do not unregister async
    }

    [[nodiscard]] ReturnCode runCompletionFor(AsyncResult::Connect& result, const OVERLAPPED_ENTRY&)
    {
        Async::Connect& operation = *result.async.asConnect();
        SC_TRY_IF(checkWSAResult(operation.handle, operation.overlapped.get().overlapped));
        result.async.eventLoop->numberOfActiveHandles -= 1;
        result.async.eventLoop->activeHandles.remove(result.async);
        return true;
    }

    [[nodiscard]] ReturnCode runCompletionFor(AsyncResult::Send& result, const OVERLAPPED_ENTRY&)
    {
        Async::Send& operation = *result.async.asSend();
        SC_TRY_IF(checkWSAResult(operation.handle, operation.overlapped.get().overlapped));
        result.async.eventLoop->numberOfActiveHandles -= 1;
        result.async.eventLoop->activeHandles.remove(result.async);
        return true;
    }

    [[nodiscard]] ReturnCode runCompletionFor(AsyncResult::Receive& result, const OVERLAPPED_ENTRY&)
    {
        Async::Receive& operation = *result.async.asReceive();
        SC_TRY_IF(checkWSAResult(operation.handle, operation.overlapped.get().overlapped));
        result.async.eventLoop->numberOfActiveHandles -= 1;
        result.async.eventLoop->activeHandles.remove(result.async);
        return true;
    }

    [[nodiscard]] ReturnCode runCompletionFor(AsyncResult::Read& result, const OVERLAPPED_ENTRY&)
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
        result.readBytes = transferred;
        return true;
    }

    [[nodiscard]] ReturnCode runCompletionFor(AsyncResult::Write& result, const OVERLAPPED_ENTRY&)
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

    static ReturnCode checkWSAResult(SOCKET handle, OVERLAPPED& overlapped)
    {
        DWORD transferred = 0;
        DWORD flags       = 0;
        BOOL  res         = ::WSAGetOverlappedResult(handle, &overlapped, &transferred, FALSE, &flags);
        if (res == FALSE)
        {
            // TODO: report error
            return "WSAGetOverlappedResult error"_a8;
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

struct SC::EventLoop::KernelQueue
{
    static constexpr int totalNumEvents = 128;
    OVERLAPPED_ENTRY     events[totalNumEvents];
    ULONG                newEvents = 0;

    [[nodiscard]] ReturnCode stageAsync(EventLoop& eventLoop, Async& async)
    {
        HANDLE loopHandle;
        SC_TRY_IF(eventLoop.internal.get().loopFd.get(loopHandle, "loop handle"_a8));
        switch (async.getType())
        {
        case Async::Type::Timeout:
            eventLoop.activeTimers.queueBack(async);
            eventLoop.numberOfTimers += 1;
            return true;
        case Async::Type::WakeUp:
            eventLoop.activeWakeUps.queueBack(async);
            eventLoop.numberOfWakeups += 1;
            return true;
        case Async::Type::ProcessExit: //
            SC_TRY_IF(startProcessExitWatcher(async));
            break;
        case Async::Type::Accept: SC_TRY_IF(startAcceptWatcher(loopHandle, *async.asAccept())); break;
        case Async::Type::Connect:
            SC_TRY_IF(startConnectWatcher(loopHandle, eventLoop, async, *async.asConnect()));
            break;
        case Async::Type::Send: SC_TRY_IF(startSendWatcher(loopHandle, async, *async.asSend())); break;
        case Async::Type::Receive: SC_TRY_IF(startReceiveWatcher(loopHandle, async, *async.asReceive())); break;
        case Async::Type::Read: //
            SC_TRY_IF(startReadWatcher(loopHandle, async, *async.asRead()));
            break;
        case Async::Type::Write: //
            SC_TRY_IF(startWriteWatcher(loopHandle, async, *async.asWrite()));
            break;
        }
        // On Windows we push directly to activeHandles and not stagedHandles as we've already created kernel objects
        eventLoop.activeHandles.queueBack(async);
        eventLoop.numberOfActiveHandles += 1;
        return true;
    }

    [[nodiscard]] static ReturnCode startReadWatcher(HANDLE loopHandle, Async& async, Async::Read& operation)
    {
        auto& overlapped                 = operation.overlapped.get();
        overlapped.userData              = &async;
        overlapped.overlapped.Offset     = static_cast<DWORD>(operation.offset & 0xffffffff);
        overlapped.overlapped.OffsetHigh = static_cast<DWORD>((operation.offset >> 32) & 0xffffffff);

        HANDLE iocp = ::CreateIoCompletionPort(operation.fileDescriptor, loopHandle, 0, 0);
        SC_TRY_MSG(iocp == loopHandle, "startReadWatcher CreateIoCompletionPort failed"_a8);

        DWORD numBytes;
        BOOL  res =
            ::ReadFile(operation.fileDescriptor, operation.readBuffer.data(),
                       static_cast<DWORD>(operation.readBuffer.sizeInBytes()), &numBytes, &overlapped.overlapped);
        if (res == FALSE and GetLastError() != ERROR_IO_PENDING)
        {
            return "ReadFile failed"_a8;
        }
        return true;
    }

    [[nodiscard]] static ReturnCode stopReadWatcher(Async::Read& async)
    {
        SC_UNUSED(async);
        return true;
    }

    [[nodiscard]] static ReturnCode startWriteWatcher(HANDLE loopHandle, Async& async, Async::Write& operation)
    {
        auto& overlapped                 = operation.overlapped.get();
        overlapped.userData              = &async;
        overlapped.overlapped.Offset     = static_cast<DWORD>(operation.offset & 0xffffffff);
        overlapped.overlapped.OffsetHigh = static_cast<DWORD>((operation.offset >> 32) & 0xffffffff);

        HANDLE iocp = ::CreateIoCompletionPort(operation.fileDescriptor, loopHandle, 0, 0);
        SC_TRY_MSG(iocp == loopHandle, "startWriteWatcher CreateIoCompletionPort failed"_a8);

        DWORD numBytes;
        BOOL  res =
            ::WriteFile(operation.fileDescriptor, operation.writeBuffer.data(),
                        static_cast<DWORD>(operation.writeBuffer.sizeInBytes()), &numBytes, &overlapped.overlapped);
        if (res == FALSE and GetLastError() != ERROR_IO_PENDING)
        {
            return "WriteFile failed"_a8;
        }
        return true;
    }

    [[nodiscard]] static ReturnCode stopWriteWatcher(Async::Write& async)
    {
        SC_UNUSED(async);
        return true;
    }

    [[nodiscard]] static ReturnCode startAcceptWatcher(HANDLE loopHandle, Async::Accept& asyncAccept)
    {
        SC_TRY_IF(SystemFunctions::isNetworkingInited());
        HANDLE listenHandle = reinterpret_cast<HANDLE>(asyncAccept.handle);
        HANDLE iocp         = ::CreateIoCompletionPort(listenHandle, loopHandle, 0, 0);
        SC_TRY_MSG(iocp == loopHandle, "startAcceptWatcher CreateIoCompletionPort failed"_a8);
        return true;
    }

    [[nodiscard]] ReturnCode stopAcceptWatcher(EventLoop& eventLoop, Async::Accept& asyncAccept)
    {
        HANDLE listenHandle = reinterpret_cast<HANDLE>(asyncAccept.handle);
        // This will cause one more event loop run with GetOverlappedIO failing
        SC_TRY_IF(asyncAccept.clientSocket.close());
        struct FILE_COMPLETION_INFORMATION file_completion_info;
        file_completion_info.Key  = NULL;
        file_completion_info.Port = NULL;
        struct IO_STATUS_BLOCK status_block;
        memset(&status_block, 0, sizeof(status_block));

        NTSTATUS status = eventLoop.internal.get().pNtSetInformationFile(
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

    [[nodiscard]] static ReturnCode startConnectWatcher(HANDLE loopHandle, EventLoop& eventLoop, Async& async,
                                                        Async::Connect& asyncConnect)
    {
        SC_TRY_IF(SystemFunctions::isNetworkingInited());

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
        HANDLE connectHandle = reinterpret_cast<HANDLE>(asyncConnect.handle);

        HANDLE iocp = ::CreateIoCompletionPort(connectHandle, loopHandle, 0, 0);
        SC_TRY_MSG(iocp == loopHandle, "startConnectWatcher CreateIoCompletionPort failed"_a8);

        auto& overlapped                   = asyncConnect.overlapped.get();
        overlapped.userData                = &async;
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

    [[nodiscard]] static ReturnCode stopConnectWatcher(Async::Connect& asyncConnect)
    {
        SC_UNUSED(asyncConnect);
        return true;
    }

    [[nodiscard]] static ReturnCode startSendWatcher(HANDLE loopHandle, Async& async, Async::Send& operation)
    {
        SC_TRY_IF(SystemFunctions::isNetworkingInited());
        HANDLE iocp = ::CreateIoCompletionPort(reinterpret_cast<HANDLE>(operation.handle), loopHandle, 0, 0);
        SC_TRY_MSG(iocp == loopHandle, "startSendWatcher CreateIoCompletionPort failed"_a8);
        auto& overlapped    = operation.overlapped.get();
        overlapped.userData = &async;
        WSABUF buffer;
        // this const_cast is caused by WSABUF being used for both send and receive
        buffer.buf = const_cast<CHAR*>(operation.data.data());
        buffer.len = static_cast<ULONG>(operation.data.sizeInBytes());
        DWORD     transferred;
        const int res = ::WSASend(operation.handle, &buffer, 1, &transferred, 0, &overlapped.overlapped, nullptr);
        SC_TRY_MSG(res != SOCKET_ERROR, "WSASend failed"_a8);
        return true;
    }

    [[nodiscard]] ReturnCode stopSendWatcher(Async::Send& operation)
    {
        SC_UNUSED(operation);
        return true;
    }

    [[nodiscard]] static ReturnCode startReceiveWatcher(HANDLE loopHandle, Async& async, Async::Receive& operation)
    {
        SC_TRY_IF(SystemFunctions::isNetworkingInited());
        HANDLE iocp = ::CreateIoCompletionPort(reinterpret_cast<HANDLE>(operation.handle), loopHandle, 0, 0);
        SC_TRY_MSG(iocp == loopHandle, "startReceiveWatcher CreateIoCompletionPort failed"_a8);
        auto& overlapped    = operation.overlapped.get();
        overlapped.userData = &async;
        WSABUF buffer;
        buffer.buf = operation.data.data();
        buffer.len = static_cast<ULONG>(operation.data.sizeInBytes());
        DWORD     transferred;
        DWORD     flags = 0;
        const int res = ::WSARecv(operation.handle, &buffer, 1, &transferred, &flags, &overlapped.overlapped, nullptr);
        SC_TRY_MSG(res != SOCKET_ERROR, "WSARecv failed"_a8);
        return true;
    }

    [[nodiscard]] ReturnCode stopReceiveWatcher(Async::Receive& operation)
    {
        SC_UNUSED(operation);
        return true;
    }

    [[nodiscard]] static ReturnCode startProcessExitWatcher(Async& async)
    {
        const ProcessDescriptor::Handle processHandle = async.asProcessExit()->handle;

        async.asProcessExit()->overlapped.get().userData = &async;

        HANDLE waitHandle;
        BOOL result = RegisterWaitForSingleObject(&waitHandle, processHandle, KernelQueue::processExitCallback, &async,
                                                  INFINITE, WT_EXECUTEINWAITTHREAD | WT_EXECUTEONLYONCE);
        if (result == FALSE)
        {
            return "RegisterWaitForSingleObject failed"_a8;
        }
        return async.asProcessExit()->waitHandle.assign(waitHandle);
    }

    [[nodiscard]] static ReturnCode stopProcessExitWatcher(Async::ProcessExit& async)
    {
        return async.waitHandle.close();
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

    [[nodiscard]] static ReturnCode activateAsync(EventLoop& eventLoop, Async& async)
    {
        HANDLE loopHandle;
        SC_TRY_IF(eventLoop.internal.get().loopFd.get(loopHandle, "loop handle"_a8));
        switch (async.getType())
        {
        case Async::Type::Accept: return activateAcceptWatcher(eventLoop, loopHandle, async);
        default: break;
        }
        return true;
    }

    [[nodiscard]] static ReturnCode activateAcceptWatcher(EventLoop& eventLoop, HANDLE loopHandle, Async& async)
    {
        Async::Accept& asyncAccept  = *async.asAccept();
        SOCKET         clientSocket = ::WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0,
                                                   WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
        SC_TRY_MSG(clientSocket != INVALID_SOCKET, "WSASocketW failed"_a8);
        auto   deferDeleteSocket = MakeDeferred([&] { closesocket(clientSocket); });
        HANDLE socketHandle      = reinterpret_cast<HANDLE>(clientSocket);
        HANDLE iocp              = ::CreateIoCompletionPort(socketHandle, loopHandle, 0, 0);
        SC_TRY_MSG(iocp == loopHandle, "CreateIoCompletionPort client"_a8);

        ::SetFileCompletionNotificationModes(socketHandle,
                                             FILE_SKIP_COMPLETION_PORT_ON_SUCCESS | FILE_SKIP_SET_EVENT_ON_HANDLE);

        static_assert(sizeof(AsyncAccept::acceptBuffer) == sizeof(struct sockaddr_storage) * 2 + 32,
                      "Check acceptBuffer size");

        EventLoopWinOverlapped& overlapped = asyncAccept.overlapped.get();

        overlapped.userData = &async;

        DWORD sync_bytes_read = 0;

        SC_TRY_IF(eventLoop.internal.get().ensureAcceptFunction(asyncAccept.handle));
        BOOL res;
        res = eventLoop.internal.get().pAcceptEx(
            asyncAccept.handle, clientSocket, asyncAccept.acceptBuffer, 0, sizeof(struct sockaddr_storage) + 16,
            sizeof(struct sockaddr_storage) + 16, &sync_bytes_read, &overlapped.overlapped);
        if (res == FALSE and WSAGetLastError() != WSA_IO_PENDING)
        {
            // TODO: Check AcceptEx WSA error codes
            return "AcceptEx failed"_a8;
        }
        // TODO: Handle synchronous success
        deferDeleteSocket.disarm();
        return asyncAccept.clientSocket.assign(clientSocket);
    }

    [[nodiscard]] ReturnCode cancelAsync(EventLoop& eventLoop, Async& async)
    {
        SC_UNUSED(eventLoop);
        switch (async.getType())
        {
        case Async::Type::Timeout: //
            eventLoop.numberOfTimers -= 1;
            return true;
        case Async::Type::WakeUp: //
            eventLoop.numberOfWakeups -= 1;
            return true;
        case Async::Type::ProcessExit: //
            SC_TRY_IF(stopProcessExitWatcher(*async.asProcessExit()));
            break;
        case Async::Type::Accept: //
            SC_TRY_IF(stopAcceptWatcher(eventLoop, *async.asAccept()));
            break;
        case Async::Type::Connect: //
            SC_TRY_IF(stopConnectWatcher(*async.asConnect()));
            break;
        case Async::Type::Send: //
            SC_TRY_IF(stopSendWatcher(*async.asSend()));
            break;
        case Async::Type::Receive: //
            SC_TRY_IF(stopReceiveWatcher(*async.asReceive()));
            break;
        case Async::Type::Read: //
            SC_TRY_IF(stopReadWatcher(*async.asRead()));
            break;
        case Async::Type::Write: //
            SC_TRY_IF(stopWriteWatcher(*async.asWrite()));
            break;
        }
        eventLoop.numberOfActiveHandles -= 1;
        return true;
    }

  private:
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
