#include <dlfcn.h>
#include <stdint.h>
#include <string.h>
#include <sys/uio.h> // for iovec
#include <unistd.h>

struct AsyncLinuxAPI
{
    void* liburingHandle = nullptr;

    bool isValid() const { return liburingHandle != nullptr; }

    void (*io_uring_queue_exit)(struct io_uring* ring)                                                     = nullptr;
    int (*io_uring_queue_init)(unsigned entries, struct io_uring* ring, unsigned flags)                    = nullptr;
    struct io_uring_sqe* (*io_uring_get_sqe)(struct io_uring* ring)                                        = nullptr;
    unsigned (*io_uring_peek_batch_cqe)(struct io_uring* ring, struct io_uring_cqe** cqes, unsigned count) = nullptr;
    int (*io_uring_submit)(struct io_uring* ring)                                                          = nullptr;
    int (*io_uring_submit_and_wait)(struct io_uring* ring, unsigned wait_nr)                               = nullptr;

    [[nodiscard]] bool init()
    {
        if (liburingHandle)
        {
            return true;
        }

        liburingHandle = ::dlopen("liburing.so", RTLD_NOW);
        if (liburingHandle == nullptr)
        {
            return false;
        }
        // clang-format off
        io_uring_queue_exit = reinterpret_cast<decltype(io_uring_queue_exit)>(::dlsym(liburingHandle, "io_uring_queue_exit"));
        io_uring_queue_init = reinterpret_cast<decltype(io_uring_queue_init)>(::dlsym(liburingHandle, "io_uring_queue_init"));
        io_uring_get_sqe = reinterpret_cast<decltype(io_uring_get_sqe)>(::dlsym(liburingHandle, "io_uring_get_sqe"));
        io_uring_peek_batch_cqe = reinterpret_cast<decltype(io_uring_peek_batch_cqe)>(::dlsym(liburingHandle, "io_uring_peek_batch_cqe"));
        io_uring_submit = reinterpret_cast<decltype(io_uring_submit)>(::dlsym(liburingHandle, "io_uring_submit"));
        io_uring_submit_and_wait = reinterpret_cast<decltype(io_uring_submit_and_wait)>(::dlsym(liburingHandle, "io_uring_submit_and_wait"));
        // clang-format on
        return true;
    }

    void close()
    {
        if (liburingHandle)
        {
            ::dlclose(liburingHandle);
        }
    }
};

// liburing/barrier.h includes <atomic> in C++ mode so this is my best bet to avoid
// giving up on the lovely `--nostdinc++` flag for just a few functions...
// It's ugly but it (hopefully) works.

#if SC_ASYNC_INCLUDE_LIBURING_HEADER

static inline uint32_t IO_URING_READ_ONCE(const uint32_t& var)
{
    uint32_t res;
    __atomic_load(&var, &res, __ATOMIC_RELAXED);
    return res;
}

static inline void IO_URING_WRITE_ONCE(uint32_t& var, uint32_t val)
{
    // std::atomic_store_explicit(reinterpret_cast<std::atomic<T>*>(&var), val, std::memory_order_relaxed);
    __atomic_store(&var, &val, __ATOMIC_RELAXED);
}

static inline void io_uring_smp_store_release(uint32_t* p, uint32_t v)
{
    // std::atomic_store_explicit(reinterpret_cast<std::atomic<T>*>(p), v, std::memory_order_release);
    __atomic_store(p, &v, __ATOMIC_RELEASE);
}

static inline void io_uring_smp_store_release(uint16_t* p, uint16_t v)
{
    // std::atomic_store_explicit(reinterpret_cast<std::atomic<T>*>(p), v, std::memory_order_release);
    __atomic_store(p, &v, __ATOMIC_RELEASE);
}

static inline uint32_t io_uring_smp_load_acquire(const uint32_t* value)
{
    // return std::atomic_load_explicit(reinterpret_cast<const std::atomic<T>*>(p), std::memory_order_acquire);
    uint32_t res;
    __atomic_load(value, &res, __ATOMIC_ACQUIRE);
    return res;
}
#define LIBURING_BARRIER_H
#include <liburing.h>

struct AsyncLinuxLibURingLoader : public AsyncLinuxAPI
{
    // clang-format off
    void (*io_uring_sqe_set_data)(struct io_uring_sqe* sqe, void* data) = nullptr;
    void*(*io_uring_cqe_get_data)(const struct io_uring_cqe* cqe) = nullptr;
    void (*io_uring_cq_advance)(struct io_uring* ring, unsigned nr) = nullptr;

    void (*io_uring_prep_timeout)(struct io_uring_sqe* sqe, struct __kernel_timespec* ts, unsigned count, unsigned flags) = nullptr;
    void (*io_uring_prep_timeout_remove)(struct io_uring_sqe* sqe, __u64 user_data, unsigned flags) = nullptr;
    void (*io_uring_prep_timeout_update)(struct io_uring_sqe *sqe, struct __kernel_timespec *ts, __u64 user_data, unsigned flags) = nullptr;
    void (*io_uring_prep_accept)(struct io_uring_sqe* sqe, int fd, struct sockaddr* addr, socklen_t* addrlen, int flags) = nullptr;
                                           
    void (*io_uring_prep_connect)(struct io_uring_sqe* sqe, int fd, const struct sockaddr* addr, socklen_t addrlen) = nullptr;
    void (*io_uring_prep_send)(struct io_uring_sqe* sqe, int sockfd, const void* buf, size_t len, int flags) = nullptr;
    void (*io_uring_prep_recv)(struct io_uring_sqe* sqe, int sockfd, void* buf, size_t len, int flags) = nullptr;
    void (*io_uring_prep_sendmsg)(struct io_uring_sqe* sqe, int sockfd, const struct msghdr* msg, unsigned flags) = nullptr;
    void (*io_uring_prep_recvmsg)(struct io_uring_sqe* sqe, int sockfd, struct msghdr* msg, unsigned flags) = nullptr;

    void (*io_uring_prep_close)(struct io_uring_sqe* sqe, int fd) = nullptr;

    void (*io_uring_prep_read)(struct io_uring_sqe* sqe, int fd, void* buf, unsigned nbytes, __u64 offset) = nullptr;
    void (*io_uring_prep_write)(struct io_uring_sqe* sqe, int fd, const void* buf, unsigned nbytes, __u64 offset) = nullptr;
    void (*io_uring_prep_writev)(struct io_uring_sqe* sqe, int fd, const iovec* vecs, unsigned nvecs, __u64 offset) = nullptr;

    void (*io_uring_prep_poll_add)(struct io_uring_sqe* sqe, int fd, unsigned poll_mask) = nullptr;
    void (*io_uring_prep_poll_remove)(struct io_uring_sqe* sqe, __u64 user_data) = nullptr;
    void (*io_uring_prep_cancel)(struct io_uring_sqe* sqe, void* user_data, int flags) = nullptr;
    void (*io_uring_prep_openat)(struct io_uring_sqe* sqe, int fd, const char* pathname, int flags, mode_t mode) = nullptr;

    // clang-format on
    AsyncLinuxLibURingLoader()
    {
        this->io_uring_sqe_set_data        = &::io_uring_sqe_set_data;
        this->io_uring_cqe_get_data        = &::io_uring_cqe_get_data;
        this->io_uring_cq_advance          = &::io_uring_cq_advance;
        this->io_uring_prep_timeout        = &::io_uring_prep_timeout;
        this->io_uring_prep_timeout_remove = &::io_uring_prep_timeout_remove;
        this->io_uring_prep_timeout_update = &::io_uring_prep_timeout_update;
        this->io_uring_prep_accept         = &::io_uring_prep_accept;
        this->io_uring_prep_connect        = &::io_uring_prep_connect;
        this->io_uring_prep_send           = &::io_uring_prep_send;
        this->io_uring_prep_recv           = &::io_uring_prep_recv;
        this->io_uring_prep_sendmsg        = &::io_uring_prep_sendmsg;
        this->io_uring_prep_recvmsg        = &::io_uring_prep_recvmsg;
        this->io_uring_prep_close          = &::io_uring_prep_close;
        this->io_uring_prep_read           = &::io_uring_prep_read;
        this->io_uring_prep_write          = &::io_uring_prep_write;
        this->io_uring_prep_writev         = &::io_uring_prep_writev;
        this->io_uring_prep_poll_add       = &::io_uring_prep_poll_add;
        this->io_uring_prep_poll_remove    = &::io_uring_prep_poll_remove;
        this->io_uring_prep_cancel         = &::io_uring_prep_cancel;
        this->io_uring_prep_openat         = &::io_uring_prep_openat;
    }
};

#else

// I don't like this, but liburing provides some functions only as static inlines (making it a kinda of semi-header only
// library). These functions are exported in a special build of the library (called liburing-ffi) but I've verified that
// this is not deployed on not so old systems (Ubuntu 22.04 doesn't have it for example). So to be compatible with older
// distros, while still avoiding to link at build time with liburing the only thing we can do is copying the very
// minimal amount of headers/struct for the functions used by the SC::Async library :-(
//
// TODO: Use the liburing-ffi supplied functions if they're available on the system

#include <linux/io_uring.h>   // io_uring
#include <linux/time_types.h> // __kernel_timespec

struct io_uring_sq
{
    unsigned* khead;
    unsigned* ktail;
    unsigned* kring_mask;
    unsigned* kring_entries;
    unsigned* kflags;
    unsigned* kdropped;
    unsigned* array;

    struct io_uring_sqe* sqes;

    unsigned sqe_head;
    unsigned sqe_tail;

    size_t ring_sz;
    void*  ring_ptr;

    unsigned pad[4];
};

struct io_uring_cq
{
    unsigned* khead;
    unsigned* ktail;
    unsigned* kring_mask;
    unsigned* kring_entries;
    unsigned* kflags;
    unsigned* koverflow;

    struct io_uring_cqe* cqes;

    size_t ring_sz;
    void*  ring_ptr;

    unsigned pad[4];
};

struct io_uring
{
    struct io_uring_sq sq;
    struct io_uring_cq cq;

    unsigned flags;
    int      ring_fd;

    unsigned features;
    unsigned pad[3];
};

struct AsyncLinuxLibURingLoader : public AsyncLinuxAPI
{
    static inline void io_uring_smp_store_release(uint32_t* p, uint32_t v)
    {
        // std::atomic_store_explicit(reinterpret_cast<std::atomic<T>*>(p), v, std::memory_order_release);
        __atomic_store(p, &v, __ATOMIC_RELEASE);
    }

    static inline void io_uring_sqe_set_data(struct io_uring_sqe* sqe, void* data)
    {
        sqe->user_data = reinterpret_cast<unsigned long>(data);
    }

    static inline void* io_uring_cqe_get_data(const struct io_uring_cqe* cqe)
    {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(cqe->user_data));
    }

    static inline void io_uring_cq_advance(struct io_uring* ring, unsigned nr)
    {
        if (nr != 0)
        {
            struct io_uring_cq* cq = &ring->cq;
            io_uring_smp_store_release(cq->khead, *cq->khead + nr);
        }
    }

    static inline void io_uring_prep_rw(int op, struct io_uring_sqe* sqe, int fd, const void* addr, unsigned len,
                                        __u64 offset)
    {
        memset(sqe, 0, sizeof(io_uring_sqe));
        sqe->opcode = (__u8)op;
        sqe->fd     = fd;
        sqe->off    = offset;
        sqe->addr   = (unsigned long)addr;
        sqe->len    = len;
    }

    static inline void io_uring_prep_timeout(struct io_uring_sqe* sqe, struct __kernel_timespec* ts, unsigned count,
                                             unsigned flags)
    {
        io_uring_prep_rw(IORING_OP_TIMEOUT, sqe, -1, ts, 1, count);
        sqe->timeout_flags = flags;
    }

    static inline void io_uring_prep_timeout_remove(struct io_uring_sqe* sqe, __u64 user_data, unsigned flags)
    {
        io_uring_prep_rw(IORING_OP_TIMEOUT_REMOVE, sqe, -1, (void*)(unsigned long)user_data, 0, 0);
        sqe->timeout_flags = flags;
    }

    static inline void io_uring_prep_timeout_update(struct io_uring_sqe* sqe, struct __kernel_timespec* ts,
                                                    __u64 user_data, unsigned flags)
    {
        io_uring_prep_rw(IORING_OP_TIMEOUT_REMOVE, sqe, -1, NULL, 0, (uintptr_t)ts);
        sqe->addr          = user_data;
        sqe->timeout_flags = flags | IORING_TIMEOUT_UPDATE;
    }

    static inline void io_uring_prep_accept(struct io_uring_sqe* sqe, int fd, struct sockaddr* addr, socklen_t* addrlen,
                                            int flags)
    {
        io_uring_prep_rw(IORING_OP_ACCEPT, sqe, fd, addr, 0, (__u64)(unsigned long)addrlen);
        sqe->accept_flags = (__u32)flags;
    }

    static inline void io_uring_prep_connect(struct io_uring_sqe* sqe, int fd, const struct sockaddr* addr,
                                             socklen_t addrlen)
    {
        io_uring_prep_rw(IORING_OP_CONNECT, sqe, fd, addr, 0, addrlen);
    }

    static inline void io_uring_prep_send(struct io_uring_sqe* sqe, int sockfd, const void* buf, size_t len, int flags)
    {
        io_uring_prep_rw(IORING_OP_SEND, sqe, sockfd, buf, (__u32)len, 0);
        sqe->msg_flags = (__u32)flags;
    }

    static inline void io_uring_prep_recv(struct io_uring_sqe* sqe, int sockfd, void* buf, size_t len, int flags)
    {
        io_uring_prep_rw(IORING_OP_RECV, sqe, sockfd, buf, (__u32)len, 0);
        sqe->msg_flags = (__u32)flags;
    }

    static inline void io_uring_prep_close(struct io_uring_sqe* sqe, int fd)
    {
        io_uring_prep_rw(IORING_OP_CLOSE, sqe, fd, NULL, 0, 0);
    }

    static inline void io_uring_prep_read(struct io_uring_sqe* sqe, int fd, void* buf, unsigned nbytes, __u64 offset)
    {
        io_uring_prep_rw(IORING_OP_READ, sqe, fd, buf, nbytes, offset);
    }

    static inline void io_uring_prep_write(struct io_uring_sqe* sqe, int fd, const void* buf, unsigned nbytes,
                                           __u64 offset)
    {
        io_uring_prep_rw(IORING_OP_WRITE, sqe, fd, buf, nbytes, offset);
    }

    static inline void io_uring_prep_writev(struct io_uring_sqe* sqe, int fd, const iovec* vecs, unsigned nvecs,
                                            __u64 offset)
    {
        io_uring_prep_rw(IORING_OP_WRITEV, sqe, fd, vecs, nvecs, offset);
    }

    static inline unsigned static__io_uring_prep_poll_mask(unsigned poll_mask)
    {
#if __BYTE_ORDER == __BIG_ENDIAN
        poll_mask = __swahw32(poll_mask);
#endif
        return poll_mask;
    }

    static inline void io_uring_prep_poll_add(struct io_uring_sqe* sqe, int fd, unsigned poll_mask)
    {
        io_uring_prep_rw(IORING_OP_POLL_ADD, sqe, fd, NULL, 0, 0);
        sqe->poll32_events = static__io_uring_prep_poll_mask(poll_mask);
    }

    static inline void io_uring_prep_poll_remove(struct io_uring_sqe* sqe, __u64 user_data)
    {
        io_uring_prep_rw(IORING_OP_POLL_REMOVE, sqe, -1, (const void*)user_data, 0, 0);
    }

    static inline void io_uring_prep_cancel(struct io_uring_sqe* sqe, const void* user_data, int flags)
    {
        io_uring_prep_rw(IORING_OP_ASYNC_CANCEL, sqe, -1, user_data, 0, 0);
        sqe->cancel_flags = (__u32)flags;
    }

    static inline void io_uring_prep_openat(struct io_uring_sqe* sqe, int dfd, const char* pathname, int flags,
                                            mode_t mode)
    {
        io_uring_prep_rw(IORING_OP_OPENAT, sqe, dfd, pathname, mode, 0);
        sqe->open_flags = flags;
    }

    static inline void io_uring_prep_sendmsg(struct io_uring_sqe* sqe, int sockfd, const struct msghdr* msg, int flags)
    {
        io_uring_prep_rw(IORING_OP_SENDMSG, sqe, sockfd, msg, 1, 0);
        sqe->msg_flags = (__u32)flags;
    }

    static inline void io_uring_prep_recvmsg(struct io_uring_sqe* sqe, int sockfd, struct msghdr* msg, int flags)
    {
        io_uring_prep_rw(IORING_OP_RECVMSG, sqe, sockfd, msg, 1, 0);
        sqe->msg_flags = (__u32)flags;
    }
};

#endif
