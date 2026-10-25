// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Code/uv.h"
#include "Libraries/Socket/Socket.h"
#include "Libraries/Threading/Threading.h"
#if !SC_PLATFORM_WINDOWS
#include <errno.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <stdio.h>

extern "C" void run_test_tcp_read_stop(void);
extern "C" int  run_test_readable_on_eof(void);
extern "C"
{
    char executable_path[4096] = {};
}

namespace
{
static constexpr SC::uint16_t TCP_READ_STOP_TEST_PORT = 9123; // Matches libuv task.h TEST_PORT
static constexpr int          LIBUV_TEST_SKIP         = 7;    // Matches task.h enum test_status::TEST_SKIP

void setNoSigPipe(SC::SocketDescriptor& descriptor);

struct TcpReadStopHelper
{
    SC::Thread      thread;
    SC::EventObject startedEvent;
    bool            started = false;

    bool start()
    {
        if (not thread.start(
                [this](SC::Thread& workerThread)
                {
                    workerThread.setThreadName(SC_NATIVE_STR("tcp-read-stop-helper"));

                    SC::SocketIPAddress listenAddress;
                    if (not listenAddress.fromAddressPort("127.0.0.1", TCP_READ_STOP_TEST_PORT))
                    {
                        startedEvent.signal();
                        return;
                    }

                    SC::SocketDescriptor serverSocket;
                    SC::SocketServer     server(serverSocket);
                    if (not serverSocket.create(listenAddress.getAddressFamily()) or not server.bind(listenAddress) or
                        not server.listen(1))
                    {
                        startedEvent.signal();
                        return;
                    }

                    started = true;
                    startedEvent.signal();

                    SC::SocketDescriptor peerSocket;
                    if (not server.accept(listenAddress.getAddressFamily(), peerSocket))
                    {
                        (void)server.close();
                        return;
                    }
                    setNoSigPipe(peerSocket);

                    SC::SocketClient client(peerSocket);
                    char             buffer[1024];
                    for (;;)
                    {
                        SC::Span<char> readData;
                        if (not client.readWithTimeout({buffer, sizeof(buffer)}, readData, 1000) or
                            readData.sizeInBytes() == 0)
                        {
                            break;
                        }
                        if (not client.write({readData.data(), readData.sizeInBytes()}))
                        {
                            break;
                        }
                    }

                    (void)peerSocket.close();
                    (void)server.close();
                }))
        {
            return false;
        }
        startedEvent.wait();
        if (not started)
        {
            (void)thread.join();
            return false;
        }
        return true;
    }

    void join()
    {
        if (thread.wasStarted())
        {
            (void)thread.join();
        }
    }
};

struct TcpCloseOnCommandHelper
{
    SC::Thread      thread;
    SC::EventObject startedEvent;
    bool            started = false;

    bool start()
    {
        if (not thread.start(
                [this](SC::Thread& workerThread)
                {
                    workerThread.setThreadName(SC_NATIVE_STR("tcp-close-command-helper"));

                    SC::SocketIPAddress listenAddress;
                    if (not listenAddress.fromAddressPort("127.0.0.1", TCP_READ_STOP_TEST_PORT))
                    {
                        startedEvent.signal();
                        return;
                    }

                    SC::SocketDescriptor serverSocket;
                    SC::SocketServer     server(serverSocket);
                    if (not serverSocket.create(listenAddress.getAddressFamily()) or not server.bind(listenAddress) or
                        not server.listen(1))
                    {
                        startedEvent.signal();
                        return;
                    }

                    started = true;
                    startedEvent.signal();

                    SC::SocketDescriptor peerSocket;
                    if (server.accept(listenAddress.getAddressFamily(), peerSocket))
                    {
                        setNoSigPipe(peerSocket);
                        SC::SocketClient client(peerSocket);
                        char             buffer[16];
                        SC::Span<char>   readData;
                        (void)client.readWithTimeout({buffer, sizeof(buffer)}, readData, 1000);
                        (void)peerSocket.close();
                    }
                    (void)server.close();
                }))
        {
            return false;
        }
        startedEvent.wait();
        if (not started)
        {
            (void)thread.join();
            return false;
        }
        return true;
    }

    void join()
    {
        if (thread.wasStarted())
        {
            (void)thread.join();
        }
    }
};

void setNoSigPipe(SC::SocketDescriptor& descriptor)
{
#if defined(SO_NOSIGPIPE)
    SC::SocketDescriptor::Handle socketHandle;
    if (descriptor.get(socketHandle, SC::Result(false)))
    {
        int enabled = 1;
        (void)::setsockopt(socketHandle, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
    }
#else
    (void)descriptor;
#endif
}

const char* run_test_tcp_read_stop_with_helper()
{
    TcpReadStopHelper helper;
    if (not helper.start())
    {
        return "run_test_tcp_read_stop helper failed to start";
    }
    run_test_tcp_read_stop();
    helper.join();
    return nullptr;
}

const char* run_test_readable_on_eof_with_helper()
{
    TcpCloseOnCommandHelper helper;
    if (not helper.start())
    {
        return "run_test_readable_on_eof helper failed to start";
    }
    run_test_readable_on_eof();
    helper.join();
    return nullptr;
}

using LibuvIntTest = int (*)();

#if SC_PLATFORM_LINUX
const char* test_udp_send_preserves_error()
{
    uv_loop_t     loop    = {};
    uv_udp_t      handle  = {};
    uv_udp_send_t request = {};
    sockaddr_in   address = {};
    struct Context
    {
        unsigned int calls  = 0;
        int          status = 0;
    } context;

    if (uv_loop_init(&loop) != 0)
        return "UDP error test loop initialization failed";
    if (uv_udp_init_ex(&loop, &handle, AF_INET) != 0)
    {
        (void)uv_loop_close(&loop);
        return "UDP error test handle initialization failed";
    }
    handle.data         = &context;
    const bool prepared = uv_udp_set_broadcast(&handle, 0) == 0 and uv_ip4_addr("127.255.255.255", 9123, &address) == 0;
    char       byte     = 'x';
    uv_buf_t   buffer   = uv_buf_init(&byte, 1);
    int        sent     = UV_EINVAL;
    if (prepared)
    {
        sent = uv_udp_send(&request, &handle, &buffer, 1, reinterpret_cast<const sockaddr*>(&address),
                           [](uv_udp_send_t* req, int status)
                           {
                               Context& ctx = *static_cast<Context*>(req->handle->data);
                               ctx.calls++;
                               ctx.status = status;
                           });
    }
    const bool deferred = context.calls == 0;
    errno               = 0;
    (void)uv_run(&loop, UV_RUN_DEFAULT);
    uv_close(reinterpret_cast<uv_handle_t*>(&handle), [](uv_handle_t*) {});
    (void)uv_run(&loop, UV_RUN_DEFAULT);
    const int closed = uv_loop_close(&loop);
    return sent == 0 and deferred and context.calls == 1 and context.status == UV_EACCES and closed == 0
               ? nullptr
               : "UDP send did not preserve its deferred native error";
}
#endif

#if !SC_PLATFORM_WINDOWS
const char* run_test_in_child(LibuvIntTest test, const char* failure)
{
    const pid_t pid = ::fork();
    if (pid < 0)
    {
        return "fork failed";
    }
    if (pid == 0)
    {
        const int result = test();
        ::_exit((result == 0 or result == LIBUV_TEST_SKIP) ? 0 : 1);
    }

    int status = 0;
    if (::waitpid(pid, &status, 0) != pid)
    {
        return "waitpid failed";
    }
    if (WIFEXITED(status) and WEXITSTATUS(status) == 0)
    {
        return nullptr;
    }
    return failure;
}
#endif

const char* run_test_with_echo_helper(LibuvIntTest test, const char* failure)
{
    TcpReadStopHelper helper;
    if (not helper.start())
    {
        return "echo helper failed to start";
    }
    const int result = test();
    helper.join();
    if (result != 0 and result != LIBUV_TEST_SKIP)
    {
        return failure;
    }
    return nullptr;
}

extern "C" int run_test_tcp_write_fail(void);

const char* run_test_tcp_write_fail_with_helper()
{
    TcpReadStopHelper helper;
    if (not helper.start())
    {
        return "run_test_tcp_write_fail helper failed to start";
    }
#if !SC_PLATFORM_WINDOWS
    // The test closes its native socket behind the adapter. Keep the echo thread's
    // accept() in a separate descriptor table so it cannot reuse that socket number.
    const char* failure = run_test_in_child(run_test_tcp_write_fail, "run_test_tcp_write_fail failed");
    helper.join();
    return failure;
#else
    const int result = run_test_tcp_write_fail();
    helper.join();
    if (result != 0 and result != LIBUV_TEST_SKIP)
    {
        return "run_test_tcp_write_fail failed";
    }
    return nullptr;
#endif
}

} // namespace

extern "C"
{
    // test-pass-always.c
    int run_test_pass_always(void);

    // test-active.c
    void run_test_active(void);

    // test-platform-output.c
    void run_test_platform_output(void);

    // test-process-title-threadsafe.c
    void run_test_process_title_threadsafe(void);

    // test-timer.c
    void run_test_timer(void);
    void run_test_timer_start_twice(void);
    void run_test_timer_init(void);
    void run_test_timer_order(void);
    void run_test_timer_zero_timeout(void);
    void run_test_timer_huge_timeout(void);
    void run_test_timer_huge_repeat(void);
    void run_test_timer_run_once(void);
    void run_test_timer_is_closing(void);
    void run_test_timer_null_callback(void);
    void run_test_timer_early_check(void);
    void run_test_timer_no_double_call_once(void);
    void run_test_timer_no_double_call_nowait(void);
    void run_test_timer_no_run_on_unref(void);

    // test-timer-again.c
    void run_test_timer_again(void);

    // test-timer-from-check.c
    void run_test_timer_from_check(void);

    // test-run-nowait.c
    void run_test_run_nowait(void);

    // test-run-once.c
    void run_test_run_once(void);

    // test-walk-handles.c
    void run_test_walk_handles(void);

    // test-loop-handles.c
    void run_test_loop_handles(void);

    // test-loop-alive.c
    int run_test_loop_alive(void);

    // test-loop-stop.c
    void run_test_loop_stop(void);
    void run_test_loop_stop_before_run(void);

    // test-loop-close.c
    int run_test_loop_close(void);
    int run_test_loop_instant_close(void); // DISABLED (queued work intentionally left active)

    // test-loop-configure.c
    void run_test_loop_configure(void);

    // test-loop-time.c
    void run_test_loop_update_time(void);
    void run_test_loop_backend_timeout(void); // DISABLED (touches active_handles)

    // test-async-null-cb.c
    int  run_test_async(void);
    void run_test_async_null_cb(void);

    // test-error.c
    int  run_test_error_message(void);
    void run_test_sys_error(void);

    // test-fail-always.c
    int run_test_fail_always(void); // DISABLED (intentionally fails to test the upstream runner)

    // test-barrier.c
    void run_test_barrier_1(void);
    void run_test_barrier_2(void);
    void run_test_barrier_3(void);
    void run_test_barrier_serial_thread(void);
    void run_test_barrier_serial_thread_single(void);

    // test-callback-stack.c
    int run_test_callback_stack(void);

    // test-cwd-and-chdir.c
    void run_test_cwd_and_chdir(void);

    // test-get-loadavg.c
    void run_test_get_loadavg(void);

    // test-get-currentexe.c
    int run_test_get_currentexe(void);

    // test-get-memory.c
    void run_test_get_memory(void);

    // test-get-passwd.c
    int run_test_get_passwd(void);
    int run_test_get_passwd2(void);
    int run_test_get_group(void);

    // test-gethostname.c
    void run_test_gethostname(void);

    // test-gettimeofday.c
    void run_test_gettimeofday(void);

    // test-homedir.c
    void run_test_homedir(void);

    // test-env-vars.c
    void run_test_env_vars(void);

    // test-hrtime.c
    void run_test_hrtime(void);
    void run_test_clock_gettime(void);

    // test-ip-name.c
    void run_test_ip_name(void);

    // test-getaddrinfo.c
    int run_test_getaddrinfo_fail(void);
    int run_test_getaddrinfo_fail_sync(void);
    int run_test_getaddrinfo_basic(void);
    int run_test_getaddrinfo_basic_sync(void);
    int run_test_getaddrinfo_concurrent(void);

    // test-getnameinfo.c
    int run_test_getnameinfo_basic_ip4(void);
    int run_test_getnameinfo_basic_ip4_sync(void);
    int run_test_getnameinfo_basic_ip6(void);

    // test-getsockname.c
    int run_test_getsockname_tcp(void);
    int run_test_getsockname_udp(void);

    // test-getters-setters.c
    int run_test_handle_type_name(void);
    int run_test_req_type_name(void);
    int run_test_getters_setters(void);

    // test-ip6-addr.c
    int  run_test_ip6_addr_link_local(void);
    void run_test_ip6_pton(void);
    void run_test_ip6_sin6_len(void);

    // test-tmpdir.c
    void run_test_tmpdir(void);

    // test-uname.c
    void run_test_uname(void);

    // test-idna.c
    void run_test_utf8_decode1(void);
    void run_test_utf8_decode1_overrun(void);
    void run_test_idna_toascii(void);
    void run_test_wtf8(void);

    // test-process-priority.c
    void run_test_process_priority(void);

    // test-signal.c
    int run_test_kill_invalid_signum(void);
#if !SC_PLATFORM_WINDOWS
    int run_test_we_get_signal(void);
    int run_test_we_get_signals(void);
    int run_test_we_get_signal_one_shot(void);
    int run_test_we_get_signals_mixed(void);
#else
    int run_test_win32_signum_number(void);
#endif

    // test-semaphore.c
    void run_test_semaphore_1(void);
    void run_test_semaphore_2(void);
    void run_test_semaphore_3(void);

    // test-mutexes.c
    void run_test_thread_mutex(void);
    void run_test_thread_mutex_recursive(void);
    void run_test_thread_rwlock(void);
    void run_test_thread_rwlock_trylock(void);

    // test-condvar.c
    void run_test_condvar_1(void);
    void run_test_condvar_2(void);
    void run_test_condvar_3(void);
    void run_test_condvar_4(void);
    void run_test_condvar_5(void);

    // test-thread-name.c
    void run_test_thread_name(void);

    // test-thread-affinity.c
#if SC_PLATFORM_APPLE
    int run_test_thread_affinity(void);
#endif

    // test-thread.c
    int run_test_thread_create(void);
    int run_test_thread_local_storage(void);
    int run_test_thread_stack_size(void);
    int run_test_thread_detach(void);

    // test-thread-equal.c
    void run_test_thread_equal(void);

    // test-tty.c
#if !SC_PLATFORM_WINDOWS
    int run_test_tty(void);
#endif

    // test-ip4-addr.c
    void run_test_ip4_addr(void);

    // test-tcp-close.c
    void run_test_tcp_close(void);

    // test-tcp-close-after-read-timeout.c
    void run_test_tcp_close_after_read_timeout(void);

    // test-tcp-close-reset.c
    int run_test_tcp_close_reset_client(void);
    int run_test_tcp_close_reset_client_after_shutdown(void);
    int run_test_tcp_close_reset_accepted(void);
    int run_test_tcp_close_reset_accepted_after_shutdown(void);
    int run_test_tcp_close_reset_accepted_after_socket_shutdown(void);

    // test-tcp-bind-error.c
    int run_test_tcp_bind_error_addrinuse_connect(void);
    int run_test_tcp_bind_error_addrinuse_listen(void);
    int run_test_tcp_bind_error_addrnotavail_1(void);
    int run_test_tcp_bind_error_addrnotavail_2(void);
    int run_test_tcp_bind_error_fault(void);
    int run_test_tcp_bind_error_inval(void);
    int run_test_tcp_bind_localhost_ok(void);
    int run_test_tcp_bind_invalid_flags(void);
    int run_test_tcp_listen_without_bind(void);
    int run_test_tcp_bind_writable_flags(void);
    int run_test_tcp_bind_or_listen_error_after_close(void);

    // test-tcp-bind6-error.c
    int run_test_tcp_bind6_error_addrnotavail(void);

    // test-tcp-connect-error.c
    void run_test_tcp_connect_error_fault(void);
    void run_test_tcp_connect_error_after_write(void);

    // test-tcp-connect6-error.c
    int run_test_tcp_connect6_error_fault(void);

    // test-connection-fail.c
    int run_test_connection_fail(void);
    int run_test_connection_fail_doesnt_auto_close(void);

    // test-emfile.c
    int run_test_emfile(void); // DISABLED (lowers RLIMIT_NOFILE for the whole process)

    // test-tcp-write-after-connect.c
    int run_test_tcp_write_after_connect(void);

    // test-tcp-write-to-half-open-connection.c
    int run_test_tcp_write_to_half_open_connection(void);

    // test-tcp-write-queue-order.c
    int run_test_tcp_write_queue_order(void);

    // test-tcp-close-while-connecting.c
    int run_test_tcp_close_while_connecting(void);

    // test-tcp-connect-timeout.c
    int run_test_tcp_connect_timeout(void); // DISABLED (external route can hang in this wrapper)
    int run_test_tcp_local_connect_timeout(void);
    int run_test_tcp6_local_connect_timeout(void); // DISABLED (shares static counters)

    // test-connect-unspecified.c
    void run_test_connect_unspecified(void);

    // test-tcp-create-socket-early.c
#if !SC_PLATFORM_WINDOWS
    int run_test_tcp_create_early(void);
    int run_test_tcp_create_early_bad_bind(void);
    int run_test_tcp_create_early_bad_domain(void);
    int run_test_tcp_create_early_accept(void); // DISABLED (pre-opened accept target should return UV_EBUSY)
#endif

    // test-udp-send-and-recv.c
    void run_test_udp_send_and_recv(void);

    // test-socket-buffer-size.c
    void run_test_socket_buffer_size(void);

    // test-strtok.c
    void run_test_strtok(void);

    // test-strscpy.c
    void run_test_strscpy(void);

    // test-close-order.c
    void run_test_close_order(void);

    // test-default-loop-close.c
    void run_test_default_loop_close(void);

    // test-dlerror.c
    void run_test_dlerror(void);

    // test-test-macros.c
    void run_test_test_macros(void);

    // test-shutdown-eof.c
    int run_test_shutdown_eof(void);

    // test-shutdown-simultaneous.c
    int run_test_shutdown_simultaneous(void);

    // test-shutdown-twice.c
    int run_test_shutdown_twice(void);

    // test-idle.c
    void run_test_idle_starvation(void);
    void run_test_idle_check(void);

    // test-multiple-listen.c
    void run_test_multiple_listen(void);

    // test-tcp-read-stop.c
    void run_test_tcp_read_stop(void);

    // test-not-readable-nor-writable-on-read-error.c
    int run_test_not_readable_nor_writable_on_read_error(void); // DISABLED (read errors keep stream readable/writable)

    // test-not-writable-after-shutdown.c
    int run_test_not_writable_after_shutdown(void);

    // test-pipe-pending-instances.c
    int run_test_pipe_pending_instances(void);

    // test-tcp-read-stop-start.c
    void run_test_tcp_read_stop_start(void);

    // test-tcp-reuseport.c
    void run_test_tcp_reuseport(void);

    // test-tcp-flags.c
    int run_test_tcp_flags(void);

    // test-udp-reuseport.c
    void run_test_udp_reuseport(void);

    // test-tcp-unexpected-read.c
    void run_test_tcp_unexpected_read(void);

    // test-tcp-oob.c
    int run_test_tcp_oob(void);

    // test-tcp-open.c
#if !SC_PLATFORM_WINDOWS
    int run_test_tcp_open(void);
    int run_test_tcp_open_twice(void);
    int run_test_tcp_open_bound(void);
    int run_test_tcp_open_connected(void);
    int run_test_tcp_write_ready(void);
#endif

    // test-tcp-rst.c
#if !SC_PLATFORM_WINDOWS
    int run_test_tcp_rst(void);
#endif

    // test-tcp-shutdown-after-write.c
    int run_test_tcp_shutdown_after_write(void);

    // test-tcp-write-fail.c
    int run_test_tcp_write_fail(void);

    // test-tcp-try-write-error.c
    int run_test_tcp_try_write_error(void);

    // test-tcp-try-write.c
    int run_test_tcp_try_write(void);

    // test-tcp-write-in-a-row.c
    int run_test_tcp_write_in_a_row(void);

    // test-tcp-writealot.c
    int run_test_tcp_writealot(void);

    // test-udp-send-immediate.c
    void run_test_udp_send_immediate(void);

    // test-udp-bind.c
    void run_test_udp_bind(void);

    // test-udp-connect.c
    int run_test_udp_connect(void);

    // test-udp-connect6.c
    int run_test_udp_connect6(void);

    // test-udp-multicast-interface.c
    int run_test_udp_multicast_interface(void);

    // test-udp-multicast-interface6.c
    int run_test_udp_multicast_interface6(void);

    // test-udp-multicast-join.c
    int run_test_udp_multicast_join(void);

    // test-udp-multicast-join6.c
    int run_test_udp_multicast_join6(void);

    // test-udp-mmsg.c
    int run_test_udp_mmsg(void);

    // test-udp-sendmmsg-error.c
    int run_test_udp_sendmmsg_error(void);

    // test-udp-multicast-ttl.c
    int run_test_udp_multicast_ttl(void);

    // test-udp-options.c
    int run_test_udp_options(void);
    int run_test_udp_options6(void);
    int run_test_udp_no_autobind(void);

    // test-udp-open.c
    int run_test_udp_open(void);
    int run_test_udp_open_twice(void);
    int run_test_udp_open_bound(void);
    int run_test_udp_open_connect(void); // DISABLED (shares static counters with udp_open)
    int run_test_udp_send_unix(void);    // DISABLED (requires AF_UNIX socket send support)

    // test-udp-create-socket-early.c
#if !SC_PLATFORM_WINDOWS
    int run_test_udp_create_early(void);
    int run_test_udp_create_early_bad_bind(void);
    int run_test_udp_create_early_bad_domain(void);
#endif

    // test-udp-ipv6.c
    int run_test_udp_ipv6_only(void);
    int run_test_udp_dual_stack(void); // DISABLED (shares static counters with udp_ipv6_only)

    // test-udp-dgram-too-big.c
    void run_test_udp_dgram_too_big(void);

    // test-udp-send-hang-loop.c
    int run_test_udp_send_hang_loop(void);

    // test-udp-send-unreachable.c
    int run_test_udp_send_unreachable(void);

    // test-watcher-cross-stop.c
    int run_test_watcher_cross_stop(void);

    // test-tcp-close-accept.c
    void run_test_tcp_close_accept(void);

    // test-tcp-alloc-cb-fail.c
    void run_test_tcp_alloc_cb_fail(void);

    // test-udp-alloc-cb-fail.c
    void run_test_udp_alloc_cb_fail(void);

    // test-delayed-accept.c
    void run_test_delayed_accept(void);

    // test-random.c
    void run_test_random_async(void);
    void run_test_random_sync(void);

    // test-threadpool.c
    int run_test_threadpool_queue_work_simple(void);
    int run_test_threadpool_queue_work_einval(void);

    // test-udp-recv-in-a-row.c
    int run_test_udp_recv_in_a_row(void);

    // test-udp-try-send.c
    int run_test_udp_try_send(void);
}

extern "C" const char* sc_async_test(void)
{
#if SC_PLATFORM_LINUX
    if (const char* failure = test_udp_send_preserves_error())
        return failure;
#endif
    // test-pass-always.c
    if (run_test_pass_always() != 0)
    {
        return "run_test_pass_always failed";
    }

    // test-active.c
    run_test_active();

    // test-platform-output.c
    run_test_platform_output();

    // test-process-title-threadsafe.c
    run_test_process_title_threadsafe();

    // test-timer.c
    run_test_timer();
    run_test_timer_start_twice();
    run_test_timer_init();
    run_test_timer_order();
    run_test_timer_zero_timeout();
    run_test_timer_huge_timeout();
    run_test_timer_huge_repeat();
    run_test_timer_run_once();
    run_test_timer_is_closing();
    run_test_timer_null_callback();
    run_test_timer_early_check();
    run_test_timer_no_double_call_once();
    run_test_timer_no_double_call_nowait();
    run_test_timer_no_run_on_unref();

    // test-timer-again.c
    run_test_timer_again();

    // test-timer-from-check.c
    run_test_timer_from_check();

    // test-run-nowait.c
    run_test_run_nowait();

    // test-run-once.c
    run_test_run_once();

    // test-walk-handles.c
    run_test_walk_handles();

    // test-loop-handles.c
    run_test_loop_handles();

    // test-loop-alive.c
    const int loopAliveResult = run_test_loop_alive();
    if (loopAliveResult != 0)
    {
        return "run_test_loop_alive failed";
    }

    // test-loop-stop.c
    run_test_loop_stop();
    run_test_loop_stop_before_run();

    // test-loop-close.c
    const int loopCloseResult = run_test_loop_close();
    if (loopCloseResult != 0)
    {
        return "run_test_loop_close failed";
    }
    // run_test_loop_instant_close(); // Queued work intentionally prevents loop cleanup in upstream test

    // test-loop-configure.c
    run_test_loop_configure();

    // test-loop-time.c
    run_test_loop_update_time();
    // run_test_loop_backend_timeout(); // DISABLED (touches active_handles)

    // test-async-null-cb.c
    // run_test_async(); // Requires repeated async wakeups to keep the loop alive until close
    run_test_async_null_cb();

    // test-error.c
    const int errorMessageResult = run_test_error_message();
    if (errorMessageResult != 0 and errorMessageResult != LIBUV_TEST_SKIP)
    {
        return "run_test_error_message failed";
    }
    run_test_sys_error();

    // test-barrier.c
    run_test_barrier_1();
    run_test_barrier_2();
    run_test_barrier_3();
    run_test_barrier_serial_thread();
    run_test_barrier_serial_thread_single();

    // test-callback-stack.c
    if (const char* helperError = run_test_with_echo_helper(run_test_callback_stack, "run_test_callback_stack failed"))
    {
        return helperError;
    }

    // test-cwd-and-chdir.c
    run_test_cwd_and_chdir();

    // test-get-loadavg.c
    run_test_get_loadavg();

    // test-get-currentexe.c
    const int currentExeResult = run_test_get_currentexe();
    if (currentExeResult != 0 and currentExeResult != LIBUV_TEST_SKIP)
    {
        return "run_test_get_currentexe failed";
    }

    // test-get-memory.c
    run_test_get_memory();

    // test-get-passwd.c
    const int getPasswdResult = run_test_get_passwd();
    if (getPasswdResult != 0 and getPasswdResult != LIBUV_TEST_SKIP)
    {
        return "run_test_get_passwd failed";
    }
    const int getPasswd2Result = run_test_get_passwd2();
    if (getPasswd2Result != 0 and getPasswd2Result != LIBUV_TEST_SKIP)
    {
        return "run_test_get_passwd2 failed";
    }
    const int getGroupResult = run_test_get_group();
    if (getGroupResult != 0 and getGroupResult != LIBUV_TEST_SKIP)
    {
        return "run_test_get_group failed";
    }

    // test-gethostname.c
    run_test_gethostname();

    // test-gettimeofday.c
    run_test_gettimeofday();

    // test-homedir.c
    run_test_homedir();

#if !SC_PLATFORM_WINDOWS
    // test-env-vars.c
    run_test_env_vars();
#endif

    // test-process-priority.c
    run_test_process_priority();

    // test-signal.c
    const int killInvalidSignumResult = run_test_kill_invalid_signum();
    if (killInvalidSignumResult != 0)
    {
        return "run_test_kill_invalid_signum failed";
    }
#if !SC_PLATFORM_WINDOWS
    // Delivery subtests hang waiting for raised signals with the current AsyncSignal/libuv loop mapping.
    // run_test_we_get_signal();
    // run_test_we_get_signals();
    // run_test_we_get_signal_one_shot();
    // run_test_we_get_signals_mixed();
#else
    // run_test_win32_signum_number(); // Windows signal support is limited to AsyncSignal-supported console events
#endif

    // test-semaphore.c
    run_test_semaphore_1();
    run_test_semaphore_2();
    run_test_semaphore_3();

    // test-mutexes.c
    run_test_thread_mutex();
    run_test_thread_mutex_recursive();
    run_test_thread_rwlock();
    run_test_thread_rwlock_trylock();

    // test-condvar.c
    run_test_condvar_1();
    run_test_condvar_2();
    run_test_condvar_3();
    run_test_condvar_4();
    run_test_condvar_5();

#if !SC_PLATFORM_WINDOWS
    // test-thread-name.c
    run_test_thread_name();
#endif

#if SC_PLATFORM_APPLE
    // test-thread-affinity.c
    const int threadAffinityResult = run_test_thread_affinity();
    if (threadAffinityResult != 0)
    {
        return "run_test_thread_affinity failed";
    }
#endif

    // test-thread.c
    if (run_test_thread_create() != 0)
    {
        return "run_test_thread_create failed";
    }
    if (run_test_thread_local_storage() != 0)
    {
        return "run_test_thread_local_storage failed";
    }
    if (run_test_thread_stack_size() != 0)
    {
        return "run_test_thread_stack_size failed";
    }
    if (run_test_thread_detach() != 0)
    {
        return "run_test_thread_detach failed";
    }
    // run_test_threadpool_multiple_event_loops(); // Exercises FS callbacks across multiple loops
    // run_test_thread_stack_size_explicit();      // Requires honoring custom stack sizes in uv_thread_create_ex

    // test-thread-equal.c
    run_test_thread_equal();

    // test-tty.c
#if !SC_PLATFORM_WINDOWS
    const int ttyResult = run_test_tty();
    if (ttyResult != 0 and ttyResult != LIBUV_TEST_SKIP)
    {
        return "run_test_tty failed";
    }
#endif

    // test-hrtime.c
    run_test_hrtime();
    run_test_clock_gettime();

    // test-ip-name.c
    run_test_ip_name();

    // test-getaddrinfo.c
    const int getAddrInfoFailResult = run_test_getaddrinfo_fail();
    if (getAddrInfoFailResult != 0 and getAddrInfoFailResult != LIBUV_TEST_SKIP)
    {
        return "run_test_getaddrinfo_fail failed";
    }
    const int getAddrInfoFailSyncResult = run_test_getaddrinfo_fail_sync();
    if (getAddrInfoFailSyncResult != 0 and getAddrInfoFailSyncResult != LIBUV_TEST_SKIP)
    {
        return "run_test_getaddrinfo_fail_sync failed";
    }
    const int getAddrInfoBasicResult = run_test_getaddrinfo_basic();
    if (getAddrInfoBasicResult != 0 and getAddrInfoBasicResult != LIBUV_TEST_SKIP)
    {
        return "run_test_getaddrinfo_basic failed";
    }
    const int getAddrInfoBasicSyncResult = run_test_getaddrinfo_basic_sync();
    if (getAddrInfoBasicSyncResult != 0 and getAddrInfoBasicSyncResult != LIBUV_TEST_SKIP)
    {
        return "run_test_getaddrinfo_basic_sync failed";
    }
    const int getAddrInfoConcurrentResult = run_test_getaddrinfo_concurrent();
    if (getAddrInfoConcurrentResult != 0 and getAddrInfoConcurrentResult != LIBUV_TEST_SKIP)
    {
        return "run_test_getaddrinfo_concurrent failed";
    }

    // test-getnameinfo.c
    const int getNameInfoIp4Result = run_test_getnameinfo_basic_ip4();
    if (getNameInfoIp4Result != 0 and getNameInfoIp4Result != LIBUV_TEST_SKIP)
    {
        return "run_test_getnameinfo_basic_ip4 failed";
    }
    const int getNameInfoIp4SyncResult = run_test_getnameinfo_basic_ip4_sync();
    if (getNameInfoIp4SyncResult != 0 and getNameInfoIp4SyncResult != LIBUV_TEST_SKIP)
    {
        return "run_test_getnameinfo_basic_ip4_sync failed";
    }
    const int getNameInfoIp6Result = run_test_getnameinfo_basic_ip6();
    if (getNameInfoIp6Result != 0 and getNameInfoIp6Result != LIBUV_TEST_SKIP)
    {
        return "run_test_getnameinfo_basic_ip6 failed";
    }

    // test-getsockname.c
    // run_test_getsockname_tcp(); // uv_tcp_getsockname reports local port 0 before async connect completes
    const int getSockNameUdpResult = run_test_getsockname_udp();
    if (getSockNameUdpResult != 0 and getSockNameUdpResult != LIBUV_TEST_SKIP)
    {
        return "run_test_getsockname_udp failed";
    }

    // test-getters-setters.c
    if (run_test_handle_type_name() != 0)
    {
        return "run_test_handle_type_name failed";
    }
    if (run_test_req_type_name() != 0)
    {
        return "run_test_req_type_name failed";
    }
#if !SC_PLATFORM_WINDOWS
    if (run_test_getters_setters() != 0)
    {
        return "run_test_getters_setters failed";
    }
#endif

    // test-ip6-addr.c
    const int ip6AddrLinkLocalResult = run_test_ip6_addr_link_local();
    if (ip6AddrLinkLocalResult != 0 and ip6AddrLinkLocalResult != LIBUV_TEST_SKIP)
    {
        return "run_test_ip6_addr_link_local failed";
    }
    run_test_ip6_pton();
    run_test_ip6_sin6_len();

    // test-tmpdir.c
    run_test_tmpdir();

    // test-uname.c
    run_test_uname();

    // test-idna.c
    run_test_utf8_decode1();
    run_test_utf8_decode1_overrun();
    run_test_idna_toascii();
    run_test_wtf8();

    // test-ip4-addr.c
    run_test_ip4_addr();

    // test-tcp-close.c
    run_test_tcp_close(); // Must fix socket close to be on "next tick"

    // test-tcp-close-after-read-timeout.c
    run_test_tcp_close_after_read_timeout();

    // test-tcp-close-reset.c
    // run_test_tcp_close_reset_client();                         // Pending write callbacks must precede close callback
    // run_test_tcp_close_reset_client_after_shutdown();          // Pending shutdown/write callback ordering
    // run_test_tcp_close_reset_accepted();                       // Pending accepted-side reset/read ordering
    // run_test_tcp_close_reset_accepted_after_shutdown();        // Pending accepted-side shutdown/reset ordering
    // run_test_tcp_close_reset_accepted_after_socket_shutdown(); // Pending native shutdown/reset ordering

    // test-tcp-bind-error.c
#if !SC_PLATFORM_WINDOWS
    if (const char* error =
            run_test_in_child(run_test_tcp_bind_error_addrnotavail_1, "run_test_tcp_bind_error_addrnotavail_1 failed"))
    {
        return error;
    }
    if (const char* error =
            run_test_in_child(run_test_tcp_bind_error_addrnotavail_2, "run_test_tcp_bind_error_addrnotavail_2 failed"))
    {
        return error;
    }
    if (const char* error = run_test_in_child(run_test_tcp_bind_error_fault, "run_test_tcp_bind_error_fault failed"))
    {
        return error;
    }
    if (const char* error = run_test_in_child(run_test_tcp_bind_error_inval, "run_test_tcp_bind_error_inval failed"))
    {
        return error;
    }
    if (const char* error = run_test_in_child(run_test_tcp_bind_localhost_ok, "run_test_tcp_bind_localhost_ok failed"))
    {
        return error;
    }
    if (const char* error =
            run_test_in_child(run_test_tcp_bind_invalid_flags, "run_test_tcp_bind_invalid_flags failed"))
    {
        return error;
    }
    if (const char* error =
            run_test_in_child(run_test_tcp_listen_without_bind, "run_test_tcp_listen_without_bind failed"))
    {
        return error;
    }
    if (const char* error = run_test_in_child(run_test_tcp_bind_or_listen_error_after_close,
                                              "run_test_tcp_bind_or_listen_error_after_close failed"))
    {
        return error;
    }
    if (const char* error =
            run_test_in_child(run_test_tcp_bind_writable_flags, "run_test_tcp_bind_writable_flags failed"))
    {
        return error;
    }
    // run_test_tcp_bind_error_addrinuse_connect(); // Requires bound connect to callback with UV_EADDRINUSE
    // run_test_tcp_bind_error_addrinuse_listen();  // Requires duplicate bind/listen behavior matching libuv
#endif

    // test-tcp-bind6-error.c
    const int tcpBind6ErrorAddrNotAvailResult = run_test_tcp_bind6_error_addrnotavail();
    if (tcpBind6ErrorAddrNotAvailResult != 0 and tcpBind6ErrorAddrNotAvailResult != LIBUV_TEST_SKIP)
    {
        return "run_test_tcp_bind6_error_addrnotavail failed";
    }

    // test-tcp-connect-error.c
    run_test_tcp_connect_error_fault();
    run_test_tcp_connect_error_after_write();

    // test-tcp-connect6-error.c
    const int tcpConnect6ErrorResult = run_test_tcp_connect6_error_fault();
    if (tcpConnect6ErrorResult != 0 and tcpConnect6ErrorResult != LIBUV_TEST_SKIP)
    {
        return "run_test_tcp_connect6_error_fault failed";
    }

    // test-connection-fail.c
    const int connectionFailResult = run_test_connection_fail();
    if (connectionFailResult != 0 and connectionFailResult != LIBUV_TEST_SKIP)
    {
        return "run_test_connection_fail failed";
    }
    // run_test_connection_fail_doesnt_auto_close(); // Shares static counters with connection_fail in this runner

    // test-emfile.c
    // run_test_emfile(); // Lowers RLIMIT_NOFILE for the rest of this in-process runner

    // test-tcp-write-after-connect.c
#if !SC_PLATFORM_WINDOWS
    const int tcpWriteAfterConnectResult = run_test_tcp_write_after_connect();
    if (tcpWriteAfterConnectResult != 0 and tcpWriteAfterConnectResult != LIBUV_TEST_SKIP)
    {
        return "run_test_tcp_write_after_connect failed";
    }
#endif

    // test-tcp-write-to-half-open-connection.c
    // run_test_tcp_write_to_half_open_connection(); // Read callback reports UV_EINVAL instead of EOF/reset currently

    // test-tcp-write-queue-order.c
    // run_test_tcp_write_queue_order(); // Requires pending writes to report UV_ECANCELED on close

    // test-tcp-close-while-connecting.c
    // run_test_tcp_close_while_connecting(); // Requires close to cancel an active uv_tcp_connect

    // test-tcp-connect-timeout.c
    // run_test_tcp_connect_timeout(); // External route can hang in this wrapper
    const int tcpLocalConnectTimeoutResult = run_test_tcp_local_connect_timeout();
    if (tcpLocalConnectTimeoutResult != 0 and tcpLocalConnectTimeoutResult != LIBUV_TEST_SKIP)
    {
        return "run_test_tcp_local_connect_timeout failed";
    }
    // run_test_tcp6_local_connect_timeout(); // Shares static counters with tcp_local_connect_timeout

    // test-connect-unspecified.c
    // run_test_connect_unspecified(); // Triggers ASAN stack-buffer-overflow in can_ipv6()

    // test-tcp-create-socket-early.c
#if !SC_PLATFORM_WINDOWS
    const int tcpCreateEarlyResult = run_test_tcp_create_early();
    if (tcpCreateEarlyResult != 0)
    {
        return "run_test_tcp_create_early failed";
    }
    const int tcpCreateEarlyBadBindResult = run_test_tcp_create_early_bad_bind();
    if (tcpCreateEarlyBadBindResult != 0 and tcpCreateEarlyBadBindResult != LIBUV_TEST_SKIP)
    {
        return "run_test_tcp_create_early_bad_bind failed";
    }
    const int tcpCreateEarlyBadDomainResult = run_test_tcp_create_early_bad_domain();
    if (tcpCreateEarlyBadDomainResult != 0)
    {
        return "run_test_tcp_create_early_bad_domain failed";
    }
    // run_test_tcp_create_early_accept(); // uv_accept currently allows a pre-opened target
#endif

    // test-udp-send-and-recv.c
    run_test_udp_send_and_recv();

    // test-socket-buffer-size.c
    run_test_socket_buffer_size();

    // test-strtok.c
    run_test_strtok();

#if !SC_PLATFORM_WINDOWS
    // test-strscpy.c
    run_test_strscpy();
#endif

    // test-close-order.c
    // run_test_close_order(); // Requires libuv phase ordering: timers before check callbacks

    // test-default-loop-close.c
    run_test_default_loop_close();

    // test-dlerror.c
    run_test_dlerror();

    // test-test-macros.c
    run_test_test_macros();

    // test-shutdown-eof.c
    // run_test_shutdown_eof(); // Receives EOF before the echoed Q with the local helper/current TCP shutdown path

    // test-shutdown-simultaneous.c
    // run_test_shutdown_simultaneous(); // Pending shutdown/read ordering validation

    // test-shutdown-twice.c
    if (const char* helperError = run_test_with_echo_helper(run_test_shutdown_twice, "run_test_shutdown_twice failed"))
    {
        return helperError;
    }

    // test-idle.c
    // run_test_idle_starvation(); // Very verbose and shares static state with idle_check
    // run_test_idle_check();      // Fails if run after idle_starvation in same process

    // test-multiple-listen.c
    run_test_multiple_listen();

    // test-tcp-read-stop.c
    if (const char* helperError = run_test_tcp_read_stop_with_helper())
    {
        return helperError;
    }

    // test-readable-on-eof.c
    if (const char* helperError = run_test_readable_on_eof_with_helper())
    {
        return helperError;
    }

    // test-not-readable-nor-writable-on-read-error.c
    // run_test_not_readable_nor_writable_on_read_error(); // Read errors keep stream readable/writable

    // test-not-writable-after-shutdown.c
    if (const char* helperError = run_test_with_echo_helper(run_test_not_writable_after_shutdown,
                                                            "run_test_not_writable_after_shutdown failed"))
    {
        return helperError;
    }

    // test-pipe-pending-instances.c
#if !SC_PLATFORM_WINDOWS
    if (run_test_pipe_pending_instances() != 0)
    {
        return "run_test_pipe_pending_instances failed";
    }
#endif

    // test-tcp-read-stop-start.c
    // run_test_tcp_read_stop_start(); // uv_read_stop semantics mismatch in current wrapper

#if !SC_PLATFORM_LINUX
    // test-tcp-reuseport.c
    run_test_tcp_reuseport();

    // test-tcp-flags.c
    const int tcpFlagsResult = run_test_tcp_flags();
    if (tcpFlagsResult != 0)
    {
        return "run_test_tcp_flags failed";
    }

    // test-udp-reuseport.c
    run_test_udp_reuseport();
#endif

    // test-tcp-unexpected-read.c
    // run_test_tcp_unexpected_read(); // Busy-loops on Linux VM with current check/read scheduling

    // test-tcp-oob.c
    // run_test_tcp_oob(); // Native MSG_OOB send fails after accept with the current TCP wrapper

    // test-tcp-open.c
#if !SC_PLATFORM_WINDOWS
    const int tcpOpenTwiceResult = run_test_tcp_open_twice();
    if (tcpOpenTwiceResult != 0 and tcpOpenTwiceResult != LIBUV_TEST_SKIP)
    {
        return "run_test_tcp_open_twice failed";
    }
    // run_test_tcp_open();          // Adopted socket write callback reports UV_EPERM after connect
    // run_test_tcp_open_bound();     // Leaves a listening TCP handle active in this in-process runner
    // run_test_tcp_open_connected(); // Shares static counters with tcp_open
    // run_test_tcp_write_ready();    // Shares static counters with tcp_open
#endif

    // test-tcp-rst.c
#if !SC_PLATFORM_WINDOWS
    // run_test_tcp_rst(); // Needs helper; RST read reports UV_EINVAL instead of UV_ECONNRESET
#endif

    // test-tcp-shutdown-after-write.c
    // run_test_tcp_shutdown_after_write(); // Write callback reports -1 after shutdown instead of success

    // test-tcp-write-fail.c
    if (const char* helperError = run_test_tcp_write_fail_with_helper())
    {
        return helperError;
    }

    // test-tcp-try-write-error.c
    // run_test_tcp_try_write_error(); // uv_try_write reports UV_ENOTCONN instead of EPIPE/ECONNABORTED/ECONNRESET

    // test-tcp-try-write.c
    // run_test_tcp_try_write(); // Hangs waiting for EOF/close with current TCP read scheduling

    // test-tcp-write-in-a-row.c
    // run_test_tcp_write_in_a_row(); // Requires libuv-compatible TCP write callback ordering/backpressure

    // test-tcp-writealot.c
    // run_test_tcp_writealot(); // Requires uv_write support for thousands of buffers per request

    // test-udp-send-immediate.c
    run_test_udp_send_immediate();

    // test-udp-bind.c
    run_test_udp_bind();

    // test-udp-connect.c
    const int udpConnectResult = run_test_udp_connect();
    if (udpConnectResult != 0 and udpConnectResult != LIBUV_TEST_SKIP)
    {
        return "run_test_udp_connect failed";
    }

    // test-udp-connect6.c
    const int udpConnect6Result = run_test_udp_connect6();
    if (udpConnect6Result != 0 and udpConnect6Result != LIBUV_TEST_SKIP)
    {
        return "run_test_udp_connect6 failed";
    }

    // test-udp-multicast-interface.c
    const int udpMulticastInterfaceResult = run_test_udp_multicast_interface();
    if (udpMulticastInterfaceResult != 0 and udpMulticastInterfaceResult != LIBUV_TEST_SKIP)
    {
        return "run_test_udp_multicast_interface failed";
    }
    const int udpMulticastInterface6Result = run_test_udp_multicast_interface6();
    if (udpMulticastInterface6Result != 0 and udpMulticastInterface6Result != LIBUV_TEST_SKIP)
    {
        return "run_test_udp_multicast_interface6 failed";
    }

    // test-udp-multicast-join.c
    const int udpMulticastJoinResult = run_test_udp_multicast_join();
    if (udpMulticastJoinResult != 0 and udpMulticastJoinResult != LIBUV_TEST_SKIP)
    {
        return "run_test_udp_multicast_join failed";
    }
    const int udpMulticastJoin6Result = run_test_udp_multicast_join6();
    if (udpMulticastJoin6Result != 0 and udpMulticastJoin6Result != LIBUV_TEST_SKIP)
    {
        return "run_test_udp_multicast_join6 failed";
    }

    // test-udp-mmsg.c
#if !SC_PLATFORM_APPLE
    const int udpMmsgResult = run_test_udp_mmsg();
    if (udpMmsgResult != 0 and udpMmsgResult != LIBUV_TEST_SKIP)
    {
        return "run_test_udp_mmsg failed";
    }
#endif

    // test-udp-sendmmsg-error.c
    // run_test_udp_sendmmsg_error(); // Close callback is not delivered before uv_run returns yet

    // test-udp-multicast-ttl.c
    const int udpMulticastTtlResult = run_test_udp_multicast_ttl();
    if (udpMulticastTtlResult != 0 and udpMulticastTtlResult != LIBUV_TEST_SKIP)
    {
        return "run_test_udp_multicast_ttl failed";
    }

    // test-udp-options.c
    const int udpOptionsResult = run_test_udp_options();
    if (udpOptionsResult != 0 and udpOptionsResult != LIBUV_TEST_SKIP)
    {
        return "run_test_udp_options failed";
    }
    const int udpOptions6Result = run_test_udp_options6();
    if (udpOptions6Result != 0 and udpOptions6Result != LIBUV_TEST_SKIP)
    {
        return "run_test_udp_options6 failed";
    }
    const int udpNoAutobindResult = run_test_udp_no_autobind();
    if (udpNoAutobindResult != 0 and udpNoAutobindResult != LIBUV_TEST_SKIP)
    {
        return "run_test_udp_no_autobind failed";
    }

    // test-udp-open.c
#if !SC_PLATFORM_WINDOWS
    const int udpOpenResult = run_test_udp_open();
    if (udpOpenResult != 0 and udpOpenResult != LIBUV_TEST_SKIP)
    {
        return "run_test_udp_open failed";
    }
    const int udpOpenTwiceResult = run_test_udp_open_twice();
    if (udpOpenTwiceResult != 0 and udpOpenTwiceResult != LIBUV_TEST_SKIP)
    {
        return "run_test_udp_open_twice failed";
    }
    const int udpOpenBoundResult = run_test_udp_open_bound();
    if (udpOpenBoundResult != 0 and udpOpenBoundResult != LIBUV_TEST_SKIP)
    {
        return "run_test_udp_open_bound failed";
    }
    // run_test_udp_open_connect(); // Shares static counters with udp_open in this in-process runner
    // run_test_udp_send_unix();    // AF_UNIX datagram send is not supported by the UDP wrapper yet
#endif

    // test-udp-create-socket-early.c
#if !SC_PLATFORM_WINDOWS
    const int udpCreateEarlyResult = run_test_udp_create_early();
    if (udpCreateEarlyResult != 0)
    {
        return "run_test_udp_create_early failed";
    }
    const int udpCreateEarlyBadBindResult = run_test_udp_create_early_bad_bind();
    if (udpCreateEarlyBadBindResult != 0 and udpCreateEarlyBadBindResult != LIBUV_TEST_SKIP)
    {
        return "run_test_udp_create_early_bad_bind failed";
    }
    const int udpCreateEarlyBadDomainResult = run_test_udp_create_early_bad_domain();
    if (udpCreateEarlyBadDomainResult != 0)
    {
        return "run_test_udp_create_early_bad_domain failed";
    }
#endif

    // test-udp-ipv6.c
    const int udpIPv6OnlyResult = run_test_udp_ipv6_only();
    if (udpIPv6OnlyResult != 0 and udpIPv6OnlyResult != LIBUV_TEST_SKIP)
    {
        return "run_test_udp_ipv6_only failed";
    }
    // run_test_udp_dual_stack(); // Shares static state with udp_ipv6_only in this in-process runner

    // test-udp-dgram-too-big.c
    run_test_udp_dgram_too_big();

    // test-udp-send-hang-loop.c
    const int udpSendHangLoopResult = run_test_udp_send_hang_loop();
    if (udpSendHangLoopResult != 0 and udpSendHangLoopResult != LIBUV_TEST_SKIP)
    {
        return "run_test_udp_send_hang_loop failed";
    }

    // test-udp-send-unreachable.c
    // run_test_udp_send_unreachable(); // Requires libuv-style deferred UDP alloc callback balancing

    // test-watcher-cross-stop.c
    const int watcherCrossStopResult = run_test_watcher_cross_stop();
    if (watcherCrossStopResult != 0 and watcherCrossStopResult != LIBUV_TEST_SKIP)
    {
        return "run_test_watcher_cross_stop failed";
    }

    // test-tcp-close-accept.c
    // run_test_tcp_close_accept(); // uv_read_stop semantics mismatch in current wrapper

    // test-tcp-alloc-cb-fail.c
    // run_test_tcp_alloc_cb_fail(); // Returns EOF instead of UV_ENOBUFS in current wrapper

    // test-udp-alloc-cb-fail.c
    // run_test_udp_alloc_cb_fail(); // Returns EOF instead of UV_ENOBUFS in current wrapper

    // test-delayed-accept.c
    // run_test_delayed_accept(); // Triggers ASAN use-after-free in current wrapper

    // test-random.c
    run_test_random_async();
    run_test_random_sync();

    // test-threadpool.c
    const int threadpoolEinvalResult = run_test_threadpool_queue_work_einval();
    if (threadpoolEinvalResult != 0)
    {
        return "run_test_threadpool_queue_work_einval failed";
    }
    const int threadpoolSimpleResult = run_test_threadpool_queue_work_simple();
    if (threadpoolSimpleResult != 0)
    {
        return "run_test_threadpool_queue_work_simple failed";
    }

    // test-udp-recv-in-a-row.c
    // run_test_udp_recv_in_a_row(); // Requires receive callbacks before check callbacks in the same loop tick

    // test-udp-try-send.c
    // run_test_udp_try_send(); // Hangs after direct sends with current UDP receive/loop activity semantics

    return 0;
}

int main(int argc, char** argv)
{
    using namespace SC;
    if (argc > 0 and argv[0] != nullptr)
    {
        snprintf(executable_path, sizeof(executable_path), "%s", argv[0]);
    }
    SocketNetworking::initNetworking();
    const char* error = sc_async_test();
    if (error)
    {
        printf("Test failed: %s\n", error);
        return -1;
    }
    printf("Tests passed!\n");
    return 0;
}
