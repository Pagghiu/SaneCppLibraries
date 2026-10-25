// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <BaseTsd.h>
#include <signal.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#ifndef SIGHUP
#define SIGHUP 1
#endif
#ifndef SIGQUIT
#define SIGQUIT 3
#endif
#ifndef SIGKILL
#define SIGKILL 9
#endif
#ifndef SIGWINCH
#define SIGWINCH 28
#endif
#if defined(NSIG) && NSIG <= SIGWINCH
#undef NSIG
#endif
#ifndef NSIG
#define NSIG (SIGWINCH + 1)
#endif
#ifndef SIGABRT_COMPAT
#define SIGABRT_COMPAT 6
#endif
    typedef SSIZE_T ssize_t;
    typedef int     socklen_t;
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#ifndef UV_EXTERN
#define UV_EXTERN
#endif

#ifndef UV_API
#define UV_API UV_EXTERN
#endif

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4201) // nameless struct/union for source-compatible field access
#endif

#if !defined(_WIN32)
#include <errno.h>
#define UV__ERR(x)       (-(x))
#define UV_E2BIG         UV__ERR(E2BIG)
#define UV_EADDRINUSE    UV__ERR(EADDRINUSE)
#define UV_EADDRNOTAVAIL UV__ERR(EADDRNOTAVAIL)
#define UV_EACCES        UV__ERR(EACCES)
#define UV_EINVAL        UV__ERR(EINVAL)
#define UV_EAFNOSUPPORT  UV__ERR(EAFNOSUPPORT)
#define UV_EAGAIN        UV__ERR(EAGAIN)
#define UV_EALREADY      UV__ERR(EALREADY)
#define UV_EBADF         UV__ERR(EBADF)
#define UV_EBUSY         UV__ERR(EBUSY)
#define UV_ECANCELED     UV__ERR(ECANCELED)
#if defined(ECONNABORTED)
#define UV_ECONNABORTED UV__ERR(ECONNABORTED)
#else
#define UV_ECONNABORTED (-4079)
#endif
#define UV_ECONNREFUSED UV__ERR(ECONNREFUSED)
#define UV_ECONNRESET   UV__ERR(ECONNRESET)
#define UV_EDESTADDRREQ UV__ERR(EDESTADDRREQ)
#define UV_EEXIST       UV__ERR(EEXIST)
#define UV_EFAULT       UV__ERR(EFAULT)
#define UV_EHOSTUNREACH UV__ERR(EHOSTUNREACH)
#define UV_EISCONN      UV__ERR(EISCONN)
#define UV_EMSGSIZE     UV__ERR(EMSGSIZE)
#define UV_ENOMEM       UV__ERR(ENOMEM)
#define UV_ENODEV       UV__ERR(ENODEV)
#define UV_ENOENT       UV__ERR(ENOENT)
#define UV_ENOEXEC      UV__ERR(ENOEXEC)
#define UV_ENOSPC       UV__ERR(ENOSPC)
#define UV_ENOSYS       UV__ERR(ENOSYS)
#define UV_ENOBUFS      UV__ERR(ENOBUFS)
#define UV_ENETUNREACH  UV__ERR(ENETUNREACH)
#define UV_ENOTCONN     UV__ERR(ENOTCONN)
#define UV_ENOTSUP      UV__ERR(ENOTSUP)
#define UV_EPERM        UV__ERR(EPERM)
#define UV_EPIPE        UV__ERR(EPIPE)
#define UV_ERANGE       UV__ERR(ERANGE)
#define UV_ETIMEDOUT    UV__ERR(ETIMEDOUT)
#else
// Keep Windows values aligned with libuv's uv/errno.h mapping.
#define UV_E2BIG         (-4093)
#define UV_EADDRINUSE    (-4091)
#define UV_EADDRNOTAVAIL (-4090)
#define UV_EACCES        (-4092)
#define UV_EINVAL        (-4071)
#define UV_EAFNOSUPPORT  (-4089)
#define UV_EAGAIN        (-4088)
#define UV_EALREADY      (-4084)
#define UV_EBADF         (-4083)
#define UV_EBUSY         (-4082)
#define UV_ECANCELED     (-4081)
#define UV_ECONNABORTED  (-4079)
#define UV_ECONNREFUSED  (-4078)
#define UV_ECONNRESET    (-4077)
#define UV_EDESTADDRREQ  (-4076)
#define UV_EEXIST        (-4075)
#define UV_EFAULT        (-4085)
#define UV_EHOSTUNREACH  (-4073)
#define UV_EISCONN       (-4069)
#define UV_EMSGSIZE      (-4065)
#define UV_ENOMEM        (-4057)
#define UV_ENODEV        (-4059)
#define UV_ENOENT        (-4058)
#define UV_ENOEXEC       (-4022)
#define UV_ENOSPC        (-4055)
#define UV_ENOSYS        (-4054)
#define UV_ENOBUFS       (-4060)
#define UV_ENETUNREACH   (-4062)
#define UV_ENOTCONN      (-4053)
#define UV_ENOTSUP       (-4049)
#define UV_EPERM         (-4048)
#define UV_EPIPE         (-4046)
#define UV_ERANGE        (-4034)
#define UV_ETIMEDOUT     (-4039)
#endif
#define UV_EOF (-4095)

#define UV_MAXHOSTNAMESIZE 256
#define UV_IF_NAMESIZE     256

    struct uv__queue
    {
        struct uv__queue* next;
        struct uv__queue* prev;
    };

    struct uv__work;

    typedef struct uv_loop_s              uv_loop_t;
    typedef struct uv_handle_s            uv_handle_t;
    typedef struct uv_stream_s            uv_stream_t;
    typedef struct uv_tcp_s               uv_tcp_t;
    typedef struct uv_udp_s               uv_udp_t;
    typedef struct uv_pipe_s              uv_pipe_t;
    typedef struct uv_tty_s               uv_tty_t;
    typedef struct uv_timer_s             uv_timer_t;
    typedef struct uv_prepare_s           uv_prepare_t;
    typedef struct uv_check_s             uv_check_t;
    typedef struct uv_idle_s              uv_idle_t;
    typedef struct uv_async_s             uv_async_t;
    typedef struct uv_signal_s            uv_signal_t;
    typedef struct uv_req_s               uv_req_t;
    typedef struct uv_write_s             uv_write_t;
    typedef struct uv_shutdown_s          uv_shutdown_t;
    typedef struct uv_connect_s           uv_connect_t;
    typedef struct uv_udp_send_s          uv_udp_send_t;
    typedef struct uv_random_s            uv_random_t;
    typedef struct uv_getaddrinfo_s       uv_getaddrinfo_t;
    typedef struct uv_getnameinfo_s       uv_getnameinfo_t;
    typedef struct uv_work_s              uv_work_t;
    typedef struct uv_cpu_info_s          uv_cpu_info_t;
    typedef struct uv_interface_address_s uv_interface_address_t;
    typedef struct uv_env_item_s          uv_env_item_t;
    typedef struct uv_passwd_s            uv_passwd_t;
    typedef struct uv_group_s             uv_group_t;
    typedef struct uv_lib_s               uv_lib_t;
    typedef struct uv_fs_poll_s           uv_fs_poll_t;
    typedef struct uv_fs_s                uv_fs_t;
    typedef struct uv_metrics_s           uv_metrics_t;
    typedef struct uv_mutex_s             uv_mutex_t;
    typedef struct uv_cond_s              uv_cond_t;
    typedef struct uv_rwlock_s            uv_rwlock_t;
    typedef struct uv_sem_s               uv_sem_t;
    typedef struct uv_barrier_s           uv_barrier_t;
    typedef struct uv_thread_options_s    uv_thread_options_t;
    typedef struct uv__dirent_s           uv__dirent_t;
#if defined(_WIN32)
    typedef struct _stat uv_stat_t;
#else
typedef struct stat uv_stat_t;
#endif
    typedef int uv_pid_t;
    typedef int uv_file;
#if defined(_WIN32)
    typedef SOCKET uv_os_fd_t;
    typedef SOCKET uv_os_sock_t;
#else
typedef int uv_os_fd_t;
typedef int uv_os_sock_t;
#endif
    typedef unsigned long uv_uid_t;
    typedef unsigned long uv_gid_t;

    typedef enum
    {
        UV_UNKNOWN_HANDLE = 0,
        UV_ASYNC,
        UV_CHECK,
        UV_FS_EVENT,
        UV_FS_POLL,
        UV_HANDLE,
        UV_IDLE,
        UV_NAMED_PIPE,
        UV_POLL,
        UV_PREPARE,
        UV_PROCESS,
        UV_STREAM,
        UV_TCP,
        UV_TIMER,
        UV_TTY,
        UV_UDP,
        UV_SIGNAL,
        UV_FILE,
        UV_HANDLE_TYPE_MAX
    } uv_handle_type;

    typedef enum
    {
        UV_UNKNOWN_REQ = 0,
        UV_REQ,
        UV_CONNECT,
        UV_WRITE,
        UV_SHUTDOWN,
        UV_UDP_SEND,
        UV_FS,
        UV_WORK,
        UV_GETADDRINFO,
        UV_GETNAMEINFO,
        UV_RANDOM,
        UV_REQ_TYPE_MAX
    } uv_req_type;

    typedef enum
    {
        UV_FS_UNKNOWN = -1,
        UV_FS_STAT
    } uv_fs_type;

    typedef enum
    {
        UV_RUN_DEFAULT = 0,
        UV_RUN_ONCE,
        UV_RUN_NOWAIT
    } uv_run_mode;

    typedef enum
    {
        UV_LOOP_BLOCK_SIGNAL = 0,
    } uv_loop_option;

    typedef enum
    {
        UV_CLOCK_MONOTONIC,
        UV_CLOCK_REALTIME,
    } uv_clock_id;

    typedef enum
    {
        UV_TTY_MODE_NORMAL,
        UV_TTY_MODE_RAW,
        UV_TTY_MODE_IO
    } uv_tty_mode_t;

    typedef enum
    {
        UV_DIRENT_UNKNOWN,
        UV_DIRENT_FILE,
        UV_DIRENT_DIR,
        UV_DIRENT_LINK,
        UV_DIRENT_FIFO,
        UV_DIRENT_SOCKET,
        UV_DIRENT_CHAR,
        UV_DIRENT_BLOCK
    } uv_dirent_type_t;

    typedef struct
    {
        long tv_sec;
        long tv_nsec;
    } uv_timespec_t;

    typedef struct
    {
        int64_t tv_sec;
        int32_t tv_nsec;
    } uv_timespec64_t;

    typedef struct
    {
        long tv_sec;
        long tv_usec;
    } uv_timeval_t;

    typedef struct
    {
        int64_t tv_sec;
        int32_t tv_usec;
    } uv_timeval64_t;

    typedef struct
    {
        char sysname[256];
        char release[256];
        char version[256];
        char machine[256];
    } uv_utsname_t;

    enum uv_tcp_flags
    {
        UV_TCP_IPV6ONLY  = 1,
        UV_TCP_REUSEPORT = 2,
    };

    enum uv_udp_flags
    {
        UV_UDP_IPV6ONLY      = 1,
        UV_UDP_PARTIAL       = 2,
        UV_UDP_REUSEADDR     = 4,
        UV_UDP_MMSG_CHUNK    = 8,
        UV_UDP_MMSG_FREE     = 16,
        UV_UDP_LINUX_RECVERR = 32,
        UV_UDP_REUSEPORT     = 64,
        UV_UDP_RECVMMSG      = 256,
    };

    typedef enum
    {
        UV_LEAVE_GROUP = 0,
        UV_JOIN_GROUP
    } uv_membership;

    typedef struct uv_buf_t
    {
        char*        base;
        unsigned int len;
    } uv_buf_t;

    typedef struct uv_interface_address_socket_s
    {
        union
        {
            struct sockaddr_in  address4;
            struct sockaddr_in6 address6;
            struct sockaddr_in  netmask4;
            struct sockaddr_in6 netmask6;
        };
    } uv_interface_address_socket_t;

    struct uv_cpu_times_s
    {
        uint64_t user;
        uint64_t nice;
        uint64_t sys;
        uint64_t idle;
        uint64_t irq;
    };

    struct uv_cpu_info_s
    {
        char*                 model;
        int                   speed;
        struct uv_cpu_times_s cpu_times;
    };

    struct uv_interface_address_s
    {
        char*                         name;
        char                          phys_addr[6];
        int                           is_internal;
        uv_interface_address_socket_t address;
        uv_interface_address_socket_t netmask;
    };

    struct uv_env_item_s
    {
        char* name;
        char* value;
    };

    struct uv_passwd_s
    {
        char*         username;
        unsigned long uid;
        unsigned long gid;
        char*         shell;
        char*         homedir;
    };

    struct uv_group_s
    {
        char*         groupname;
        unsigned long gid;
        char**        members;
    };

    struct uv_lib_s
    {
        void* handle;
        char* errmsg;
    };

    struct uv_metrics_s
    {
        uint64_t  loop_count;
        uint64_t  events;
        uint64_t  events_waiting;
        uint64_t* reserved[13];
    };

    typedef struct
    {
        uv_timeval_t ru_utime;
        uv_timeval_t ru_stime;
        uint64_t     ru_maxrss;
        uint64_t     ru_ixrss;
        uint64_t     ru_idrss;
        uint64_t     ru_isrss;
        uint64_t     ru_minflt;
        uint64_t     ru_majflt;
        uint64_t     ru_nswap;
        uint64_t     ru_inblock;
        uint64_t     ru_oublock;
        uint64_t     ru_msgsnd;
        uint64_t     ru_msgrcv;
        uint64_t     ru_nsignals;
        uint64_t     ru_nvcsw;
        uint64_t     ru_nivcsw;
    } uv_rusage_t;

    typedef void*     uv_thread_t;
    typedef uintptr_t uv_key_t;
    typedef void (*uv_thread_cb)(void* arg);

    typedef enum
    {
        UV_THREAD_NO_FLAGS       = 0,
        UV_THREAD_HAS_STACK_SIZE = 1
    } uv_thread_create_flags;

    struct uv_thread_options_s
    {
        unsigned int flags;
        size_t       stack_size;
    };

#define UV_PRIORITY_LOW          19
#define UV_PRIORITY_BELOW_NORMAL 10
#define UV_PRIORITY_NORMAL       0
#define UV_PRIORITY_ABOVE_NORMAL -7
#define UV_PRIORITY_HIGH         -14
#define UV_PRIORITY_HIGHEST      -20

    typedef void (*uv_close_cb)(uv_handle_t* handle);
    typedef void (*uv_walk_cb)(uv_handle_t* handle, void* arg);
    typedef void (*uv_connection_cb)(uv_stream_t* server, int status);
    typedef void (*uv_alloc_cb)(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
    typedef void (*uv_read_cb)(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
    typedef void (*uv_write_cb)(uv_write_t* req, int status);
    typedef void (*uv_shutdown_cb)(uv_shutdown_t* req, int status);
    typedef void (*uv_udp_send_cb)(uv_udp_send_t* req, int status);
    typedef void (*uv_udp_recv_cb)(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr,
                                   unsigned flags);
    typedef void (*uv_timer_cb)(uv_timer_t* handle);
    typedef void (*uv_async_cb)(uv_async_t* handle);
    typedef void (*uv_prepare_cb)(uv_prepare_t* handle);
    typedef void (*uv_check_cb)(uv_check_t* handle);
    typedef void (*uv_idle_cb)(uv_idle_t* handle);
    typedef void (*uv_signal_cb)(uv_signal_t* handle, int signum);
    typedef void (*uv_connect_cb)(uv_connect_t* req, int status);
    typedef void (*uv_random_cb)(uv_random_t* req, int status, void* buf, size_t buflen);
    typedef void (*uv_getaddrinfo_cb)(uv_getaddrinfo_t* req, int status, struct addrinfo* res);
    typedef void (*uv_getnameinfo_cb)(uv_getnameinfo_t* req, int status, const char* hostname, const char* service);
    typedef void (*uv_work_cb)(uv_work_t* req);
    typedef void (*uv_after_work_cb)(uv_work_t* req, int status);

#define SC_UV_LOOP_OPAQUE_WORDS_APPLE   192
#define SC_UV_LOOP_OPAQUE_WORDS_LINUX   192
#define SC_UV_LOOP_OPAQUE_WORDS_WINDOWS 192

#define SC_UV_HANDLE_OPAQUE_WORDS_APPLE   96
#define SC_UV_HANDLE_OPAQUE_WORDS_LINUX   96
#define SC_UV_HANDLE_OPAQUE_WORDS_WINDOWS 96

#define SC_UV_STREAM_OPAQUE_WORDS_APPLE   96
#define SC_UV_STREAM_OPAQUE_WORDS_LINUX   96
#define SC_UV_STREAM_OPAQUE_WORDS_WINDOWS 96

#define SC_UV_TCP_OPAQUE_WORDS_APPLE   128
#define SC_UV_TCP_OPAQUE_WORDS_LINUX   128
#define SC_UV_TCP_OPAQUE_WORDS_WINDOWS 128

#define SC_UV_UDP_OPAQUE_WORDS_APPLE   128
#define SC_UV_UDP_OPAQUE_WORDS_LINUX   128
#define SC_UV_UDP_OPAQUE_WORDS_WINDOWS 128

#define SC_UV_TIMER_OPAQUE_WORDS_APPLE   64
#define SC_UV_TIMER_OPAQUE_WORDS_LINUX   64
#define SC_UV_TIMER_OPAQUE_WORDS_WINDOWS 64

#define SC_UV_ASYNC_OPAQUE_WORDS_APPLE   64
#define SC_UV_ASYNC_OPAQUE_WORDS_LINUX   64
#define SC_UV_ASYNC_OPAQUE_WORDS_WINDOWS 64

#define SC_UV_SIGNAL_OPAQUE_WORDS_APPLE   64
#define SC_UV_SIGNAL_OPAQUE_WORDS_LINUX   64
#define SC_UV_SIGNAL_OPAQUE_WORDS_WINDOWS 64

#define SC_UV_PREPARE_OPAQUE_WORDS_APPLE   32
#define SC_UV_PREPARE_OPAQUE_WORDS_LINUX   32
#define SC_UV_PREPARE_OPAQUE_WORDS_WINDOWS 32

#define SC_UV_CHECK_OPAQUE_WORDS_APPLE   32
#define SC_UV_CHECK_OPAQUE_WORDS_LINUX   32
#define SC_UV_CHECK_OPAQUE_WORDS_WINDOWS 32

#define SC_UV_IDLE_OPAQUE_WORDS_APPLE   32
#define SC_UV_IDLE_OPAQUE_WORDS_LINUX   32
#define SC_UV_IDLE_OPAQUE_WORDS_WINDOWS 32

#define SC_UV_REQ_OPAQUE_WORDS_APPLE   32
#define SC_UV_REQ_OPAQUE_WORDS_LINUX   32
#define SC_UV_REQ_OPAQUE_WORDS_WINDOWS 32

#define SC_UV_WRITE_REQ_OPAQUE_WORDS_APPLE   48
#define SC_UV_WRITE_REQ_OPAQUE_WORDS_LINUX   48
#define SC_UV_WRITE_REQ_OPAQUE_WORDS_WINDOWS 48

#define SC_UV_CONNECT_REQ_OPAQUE_WORDS_APPLE   32
#define SC_UV_CONNECT_REQ_OPAQUE_WORDS_LINUX   32
#define SC_UV_CONNECT_REQ_OPAQUE_WORDS_WINDOWS 32

#define SC_UV_UDP_SEND_REQ_OPAQUE_WORDS_APPLE   64
#define SC_UV_UDP_SEND_REQ_OPAQUE_WORDS_LINUX   80
#define SC_UV_UDP_SEND_REQ_OPAQUE_WORDS_WINDOWS 80

#define SC_UV_WORK_REQ_OPAQUE_WORDS_APPLE   32
#define SC_UV_WORK_REQ_OPAQUE_WORDS_LINUX   32
#define SC_UV_WORK_REQ_OPAQUE_WORDS_WINDOWS 32

#if defined(_WIN32)
#define SC_UV_LOOP_OPAQUE_WORDS         SC_UV_LOOP_OPAQUE_WORDS_WINDOWS
#define SC_UV_HANDLE_OPAQUE_WORDS       SC_UV_HANDLE_OPAQUE_WORDS_WINDOWS
#define SC_UV_STREAM_OPAQUE_WORDS       SC_UV_STREAM_OPAQUE_WORDS_WINDOWS
#define SC_UV_TCP_OPAQUE_WORDS          SC_UV_TCP_OPAQUE_WORDS_WINDOWS
#define SC_UV_UDP_OPAQUE_WORDS          SC_UV_UDP_OPAQUE_WORDS_WINDOWS
#define SC_UV_TIMER_OPAQUE_WORDS        SC_UV_TIMER_OPAQUE_WORDS_WINDOWS
#define SC_UV_ASYNC_OPAQUE_WORDS        SC_UV_ASYNC_OPAQUE_WORDS_WINDOWS
#define SC_UV_SIGNAL_OPAQUE_WORDS       SC_UV_SIGNAL_OPAQUE_WORDS_WINDOWS
#define SC_UV_PREPARE_OPAQUE_WORDS      SC_UV_PREPARE_OPAQUE_WORDS_WINDOWS
#define SC_UV_CHECK_OPAQUE_WORDS        SC_UV_CHECK_OPAQUE_WORDS_WINDOWS
#define SC_UV_IDLE_OPAQUE_WORDS         SC_UV_IDLE_OPAQUE_WORDS_WINDOWS
#define SC_UV_REQ_OPAQUE_WORDS          SC_UV_REQ_OPAQUE_WORDS_WINDOWS
#define SC_UV_WRITE_REQ_OPAQUE_WORDS    SC_UV_WRITE_REQ_OPAQUE_WORDS_WINDOWS
#define SC_UV_CONNECT_REQ_OPAQUE_WORDS  SC_UV_CONNECT_REQ_OPAQUE_WORDS_WINDOWS
#define SC_UV_UDP_SEND_REQ_OPAQUE_WORDS SC_UV_UDP_SEND_REQ_OPAQUE_WORDS_WINDOWS
#define SC_UV_WORK_REQ_OPAQUE_WORDS     SC_UV_WORK_REQ_OPAQUE_WORDS_WINDOWS
#elif defined(__linux__)
#define SC_UV_LOOP_OPAQUE_WORDS         SC_UV_LOOP_OPAQUE_WORDS_LINUX
#define SC_UV_HANDLE_OPAQUE_WORDS       SC_UV_HANDLE_OPAQUE_WORDS_LINUX
#define SC_UV_STREAM_OPAQUE_WORDS       SC_UV_STREAM_OPAQUE_WORDS_LINUX
#define SC_UV_TCP_OPAQUE_WORDS          SC_UV_TCP_OPAQUE_WORDS_LINUX
#define SC_UV_UDP_OPAQUE_WORDS          SC_UV_UDP_OPAQUE_WORDS_LINUX
#define SC_UV_TIMER_OPAQUE_WORDS        SC_UV_TIMER_OPAQUE_WORDS_LINUX
#define SC_UV_ASYNC_OPAQUE_WORDS        SC_UV_ASYNC_OPAQUE_WORDS_LINUX
#define SC_UV_SIGNAL_OPAQUE_WORDS       SC_UV_SIGNAL_OPAQUE_WORDS_LINUX
#define SC_UV_PREPARE_OPAQUE_WORDS      SC_UV_PREPARE_OPAQUE_WORDS_LINUX
#define SC_UV_CHECK_OPAQUE_WORDS        SC_UV_CHECK_OPAQUE_WORDS_LINUX
#define SC_UV_IDLE_OPAQUE_WORDS         SC_UV_IDLE_OPAQUE_WORDS_LINUX
#define SC_UV_REQ_OPAQUE_WORDS          SC_UV_REQ_OPAQUE_WORDS_LINUX
#define SC_UV_WRITE_REQ_OPAQUE_WORDS    SC_UV_WRITE_REQ_OPAQUE_WORDS_LINUX
#define SC_UV_CONNECT_REQ_OPAQUE_WORDS  SC_UV_CONNECT_REQ_OPAQUE_WORDS_LINUX
#define SC_UV_UDP_SEND_REQ_OPAQUE_WORDS SC_UV_UDP_SEND_REQ_OPAQUE_WORDS_LINUX
#define SC_UV_WORK_REQ_OPAQUE_WORDS     SC_UV_WORK_REQ_OPAQUE_WORDS_LINUX
#else
#define SC_UV_LOOP_OPAQUE_WORDS         SC_UV_LOOP_OPAQUE_WORDS_APPLE
#define SC_UV_HANDLE_OPAQUE_WORDS       SC_UV_HANDLE_OPAQUE_WORDS_APPLE
#define SC_UV_STREAM_OPAQUE_WORDS       SC_UV_STREAM_OPAQUE_WORDS_APPLE
#define SC_UV_TCP_OPAQUE_WORDS          SC_UV_TCP_OPAQUE_WORDS_APPLE
#define SC_UV_UDP_OPAQUE_WORDS          SC_UV_UDP_OPAQUE_WORDS_APPLE
#define SC_UV_TIMER_OPAQUE_WORDS        SC_UV_TIMER_OPAQUE_WORDS_APPLE
#define SC_UV_ASYNC_OPAQUE_WORDS        SC_UV_ASYNC_OPAQUE_WORDS_APPLE
#define SC_UV_SIGNAL_OPAQUE_WORDS       SC_UV_SIGNAL_OPAQUE_WORDS_APPLE
#define SC_UV_PREPARE_OPAQUE_WORDS      SC_UV_PREPARE_OPAQUE_WORDS_APPLE
#define SC_UV_CHECK_OPAQUE_WORDS        SC_UV_CHECK_OPAQUE_WORDS_APPLE
#define SC_UV_IDLE_OPAQUE_WORDS         SC_UV_IDLE_OPAQUE_WORDS_APPLE
#define SC_UV_REQ_OPAQUE_WORDS          SC_UV_REQ_OPAQUE_WORDS_APPLE
#define SC_UV_WRITE_REQ_OPAQUE_WORDS    SC_UV_WRITE_REQ_OPAQUE_WORDS_APPLE
#define SC_UV_CONNECT_REQ_OPAQUE_WORDS  SC_UV_CONNECT_REQ_OPAQUE_WORDS_APPLE
#define SC_UV_UDP_SEND_REQ_OPAQUE_WORDS SC_UV_UDP_SEND_REQ_OPAQUE_WORDS_APPLE
#define SC_UV_WORK_REQ_OPAQUE_WORDS     SC_UV_WORK_REQ_OPAQUE_WORDS_APPLE
#endif

    struct uv_loop_s
    {
        void*    data;
        int      active_handles;
        uint64_t sc_opaque[SC_UV_LOOP_OPAQUE_WORDS];
    };

    struct uv_req_s
    {
        void*       data;
        uv_req_type type;
        void*       reserved[6];
        uint64_t    sc_opaque[SC_UV_REQ_OPAQUE_WORDS];
    };

#define UV_HANDLE_FIELDS                                                                                               \
    void*            data;                                                                                             \
    uv_loop_t*       loop;                                                                                             \
    uv_handle_type   type;                                                                                             \
    uv_close_cb      close_cb;                                                                                         \
    struct uv__queue handle_queue;                                                                                     \
    union                                                                                                              \
    {                                                                                                                  \
        int   fd;                                                                                                      \
        void* reserved[4];                                                                                             \
    } u

#define UV_STREAM_FIELDS                                                                                               \
    size_t      write_queue_size;                                                                                      \
    uv_alloc_cb alloc_cb;                                                                                              \
    uv_read_cb  read_cb

    struct uv_handle_s
    {
        UV_HANDLE_FIELDS;
        uint64_t sc_opaque[SC_UV_HANDLE_OPAQUE_WORDS];
    };

    struct uv_stream_s
    {
        UV_HANDLE_FIELDS;
        UV_STREAM_FIELDS;
        uint64_t sc_opaque[SC_UV_STREAM_OPAQUE_WORDS];
    };

    struct uv_tcp_s
    {
        UV_HANDLE_FIELDS;
        UV_STREAM_FIELDS;
        uint64_t sc_opaque[SC_UV_TCP_OPAQUE_WORDS];
    };

    struct uv_udp_s
    {
        UV_HANDLE_FIELDS;
        size_t   send_queue_size;
        uint64_t sc_opaque[SC_UV_UDP_OPAQUE_WORDS];
    };

    struct uv_pipe_s
    {
        UV_HANDLE_FIELDS;
        UV_STREAM_FIELDS;
        uint64_t sc_opaque[SC_UV_STREAM_OPAQUE_WORDS];
    };

    struct uv_tty_s
    {
        UV_HANDLE_FIELDS;
        UV_STREAM_FIELDS;
        int      readable;
        int      writable;
        int      flags;
        uint64_t sc_opaque[SC_UV_STREAM_OPAQUE_WORDS];
    };

    struct uv_timer_s
    {
        UV_HANDLE_FIELDS;
        uint64_t sc_opaque[SC_UV_TIMER_OPAQUE_WORDS];
    };

    struct uv_async_s
    {
        UV_HANDLE_FIELDS;
        uint64_t sc_opaque[SC_UV_ASYNC_OPAQUE_WORDS];
    };

    struct uv_signal_s
    {
        UV_HANDLE_FIELDS;
        uv_signal_cb signal_cb;
        int          signum;
        uint64_t     sc_opaque[SC_UV_SIGNAL_OPAQUE_WORDS];
    };

    struct uv_prepare_s
    {
        UV_HANDLE_FIELDS;
        uv_prepare_cb prepare_cb;
        uint64_t      sc_opaque[SC_UV_PREPARE_OPAQUE_WORDS];
    };

    struct uv_check_s
    {
        UV_HANDLE_FIELDS;
        uv_check_cb check_cb;
        uint64_t    sc_opaque[SC_UV_CHECK_OPAQUE_WORDS];
    };

    struct uv_idle_s
    {
        UV_HANDLE_FIELDS;
        uv_idle_cb idle_cb;
        uint64_t   sc_opaque[SC_UV_IDLE_OPAQUE_WORDS];
    };

    struct uv_write_s
    {
        void*        data;
        uv_req_type  type;
        void*        reserved[6];
        uv_write_cb  cb;
        uv_stream_t* send_handle;
        uv_stream_t* handle;
        uint64_t     sc_opaque[SC_UV_WRITE_REQ_OPAQUE_WORDS];
    };

    struct uv_shutdown_s
    {
        void*          data;
        uv_req_type    type;
        void*          reserved[6];
        uv_shutdown_cb cb;
        uv_stream_t*   handle;
        uint64_t       sc_opaque[SC_UV_REQ_OPAQUE_WORDS];
    };

    struct uv_connect_s
    {
        void*         data;
        uv_req_type   type;
        void*         reserved[6];
        uv_connect_cb cb;
        uv_stream_t*  handle;
        uint64_t      sc_opaque[SC_UV_CONNECT_REQ_OPAQUE_WORDS];
    };

    struct uv_udp_send_s
    {
        void*          data;
        uv_req_type    type;
        void*          reserved[6];
        uv_udp_send_cb cb;
        uv_udp_t*      handle;
        uint64_t       sc_opaque[SC_UV_UDP_SEND_REQ_OPAQUE_WORDS];
    };

    struct uv_random_s
    {
        void*       data;
        uv_req_type type;
        void*       reserved[6];
        uint64_t    sc_opaque[SC_UV_REQ_OPAQUE_WORDS];
    };

    struct uv_getaddrinfo_s
    {
        void*             data;
        uv_req_type       type;
        void*             reserved[6];
        uv_loop_t*        loop;
        uv_getaddrinfo_cb getaddrinfo_cb;
        struct addrinfo*  addrinfo;
        uint64_t          sc_opaque[SC_UV_REQ_OPAQUE_WORDS];
    };

    struct uv_getnameinfo_s
    {
        void*             data;
        uv_req_type       type;
        void*             reserved[6];
        uv_loop_t*        loop;
        uv_getnameinfo_cb getnameinfo_cb;
        char              host[1025];
        char              service[32];
        uint64_t          sc_opaque[SC_UV_REQ_OPAQUE_WORDS];
    };

    struct uv_work_s
    {
        void*       data;
        uv_req_type type;
        void*       reserved[6];
        uint64_t    sc_opaque[SC_UV_WORK_REQ_OPAQUE_WORDS];
    };

    struct uv_fs_s
    {
        void*       data;
        uv_req_type type;
        void*       reserved[6];
        uv_loop_t*  loop;
        uv_fs_type  fs_type;
        ssize_t     result;
        const char* path;
        void*       ptr;
        uv_stat_t   statbuf;
        uint64_t    sc_opaque[SC_UV_REQ_OPAQUE_WORDS];
    };

    struct uv_mutex_s
    {
        uint64_t sc_opaque[16];
    };

    struct uv_cond_s
    {
        uint64_t sc_opaque[16];
    };

    struct uv_rwlock_s
    {
        uint64_t sc_opaque[32];
    };

    struct uv_sem_s
    {
        uint64_t sc_opaque[32];
    };

    struct uv_barrier_s
    {
        uint64_t sc_opaque[32];
    };

    UV_EXTERN uv_loop_t* uv_default_loop(void);
    UV_EXTERN void       uv_library_shutdown(void);
    UV_EXTERN int        uv_loop_init(uv_loop_t* uvLoop);
    UV_EXTERN int        uv_loop_close(uv_loop_t* uvLoop);
    UV_EXTERN int        uv_run(uv_loop_t* uvLoop, uv_run_mode mode);
    UV_EXTERN void       uv_stop(uv_loop_t* uvLoop);
    UV_EXTERN void       uv_sleep(unsigned int ms);
    UV_EXTERN uint64_t   uv_hrtime(void);
    UV_EXTERN int        uv_clock_gettime(uv_clock_id clock_id, uv_timespec64_t* ts);
    UV_EXTERN int        uv_gettimeofday(uv_timeval64_t* tv);
    UV_EXTERN uint64_t   uv_now(const uv_loop_t* uvLoop);
    UV_EXTERN void       uv_update_time(uv_loop_t* uvLoop);
    UV_EXTERN int        uv_loop_alive(const uv_loop_t* uvLoop);
    UV_EXTERN int        uv_loop_configure(uv_loop_t* uvLoop, uv_loop_option option, ...);
    UV_EXTERN int        uv_backend_timeout(const uv_loop_t* uvLoop);
    UV_EXTERN void       uv_walk(uv_loop_t* uvLoop, uv_walk_cb cb, void* userData);
    UV_EXTERN size_t     uv_loop_size(void);
    UV_EXTERN void       uv_loop_set_data(uv_loop_t* loop, void* data);
    UV_EXTERN void*      uv_loop_get_data(const uv_loop_t* loop);

    UV_EXTERN void           uv_close(uv_handle_t* handle, uv_close_cb close_cb);
    UV_EXTERN void           uv_ref(uv_handle_t* handle);
    UV_EXTERN void           uv_unref(uv_handle_t* handle);
    UV_EXTERN int            uv_has_ref(const uv_handle_t* handle);
    UV_EXTERN int            uv_is_active(const uv_handle_t* handle);
    UV_EXTERN int            uv_is_closing(const uv_handle_t* handle);
    UV_EXTERN int            uv_fileno(const uv_handle_t* handle, uv_os_fd_t* fd);
    UV_EXTERN uv_handle_type uv_guess_handle(uv_file file);
    UV_EXTERN const char*    uv_handle_type_name(uv_handle_type type);
    UV_EXTERN size_t         uv_handle_size(uv_handle_type type);
    UV_EXTERN uv_handle_type uv_handle_get_type(const uv_handle_t* handle);
    UV_EXTERN uv_loop_t*     uv_handle_get_loop(const uv_handle_t* handle);
    UV_EXTERN void*          uv_handle_get_data(const uv_handle_t* handle);
    UV_EXTERN void           uv_handle_set_data(uv_handle_t* handle, void* data);
    UV_EXTERN int            uv_is_readable(const uv_stream_t* handle);
    UV_EXTERN int            uv_is_writable(const uv_stream_t* handle);
    UV_EXTERN const char*    uv_req_type_name(uv_req_type type);
    UV_EXTERN size_t         uv_req_size(uv_req_type type);

    UV_EXTERN int uv_async_init(uv_loop_t* uvLoop, uv_async_t* handle, uv_async_cb async_cb);
    UV_EXTERN int uv_async_send(uv_async_t* async);

    UV_EXTERN int uv_signal_init(uv_loop_t* loop, uv_signal_t* signal);
    UV_EXTERN int uv_signal_start(uv_signal_t* signal, uv_signal_cb signal_cb, int signum);
    UV_EXTERN int uv_signal_start_oneshot(uv_signal_t* signal, uv_signal_cb signal_cb, int signum);
    UV_EXTERN int uv_signal_stop(uv_signal_t* signal);
    UV_EXTERN int uv_kill(uv_pid_t pid, int signum);

    UV_EXTERN int      uv_timer_init(uv_loop_t* loop, uv_timer_t* handle);
    UV_EXTERN int      uv_timer_start(uv_timer_t* handle, uv_timer_cb cb, uint64_t timeout, uint64_t repeat);
    UV_EXTERN int      uv_timer_stop(uv_timer_t* handle);
    UV_EXTERN int      uv_timer_again(uv_timer_t* handle);
    UV_EXTERN void     uv_timer_set_repeat(uv_timer_t* handle, uint64_t repeat);
    UV_EXTERN uint64_t uv_timer_get_repeat(const uv_timer_t* handle);
    UV_EXTERN uint64_t uv_timer_get_due_in(const uv_timer_t* handle);

    UV_EXTERN int    uv_write(uv_write_t* req, uv_stream_t* handle, const uv_buf_t bufs[], unsigned int nbufs,
                              uv_write_cb cb);
    UV_EXTERN int    uv_shutdown(uv_shutdown_t* req, uv_stream_t* handle, uv_shutdown_cb cb);
    UV_EXTERN int    uv_listen(uv_stream_t* stream, int backlog, uv_connection_cb cb);
    UV_EXTERN int    uv_accept(uv_stream_t* server, uv_stream_t* client);
    UV_EXTERN int    uv_read_start(uv_stream_t* stream, uv_alloc_cb alloc_cb, uv_read_cb read_cb);
    UV_EXTERN int    uv_read_stop(uv_stream_t* stream);
    UV_EXTERN size_t uv_stream_get_write_queue_size(const uv_stream_t* stream);
    UV_EXTERN int    uv_stream_set_blocking(uv_stream_t* stream, int blocking);
    UV_EXTERN int    uv_try_write(uv_stream_t* stream, const uv_buf_t bufs[], unsigned int nbufs);
    UV_EXTERN int    uv_prepare_init(uv_loop_t* uvLoop, uv_prepare_t* handle);
    UV_EXTERN int    uv_prepare_start(uv_prepare_t* handle, uv_prepare_cb cb);
    UV_EXTERN int    uv_prepare_stop(uv_prepare_t* handle);
    UV_EXTERN int    uv_check_init(uv_loop_t* uvLoop, uv_check_t* handle);
    UV_EXTERN int    uv_check_start(uv_check_t* handle, uv_check_cb cb);
    UV_EXTERN int    uv_check_stop(uv_check_t* handle);
    UV_EXTERN int    uv_idle_init(uv_loop_t* uvLoop, uv_idle_t* handle);
    UV_EXTERN int    uv_idle_start(uv_idle_t* handle, uv_idle_cb cb);
    UV_EXTERN int    uv_idle_stop(uv_idle_t* handle);

    UV_EXTERN uv_buf_t uv_buf_init(char* base, unsigned int len);

    UV_EXTERN int uv_tcp_init(uv_loop_t* uvLoop, uv_tcp_t* handle);
    UV_EXTERN int uv_tcp_init_ex(uv_loop_t* uvLoop, uv_tcp_t* handle, unsigned int flags);
    UV_EXTERN int uv_tcp_bind(uv_tcp_t* handle, const struct sockaddr* addr, unsigned int flags);
    UV_EXTERN int uv_tcp_connect(uv_connect_t* req, uv_tcp_t* handle, const struct sockaddr* addr, uv_connect_cb cb);
    UV_EXTERN int uv_tcp_open(uv_tcp_t* handle, uv_os_sock_t sock);
    UV_EXTERN int uv_tcp_nodelay(uv_tcp_t* handle, int enable);
    UV_EXTERN int uv_tcp_keepalive(uv_tcp_t* handle, int enable, unsigned int delay);
    UV_EXTERN int uv_tcp_close_reset(uv_tcp_t* handle, uv_close_cb close_cb);
    UV_EXTERN int uv_tcp_getsockname(const uv_tcp_t* handle, struct sockaddr* name, int* namelen);
    UV_EXTERN int uv_tcp_getpeername(const uv_tcp_t* handle, struct sockaddr* name, int* namelen);

    UV_EXTERN int  uv_pipe(int fds[2], int read_flags, int write_flags);
    UV_EXTERN int  uv_pipe_init(uv_loop_t* loop, uv_pipe_t* handle, int ipc);
    UV_EXTERN int  uv_pipe_bind(uv_pipe_t* handle, const char* name);
    UV_EXTERN int  uv_pipe_open(uv_pipe_t* handle, uv_file file);
    UV_EXTERN void uv_pipe_pending_instances(uv_pipe_t* handle, int count);

    UV_EXTERN int uv_tty_init(uv_loop_t* loop, uv_tty_t* handle, uv_file fd, int readable);
    UV_EXTERN int uv_tty_set_mode(uv_tty_t* handle, uv_tty_mode_t mode);
    UV_EXTERN int uv_tty_reset_mode(void);
    UV_EXTERN int uv_tty_get_winsize(uv_tty_t* handle, int* width, int* height);

    UV_EXTERN int uv_udp_init(uv_loop_t* uvLoop, uv_udp_t* handle);
    UV_EXTERN int uv_udp_init_ex(uv_loop_t* uvLoop, uv_udp_t* handle, unsigned int flags);
    UV_EXTERN int uv_udp_open(uv_udp_t* handle, uv_os_sock_t sock);
    UV_EXTERN int uv_udp_bind(uv_udp_t* handle, const struct sockaddr* addr, unsigned int flags);
    UV_EXTERN int uv_udp_getsockname(const uv_udp_t* handle, struct sockaddr* name, int* namelen);
    UV_EXTERN int uv_udp_connect(uv_udp_t* handle, const struct sockaddr* addr);
    UV_EXTERN int uv_udp_getpeername(const uv_udp_t* handle, struct sockaddr* name, int* namelen);
    UV_EXTERN int uv_udp_set_broadcast(uv_udp_t* handle, int on);
    UV_EXTERN int uv_udp_set_ttl(uv_udp_t* handle, int ttl);
    UV_EXTERN int uv_udp_set_multicast_loop(uv_udp_t* handle, int on);
    UV_EXTERN int uv_udp_set_multicast_ttl(uv_udp_t* handle, int ttl);
    UV_EXTERN int uv_udp_set_multicast_interface(uv_udp_t* handle, const char* interface_addr);
    UV_EXTERN int uv_udp_set_membership(uv_udp_t* handle, const char* multicast_addr, const char* interface_addr,
                                        uv_membership membership);
    UV_EXTERN int uv_udp_set_source_membership(uv_udp_t* handle, const char* multicast_addr, const char* interface_addr,
                                               const char* source_addr, uv_membership membership);
    UV_EXTERN int uv_udp_recv_start(uv_udp_t* handle, uv_alloc_cb alloc_cb, uv_udp_recv_cb recv_cb);
    UV_EXTERN int uv_udp_send(uv_udp_send_t* req, uv_udp_t* handle, const uv_buf_t bufs[], unsigned int nbufs,
                              const struct sockaddr* addr, uv_udp_send_cb send_cb);
    UV_EXTERN int uv_udp_try_send(uv_udp_t* handle, const uv_buf_t bufs[], unsigned int nbufs,
                                  const struct sockaddr* addr);
    UV_EXTERN int uv_udp_try_send2(uv_udp_t* handle, unsigned int count, uv_buf_t* bufs[], unsigned int nbufs[],
                                   struct sockaddr* addrs[], unsigned int flags);
    UV_EXTERN int uv_udp_using_recvmmsg(const uv_udp_t* handle);
    UV_EXTERN int uv_udp_recv_stop(uv_udp_t* handle);

    UV_EXTERN int  uv_interface_addresses(uv_interface_address_t**, int*);
    UV_EXTERN void uv_free_interface_addresses(uv_interface_address_t*, int);
    UV_EXTERN int  uv_ip4_addr(const char* ip, int port, struct sockaddr_in* addr);
    UV_EXTERN int  uv_ip6_addr(const char* ip, int port, struct sockaddr_in6* addr);
    UV_EXTERN int  uv_ip4_name(const struct sockaddr_in* src, char* dst, size_t size);
    UV_EXTERN int  uv_ip6_name(const struct sockaddr_in6* src, char* dst, size_t size);
    UV_EXTERN int  uv_ip_name(const struct sockaddr* src, char* dst, size_t size);
    UV_EXTERN int  uv_inet_ntop(int af, const void* src, char* dst, size_t size);
    UV_EXTERN int  uv_inet_pton(int af, const char* src, void* dst);
    UV_EXTERN int  uv_if_indextoname(unsigned int ifindex, char* buffer, size_t* size);
    UV_EXTERN int  uv_if_indextoiid(unsigned int ifindex, char* buffer, size_t* size);

    UV_EXTERN const char* uv_err_name(int err);
    UV_EXTERN const char* uv_strerror(int err);
    UV_EXTERN char*       uv_strerror_r(int err, char* buf, size_t buflen);
    UV_EXTERN int         uv_translate_sys_error(int sys_errno);

    UV_EXTERN size_t  uv_utf16_length_as_wtf8(const uint16_t* utf16, ssize_t utf16_len);
    UV_EXTERN int     uv_utf16_to_wtf8(const uint16_t* utf16, ssize_t utf16_len, char** wtf8_ptr, size_t* wtf8_len_ptr);
    UV_EXTERN ssize_t uv_wtf8_length_as_utf16(const char* wtf8);
    UV_EXTERN void    uv_wtf8_to_utf16(const char* wtf8, uint16_t* utf16, size_t utf16_len);

    UV_EXTERN int         uv_exepath(char* buffer, size_t* size);
    UV_EXTERN int         uv_dlopen(const char* filename, uv_lib_t* lib);
    UV_EXTERN void        uv_dlclose(uv_lib_t* lib);
    UV_EXTERN const char* uv_dlerror(const uv_lib_t* lib);
    UV_EXTERN int         uv_set_process_title(const char* title);
    UV_EXTERN int         uv_get_process_title(char* buffer, size_t size);

    UV_EXTERN int      uv_cwd(char* buffer, size_t* size);
    UV_EXTERN int      uv_chdir(const char* dir);
    UV_EXTERN int      uv_os_homedir(char* buffer, size_t* size);
    UV_EXTERN int      uv_os_tmpdir(char* buffer, size_t* size);
    UV_EXTERN int      uv_os_gethostname(char* buffer, size_t* size);
    UV_EXTERN int      uv_resident_set_memory(size_t* rss);
    UV_EXTERN int      uv_uptime(double* uptime);
    UV_EXTERN int      uv_getrusage(uv_rusage_t* rusage);
    UV_EXTERN int      uv_os_setenv(const char* name, const char* value);
    UV_EXTERN int      uv_os_getenv(const char* name, char* buffer, size_t* size);
    UV_EXTERN int      uv_os_unsetenv(const char* name);
    UV_EXTERN int      uv_os_environ(uv_env_item_t** envitems, int* count);
    UV_EXTERN void     uv_os_free_environ(uv_env_item_t* envitems, int count);
    UV_EXTERN int      uv_os_get_passwd(uv_passwd_t* pwd);
    UV_EXTERN int      uv_os_get_passwd2(uv_passwd_t* pwd, uv_uid_t uid);
    UV_EXTERN void     uv_os_free_passwd(uv_passwd_t* pwd);
    UV_EXTERN int      uv_os_get_group(uv_group_t* grp, uv_gid_t gid);
    UV_EXTERN void     uv_os_free_group(uv_group_t* grp);
    UV_EXTERN uv_pid_t uv_os_getpid(void);
    UV_EXTERN uv_pid_t uv_os_getppid(void);
    UV_EXTERN int      uv_os_getpriority(uv_pid_t pid, int* priority);
    UV_EXTERN int      uv_os_setpriority(uv_pid_t pid, int priority);
    UV_EXTERN int      uv_recv_buffer_size(uv_handle_t* handle, int* value);
    UV_EXTERN int      uv_send_buffer_size(uv_handle_t* handle, int* value);
    UV_EXTERN void     uv_loadavg(double avg[3]);
    UV_EXTERN uint64_t uv_get_free_memory(void);
    UV_EXTERN uint64_t uv_get_total_memory(void);
    UV_EXTERN uint64_t uv_get_constrained_memory(void);
    UV_EXTERN uint64_t uv_get_available_memory(void);
    UV_EXTERN int      uv_os_uname(uv_utsname_t* buffer);

    UV_EXTERN unsigned int uv_available_parallelism(void);

    UV_EXTERN int  uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count);
    UV_EXTERN void uv_free_cpu_info(uv_cpu_info_t* cpu_infos, int count);

    UV_EXTERN int  uv_random(uv_loop_t* loop, uv_random_t* req, void* buf, size_t buflen, unsigned int flags,
                             uv_random_cb cb);
    UV_EXTERN int  uv_getaddrinfo(uv_loop_t* loop, uv_getaddrinfo_t* req, uv_getaddrinfo_cb getaddrinfo_cb,
                                  const char* node, const char* service, const struct addrinfo* hints);
    UV_EXTERN void uv_freeaddrinfo(struct addrinfo* ai);
    UV_EXTERN int  uv_getnameinfo(uv_loop_t* loop, uv_getnameinfo_t* req, uv_getnameinfo_cb getnameinfo_cb,
                                  const struct sockaddr* addr, int flags);
    UV_EXTERN int  uv_queue_work(uv_loop_t* loop, uv_work_t* req, uv_work_cb work_cb, uv_after_work_cb after_work_cb);

    typedef void (*uv_fs_cb)(uv_fs_t* req);
    UV_EXTERN int         uv_fs_stat(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb);
    UV_EXTERN void        uv_fs_req_cleanup(uv_fs_t* req);
    UV_EXTERN uv_fs_type  uv_fs_get_type(const uv_fs_t* req);
    UV_EXTERN ssize_t     uv_fs_get_result(const uv_fs_t* req);
    UV_EXTERN void*       uv_fs_get_ptr(const uv_fs_t* req);
    UV_EXTERN const char* uv_fs_get_path(const uv_fs_t* req);
    UV_EXTERN uv_stat_t*  uv_fs_get_statbuf(uv_fs_t* req);

    UV_EXTERN int         uv_thread_create(uv_thread_t* tid, uv_thread_cb entry, void* arg);
    UV_EXTERN int         uv_thread_create_ex(uv_thread_t* tid, const uv_thread_options_t* params, uv_thread_cb entry,
                                              void* arg);
    UV_EXTERN int         uv_thread_detach(uv_thread_t* tid);
    UV_EXTERN int         uv_thread_join(uv_thread_t* tid);
    UV_EXTERN uv_thread_t uv_thread_self(void);
    UV_EXTERN int         uv_thread_equal(const uv_thread_t* t1, const uv_thread_t* t2);
    UV_EXTERN int         uv_cpumask_size(void);
    UV_EXTERN int         uv_thread_getcpu(void);
    UV_EXTERN int         uv_thread_setaffinity(uv_thread_t* tid, char* cpumask, char* oldmask, size_t mask_size);
    UV_EXTERN int         uv_thread_getaffinity(uv_thread_t* tid, char* cpumask, size_t mask_size);
    UV_EXTERN int         uv_thread_setname(const char* name);
    UV_EXTERN int         uv_thread_getname(uv_thread_t* tid, char* name, size_t size);
    UV_EXTERN int         uv_key_create(uv_key_t* key);
    UV_EXTERN void        uv_key_delete(uv_key_t* key);
    UV_EXTERN void*       uv_key_get(uv_key_t* key);
    UV_EXTERN void        uv_key_set(uv_key_t* key, void* value);
    UV_EXTERN int         uv_mutex_init(uv_mutex_t* handle);
    UV_EXTERN int         uv_mutex_init_recursive(uv_mutex_t* handle);
    UV_EXTERN void        uv_mutex_destroy(uv_mutex_t* handle);
    UV_EXTERN void        uv_mutex_lock(uv_mutex_t* handle);
    UV_EXTERN int         uv_mutex_trylock(uv_mutex_t* handle);
    UV_EXTERN void        uv_mutex_unlock(uv_mutex_t* handle);
    UV_EXTERN int         uv_rwlock_init(uv_rwlock_t* rwlock);
    UV_EXTERN void        uv_rwlock_destroy(uv_rwlock_t* rwlock);
    UV_EXTERN void        uv_rwlock_rdlock(uv_rwlock_t* rwlock);
    UV_EXTERN int         uv_rwlock_tryrdlock(uv_rwlock_t* rwlock);
    UV_EXTERN void        uv_rwlock_rdunlock(uv_rwlock_t* rwlock);
    UV_EXTERN void        uv_rwlock_wrlock(uv_rwlock_t* rwlock);
    UV_EXTERN int         uv_rwlock_trywrlock(uv_rwlock_t* rwlock);
    UV_EXTERN void        uv_rwlock_wrunlock(uv_rwlock_t* rwlock);
    UV_EXTERN int         uv_cond_init(uv_cond_t* cond);
    UV_EXTERN void        uv_cond_destroy(uv_cond_t* cond);
    UV_EXTERN void        uv_cond_signal(uv_cond_t* cond);
    UV_EXTERN void        uv_cond_broadcast(uv_cond_t* cond);
    UV_EXTERN void        uv_cond_wait(uv_cond_t* cond, uv_mutex_t* mutex);
    UV_EXTERN int         uv_cond_timedwait(uv_cond_t* cond, uv_mutex_t* mutex, uint64_t timeout);
    UV_EXTERN int         uv_sem_init(uv_sem_t* sem, unsigned int value);
    UV_EXTERN void        uv_sem_destroy(uv_sem_t* sem);
    UV_EXTERN void        uv_sem_post(uv_sem_t* sem);
    UV_EXTERN void        uv_sem_wait(uv_sem_t* sem);
    UV_EXTERN int         uv_sem_trywait(uv_sem_t* sem);
    UV_EXTERN int         uv_barrier_init(uv_barrier_t* barrier, unsigned int count);
    UV_EXTERN void        uv_barrier_destroy(uv_barrier_t* barrier);
    UV_EXTERN int         uv_barrier_wait(uv_barrier_t* barrier);

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#ifdef __cplusplus
} // extern "C"
#endif
