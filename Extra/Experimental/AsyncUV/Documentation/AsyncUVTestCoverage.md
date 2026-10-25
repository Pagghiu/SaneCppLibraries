@page page_async_uv_test_coverage libuv Test Coverage Plan (SC::Async C Wrapper)

[TOC]

This page tracks progress for the source-compatible `uv` wrapper implemented in `Extra/Experimental/AsyncUV/Code/uv.h`.

The standalone runner also checks on Linux that an immediate UDP send error survives changes to `errno`
before its deferred callback, and that the callback is not invoked synchronously from `uv_udp_send`.
On POSIX, a native multicast send probes route availability before IPv4 multicast delivery tests.
If the native socket reports a missing route or permission denial, the runner verifies that AsyncUV delivers
the same error asynchronously and skips only the three IPv4 multicast delivery cases with an explicit reason.
IPv6 cases retain their upstream capability checks.

# Current Snapshot
- Upstream inventory discovered from `_Build/_PackagesCache/libuv/*/test/test-*.c`: **175** tests
- Tests currently wired into `SCAsyncUVTest` build (`Tools/SC-build.cpp`): **128** tests
- Missing tests that can be attempted with current API surface: **0**
- Missing tests that require new `uv` APIs first: **48**

# AI Agent Workflow (Coverage Extension)
When extending libuv test coverage, use this decision rule:

1. If a missing test can be implemented using the **existing** `uv.h` surface and needs only wrapper-side work, proceed directly.
2. If a test needs **new `uv` APIs** or implies **substantial `SC::Async` design/state-machine changes**, stop and present a plan to the user before implementing.
3. Small and clear bug fixes in `SC::Async` needed to make an existing API behave correctly are allowed without a separate plan.

# Wired Tests (128)

These are already included in `SCAsyncUVTest` build setup.

- [x] `test-pass-always.c`
- [x] `test-platform-output.c`
- [x] `test-active.c`
- [x] `test-process-title-threadsafe.c`
- [x] `test-timer.c` (multiple timer sub-tests)
- [x] `test-timer-again.c`
- [x] `test-timer-from-check.c`
- [x] `test-run-nowait.c`
- [x] `test-run-once.c`
- [x] `test-walk-handles.c`
- [x] `test-loop-handles.c`
- [x] `test-loop-alive.c`
- [x] `test-loop-stop.c` (includes `loop_stop_before_run`)
- [ ] `test-loop-close.c` (partial: `loop_close` executed, `loop_instant_close` disabled because it intentionally leaves queued work active)
- [x] `test-loop-configure.c`
- [ ] `test-loop-time.c` (partial: `loop_update_time` executed, `loop_backend_timeout` disabled)
- [ ] `test-async.c` (wired but disabled; requires repeated async wakeups to keep the loop alive until close)
- [x] `test-async-null-cb.c`
- [x] `test-error.c` (includes `error_message`, `sys_error`)
- [ ] `test-fail-always.c` (wired but disabled; intentionally fails to test the upstream runner)
- [x] `test-barrier.c` (includes `barrier_1`, `barrier_2`, `barrier_3`, `barrier_serial_thread`, `barrier_serial_thread_single`)
- [x] `test-cwd-and-chdir.c`
- [x] `test-get-currentexe.c`
- [x] `test-get-loadavg.c`
- [x] `test-get-memory.c`
- [x] `test-get-passwd.c` (includes `get_passwd`, `get_passwd2`, `get_group`)
- [x] `test-gethostname.c`
- [x] `test-gettimeofday.c`
- [x] `test-homedir.c`
- [x] `test-env-vars.c` (non-Windows; includes environment enumeration)
- [x] `test-hrtime.c` (includes `hrtime`, `clock_gettime`)
- [x] `test-ip-name.c`
- [x] `test-getaddrinfo.c` (includes failure, sync, async, and concurrent cases)
- [x] `test-getnameinfo.c` (includes IPv4 sync/async and IPv6 async cases)
- [ ] `test-getsockname.c` (partial: UDP subtest executed; TCP subtest disabled because `uv_tcp_getsockname` reports local port 0 before async connect completes)
- [ ] `test-getters-setters.c` (partial: handle/req type names execute on all platforms; full getters/setters subtest executes on non-Windows with minimal pipe/fs stat support)
- [x] `test-ip6-addr.c` (includes `ip6_addr_link_local`, `ip6_pton`, `ip6_sin6_len`)
- [x] `test-tmpdir.c`
- [x] `test-uname.c`
- [x] `test-idna.c` (includes `utf8_decode1`, `utf8_decode1_overrun`, `idna_toascii`, `wtf8`)
- [x] `test-process-priority.c`
- [ ] `test-signal.c` (partial: `kill_invalid_signum` executed; delivery subtests disabled because the current
      `uv_signal`/`AsyncSignal` mapping hangs waiting for raised signals)
- [x] `test-semaphore.c` (includes `semaphore_1`, `semaphore_2`, `semaphore_3`)
- [x] `test-mutexes.c` (includes `thread_mutex`, `thread_mutex_recursive`, `thread_rwlock`, `thread_rwlock_trylock`)
- [x] `test-condvar.c` (includes `condvar_1` through `condvar_5`)
- [x] `test-thread-name.c` (non-Windows)
- [x] `test-thread-equal.c`
- [x] `test-ip4-addr.c`
- [x] `test-tcp-close.c`
- [x] `test-tcp-close-after-read-timeout.c`
- [x] `test-tcp-connect-error.c` (via `run_test_tcp_connect_error_fault`)
- [x] `test-tcp-connect-error-after-write.c`
- [x] `test-tcp-connect6-error.c` (via `run_test_tcp_connect6_error_fault`)
- [ ] `test-tcp-bind6-error.c` (partial: `tcp_bind6_error_addrnotavail` executed with skip handling when IPv6 is unavailable)
- [ ] `test-tcp-bind-error.c` (partial on non-Windows: `addrnotavail_1`, `addrnotavail_2`, `fault`, `inval`, `localhost_ok`, `invalid_flags`, `listen_without_bind`, `bind_or_listen_error_after_close`, and `bind_writable_flags` execute in child processes; `addrinuse_connect` and `addrinuse_listen` disabled pending duplicate bind/connect/listen semantics)
- [ ] `test-connection-fail.c` (partial: `connection_fail` executed, `connection_fail_doesnt_auto_close` disabled because upstream subtests share static counters in this runner)
- [ ] `test-emfile.c` (wired but disabled; lowers `RLIMIT_NOFILE` for the whole process and is unsafe in this in-process runner)
- [x] `test-tcp-write-after-connect.c`
- [ ] `test-tcp-write-to-half-open-connection.c` (wired but disabled; read callback reports `UV_EINVAL` instead of EOF/reset)
- [ ] `test-tcp-write-queue-order.c` (wired but disabled; pending writes do not report `UV_ECANCELED` on close)
- [ ] `test-tcp-close-while-connecting.c` (wired but disabled; close does not cancel an active `uv_tcp_connect` with libuv-compatible status yet)
- [ ] `test-tcp-close-reset.c` (wired but disabled; `uv_tcp_close_reset` exists, but subtests need libuv-compatible pending write/shutdown/read callback ordering)
- [ ] `test-tcp-connect-timeout.c` (partial: local connect timeout executed; external route subtest disabled because it can hang in this wrapper)
- [ ] `test-connect-unspecified.c` (wired but disabled; triggers ASAN stack-buffer-overflow in `can_ipv6()`)
- [ ] `test-tcp-create-socket-early.c` (non-Windows partial: early create/bad bind/bad domain executed, pre-opened accept target case disabled)
- [x] `test-tcp-flags.c`
- [x] `test-socket-buffer-size.c`
- [x] `test-strtok.c`
- [x] `test-strscpy.c`
- [ ] `test-close-order.c` (wired but disabled; requires libuv phase ordering: timers before check callbacks)
- [x] `test-default-loop-close.c`
- [x] `test-dlerror.c`
- [ ] `test-idle.c` (wired but disabled; upstream subtests share static state when run in same process)
- [x] `test-test-macros.c`
- [x] `test-callback-stack.c` (executed with local helper server)
- [ ] `test-shutdown-eof.c` (wired but disabled; receives EOF before the echoed `Q` with the local helper/current TCP shutdown path)
- [ ] `test-shutdown-simultaneous.c` (wired but disabled; pending shutdown/read ordering validation)
- [x] `test-shutdown-twice.c` (executed with local helper server)
- [x] `test-multiple-listen.c`
- [x] `test-tcp-read-stop.c` (executed with local helper server)
- [ ] `test-tcp-read-stop-start.c` (wired but disabled; `uv_read_stop` returns `UV_EINVAL`)
- [ ] `test-tcp-unexpected-read.c` (wired but disabled; busy-loops on Linux VM with current check/read scheduling)
- [ ] `test-tcp-oob.c` (wired on non-Windows but disabled; `uv_stream_set_blocking` and deferred `uv_tcp_nodelay` are present, but native `MSG_OOB` send still fails after accept with the current TCP wrapper)
- [ ] `test-tcp-open.c` (non-Windows partial: `tcp_open_twice` executed; main open/write path reports `UV_EPERM` after connect; bound/connected/write_ready subtests remain disabled for in-process/static-state reasons)
- [ ] `test-tcp-rst.c` (wired on non-Windows but disabled; when attempted with a reset-on-command helper, the read callback reported `UV_EINVAL` instead of `UV_ECONNRESET`)
- [ ] `test-tcp-shutdown-after-write.c` (wired but disabled; write callback reports `-1` after shutdown instead of success)
- [ ] `test-tcp-try-write-error.c` (wired but disabled; `uv_try_write` reports `UV_ENOTCONN` instead of `UV_EPIPE`/`UV_ECONNABORTED`/`UV_ECONNRESET`)
- [ ] `test-tcp-try-write.c` (wired but disabled; `uv_try_write` exists, but the upstream test hangs waiting for EOF/close with current TCP read scheduling)
- [x] `test-tcp-write-fail.c` (executed with a local helper server; POSIX client runs in a child process
      so the helper cannot reuse the client's externally closed descriptor)
- [ ] `test-tcp-write-in-a-row.c` (wired but disabled; requires libuv-compatible TCP write callback ordering/backpressure)
- [ ] `test-tcp-writealot.c` (wired but disabled; requires `uv_write` support for thousands of buffers per request)
- [x] `test-readable-on-eof.c`
- [ ] `test-not-readable-nor-writable-on-read-error.c` (wired but disabled; read errors currently leave `uv_is_readable`/`uv_is_writable` true)
- [x] `test-not-writable-after-shutdown.c` (executed with local helper server)
- [ ] `test-pipe-pending-instances.c` (wired on non-Windows; uses minimal pipe bind/listen/pending-instance support without real IPC accept/connect)
- [x] `test-udp-send-and-recv.c`
- [x] `test-udp-connect.c`
- [ ] `test-udp-connect6.c` (wired; skips when IPv6 is unavailable)
- [x] `test-udp-multicast-interface.c`
- [ ] `test-udp-multicast-interface6.c` (wired; currently skips because `uv_interface_addresses` returns an empty list)
- [ ] `test-udp-multicast-join.c` (wired; may skip on macOS due the upstream `IP_ADD_SOURCE_MEMBERSHIP` `EBUSY` path)
- [ ] `test-udp-multicast-join6.c` (wired; skips when no external IPv6 interface is available; IPv6 source membership still returns `UV_ENOSYS`)
- [ ] `test-udp-mmsg.c` (wired on non-Apple; uses the non-`recvmmsg` path; macOS disabled because the pre-loop burst hits `EAGAIN` in `uv_udp_try_send`)
- [ ] `test-udp-sendmmsg-error.c` (wired but disabled; connected UDP send callbacks pass, but close callback ordering is not libuv-compatible yet)
- [x] `test-udp-multicast-ttl.c`
- [x] `test-udp-options.c` (includes `udp_options`, `udp_options6`, `udp_no_autobind`; IPv6 subtest may skip)
- [ ] `test-udp-open.c` (partial on non-Windows: `udp_open`, `udp_open_twice`, `udp_open_bound` executed; Windows runtime disabled because the pre-opened UDP socket path stalls after send/receive; `udp_open_connect` disabled because upstream subtests share static counters in this runner; `udp_send_unix` disabled because AF_UNIX datagram send is not supported by this UDP wrapper)
- [x] `test-udp-send-immediate.c`
- [x] `test-udp-bind.c`
- [x] `test-udp-create-socket-early.c` (non-Windows; includes bad bind/domain cases when IPv6 is available)
- [ ] `test-udp-ipv6.c` (partial: `udp_ipv6_only` executed, `udp_dual_stack` disabled because upstream subtests share static counters in this runner)
- [x] `test-udp-dgram-too-big.c`
- [x] `test-udp-send-hang-loop.c`
- [ ] `test-udp-send-unreachable.c` (wired but disabled; requires libuv-style deferred UDP alloc callback balancing)
- [x] `test-watcher-cross-stop.c`
- [ ] `test-tcp-close-accept.c` (wired but disabled; `uv_read_stop` returns `UV_EINVAL`)
- [ ] `test-tcp-alloc-cb-fail.c` (wired but disabled; returns `UV_EOF` instead of `UV_ENOBUFS`)
- [ ] `test-udp-alloc-cb-fail.c` (wired but disabled; returns `UV_EOF` instead of `UV_ENOBUFS`)
- [ ] `test-delayed-accept.c` (wired but disabled; triggers ASAN use-after-free currently)
- [x] `test-random.c` (includes `random_async`, `random_sync`)
- [x] `test-threadpool.c` (includes `threadpool_queue_work_einval`, `threadpool_queue_work_simple`)
- [x] `test-thread-affinity.c` (Apple only; verifies unsupported affinity reports `UV_ENOTSUP`)
- [ ] `test-thread.c` (partial: `thread_create`, `thread_local_storage`, `thread_stack_size`, and `thread_detach`
      executed; multi-loop threadpool and explicit stack-size subtests disabled pending deeper behavior)
- [ ] `test-tty.c` (non-Windows partial: base `tty` subtest executes or skips when `/dev/tty` is unavailable;
      `tty_file`/`tty_pty` remain disabled pending fuller descriptor ownership and pty behavior)
- [ ] `test-udp-recv-in-a-row.c` (wired but disabled; requires receive callbacks before check callbacks in the same loop tick)
- [ ] `test-udp-try-send.c` (wired but disabled; hangs after direct sends with current UDP receive/loop activity semantics)
- [x] `test-tcp-reuseport.c` (non-Linux platforms; verifies unsupported reuseport reports `UV_ENOTSUP`)
- [x] `test-udp-reuseport.c` (non-Linux platforms; verifies unsupported reuseport reports `UV_ENOTSUP`)

`test-fs-fd-hash.c` is excluded: it tests libuv's private Windows descriptor hash implementation,
which AsyncUV does not use, and requires libuv's internal handle layouts.

# Missing Tests: Existing API Surface (0)

All current-surface tests are now wired into the build. Some remain disabled or partial due to behavioral gaps tracked above.

# Missing Tests: New API Required First (49)

These tests reference `uv_*` symbols not currently exposed by the wrapper header.

- [ ] `test-close-fd.c` (missing symbols: `uv_file, uv_fs_req_cleanup, uv_fs_t, uv_fs_write, uv_pipe`)
- [ ] `test-eintr-handling.c` (missing symbols: `uv_barrier_t, uv_fs_read, uv_fs_t, uv_signal_init, uv_signal_start`)
- [ ] `test-embed.c` (missing symbols: `uv_backend_fd, uv_barrier_destroy, uv_barrier_init, uv_barrier_t, uv_barrier_wait`)
- [ ] `test-fork.c` (missing symbols: `uv_file, uv_fs_close, uv_fs_event_init, uv_fs_event_start, uv_fs_event_t`)
- [ ] `test-fs-copyfile.c` (missing symbols: `uv_file, uv_fs_chmod, uv_fs_close, uv_fs_copyfile, uv_fs_open`)
- [ ] `test-fs-event.c` (missing symbols: `uv_file, uv_fs_close, uv_fs_event_getpath, uv_fs_event_init, uv_fs_event_start`)
- [ ] `test-fs-open-flags.c` (missing symbols: `uv_fs_close, uv_fs_mkdir, uv_fs_open, uv_fs_read, uv_fs_req_cleanup`)
- [ ] `test-fs-poll.c` (missing symbols: `uv_fs_poll_getpath, uv_fs_poll_init, uv_fs_poll_start, uv_fs_poll_stop, uv_fs_poll_t`)
- [ ] `test-fs-readdir.c` (missing symbols: `uv_dir_t, uv_dirent_t, uv_fs_, uv_fs_close, uv_fs_closedir`)
- [ ] `test-fs.c` (missing symbols: `uv_cwd, uv_dirent_t, uv_file, uv_fs_access, uv_fs_cb`)
- [ ] `test-handle-fileno.c` (missing symbols: `uv_pipe_bind, uv_pipe_init, uv_pipe_t, uv_tty_init, uv_tty_t`)
- [ ] `test-iouring-pollhup.c` (missing symbols: `uv_pipe_init, uv_pipe_open, uv_pipe_t`)
- [ ] `test-ipc-heavy-traffic-deadlock-bug.c` (missing symbols: `uv_pipe_init, uv_pipe_open, uv_pipe_t, uv_process_t, uv_shutdown`)
- [ ] `test-ipc-send-recv.c` (missing symbols: `uv_is_readable, uv_is_writable, uv_pipe_bind, uv_pipe_connect, uv_pipe_init`)
- [ ] `test-ipc.c` (missing symbols: `uv_exepath, uv_is_readable, uv_is_writable, uv_pipe_init, uv_pipe_open`)
- [ ] `test-metrics.c` (missing symbols: `uv_async, uv_freeaddrinfo, uv_fs_close, uv_fs_open, uv_fs_req_cleanup`)
- [ ] `test-osx-select.c` (missing symbols: `uv_tty_init, uv_tty_t`)
- [ ] `test-ping-pong.c` (missing symbols: `uv_pipe, uv_pipe_connect, uv_pipe_init, uv_pipe_open, uv_pipe_t`)
- [ ] `test-pipe-bind-error.c` (missing symbols: `uv_pipe_bind, uv_pipe_bind2, uv_pipe_connect, uv_pipe_connect2, uv_pipe_init`)
- [ ] `test-pipe-close-stdout-read-stdin.c` (missing symbols: `uv_pipe_init, uv_pipe_open, uv_pipe_t`)
- [ ] `test-pipe-connect-error.c` (missing symbols: `uv_pipe_connect, uv_pipe_init, uv_pipe_t`)
- [ ] `test-pipe-connect-multiple.c` (missing symbols: `uv_pipe_bind, uv_pipe_connect, uv_pipe_init, uv_pipe_t`)
- [ ] `test-pipe-connect-prepare.c` (missing symbols: `uv_pipe_connect, uv_pipe_init, uv_pipe_t`)
- [ ] `test-pipe-getsockname.c` (missing symbols: `uv_pipe_bind2, uv_pipe_connect2, uv_pipe_getpeername, uv_pipe_getsockname, uv_pipe_init`)
- [ ] `test-pipe-sendmsg.c` (missing symbols: `uv_os_sock_t, uv_pipe_init, uv_pipe_open, uv_pipe_pending_count, uv_pipe_pending_type`)
- [ ] `test-pipe-server-close.c` (missing symbols: `uv_pipe_bind, uv_pipe_connect, uv_pipe_init, uv_pipe_t`)
- [ ] `test-pipe-set-fchmod.c` (missing symbols: `uv_pipe_bind, uv_pipe_chmod, uv_pipe_init, uv_pipe_t`)
- [ ] `test-pipe-set-non-blocking.c` (missing symbols: `uv_barrier_destroy, uv_barrier_init, uv_barrier_t, uv_barrier_wait, uv_errno`)
- [ ] `test-poll-close-doesnt-corrupt-stack.c` (missing symbols: `uv_os_sock_t, uv_poll_init_socket, uv_poll_start, uv_poll_t`)
- [ ] `test-poll-close.c` (missing symbols: `uv_os_sock_t, uv_poll_init_socket, uv_poll_start, uv_poll_t`)
- [ ] `test-poll-closesocket.c` (missing symbols: `uv_os_sock_t, uv_poll_init_socket, uv_poll_start, uv_poll_t`)
- [ ] `test-poll-multiple-handles.c` (missing symbols: `uv_os_sock_t, uv_poll_init_socket, uv_poll_start, uv_poll_stop, uv_poll_t`)
- [ ] `test-poll-oob.c` (missing symbols: `uv_fileno, uv_os_fd_t, uv_poll_init_socket, uv_poll_start, uv_poll_stop`)
- [ ] `test-poll.c` (missing symbols: `uv_os_sock_t, uv_poll_cb, uv_poll_init, uv_poll_init_socket, uv_poll_start`)
- [ ] `test-process-title.c` (missing symbols: `uv_exit_cb, uv_process_options_t, uv_process_t, uv_spawn`)
- [ ] `test-queue-foreach-delete.c` (missing symbols: `uv_fs_event_cb, uv_fs_event_init, uv_fs_event_start, uv_fs_event_t, uv_fs_req_cleanup`)
- [ ] `test-ref.c` (missing symbols: `uv_exepath, uv_fs_event_cb, uv_fs_event_init, uv_fs_event_start, uv_fs_event_t`)
- [ ] `test-shutdown-close.c` (missing symbols: `uv_pipe_connect, uv_pipe_init, uv_pipe_t, uv_shutdown, uv_shutdown_t`)
- [ ] `test-signal-multiple-loops.c` (missing symbols: `uv_kill, uv_signal_init, uv_signal_start, uv_signal_t`)
- [ ] `test-signal-pending-on-close.c` (missing symbols: `uv_kill, uv_os_getpid, uv_pipe_init, uv_pipe_open, uv_pipe_t`)
- [ ] `test-spawn.c` (missing symbols: `uv_exepath, uv_exit_cb, uv_file, uv_fileno, uv_fs_close`)
- [ ] `test-stdio-over-pipes.c` (missing symbols: `uv_exepath, uv_exit_cb, uv_pipe_init, uv_pipe_open`)
- [ ] `test-thread-priority.c` (missing symbols: `UV_THREAD_PRIORITY_LOWEST, uv_thread_getpriority, uv_thread_setpriority`)
- [ ] `test-threadpool-cancel.c` (missing symbols: `uv_cancel, uv_freeaddrinfo, uv_fs_chmod, uv_fs_chown, uv_fs_close`)
- [ ] `test-tty-duplicate-key.c` (missing symbols: `uv_tty_init, uv_tty_set_mode`)
- [ ] `test-tty-escape-sequence-processing.c` (missing symbols: `uv_tty_init, uv_tty_set_vterm_state, uv_tty_t`)

# Notes
- This classification is symbol-based (from test sources vs current `uv.h`) and should be treated as a planning baseline.
- A test in the "existing API" bucket can still fail due to behavioral gaps; those are wrapper/SC::Async fixes, not API-surface additions.
- Some upstream tests depend on libuv private internals (`uv__*`) and may still need adaptation even if no public `uv_*` symbols are missing.
- `uv_guess_handle` currently returns `UV_UNKNOWN_HANDLE` on Windows; the POSIX implementation covers tty, FIFO, socket, and file descriptors.
