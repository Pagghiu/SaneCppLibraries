@page page_async_uv_bindings libuv C Wrapper API (SC::Async)

[TOC]

This page documents the currently supported **source-compatible** subset of libuv exposed by:

- `Extra/Experimental/AsyncUV/Code/uv.h`
- `Extra/Experimental/AsyncUV/Code/SCAsyncUV.cpp`

The wrapper is source-compatible for the implemented subset, not ABI-compatible with upstream libuv.

# Goals and Scope

- Keep libuv naming (`uv_*`) for the implemented subset.
- Use project-owned structs with SC-owned opaque storage (`sc_opaque[...]`).
- Prioritize no-allocation core request flows (`uv_write` / `uv_udp_send` request state stored inline).

# Supported Types

## Handles

- `uv_loop_t`
- `uv_handle_t`
- `uv_stream_t`
- `uv_tcp_t`
- `uv_udp_t`
- `uv_pipe_t`
- `uv_tty_t`
- `uv_timer_t`
- `uv_async_t`
- `uv_prepare_t`
- `uv_check_t`
- `uv_idle_t`
- `uv_signal_t`

## Requests

- `uv_req_t`
- `uv_fs_t`
- `uv_connect_t`
- `uv_write_t`
- `uv_udp_send_t`

## Utility and callback types

- `uv_buf_t`
- `uv_interface_address_t`
- `uv_pid_t`
- `uv_file`
- `uv_stat_t`
- `uv_thread_t`
- `uv_timeval64_t`
- `uv_timespec64_t`
- `uv_utsname_t`
- callback typedefs used by the currently implemented subset

# Supported Functions

## Loop and time

- `uv_default_loop`
- `uv_library_shutdown`
- `uv_loop_init`
- `uv_loop_close`
- `uv_run`
- `uv_stop`
- `uv_sleep`
- `uv_hrtime`
- `uv_clock_gettime`
- `uv_gettimeofday`
- `uv_now`
- `uv_update_time`
- `uv_loop_alive`
- `uv_loop_configure` (`UV_LOOP_BLOCK_SIGNAL` accepted as a no-op on POSIX; `UV_ENOSYS` on Windows)
- `uv_backend_timeout`
- `uv_walk`
- `uv_loop_size`
- `uv_loop_set_data`
- `uv_loop_get_data`

## Handle lifecycle and state

- `uv_close`
- `uv_ref` (partial)
- `uv_unref` (partial)
- `uv_has_ref` (partial)
- `uv_is_active`
- `uv_is_closing`
- `uv_fileno`
- `uv_guess_handle`
- `uv_handle_type_name`
- `uv_handle_size`
- `uv_handle_get_type`
- `uv_handle_get_loop`
- `uv_handle_get_data`
- `uv_handle_set_data`
- `uv_req_type_name`
- `uv_req_size`

## Async handle

- `uv_async_init`
- `uv_async_send`

## Signal handle

- `uv_signal_init`
- `uv_signal_start`
- `uv_signal_start_oneshot`
- `uv_signal_stop`

## Timers

- `uv_timer_init`
- `uv_timer_start`
- `uv_timer_stop`
- `uv_timer_again`
- `uv_timer_set_repeat`
- `uv_timer_get_repeat`
- `uv_timer_get_due_in`

## Stream and TCP

- `uv_buf_init`
- `uv_write`
- `uv_stream_get_write_queue_size`
- `uv_listen`
- `uv_accept`
- `uv_read_start`
- `uv_read_stop`
- `uv_tcp_init`
- `uv_tcp_bind`
- `uv_tcp_connect`
- `uv_tcp_close_reset`

## Pipe

- `uv_pipe`
- `uv_pipe_init` (minimal)
- `uv_pipe_bind` (minimal POSIX support)
- `uv_pipe_open` (minimal)
- `uv_pipe_pending_instances` (no-op storage)

## TTY

- `uv_tty_init` (POSIX descriptor-backed support)
- `uv_tty_get_winsize`
- `uv_tty_set_mode` (validates arguments; mode changes are currently no-op)
- `uv_tty_reset_mode` (no-op)

## UDP

- `uv_udp_init`
- `uv_udp_bind`
- `uv_udp_recv_start`
- `uv_udp_send`
- `uv_udp_recv_stop`

## Prepare / check / idle

- `uv_prepare_init`
- `uv_prepare_start`
- `uv_prepare_stop`
- `uv_check_init`
- `uv_check_start`
- `uv_check_stop`
- `uv_idle_init`
- `uv_idle_start`
- `uv_idle_stop`

## Inet helpers

- `uv_interface_addresses` (stub)
- `uv_free_interface_addresses` (stub)
- `uv_ip4_addr`
- `uv_ip6_addr`
- `uv_ip4_name`
- `uv_ip6_name`
- `uv_ip_name`
- `uv_inet_ntop`
- `uv_inet_pton`
- `uv_if_indextoname`
- `uv_if_indextoiid`

## System utility helpers

- `uv_strerror`
- `uv_strerror_r`
- `uv_translate_sys_error`
- `uv_cwd`
- `uv_chdir`
- `uv_exepath`
- `uv_set_process_title` (no-op)
- `uv_os_homedir`
- `uv_os_tmpdir`
- `uv_os_gethostname`
- `uv_os_setenv`
- `uv_os_getenv`
- `uv_os_unsetenv`
- `uv_os_getpid`
- `uv_os_getpriority`
- `uv_os_setpriority`
- `uv_kill`
- `uv_loadavg`
- `uv_get_free_memory`
- `uv_get_total_memory`
- `uv_get_constrained_memory`
- `uv_get_available_memory`
- `uv_os_uname`
- `uv_recv_buffer_size`

## File system helpers

- `uv_fs_stat` (synchronous path)
- `uv_fs_req_cleanup`
- `uv_fs_get_type`
- `uv_fs_get_result`
- `uv_fs_get_ptr`
- `uv_fs_get_path`
- `uv_fs_get_statbuf`

## Thread helpers

- `uv_thread_create`
- `uv_thread_create_ex` (accepts valid flags; custom stack size is not honored yet)
- `uv_thread_detach` (detaches but retains the wrapper allocation)
- `uv_thread_join`
- `uv_thread_self`
- `uv_thread_equal`
- `uv_thread_setname`
- `uv_thread_getname`
- `uv_key_create`
- `uv_key_delete`
- `uv_key_get`
- `uv_key_set`
- `uv_cpumask_size` (unsupported: returns `UV_ENOTSUP`)
- `uv_thread_getcpu` (unsupported: returns `UV_ENOTSUP`)
- `uv_thread_setaffinity` (unsupported: returns `UV_ENOTSUP`)
- `uv_thread_getaffinity` (unsupported: returns `UV_ENOTSUP`)

## Error constants

- Common libuv-style negative errno constants used by the supported tests and APIs, including
  `UV_EACCES`, `UV_EADDRINUSE`, `UV_EBADF`, `UV_ECANCELED`, `UV_ECONNREFUSED`, `UV_ECONNRESET`,
  `UV_EFAULT`, `UV_EHOSTUNREACH`, `UV_EINVAL`, `UV_EMSGSIZE`, `UV_ENETUNREACH`, `UV_ENOBUFS`,
  `UV_ENOENT`, `UV_EPERM`, `UV_EPIPE`, `UV_ERANGE`, and `UV_EOF`.

# Known Behavioral Limits

- `uv_write` and `uv_udp_send` currently support up to **8** buffers per request (`UV_ENOBUFS` when `nbufs > 8`).
- `uv_stream_t::write_queue_size` and `uv_udp_t::send_queue_size` are maintained for queued write/send requests.
- `uv_set_process_title` accepts input but does not alter process title.
- `uv_os_setpriority` follows libuv's priority buckets; Windows maps ranges to process priority classes.
- `uv_ref` / `uv_unref` / `uv_has_ref` are currently implemented only for `UV_TIMER`, `UV_ASYNC`, `UV_SIGNAL`,
  `UV_TCP`, and `UV_UDP`.
- `uv_signal` APIs are present, but upstream signal-delivery tests currently hang with the current
  `AsyncSignal`/libuv loop mapping; only invalid `uv_kill` coverage is wired.
- `uv_interface_addresses` currently returns success without enumerating interfaces.
- `uv_udp_send` multi-buffer path has known TODOs; `nbufs == 1` is the most stable path today.
- `uv_tcp_bind` supports `UV_TCP_IPV6ONLY` and reports unsupported `UV_TCP_REUSEPORT`; `uv_udp_bind` still ignores
  advanced bind flags semantics beyond basic bind behavior.
- Pipe support is currently minimal and intended for non-traffic tests; IPC accept/connect/read/write semantics are not
  implemented.
- TTY support is currently aimed at descriptor classification and size queries; raw/normal mode calls validate inputs but
  do not alter terminal mode yet.

# Unsupported APIs

Any `uv_*` API that is not declared in `Extra/Experimental/AsyncUV/Code/uv.h` is currently out of scope for this wrapper.

Planned expansion is tracked in @ref page_async_uv_test_coverage.
