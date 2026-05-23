// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../../Async/Internal/AsyncLinuxUAPI.h"
#include "../../Foundation/Compiler.h"

#include <errno.h>
#include <fcntl.h> // for AT_FDCWD
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/uio.h> // for iovec
#include <unistd.h>

struct AsyncLinuxIOUringSQ
{
    unsigned*     khead           = nullptr;
    unsigned*     ktail           = nullptr;
    unsigned*     kringMask       = nullptr;
    unsigned*     kringEntries    = nullptr;
    unsigned*     kflags          = nullptr;
    unsigned*     kdropped        = nullptr;
    unsigned*     array           = nullptr;
    io_uring_sqe* sqes            = nullptr;
    unsigned      sqeHead         = 0;
    unsigned      sqeTail         = 0;
    size_t        ringSize        = 0;
    void*         ringPointer     = nullptr;
    unsigned      ringMask        = 0;
    unsigned      ringEntries     = 0;
    size_t        submissionsSize = 0;
};

struct AsyncLinuxIOUringCQ
{
    unsigned*     khead        = nullptr;
    unsigned*     ktail        = nullptr;
    unsigned*     kringMask    = nullptr;
    unsigned*     kringEntries = nullptr;
    unsigned*     kflags       = nullptr;
    unsigned*     koverflow    = nullptr;
    io_uring_cqe* cqes         = nullptr;
    size_t        ringSize     = 0;
    void*         ringPointer  = nullptr;
    unsigned      ringMask     = 0;
    unsigned      ringEntries  = 0;
};

struct AsyncLinuxIOUring
{
    AsyncLinuxIOUringSQ sq;
    AsyncLinuxIOUringCQ cq;

    unsigned flags      = 0;
    unsigned features   = 0;
    int      ringFd     = -1;
    int      reservedFd = -1;

    static bool probe()
    {
#if SC_COMPILER_FILC
        return false;
#else
        AsyncLinuxIOUring ring;
        const int         res = ring.create(2);
        if (res < 0)
        {
            return false;
        }
        ring.close();
        return true;
#endif
    }

    int create(unsigned entries)
    {
#if SC_COMPILER_FILC
        (void)entries;
        return -ENOSYS;
#else
        close();

        io_uring_params params;
        memset(&params, 0, sizeof(params));

        const int fd = static_cast<int>(::syscall(SYS_io_uring_setup, entries, &params));
        if (fd < 0)
        {
            return -errno;
        }

        ringFd = normalizeRingFileDescriptor(fd);
        if (ringFd < 0)
        {
            const int error = ringFd;
            return error;
        }
        if (::fcntl(ringFd, F_SETFD, FD_CLOEXEC) != 0)
        {
            const int error = -errno;
            close();
            return error;
        }
        const int mapResult = mapRings(params);
        if (mapResult < 0)
        {
            close();
            return mapResult;
        }

        setupRingPointers(params);

        for (unsigned idx = 0; idx < sq.ringEntries; ++idx)
        {
            sq.array[idx] = idx;
        }

        flags    = params.flags;
        features = params.features;
        return 0;
#endif
    }

    void close()
    {
        if (sq.sqes != nullptr)
        {
            ::munmap(sq.sqes, sq.submissionsSize);
        }
        if (sq.ringPointer != nullptr)
        {
            ::munmap(sq.ringPointer, sq.ringSize);
        }
        if (cq.ringPointer != nullptr and cq.ringPointer != sq.ringPointer)
        {
            ::munmap(cq.ringPointer, cq.ringSize);
        }
        if (ringFd >= 0)
        {
            ::close(ringFd);
        }
        if (reservedFd >= 0)
        {
            ::close(reservedFd);
        }
        memset(this, 0, sizeof(*this));
        ringFd     = -1;
        reservedFd = -1;
    }

    bool isValid() const { return ringFd >= 0; }

    io_uring_sqe* getSubmission()
    {
        const unsigned head = loadSQHead();
        const unsigned tail = sq.sqeTail;
        if (tail - head >= sq.ringEntries)
        {
            return nullptr;
        }

        io_uring_sqe* submission = &sq.sqes[tail & sq.ringMask];
        sq.sqeTail               = tail + 1;
        memset(submission, 0, sizeof(*submission));
        return submission;
    }

    unsigned peekBatchCompletions(io_uring_cqe** completions, unsigned count)
    {
        const unsigned ready = loadAcquire(cq.ktail) - *cq.khead;
        if (ready == 0)
        {
            if (hasPendingCQOverflow())
            {
                (void)enter(0, 0, IORING_ENTER_GETEVENTS);
            }
            else
            {
                return 0;
            }
        }

        const unsigned available = loadAcquire(cq.ktail) - *cq.khead;
        if (available == 0)
        {
            return 0;
        }

        const unsigned toCopy = available < count ? available : count;
        unsigned       head   = *cq.khead;
        for (unsigned idx = 0; idx < toCopy; ++idx)
        {
            completions[idx] = &cq.cqes[(head + idx) & cq.ringMask];
        }
        return toCopy;
    }

    void advanceCompletions(unsigned count)
    {
        if (count != 0)
        {
            storeRelease(cq.khead, *cq.khead + count);
        }
    }

    int submit() { return submitAndWait(0); }

    int submitAndWait(unsigned waitNumber)
    {
        const unsigned submitted  = flushSubmissions();
        unsigned       enterFlags = 0;
        if (waitNumber != 0 or hasPendingCQOverflow())
        {
            enterFlags |= IORING_ENTER_GETEVENTS;
        }
        if (submitted == 0 and enterFlags == 0)
        {
            return 0;
        }
        return enter(submitted, waitNumber, enterFlags);
    }

    static void setData(io_uring_sqe* sqe, void* data) { sqe->user_data = reinterpret_cast<uintptr_t>(data); }

    static void* getData(const io_uring_cqe* cqe)
    {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(cqe->user_data));
    }

    static void prepTimeout(io_uring_sqe* sqe, __kernel_timespec* ts, unsigned count, unsigned timeoutFlags)
    {
        prepReadWrite(IORING_OP_TIMEOUT, sqe, -1, ts, 1, count);
        sqe->timeout_flags = timeoutFlags;
    }

    static void prepTimeoutRemove(io_uring_sqe* sqe, __u64 userData, unsigned timeoutFlags)
    {
        prepReadWrite(IORING_OP_TIMEOUT_REMOVE, sqe, -1, reinterpret_cast<void*>(static_cast<uintptr_t>(userData)), 0,
                      0);
        sqe->timeout_flags = timeoutFlags;
    }

    static void prepTimeoutUpdate(io_uring_sqe* sqe, __kernel_timespec* ts, __u64 userData, unsigned timeoutFlags)
    {
        prepReadWrite(IORING_OP_TIMEOUT_REMOVE, sqe, -1, nullptr, 0, reinterpret_cast<uintptr_t>(ts));
        sqe->addr          = userData;
        sqe->timeout_flags = timeoutFlags | IORING_TIMEOUT_UPDATE;
    }

    static void prepAccept(io_uring_sqe* sqe, int fd, struct sockaddr* addr, socklen_t* addrlen, int acceptFlags)
    {
        prepReadWrite(IORING_OP_ACCEPT, sqe, fd, addr, 0, reinterpret_cast<uintptr_t>(addrlen));
        sqe->accept_flags = static_cast<__u32>(acceptFlags);
    }

    static void prepConnect(io_uring_sqe* sqe, int fd, const struct sockaddr* addr, socklen_t addrlen)
    {
        prepReadWrite(IORING_OP_CONNECT, sqe, fd, addr, 0, addrlen);
    }

    static void prepSend(io_uring_sqe* sqe, int fd, const void* buf, size_t len, int sendFlags)
    {
        prepReadWrite(IORING_OP_SEND, sqe, fd, buf, static_cast<__u32>(len), 0);
        sqe->msg_flags = static_cast<__u32>(sendFlags);
    }

    static void prepRecv(io_uring_sqe* sqe, int fd, void* buf, size_t len, int recvFlags)
    {
        prepReadWrite(IORING_OP_RECV, sqe, fd, buf, static_cast<__u32>(len), 0);
        sqe->msg_flags = static_cast<__u32>(recvFlags);
    }

    static void prepSendMsg(io_uring_sqe* sqe, int fd, const msghdr* msg, int sendFlags)
    {
        prepReadWrite(IORING_OP_SENDMSG, sqe, fd, msg, 1, 0);
        sqe->msg_flags = static_cast<__u32>(sendFlags);
    }

    static void prepRecvMsg(io_uring_sqe* sqe, int fd, msghdr* msg, int recvFlags)
    {
        prepReadWrite(IORING_OP_RECVMSG, sqe, fd, msg, 1, 0);
        sqe->msg_flags = static_cast<__u32>(recvFlags);
    }

    static void prepClose(io_uring_sqe* sqe, int fd) { prepReadWrite(IORING_OP_CLOSE, sqe, fd, nullptr, 0, 0); }

    static void prepRead(io_uring_sqe* sqe, int fd, void* buf, unsigned nbytes, __u64 offset)
    {
        prepReadWrite(IORING_OP_READ, sqe, fd, buf, nbytes, offset);
    }

    static void prepReadv(io_uring_sqe* sqe, int fd, const iovec* vecs, unsigned nvecs, __u64 offset)
    {
        prepReadWrite(IORING_OP_READV, sqe, fd, vecs, nvecs, offset);
    }

    static void prepWrite(io_uring_sqe* sqe, int fd, const void* buf, unsigned nbytes, __u64 offset)
    {
        prepReadWrite(IORING_OP_WRITE, sqe, fd, buf, nbytes, offset);
    }

    static void prepWritev(io_uring_sqe* sqe, int fd, const iovec* vecs, unsigned nvecs, __u64 offset)
    {
        prepReadWrite(IORING_OP_WRITEV, sqe, fd, vecs, nvecs, offset);
    }

    static void prepRenameAt(io_uring_sqe* sqe, int olddfd, const char* oldpath, int newdfd, const char* newpath,
                             unsigned renameFlags)
    {
        prepReadWrite(IORING_OP_RENAMEAT, sqe, olddfd, oldpath, static_cast<__u32>(newdfd),
                      reinterpret_cast<uintptr_t>(newpath));
        sqe->rename_flags = static_cast<__u32>(renameFlags);
    }

    static void prepRename(io_uring_sqe* sqe, const char* oldpath, const char* newpath)
    {
        prepRenameAt(sqe, AT_FDCWD, oldpath, AT_FDCWD, newpath, 0);
    }

    static void prepUnlinkAt(io_uring_sqe* sqe, int dfd, const char* pathname, unsigned unlinkFlags)
    {
        prepReadWrite(IORING_OP_UNLINKAT, sqe, dfd, pathname, 0, 0);
        sqe->unlink_flags = static_cast<__u32>(unlinkFlags);
    }

    static void prepUnlink(io_uring_sqe* sqe, const char* pathname, unsigned unlinkFlags)
    {
        prepUnlinkAt(sqe, AT_FDCWD, pathname, unlinkFlags);
    }

    static void prepPollAdd(io_uring_sqe* sqe, int fd, unsigned pollMask)
    {
        prepReadWrite(IORING_OP_POLL_ADD, sqe, fd, nullptr, 0, 0);
        sqe->poll32_events = prepPollMask(pollMask);
    }

    static void prepPollRemove(io_uring_sqe* sqe, __u64 userData)
    {
        prepReadWrite(IORING_OP_POLL_REMOVE, sqe, -1, reinterpret_cast<const void*>(static_cast<uintptr_t>(userData)),
                      0, 0);
    }

    static void prepCancel(io_uring_sqe* sqe, const void* userData, int cancelFlags)
    {
        prepReadWrite(IORING_OP_ASYNC_CANCEL, sqe, -1, userData, 0, 0);
        sqe->cancel_flags = static_cast<__u32>(cancelFlags);
    }

    static void prepOpenAt(io_uring_sqe* sqe, int dfd, const char* pathname, int openFlags, mode_t mode)
    {
        prepReadWrite(IORING_OP_OPENAT, sqe, dfd, pathname, mode, 0);
        sqe->open_flags = static_cast<__u32>(openFlags);
    }

    static void prepSplice(io_uring_sqe* sqe, int fdIn, int64_t offIn, int fdOut, int64_t offOut, unsigned nbytes,
                           unsigned spliceFlags)
    {
        prepReadWrite(IORING_OP_SPLICE, sqe, fdOut, nullptr, nbytes, static_cast<__u64>(offOut));
        sqe->splice_off_in = static_cast<__u64>(offIn);
        sqe->splice_fd_in  = fdIn;
        sqe->splice_flags  = spliceFlags;
    }

  private:
    int mapRings(const io_uring_params& params)
    {
        sq.ringSize = params.sq_off.array + params.sq_entries * sizeof(unsigned);
        cq.ringSize = params.cq_off.cqes + params.cq_entries * sizeof(io_uring_cqe);

        if (params.features & IORING_FEAT_SINGLE_MMAP)
        {
            if (cq.ringSize > sq.ringSize)
            {
                sq.ringSize = cq.ringSize;
            }
            cq.ringSize = sq.ringSize;
        }

        sq.ringPointer = mapRing(sq.ringSize, IORING_OFF_SQ_RING);
        if (sq.ringPointer == nullptr)
        {
            return -errno;
        }

        if (params.features & IORING_FEAT_SINGLE_MMAP)
        {
            cq.ringPointer = sq.ringPointer;
        }
        else
        {
            cq.ringPointer = mapRing(cq.ringSize, IORING_OFF_CQ_RING);
            if (cq.ringPointer == nullptr)
            {
                return -errno;
            }
        }

        sq.submissionsSize = params.sq_entries * sizeof(io_uring_sqe);
        sq.sqes            = static_cast<io_uring_sqe*>(mapRing(sq.submissionsSize, IORING_OFF_SQES));
        if (sq.sqes == nullptr)
        {
            return -errno;
        }
        return 0;
    }

    void* mapRing(size_t size, unsigned long offset)
    {
        void* mapping = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, ringFd, offset);
        return mapping == MAP_FAILED ? nullptr : mapping;
    }

    int normalizeRingFileDescriptor(int fd)
    {
        if (fd >= 3)
        {
            return fd;
        }

        int duplicatedFd;
        do
        {
#if defined(F_DUPFD_CLOEXEC)
            duplicatedFd = ::fcntl(fd, F_DUPFD_CLOEXEC, 3);
#else
            duplicatedFd = ::fcntl(fd, F_DUPFD, 3);
#endif
        } while (duplicatedFd < 0 and errno == EINTR);
        const int error = errno;
        if (duplicatedFd < 0)
        {
            ::close(fd);
            return -error;
        }
        ::close(fd);
        reservedFd = ::open("/dev/null", O_RDWR | O_CLOEXEC);
        if (reservedFd < 0)
        {
            const int openError = -errno;
            ::close(duplicatedFd);
            return openError;
        }
#if !defined(F_DUPFD_CLOEXEC)
        if (::fcntl(duplicatedFd, F_SETFD, FD_CLOEXEC) != 0)
        {
            const int closeError = -errno;
            ::close(duplicatedFd);
            ::close(reservedFd);
            reservedFd = -1;
            return closeError;
        }
#endif
        return duplicatedFd;
    }

    void setupRingPointers(const io_uring_params& params)
    {
        char* sqRing = static_cast<char*>(sq.ringPointer);
        char* cqRing = static_cast<char*>(cq.ringPointer);

        sq.khead        = reinterpret_cast<unsigned*>(sqRing + params.sq_off.head);
        sq.ktail        = reinterpret_cast<unsigned*>(sqRing + params.sq_off.tail);
        sq.kringMask    = reinterpret_cast<unsigned*>(sqRing + params.sq_off.ring_mask);
        sq.kringEntries = reinterpret_cast<unsigned*>(sqRing + params.sq_off.ring_entries);
        sq.kflags       = reinterpret_cast<unsigned*>(sqRing + params.sq_off.flags);
        sq.kdropped     = reinterpret_cast<unsigned*>(sqRing + params.sq_off.dropped);
        sq.array        = reinterpret_cast<unsigned*>(sqRing + params.sq_off.array);

        cq.khead        = reinterpret_cast<unsigned*>(cqRing + params.cq_off.head);
        cq.ktail        = reinterpret_cast<unsigned*>(cqRing + params.cq_off.tail);
        cq.kringMask    = reinterpret_cast<unsigned*>(cqRing + params.cq_off.ring_mask);
        cq.kringEntries = reinterpret_cast<unsigned*>(cqRing + params.cq_off.ring_entries);
        cq.koverflow    = reinterpret_cast<unsigned*>(cqRing + params.cq_off.overflow);
        cq.cqes         = reinterpret_cast<io_uring_cqe*>(cqRing + params.cq_off.cqes);
        if (params.cq_off.flags != 0)
        {
            cq.kflags = reinterpret_cast<unsigned*>(cqRing + params.cq_off.flags);
        }

        sq.ringMask    = *sq.kringMask;
        sq.ringEntries = *sq.kringEntries;
        cq.ringMask    = *cq.kringMask;
        cq.ringEntries = *cq.kringEntries;
    }

    unsigned loadSQHead() const { return *sq.khead; }

    bool hasPendingCQOverflow() const { return relaxedLoad(sq.kflags) & (IORING_SQ_CQ_OVERFLOW | IORING_SQ_TASKRUN); }

    unsigned flushSubmissions()
    {
        const unsigned tail = sq.sqeTail;
        if (sq.sqeHead != tail)
        {
            unsigned head = sq.sqeHead;
            while (head != tail)
            {
                sq.array[head & sq.ringMask] = head & sq.ringMask;
                head++;
            }
            sq.sqeHead = tail;
            storeRelease(sq.ktail, tail);
        }
        return tail - relaxedLoad(sq.khead);
    }

    int enter(unsigned toSubmit, unsigned minComplete, unsigned enterFlags)
    {
        const int res = static_cast<int>(
            ::syscall(SYS_io_uring_enter, ringFd, toSubmit, minComplete, enterFlags, nullptr, static_cast<size_t>(0)));
        return res < 0 ? -errno : res;
    }

    static void prepReadWrite(int op, io_uring_sqe* sqe, int fd, const void* addr, unsigned len, __u64 offset)
    {
        memset(sqe, 0, sizeof(io_uring_sqe));
        sqe->opcode = static_cast<__u8>(op);
        sqe->fd     = fd;
        sqe->off    = offset;
        sqe->addr   = reinterpret_cast<uintptr_t>(addr);
        sqe->len    = len;
    }

    static unsigned prepPollMask(unsigned pollMask)
    {
#if defined(__BYTE_ORDER__) and __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        pollMask = ((pollMask & 0x0000ffffU) << 16) | ((pollMask & 0xffff0000U) >> 16);
#endif
        return pollMask;
    }

    static unsigned loadAcquire(const unsigned* value)
    {
        unsigned res;
        __atomic_load(value, &res, __ATOMIC_ACQUIRE);
        return res;
    }

    static unsigned relaxedLoad(const unsigned* value)
    {
        unsigned res;
        __atomic_load(value, &res, __ATOMIC_RELAXED);
        return res;
    }

    static void storeRelease(unsigned* value, unsigned newValue) { __atomic_store(value, &newValue, __ATOMIC_RELEASE); }
};
