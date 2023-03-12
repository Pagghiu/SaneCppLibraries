// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
struct SC::Loop::Internal
{
    bool           inited = false;
    FileDescriptor loopFd;

    Vector<FileDescriptorNative> watchersQueue;

    ~Internal() { SC_TRUST_RESULT(close()); }
    [[nodiscard]] ReturnCode close() { return loopFd.handle.close(); }

    [[nodiscard]] ReturnCode createLoop() { return true; }

    [[nodiscard]] ReturnCode createLoopAsyncWakeup() { return true; }
    struct KernelQueue
    {
        int newEvents = 0;

        [[nodiscard]] ReturnCode addReadWatcher(FileDescriptor& loopFd, FileDescriptorNative fileDescriptor)
        {
            return false;
        }

        [[nodiscard]] ReturnCode poll(FileDescriptor& loopFd, IntegerMilliseconds* actualTimeout) { return false; }
    };
};

SC::ReturnCode SC::Loop::wakeUpFromExternalThread() { return true; }
