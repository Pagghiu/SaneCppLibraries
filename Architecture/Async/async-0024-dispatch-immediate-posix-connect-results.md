# ASYNC-0024 - Dispatch immediate POSIX connect results

Status: Accepted

A nonblocking connect can complete during submission, including with a refusal.
Queue its result through manual completion so the callback runs during dispatch.
This lets a caller cancel between submission and dispatch regardless of when
the socket reports its result. Pending connections still use the kernel watcher
and read the socket error when it becomes ready.

A native `ECONNREFUSED` reported during submission uses the same
`SocketConnectFailed` identity as a refusal reported by the readiness path.
Other immediate errors retain their original identity.
