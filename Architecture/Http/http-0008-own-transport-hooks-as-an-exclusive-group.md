# HTTP-0008 - Own transport hooks as an exclusive group

Status: Accepted
Date: 2026-10-12

## Context

Independent transport adapters could overwrite the client's or server's callback group. Closing an older adapter
then cleared the replacement's hooks, potentially restoring plaintext socket handling without the replacement's
knowledge. Checking attachment only inside an adapter does not establish ownership of the Http object.

## Decision

Http owns a protocol-neutral, non-null opaque owner token for the complete transport hook group. `claimTransport`
rejects existing owners, including repeat claims, preexisting legacy hooks, and non-quiescent objects. All setters
return Result and accept an optional owner token: only the exact claimed owner may mutate the group; an unclaimed
group accepts only nullptr. Wrong-owner operations leave callbacks and ownership unchanged.

`clearTransportSetup` clears callbacks but retains a claim. `releaseTransport` clears the group and relinquishes
only the matching claim. Configuration is serialized on the owning loop thread. A callback-dispatch counter rejects
mutation/release from inside transport callbacks, including synchronous preflight and close callbacks.

Client quiescence excludes requests, live keep-alive sockets, connection attempts, reconnect/drain, upgrades and
pending built-in socket requests. Server quiescence requires Stopped with accept and connection teardown complete;
Stopping is not sufficient. Adapters must also finish their own native cancellation barriers before release.

Tokens are caller-owned, address-stable and remain alive through release and callback return. Pointer identity does
not prevent ABA reuse: callers must not reuse a token while stale operations remain possible. Claim/configuration
does not allocate, invoke provider code or start Http. Failed attachment rolls back only its own claim.

## Consequences

Http remains independent of TLS/provider types. Existing unowned setter syntax remains available, but callers must
check Result and configure only quiescent objects. This is an intentional source/behavior contract change. Close
retains hooks while teardown runs; callers may need to drain the loop before releasing an adapter.

## Confirmation

Test competing claims, legacy overwrite/clear, stale release, callback reentrancy, retained claims after clear,
preexisting legacy hooks, stopping servers, active/keep-alive/reconnect clients, partial-init rollback and reattach.
HTTPS tests must reject competing adapters without removing authentication hooks or permitting silent plaintext
downgrade. Validate standalone/public-header compilation and preserve the Http-to-Tls dependency boundary.

## Related

- [HTTP-0005 - Allow externally owned async transports](http-0005-allow-external-async-transports.md)
