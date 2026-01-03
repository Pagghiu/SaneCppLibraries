// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include <WinSock2.h>
// Order is important
#include <MSWSock.h> // AcceptEx, LPFN_CONNECTEX
//
#include <Ws2tcpip.h> // sockadd_in6
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <stddef.h> // offsetof

struct SC_FILE_BASIC_INFORMATION
{
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    DWORD         FileAttributes;
};

struct SC_FILE_COMPLETION_INFORMATION
{
    HANDLE Port;
    PVOID  Key;
};

#ifndef _NTDEF_
typedef _Return_type_success_(return >= 0) LONG NTSTATUS;
typedef NTSTATUS* PNTSTATUS;
#endif

struct SC_IO_STATUS_BLOCK
{
    union
    {
        NTSTATUS Status;
        PVOID    Pointer;
    };
    ULONG_PTR Information;
};

enum SC_FILE_INFORMATION_CLASS
{
    FileReplaceCompletionInformation = 0x3D
};

typedef NTSTATUS(NTAPI* SC_NtSetInformationFile)(HANDLE fileHandle, struct SC_IO_STATUS_BLOCK* ioStatusBlock,
                                                 void* fileInformation, ULONG length,
                                                 enum SC_FILE_INFORMATION_CLASS fileInformationClass);

static constexpr NTSTATUS STATUS_SUCCESS = 0;

// Undefine Windows API functions that conflict with our AsyncFileSystemOperation enumeration...
#ifdef CopyFile
#undef CopyFile
#endif
#ifdef RemoveDirectory
#undef RemoveDirectory
#endif

#include "../../Foundation/Deferred.h"
#include "../../Socket/Socket.h"
#include "AsyncInternal.h"

// We store a user pointer at a fixed offset from overlapped to allow getting back source object
// with results from GetQueuedCompletionStatusEx.
// We must do it because there is no void* userData pointer in the OVERLAPPED struct
struct SC::detail::AsyncWinOverlapped
{
    void*      userData = nullptr;
    OVERLAPPED overlapped;

    AsyncWinOverlapped() { memset(&overlapped, 0, sizeof(overlapped)); }

    template <typename T>
    [[nodiscard]] static T* getUserDataFromOverlapped(LPOVERLAPPED lpOverlapped)
    {
        constexpr size_t offsetOfOverlapped = offsetof(AsyncWinOverlapped, overlapped);
        constexpr size_t offsetOfUserData   = offsetof(AsyncWinOverlapped, userData);
        return *reinterpret_cast<T**>(reinterpret_cast<uint8_t*>(lpOverlapped) - offsetOfOverlapped + offsetOfUserData);
    }
};

#if SC_COMPILER_MSVC
// Not sure why on MSVC we don't get on Level 4 warnings for missing switch cases
#pragma warning(default : 4062)
#endif

void* SC::AsyncFilePoll::getOverlappedPtr() { return &overlapped.get().overlapped; }

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

struct SC::AsyncEventLoop::Internal::KernelQueue
{
    FileDescriptor loopFd;
    AsyncFilePoll  asyncWakeUp;

    KernelQueue() {}

    [[nodiscard]] static constexpr bool needsThreadPoolForFileOperations() { return true; }

    Result associateExternallyCreatedSocket(SocketDescriptor& outDescriptor)
    {
        SC_TRY(removeAllAssociationsFor(outDescriptor));
        HANDLE loopHandle;
        SC_TRY(loopFd.get(loopHandle, Result::Error("loop handle")));
        SOCKET socket;
        SC_TRY(outDescriptor.get(socket, Result::Error("Invalid handle")));
        HANDLE iocp = ::CreateIoCompletionPort(reinterpret_cast<HANDLE>(socket), loopHandle, 0, 0);
        SC_TRY_MSG(iocp == loopHandle, "associateExternallyCreatedSocket CreateIoCompletionPort failed");
        return Result(true);
    }

    Result associateExternallyCreatedFileDescriptor(FileDescriptor& outDescriptor)
    {
        SC_TRY(removeAllAssociationsFor(outDescriptor));
        HANDLE loopHandle;
        SC_TRY(loopFd.get(loopHandle, Result::Error("loop handle")));
        HANDLE handle;
        SC_TRY(outDescriptor.get(handle, Result::Error("Invalid handle")));
        HANDLE iocp = ::CreateIoCompletionPort(handle, loopHandle, 0, 0);
        SC_TRY_MSG(iocp == loopHandle, "associateExternallyCreatedFileDescriptor CreateIoCompletionPort failed");
        return Result(true);
    }

    static void removeAllAssociationsFor(HANDLE handle)
    {
        struct SC_FILE_COMPLETION_INFORMATION file_completion_info;
        file_completion_info.Key  = NULL;
        file_completion_info.Port = NULL;
        struct SC_IO_STATUS_BLOCK status_block;
        memset(&status_block, 0, sizeof(status_block));
        HMODULE ntdll = GetModuleHandleA("ntdll.dll");
        auto    func  = ::GetProcAddress(ntdll, "NtSetInformationFile");

        SC_NtSetInformationFile pNtSetInformationFile;
        memcpy(&pNtSetInformationFile, &func, sizeof(func));

        NTSTATUS status = pNtSetInformationFile(handle, &status_block, &file_completion_info,
                                                sizeof(file_completion_info), FileReplaceCompletionInformation);
        // SC_ASSERT_RELEASE(status == STATUS_SUCCESS);
        (void)status;
    }

    static Result removeAllAssociationsFor(SocketDescriptor& descriptor)
    {
        SOCKET socket;
        SC_TRY(descriptor.get(socket, Result::Error("descriptor")));
        removeAllAssociationsFor(reinterpret_cast<HANDLE>(socket));
        return Result(true);
    }

    static Result removeAllAssociationsFor(FileDescriptor& descriptor)
    {
        HANDLE handle;
        SC_TRY(descriptor.get(handle, Result::Error("descriptor")));
        removeAllAssociationsFor(handle);
        return Result(true);
    }

    ~KernelQueue() { SC_TRUST_RESULT(close()); }

    Result close() { return loopFd.close(); }

    Result createEventLoop(AsyncEventLoop::Options options = AsyncEventLoop::Options())
    {
        if (options.apiType != AsyncEventLoop::Options::ApiType::Automatic)
        {
            return Result::Error("createEventLoop only accepts ApiType::Automatic");
        }
        HANDLE newQueue = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
        if (newQueue == INVALID_HANDLE_VALUE)
        {
            // TODO: Better CreateIoCompletionPort error handling
            return Result::Error("AsyncEventLoop::KernelQueue::createEventLoop() - CreateIoCompletionPort");
        }
        SC_TRY(loopFd.assign(newQueue));
        return Result(true);
    }

    Result createSharedWatchers(AsyncEventLoop& eventLoop)
    {
        SC_TRY(createWakeup(eventLoop));
        SC_TRY(eventLoop.runNoWait()); // Register the read handle before everything else
        // Calls to excludeFromActiveCount must be after runNoWait()
        // WakeUp (poll) doesn't keep the kernelEvents active
        eventLoop.excludeFromActiveCount(asyncWakeUp);
        asyncWakeUp.flags |= Flag_Internal;
        return Result(true);
    }

    Result createWakeup(AsyncEventLoop& eventLoop)
    {
        asyncWakeUp.setDebugName("SharedWakeUp");
        asyncWakeUp.callback.bind<KernelQueue, &KernelQueue::completeWakeUp>(*this);
        return asyncWakeUp.start(eventLoop, 0);
    }

    void completeWakeUp(AsyncFilePoll::Result& result)
    {
        result.eventLoop.internal.executeWakeUps(result.eventLoop);
        result.reactivateRequest(true);
    }

    static Result checkWSAResult(SOCKET handle, OVERLAPPED& overlapped, size_t* size = nullptr)
    {
        DWORD transferred = 0;
        DWORD flags       = 0;

        const BOOL res = ::WSAGetOverlappedResult(handle, &overlapped, &transferred, FALSE, &flags);
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
        FileDescriptor::Handle loopHandle;
        SC_TRY(loopFd.get(loopHandle, Result::Error("watchInputs - Invalid Handle")));

        OVERLAPPED* overlapped = static_cast<OVERLAPPED*>(asyncWakeUp.getOverlappedPtr());
        if (::PostQueuedCompletionStatus(loopHandle, 0, 0, overlapped) == FALSE)
        {
            return Result::Error("AsyncEventLoop::wakeUpFromExternalThread() - PostQueuedCompletionStatus");
        }
        return Result(true);
    }
};

struct SC::AsyncEventLoop::Internal::KernelEvents
{

    OVERLAPPED_ENTRY* events;

    int&      newEvents;
    const int totalNumEvents = 0;

    KernelEvents(KernelQueue&, AsyncKernelEvents& kernelEvents)
        : newEvents(kernelEvents.numberOfEvents),
          totalNumEvents(static_cast<int>(kernelEvents.eventsMemory.sizeInBytes() / sizeof(events[0])))
    {
        events = reinterpret_cast<decltype(events)>(kernelEvents.eventsMemory.data());
    }

    uint32_t getNumEvents() const { return static_cast<uint32_t>(newEvents); }

    [[nodiscard]] AsyncRequest* getAsyncRequest(uint32_t index)
    {
        OVERLAPPED_ENTRY& event = events[index];
        if (event.lpOverlapped == nullptr)
        {
            // Just in case someone likes to PostQueuedCompletionStatus with a nullptr...
            return nullptr;
        }
        return detail::AsyncWinOverlapped::getUserDataFromOverlapped<AsyncRequest>(event.lpOverlapped);
    }

    Result syncWithKernel(AsyncEventLoop& eventLoop, Internal::SyncMode syncMode)
    {
        AsyncLoopTimeout* loopTimeout = nullptr;
        const TimeMs*     nextTimer   = nullptr;
        if (syncMode == Internal::SyncMode::ForcedForwardProgress)
        {
            loopTimeout = eventLoop.internal.findEarliestLoopTimeout();
            if (loopTimeout)
            {
                nextTimer = &loopTimeout->expirationTime;
            }
        }
        static constexpr Result errorResult = Result::Error("syncWithKernel() - Invalid Handle");
        FileDescriptor::Handle  loopFd;
        SC_TRY(eventLoop.internal.kernelQueue.get().loopFd.get(loopFd, errorResult));

        TimeMs timeout;
        if (nextTimer)
        {
            if (nextTimer->milliseconds >= eventLoop.internal.loopTime.milliseconds)
            {
                timeout.milliseconds = nextTimer->milliseconds - eventLoop.internal.loopTime.milliseconds;
            }
        }
        const DWORD ms =
            nextTimer or syncMode == Internal::SyncMode::NoWait ? static_cast<ULONG>(timeout.milliseconds) : INFINITE;
        ULONG      ulongEvents = static_cast<ULONG>(newEvents);
        const BOOL res         = ::GetQueuedCompletionStatusEx(loopFd, events, totalNumEvents, &ulongEvents, ms, FALSE);
        newEvents              = static_cast<int>(ulongEvents);
        if (res == FALSE)
        {
            if (::GetLastError() == WAIT_TIMEOUT)
            {
                if (newEvents == 1 and events[0].lpOverlapped == nullptr)
                {
                    // On Windows 10 GetQueuedCompletionStatusEx reports 1 removed entry when timeout occurs
                    newEvents = 0;
                }
            }
            else
            {
                // TODO: GetQueuedCompletionStatusEx error handling
                return Result::Error("KernelEvents::poll() - GetQueuedCompletionStatusEx error");
            }
        }
        if (loopTimeout)
        {
            eventLoop.internal.runTimers = true;
        }
        return Result(true);
    }

    [[nodiscard]] bool validateEvent(uint32_t idx, bool& continueProcessing)
    {
        AsyncRequest* async = getAsyncRequest(idx);
        if (async != nullptr and async->state == AsyncRequest::State::Cancelling)
        {
            continueProcessing = false; // Don't process cancellations
            switch (async->type)
            {
            case AsyncRequest::Type::SocketAccept:
                SC_TRUST_RESULT(static_cast<AsyncSocketAccept*>(async)->acceptData->clientSocket.close());
                break;
            default: break;
            }
        }
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // TIMEOUT
    //-------------------------------------------------------------------------------------------------------
    [[nodiscard]] static bool setupAsync(AsyncEventLoop&, AsyncLoopTimeout&) { return true; }

    Result activateAsync(AsyncEventLoop& eventLoop, AsyncLoopTimeout& async)
    {
        async.expirationTime = Internal::offsetTimeClamped(eventLoop.getLoopTime(), async.relativeTimeout);
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // WAKEUP
    //-------------------------------------------------------------------------------------------------------
    [[nodiscard]] static bool setupAsync(AsyncEventLoop&, AsyncLoopWakeUp&) { return true; }

    //-------------------------------------------------------------------------------------------------------
    // WORK
    //-------------------------------------------------------------------------------------------------------
    static bool   setupAsync(AsyncEventLoop&, AsyncLoopWork&) { return true; }
    static Result executeOperation(AsyncLoopWork& loopWork, AsyncLoopWork::CompletionData&) { return loopWork.work(); }

    //-------------------------------------------------------------------------------------------------------
    // Socket ACCEPT
    //-------------------------------------------------------------------------------------------------------
    static bool setupAsync(AsyncEventLoop&, AsyncSocketAccept& async)
    {
        async.acceptData->overlapped.get().userData = &async;
        return true;
    }

    static Result activateAsync(AsyncEventLoop&, AsyncSocketAccept& async)
    {
        SC_TRY(SocketNetworking::isNetworkingInited());

        SOCKET clientSocket = ::WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0,
                                           WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
        SC_TRY_MSG(clientSocket != INVALID_SOCKET, "WSASocketW failed");
        auto deferDeleteSocket = MakeDeferred([&] { closesocket(clientSocket); });
        static_assert(sizeof(detail::AsyncSocketAcceptData::acceptBuffer) == sizeof(struct sockaddr_storage) * 2 + 32,
                      "Check acceptBuffer size");

        detail::AsyncWinOverlapped& overlapped = async.acceptData->overlapped.get();

        DWORD sync_bytes_read = 0;

        SC_TRY(ensureAcceptFunction(async));
        BOOL res;
        res = reinterpret_cast<LPFN_ACCEPTEX>(async.acceptData->pAcceptEx)(
            async.handle, clientSocket, async.acceptData->acceptBuffer, 0, sizeof(struct sockaddr_storage) + 16,
            sizeof(struct sockaddr_storage) + 16, &sync_bytes_read, &overlapped.overlapped);
        if (res == FALSE and WSAGetLastError() != WSA_IO_PENDING)
        {
            // TODO: Check AcceptEx WSA error codes
            return Result::Error("AcceptEx failed");
        }

        // TODO: Handle synchronous success
        deferDeleteSocket.disarm();
        return async.acceptData->clientSocket.assign(clientSocket);
    }

    static Result completeAsync(AsyncSocketAccept::Result& result)
    {
        AsyncSocketAccept& operation = result.getAsync();
        SC_TRY(KernelQueue::checkWSAResult(operation.handle, operation.acceptData->overlapped.get().overlapped));
        SOCKET clientSocket;
        SC_TRY(operation.acceptData->clientSocket.get(clientSocket, Result::Error("clientSocket error")));
        const int socketOpRes = ::setsockopt(clientSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                                             reinterpret_cast<char*>(&operation.handle), sizeof(operation.handle));
        SC_TRY_MSG(socketOpRes == 0, "setsockopt SO_UPDATE_ACCEPT_CONTEXT failed");
        HANDLE loopHandle;
        SC_TRY(result.eventLoop.internal.kernelQueue.get().loopFd.get(loopHandle, Result::Error("completeAsync")));
        HANDLE iocp = ::CreateIoCompletionPort(reinterpret_cast<HANDLE>(clientSocket), loopHandle, 0, 0);
        SC_TRY_MSG(iocp == loopHandle, "completeAsync ACCEPT CreateIoCompletionPort failed");

        return result.completionData.acceptedClient.assign(move(operation.acceptData->clientSocket));
    }

    Result cancelAsync(AsyncEventLoop& eventLoop, AsyncSocketAccept& asyncAccept)
    {
        BOOL res = ::CancelIoEx(reinterpret_cast<HANDLE>(asyncAccept.handle),
                                &asyncAccept.acceptData->overlapped.get().overlapped);
        if (res == FALSE)
        {
            const DWORD lastError = ::GetLastError();
            // Ignore cancellation requests on already closed handles
            if (lastError == ERROR_INVALID_HANDLE)
                return Result(true);
            // CancelIOEx will return ERROR_NOT_FOUND if no operation to cancel has been found
            if (lastError != ERROR_NOT_FOUND)
                return Result::Error("AsyncSocketAccept: CancelEx failed");
        }
        // CancelIoEx queues a cancellation packet on the async queue
        eventLoop.internal.hasPendingKernelCancellations = true;
        return Result(true);
    }

    static Result ensureAcceptFunction(AsyncSocketAccept& async)
    {
        if (async.acceptData->pAcceptEx == nullptr)
        {
            DWORD dwBytes;
            GUID  guid = WSAID_ACCEPTEX;
            static_assert(sizeof(LPFN_ACCEPTEX) == sizeof(async.acceptData->pAcceptEx), "pAcceptEx");
            int rc = WSAIoctl(async.handle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
                              &async.acceptData->pAcceptEx, sizeof(LPFN_ACCEPTEX), &dwBytes, NULL, NULL);
            if (rc != 0)
                return Result::Error("WSAIoctl failed");
        }
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // Socket CONNECT
    //-------------------------------------------------------------------------------------------------------
    [[nodiscard]] static Result activateAsync(AsyncEventLoop&, AsyncSocketConnect& asyncConnect)
    {
        SC_TRY(SocketNetworking::isNetworkingInited());
        OVERLAPPED& overlapped = asyncConnect.overlapped.get().overlapped;
        // To allow loading connect function we must first bind the socket
        int bindRes;
        if (asyncConnect.ipAddress.getAddressFamily() == SocketFlags::AddressFamilyIPV4)
        {
            struct sockaddr_in addr;
            ZeroMemory(&addr, sizeof(addr));
            addr.sin_family      = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port        = 0;

            bindRes = ::bind(asyncConnect.handle, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
        }
        else
        {
            struct sockaddr_in6 addr;
            ZeroMemory(&addr, sizeof(addr));
            addr.sin6_family = AF_INET6;
            addr.sin6_port   = 0;

            bindRes = ::bind(asyncConnect.handle, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
        }
        if (bindRes == SOCKET_ERROR)
        {
            return Result::Error("bind failed");
        }
        SC_TRY(ensureConnectFunction(asyncConnect));

        const struct sockaddr* sockAddr = &asyncConnect.ipAddress.handle.reinterpret_as<const struct sockaddr>();

        const int sockAddrLen = asyncConnect.ipAddress.sizeOfHandle();

        DWORD dummyTransferred;
        BOOL  connectRes = reinterpret_cast<LPFN_CONNECTEX>(asyncConnect.pConnectEx)(
            asyncConnect.handle, sockAddr, sockAddrLen, nullptr, 0, &dummyTransferred, &overlapped);
        if (connectRes == FALSE and WSAGetLastError() != WSA_IO_PENDING)
        {
            return Result::Error("ConnectEx failed");
        }
        ::setsockopt(asyncConnect.handle, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0);
        return Result(true);
    }

    static Result completeAsync(AsyncSocketConnect::Result& result)
    {
        AsyncSocketConnect& operation = result.getAsync();
        SC_TRY(KernelQueue::checkWSAResult(operation.handle, operation.overlapped.get().overlapped));
        return Result(true);
    }

    static Result ensureConnectFunction(AsyncSocketConnect& async)
    {
        if (async.pConnectEx == nullptr)
        {
            DWORD dwBytes;
            GUID  guid = WSAID_CONNECTEX;
            static_assert(sizeof(LPFN_CONNECTEX) == sizeof(async.pConnectEx), "pConnectEx");
            int rc = WSAIoctl(async.handle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &async.pConnectEx,
                              sizeof(LPFN_CONNECTEX), &dwBytes, NULL, NULL);
            if (rc != 0)
                return Result::Error("WSAIoctl failed");
        }
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // Socket SEND
    //-------------------------------------------------------------------------------------------------------
    static Result activateAsync(AsyncEventLoop&, AsyncSocketSend& async)
    {
        OVERLAPPED& overlapped = async.overlapped.get().overlapped;
        DWORD       transferred;
        // Differently from Posix backend, Span layout is not compatible with WSABUF
        int res;
        if (async.singleBuffer)
        {
            WSABUF buffer;
            // this const_cast is caused by WSABUF being used for both send and receive
            buffer.buf = const_cast<CHAR*>(async.buffer.data());
            buffer.len = static_cast<ULONG>(async.buffer.sizeInBytes());
            res        = ::WSASend(async.handle, &buffer, 1, &transferred, 0, &overlapped, nullptr);
        }
        else
        {
            // TODO: Consider if we should allow user to supply memory for the WSABUF
            constexpr size_t MaxBuffers = 512;
            if (async.buffers.sizeInElements() > MaxBuffers)
            {
                return Result::Error("Cannot write more than 512 buffers at once");
            }
            DWORD  numBuffers = static_cast<DWORD>(async.buffers.sizeInElements());
            WSABUF buffers[MaxBuffers];
            for (DWORD idx = 0; idx < numBuffers; ++idx)
            {
                buffers[idx].buf = const_cast<CHAR*>(async.buffers[idx].data());
                buffers[idx].len = static_cast<ULONG>(async.buffers[idx].sizeInBytes());
            }
            res = ::WSASend(async.handle, buffers, numBuffers, &transferred, 0, &overlapped, nullptr);
        }
        SC_TRY_MSG(res != SOCKET_ERROR or WSAGetLastError() == WSA_IO_PENDING, "WSASend failed");
        // TODO: when res == 0 we could avoid the additional GetOverlappedResult syscall
        return Result(true);
    }

    static Result completeAsync(AsyncSocketSend::Result& result)
    {
        return KernelQueue::checkWSAResult(result.getAsync().handle, result.getAsync().overlapped.get().overlapped,
                                           &result.completionData.numBytes);
    }

    //-------------------------------------------------------------------------------------------------------
    // Socket SEND TO
    //-------------------------------------------------------------------------------------------------------
    static Result activateAsync(AsyncEventLoop&, AsyncSocketSendTo& async)
    {
        OVERLAPPED& overlapped = async.overlapped.get().overlapped;
        DWORD       transferred;
        int         res;

        if (async.singleBuffer)
        {
            WSABUF buffer;
            // this const_cast is caused by WSABUF being used for both send and receive
            buffer.buf = const_cast<CHAR*>(async.buffer.data());
            buffer.len = static_cast<ULONG>(async.buffer.sizeInBytes());

            const struct sockaddr* sockAddr    = &async.address.handle.reinterpret_as<const struct sockaddr>();
            const int              sockAddrLen = async.address.sizeOfHandle();

            res = ::WSASendTo(async.handle, &buffer, 1, &transferred, 0, sockAddr, sockAddrLen, &overlapped, nullptr);
        }
        else
        {
            // TODO: Consider if we should allow user to supply memory for the WSABUF
            constexpr size_t MaxBuffers = 512;
            if (async.buffers.sizeInElements() > MaxBuffers)
            {
                return Result::Error("Cannot write more than 512 buffers at once");
            }
            DWORD  numBuffers = static_cast<DWORD>(async.buffers.sizeInElements());
            WSABUF buffers[MaxBuffers];
            for (DWORD idx = 0; idx < numBuffers; ++idx)
            {
                buffers[idx].buf = const_cast<CHAR*>(async.buffers[idx].data());
                buffers[idx].len = static_cast<ULONG>(async.buffers[idx].sizeInBytes());
            }

            const struct sockaddr* sockAddr    = &async.address.handle.reinterpret_as<const struct sockaddr>();
            const int              sockAddrLen = async.address.sizeOfHandle();

            res = ::WSASendTo(async.handle, buffers, numBuffers, &transferred, 0, sockAddr, sockAddrLen, &overlapped,
                              nullptr);
        }
        SC_TRY_MSG(res != SOCKET_ERROR or WSAGetLastError() == WSA_IO_PENDING, "WSASendTo failed");
        return Result(true);
    }

    static Result cancelAsync(AsyncEventLoop& eventLoop, AsyncSocketSendTo& async)
    {
        BOOL res = ::CancelIoEx(reinterpret_cast<HANDLE>(async.handle), &async.overlapped.get().overlapped);
        if (res == FALSE)
        {
            const DWORD lastError = ::GetLastError();
            // Ignore cancellation requests on already closed handles
            if (lastError == ERROR_INVALID_HANDLE)
                return Result(true);
            // CancelIOEx will return ERROR_NOT_FOUND if no operation to cancel has been found
            if (lastError != ERROR_NOT_FOUND)
                return Result::Error("AsyncSocketSendTo: CancelEx failed");
        }
        // CancelIoEx queues a cancellation packet on the async queue
        eventLoop.internal.hasPendingKernelCancellations = true;
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // Socket RECEIVE FROM
    //-------------------------------------------------------------------------------------------------------
    static Result activateAsync(AsyncEventLoop&, AsyncSocketReceiveFrom& async)
    {
        OVERLAPPED& overlapped = async.overlapped.get().overlapped;
        WSABUF      buffer;
        buffer.buf = async.buffer.data();
        buffer.len = static_cast<ULONG>(async.buffer.sizeInBytes());
        DWORD transferred;
        DWORD flags = 0;

        struct sockaddr* sockAddr    = &async.address.handle.reinterpret_as<struct sockaddr>();
        int              sockAddrLen = async.address.sizeOfHandle();
        ::memset(sockAddr, 0, sockAddrLen);
        const int res =
            ::WSARecvFrom(async.handle, &buffer, 1, &transferred, &flags, sockAddr, &sockAddrLen, &overlapped, nullptr);
        SC_TRY_MSG(res != SOCKET_ERROR or WSAGetLastError() == WSA_IO_PENDING, "WSARecvFrom failed");
        return Result(true);
    }

    static Result cancelAsync(AsyncEventLoop& eventLoop, AsyncSocketReceiveFrom& async)
    {
        BOOL res = ::CancelIoEx(reinterpret_cast<HANDLE>(async.handle), &async.overlapped.get().overlapped);
        if (res == FALSE)
        {
            const DWORD lastError = ::GetLastError();
            // Ignore cancellation requests on already closed handles
            if (lastError == ERROR_INVALID_HANDLE)
                return Result(true);
            // CancelIOEx will return ERROR_NOT_FOUND if no operation to cancel has been found
            if (lastError != ERROR_NOT_FOUND)
                return Result::Error("AsyncSocketReceiveFrom: CancelEx failed");
        }
        // CancelIoEx queues a cancellation packet on the async queue
        eventLoop.internal.hasPendingKernelCancellations = true;
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // Socket RECEIVE
    //-------------------------------------------------------------------------------------------------------
    static Result activateAsync(AsyncEventLoop&, AsyncSocketReceive& async)
    {
        OVERLAPPED& overlapped = async.overlapped.get().overlapped;
        WSABUF      buffer;
        buffer.buf = async.buffer.data();
        buffer.len = static_cast<ULONG>(async.buffer.sizeInBytes());
        DWORD     transferred;
        DWORD     flags = 0;
        const int res   = ::WSARecv(async.handle, &buffer, 1, &transferred, &flags, &overlapped, nullptr);
        SC_TRY_MSG(res != SOCKET_ERROR or WSAGetLastError() == WSA_IO_PENDING, "WSARecv failed");
        // TODO: when res == 0 we could avoid the additional GetOverlappedResult syscall
        return Result(true);
    }

    static Result cancelAsync(AsyncEventLoop& eventLoop, AsyncSocketReceive& async)
    {
        BOOL res = ::CancelIoEx(reinterpret_cast<HANDLE>(async.handle), &async.overlapped.get().overlapped);
        if (res == FALSE)
        {
            const DWORD lastError = ::GetLastError();
            // Ignore cancellation requests on already closed handles
            if (lastError == ERROR_INVALID_HANDLE)
                return Result(true);
            // CancelIOEx will return ERROR_NOT_FOUND if no operation to cancel has been found
            if (lastError != ERROR_NOT_FOUND)
                return Result::Error("AsyncSocketReceive: CancelEx failed");
        }
        // CancelIoEx queues a cancellation packet on the async queue
        eventLoop.internal.hasPendingKernelCancellations = true;
        return Result(true);
    }

    static Result completeAsync(AsyncSocketReceive::Result& result)
    {
        Result res = KernelQueue::checkWSAResult(
            result.getAsync().handle, result.getAsync().overlapped.get().overlapped, &result.completionData.numBytes);
        if (res)
        {
            if (result.completionData.numBytes == 0)
            {
                result.completionData.disconnected = true;
            }
        }
        return res;
    }

    //-------------------------------------------------------------------------------------------------------
    // File READ / WRITE shared functions
    //-------------------------------------------------------------------------------------------------------
    template <typename Func, typename T>
    static Result executeFileOperation(Func func, T& async, AsyncEventLoop* eventLoop, decltype(T::buffer) buffer,
                                       bool synchronous, size_t& readBytes, bool* endOfFile)
    {
        OVERLAPPED& overlapped = async.overlapped.get().overlapped;
        overlapped.Offset      = static_cast<DWORD>(async.offset & 0xffffffff);
        overlapped.OffsetHigh  = static_cast<DWORD>((async.offset >> 32) & 0xffffffff);

        const DWORD bufSize = static_cast<DWORD>(buffer.sizeInBytes());

        const FileDescriptor::Handle fileDescriptor = async.handle;

        DWORD numBytes;
        if (func(fileDescriptor, buffer.data(), bufSize, &numBytes, &overlapped) == FALSE)
        {
            DWORD lastError = ::GetLastError();
            if (lastError == ERROR_IO_PENDING) // ERROR_IO_PENDING just indicates async operation is in progress
            {
                if (synchronous)
                {
                    // If we have been requested to do a synchronous operation on an async file, wait for completion
                    if (::GetOverlappedResult(fileDescriptor, &overlapped, &numBytes, TRUE) == FALSE) // bWait == TRUE
                    {
                        lastError = GetLastError();
                        if (lastError == ERROR_HANDLE_EOF or lastError == ERROR_BROKEN_PIPE)
                        {
                            if (endOfFile)
                                *endOfFile = true;
                        }
                        else
                        {
                            return Result::Error("ReadFile/WriteFile (GetOverlappedResult) error");
                        }
                    }
                }
            }
            else if (lastError == ERROR_HANDLE_EOF or lastError == ERROR_BROKEN_PIPE)
            {
                if (endOfFile)
                    *endOfFile = true;

                if (synchronous == false)
                {
                    // Async operation finished synchronously, must flag a manual completion to avoid waiting forever
                    SC_ASSERT_RELEASE(eventLoop != nullptr);
                    SC_ASSERT_RELEASE(numBytes == 0);
                    async.flags |= Internal::Flag_ManualCompletion;
                    async.endedSync = true; // GetOverlappedResult will fail, so we need to remember this is ended
                }
            }
            else
            {
                // We got an unexpected error.
                // In the async case probably the user forgot to open the file with async flags and associate it.
                // In the sync case (threadpool) we try a regular sync call to support files opened with async == false.
                if (not synchronous or func(fileDescriptor, buffer.data(), bufSize, &numBytes, nullptr) == FALSE)
                {
                    // File must have File::OpenOptions::async == true +
                    // associateExternallyCreatedFileDescriptor
                    return Result::Error("ReadFile/WriteFile failed (forgot setting File::OpenOptions::async = true or "
                                         "AsyncEventLoop::associateExternallyCreatedFileDescriptor?)");
                }
            }
        }
        else
        {
            if (synchronous == false)
            {
                // Async operation finished synchronously, must flag a manual completion to avoid waiting forever
                SC_ASSERT_RELEASE(eventLoop != nullptr);
                async.flags |= Internal::Flag_ManualCompletion;
            }
        }

        readBytes = static_cast<size_t>(numBytes);
        return Result(true);
    }

    template <typename ResultType>
    static Result completeFileOperation(ResultType& result, bool* endOfFile)
    {
        auto& async = result.getAsync();
        if (async.endedSync)
        {
            // The operation ended synchronously in activateAsync, GetOverlappedResult would fail
            if (endOfFile)
                *endOfFile = true;
            result.completionData.numBytes = 0;
            return Result(true);
        }
        OVERLAPPED& overlapped  = async.overlapped.get().overlapped;
        DWORD       transferred = 0;
        if (::GetOverlappedResult(async.handle, &overlapped, &transferred, FALSE) == FALSE)
        {
            DWORD lastError = ::GetLastError();
            // Both ERROR_BROKEN_PIPE and ERROR_HANDLE_EOF indicate end of data
            if (lastError == ERROR_HANDLE_EOF or lastError == ERROR_BROKEN_PIPE)
            {
                if (endOfFile)
                    *endOfFile = true;
            }
            else
            {
                return Result::Error("GetOverlappedResult error");
            }
        }
        result.completionData.numBytes = transferred;
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // File READ
    //-------------------------------------------------------------------------------------------------------
    static Result activateAsync(AsyncEventLoop& eventLoop, AsyncFileRead& async)
    {
        AsyncFileRead::CompletionData completionData;
        async.endedSync = false;
        return executeOperation(async, completionData, false, &eventLoop); // synchronous == false
    }

    static Result executeOperation(AsyncFileRead& async, AsyncFileRead::CompletionData& completionData,
                                   bool synchronous = true, AsyncEventLoop* eventLoop = nullptr)
    {
        if (not async.useOffset)
        {
            async.offset = async.readCursor;
        }
        SC_TRY(executeFileOperation(&::ReadFile, async, eventLoop, async.buffer, synchronous, completionData.numBytes,
                                    &completionData.endOfFile));
        if (not completionData.endOfFile)
        {
            async.readCursor = async.offset + async.buffer.sizeInBytes();
        }
        return Result(true);
    }

    static Result completeAsync(AsyncFileRead::Result& result)
    {
        return completeFileOperation(result, &result.completionData.endOfFile);
    }

    //-------------------------------------------------------------------------------------------------------
    // File WRITE
    //-------------------------------------------------------------------------------------------------------
    static Result activateAsync(AsyncEventLoop& eventLoop, AsyncFileWrite& async)
    {
        AsyncFileWrite::CompletionData completionData;
        async.endedSync = false;
        return executeOperation(async, completionData, false, &eventLoop); // synchronous == false
    }

    static Result executeOperation(AsyncFileWrite& async, AsyncFileWrite::CompletionData& completionData,
                                   bool synchronous = true, AsyncEventLoop* eventLoop = nullptr)
    {
        // TODO: Do the same as AsyncFileRead
        // To write to the end of file, specify both the Offset and OffsetHigh members of the OVERLAPPED structure as
        // 0xFFFFFFFF. This is functionally equivalent to previously calling the CreateFile function to open hFile using
        // FILE_APPEND_DATA access.
        if (async.singleBuffer)
        {
            return executeFileOperation(&::WriteFile, async, eventLoop, async.buffer, synchronous,
                                        completionData.numBytes, nullptr);
        }
        else
        {
            size_t currentBufferIndex  = 0;
            size_t partialBytesWritten = 0;

            while (partialBytesWritten < async.totalBytesWritten)
            {
                partialBytesWritten += async.buffers[currentBufferIndex].sizeInBytes();
                currentBufferIndex += 1;
            }
            SC_ASSERT_RELEASE(partialBytesWritten == async.totalBytesWritten); // Sanity check
            while (currentBufferIndex < async.buffers.sizeInElements())
            {
                size_t writtenBytes;
                auto   buffer = async.buffers[currentBufferIndex];
                SC_TRY(
                    executeFileOperation(&::WriteFile, async, eventLoop, buffer, synchronous, writtenBytes, nullptr));
                currentBufferIndex += 1;
                async.totalBytesWritten += buffer.sizeInBytes(); // writtenBytes could be == 0 in async case
                if (synchronous == false)
                    break; // The same OVERLAPPED cannot be re-used to queue multiple concurrent writes
            }
            completionData.numBytes = async.totalBytesWritten; // completeAsync will not be called in the sync case
            return Result(true);
        }
    }

    static Result completeAsync(AsyncFileWrite::Result& result)
    {
        AsyncFileWrite& async = result.getAsync();
        if (async.singleBuffer)
        {
            return completeFileOperation(result, nullptr);
        }
        else
        {
            if (async.totalBytesWritten == Internal::getSummedSizeOfBuffers(async))
            {
                SC_TRY(completeFileOperation(result, nullptr));
                // Write correct numBytes, as completeFileOperation will consider only last write
                result.completionData.numBytes = async.totalBytesWritten;
            }
            else
            {
                // Partial Write
                result.shouldCallCallback = false;
                result.reactivateRequest(true);
            }
            return Result(true);
        }
    }

    //-------------------------------------------------------------------------------------------------------
    // File POLL
    //-------------------------------------------------------------------------------------------------------
    [[nodiscard]] static bool cancelAsync(AsyncEventLoop& eventLoop, AsyncFilePoll& poll)
    {
        // The AsyncFilePoll used for wakeUp has no backing file descriptor handle and it doesn't generate a
        // cancellation on the IOCP, setting hasPendingKernelCancellations == true would block forever.
        if (poll.handle == 0)
        {
            // The AsyncFilePoll used for wakeUp has no backing file descriptor handle and it doesn't generate a
            // cancellation on the IOCP, setting hasPendingKernelCancellations == true would block forever.
            return true;
        }
        // If the handle is valid it will generate a cancellation packet on the IOCP.
        // There is no easy way to check if the handle is valid so we try to duplicate it and check if the
        // duplicated handle is valid.
        HANDLE handle  = INVALID_HANDLE_VALUE;
        HANDLE process = ::GetCurrentProcess();
        if (::DuplicateHandle(process, poll.handle, process, &handle, 0, FALSE, DUPLICATE_SAME_ACCESS) == TRUE)
        {
            if (handle != INVALID_HANDLE_VALUE)
            {
                ::CloseHandle(handle);
                handle = INVALID_HANDLE_VALUE;

                eventLoop.internal.hasPendingKernelCancellations = true;
            }
        }

        return true;
    }

    [[nodiscard]] static bool teardownAsync(AsyncFilePoll*, AsyncTeardown& teardown)
    {
        if (teardown.fileHandle == 0)
        {
            // See comment regarding AsyncFilePoll in cancelAsync
            return true;
        }
        HANDLE handle  = INVALID_HANDLE_VALUE;
        HANDLE process = ::GetCurrentProcess();
        if (::DuplicateHandle(process, teardown.fileHandle, process, &handle, 0, FALSE, DUPLICATE_SAME_ACCESS) == TRUE)
        {
            if (handle != INVALID_HANDLE_VALUE)
            {
                ::CloseHandle(handle);
                handle = INVALID_HANDLE_VALUE;

                teardown.eventLoop->internal.hasPendingKernelCancellations = true;
            }
        }
        return true;
    }

    //-------------------------------------------------------------------------------------------------------
    // Process EXIT
    //-------------------------------------------------------------------------------------------------------
    // This is executed on windows thread pool
    static void CALLBACK processExitCallback(void* data, BOOLEAN timeoutOccurred)
    {
        SC_COMPILER_UNUSED(timeoutOccurred);
        AsyncProcessExit&      async = *static_cast<AsyncProcessExit*>(data);
        FileDescriptor::Handle loopHandle;
        SC_TRUST_RESULT(async.eventLoop->internal.kernelQueue.get().loopFd.get(loopHandle, Result::Error("loopFd")));

        if (PostQueuedCompletionStatus(loopHandle, 0, 0, &async.overlapped.get().overlapped) == FALSE)
        {
            // TODO: Report error?
            // return Result::Error("AsyncEventLoop::wakeUpFromExternalThread() - PostQueuedCompletionStatus");
        }
    }

    static Result activateAsync(AsyncEventLoop& eventLoop, AsyncProcessExit& async)
    {
        async.eventLoop = &eventLoop;

        const FileDescriptor::Handle processHandle = async.handle;

        HANDLE waitHandle;

        BOOL result = ::RegisterWaitForSingleObject(&waitHandle, processHandle, KernelEvents::processExitCallback,
                                                    &async, INFINITE, WT_EXECUTEINWAITTHREAD | WT_EXECUTEONLYONCE);
        if (result == FALSE)
        {
            return Result::Error("RegisterWaitForSingleObject failed");
        }
        return async.waitHandle.assign(waitHandle);
    }

    static Result completeAsync(AsyncProcessExit::Result& result)
    {
        AsyncProcessExit& processExit = result.getAsync();
        SC_TRY(processExit.waitHandle.close());
        DWORD processStatus;
        if (GetExitCodeProcess(processExit.handle, &processStatus) == FALSE)
        {
            return Result::Error("GetExitCodeProcess failed");
        }
        result.completionData.exitStatus = static_cast<int32_t>(processStatus);
        return Result(true);
    }

    static Result cancelAsync(AsyncEventLoop&, AsyncProcessExit& async) { return async.waitHandle.close(); }

    //-------------------------------------------------------------------------------------------------------
    // File System Operation
    //-------------------------------------------------------------------------------------------------------
    [[nodiscard]] static bool setupAsync(AsyncEventLoop&, AsyncFileSystemOperation&) { return true; }

    //-------------------------------------------------------------------------------------------------------
    // Template
    //-------------------------------------------------------------------------------------------------------
    template <typename T>
    [[nodiscard]] static bool setupAsync(AsyncEventLoop&, T& async)
    {
        async.overlapped.get().userData = &async;
        return true;
    }

    // clang-format off
    template <typename T> [[nodiscard]] static bool activateAsync(AsyncEventLoop&, T&) { return true; }
    template <typename T> [[nodiscard]] static bool cancelAsync(AsyncEventLoop&, T&) { return true; }
    template <typename T> [[nodiscard]] static bool completeAsync(T&) { return true; }
    template <typename T> [[nodiscard]] static bool teardownAsync(T*, AsyncTeardown&) { return true; }

    // If False, makes re-activation a no-op, that is a lightweight optimization.
    // More importantly it prevents an assert about being Submitting state when async completes during re-activation run cycle.
    template<typename T> static bool needsSubmissionWhenReactivating(T&) { return true; }
        
    template <typename T, typename P> static Result executeOperation(T&, P&) { return Result::Error("Implement executeOperation"); }
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
