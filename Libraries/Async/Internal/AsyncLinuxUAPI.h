// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include <stdint.h>

#if defined(__has_include)
#if __has_include(<linux/time_types.h>)
#define SC_ASYNC_HAS_LINUX_TIME_TYPES_UAPI 1
#endif
#if __has_include(<linux/io_uring.h>)
#define SC_ASYNC_HAS_LINUX_IO_URING_UAPI 1
#endif
#endif

#if !defined(SC_ASYNC_HAS_LINUX_TIME_TYPES_UAPI)
#define SC_ASYNC_HAS_LINUX_TIME_TYPES_UAPI 0
#endif
#if !defined(SC_ASYNC_HAS_LINUX_IO_URING_UAPI)
#define SC_ASYNC_HAS_LINUX_IO_URING_UAPI 0
#endif

#if SC_ASYNC_HAS_LINUX_IO_URING_UAPI
#if !SC_ASYNC_HAS_LINUX_TIME_TYPES_UAPI
#define UAPI_LINUX_IO_URING_H_SKIP_LINUX_TIME_TYPES_H 1
#endif
#include <linux/io_uring.h>
#endif

#if !SC_ASYNC_HAS_LINUX_TIME_TYPES_UAPI
struct __kernel_timespec
{
    int64_t   tv_sec;
    long long tv_nsec;
};
#endif

#if !SC_ASYNC_HAS_LINUX_IO_URING_UAPI
typedef uint8_t  __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;
typedef int32_t  __s32;
typedef int32_t  __kernel_rwf_t;

struct io_uring_sqe
{
    __u8  opcode;
    __u8  flags;
    __u16 ioprio;
    __s32 fd;
    union
    {
        __u64 off;
        __u64 addr2;
        struct
        {
            __u32 cmd_op;
            __u32 __pad1;
        };
    };
    union
    {
        __u64 addr;
        __u64 splice_off_in;
        struct
        {
            __u32 level;
            __u32 optname;
        };
    };
    __u32 len;
    union
    {
        __kernel_rwf_t rw_flags;
        __u32          fsync_flags;
        __u16          poll_events;
        __u32          poll32_events;
        __u32          sync_range_flags;
        __u32          msg_flags;
        __u32          timeout_flags;
        __u32          accept_flags;
        __u32          cancel_flags;
        __u32          open_flags;
        __u32          statx_flags;
        __u32          fadvise_advice;
        __u32          splice_flags;
        __u32          rename_flags;
        __u32          unlink_flags;
    };
    __u64 user_data;
    union
    {
        __u16 buf_index;
        __u16 buf_group;
    } __attribute__((packed));
    __u16 personality;
    union
    {
        __s32 splice_fd_in;
        __u32 file_index;
        __u32 optlen;
        struct
        {
            __u16 addr_len;
            __u16 __pad3[1];
        };
    };
    union
    {
        struct
        {
            __u64 addr3;
            __u64 __pad2[1];
        };
        __u64 optval;
        __u8  cmd[0];
    };
};

struct io_uring_cqe
{
    __u64 user_data;
    __s32 res;
    __u32 flags;
    __u64 big_cqe[];
};

struct io_sqring_offsets
{
    __u32 head;
    __u32 tail;
    __u32 ring_mask;
    __u32 ring_entries;
    __u32 flags;
    __u32 dropped;
    __u32 array;
    __u32 resv1;
    __u64 user_addr;
};

struct io_cqring_offsets
{
    __u32 head;
    __u32 tail;
    __u32 ring_mask;
    __u32 ring_entries;
    __u32 overflow;
    __u32 cqes;
    __u32 flags;
    __u32 resv1;
    __u64 user_addr;
};

struct io_uring_params
{
    __u32                    sq_entries;
    __u32                    cq_entries;
    __u32                    flags;
    __u32                    sq_thread_cpu;
    __u32                    sq_thread_idle;
    __u32                    features;
    __u32                    wq_fd;
    __u32                    resv[3];
    struct io_sqring_offsets sq_off;
    struct io_cqring_offsets cq_off;
};

enum io_uring_op
{
    IORING_OP_NOP,
    IORING_OP_READV,
    IORING_OP_WRITEV,
    IORING_OP_FSYNC,
    IORING_OP_READ_FIXED,
    IORING_OP_WRITE_FIXED,
    IORING_OP_POLL_ADD,
    IORING_OP_POLL_REMOVE,
    IORING_OP_SYNC_FILE_RANGE,
    IORING_OP_SENDMSG,
    IORING_OP_RECVMSG,
    IORING_OP_TIMEOUT,
    IORING_OP_TIMEOUT_REMOVE,
    IORING_OP_ACCEPT,
    IORING_OP_ASYNC_CANCEL,
    IORING_OP_LINK_TIMEOUT,
    IORING_OP_CONNECT,
    IORING_OP_FALLOCATE,
    IORING_OP_OPENAT,
    IORING_OP_CLOSE,
    IORING_OP_FILES_UPDATE,
    IORING_OP_STATX,
    IORING_OP_READ,
    IORING_OP_WRITE,
    IORING_OP_FADVISE,
    IORING_OP_MADVISE,
    IORING_OP_SEND,
    IORING_OP_RECV,
    IORING_OP_OPENAT2,
    IORING_OP_EPOLL_CTL,
    IORING_OP_SPLICE,
    IORING_OP_PROVIDE_BUFFERS,
    IORING_OP_REMOVE_BUFFERS,
    IORING_OP_TEE,
    IORING_OP_SHUTDOWN,
    IORING_OP_RENAMEAT,
    IORING_OP_UNLINKAT,
};

#define IORING_TIMEOUT_UPDATE (1U << 1)
#define IOSQE_IO_LINK         (1U << 2)

#define IORING_OFF_SQ_RING 0ULL
#define IORING_OFF_CQ_RING 0x8000000ULL
#define IORING_OFF_SQES    0x10000000ULL

#define IORING_SQ_CQ_OVERFLOW (1U << 1)
#define IORING_SQ_TASKRUN     (1U << 2)

#define IORING_ENTER_GETEVENTS (1U << 0)

#define IORING_FEAT_SINGLE_MMAP (1U << 0)
#endif
