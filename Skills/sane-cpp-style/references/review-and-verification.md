# Review and verification

Evaluate behavior and obligations, not whether code contains preferred vocabulary.

## Behavioral checks

Make tests exercise observable contracts: correct normal result; capacity exhaustion; invalid input; partial setup; error propagation; cancellation/shutdown; and repeat/reuse after completion. For concurrency, assert a bounded peak or controlled interleaving rather than relying on timing alone. Use fault injection when the host can provide it; otherwise label cleanup gaps as source-review findings.

## Source review questions

- Can any callback, thread, descriptor, or request outlive the object it references?
- Does every successful setup step have cleanup on every later failure path?
- Does a completion predicate include all required sub-operations?
- Can every promised deadline wake the exact wait that could otherwise block indefinitely?
- Is work described as nonblocking actually nonblocking, or merely moved to a worker thread?
- Does notification also reclaim the underlying resource, or is a separate join/reap/close step required?
- Is storage movement/reallocation impossible or synchronized while borrowed?
- Are bounds and allocation/dependency policies visible and honored?
- Are public headers and transitive dependencies consistent with the intended adoption boundary?

## Evidence levels

Keep these distinct in reports: executable acceptance result; sanitizer/tool result; source-review observation; test-harness defect; and external library defect. A passing happy-path test does not settle uninjectable cleanup paths; a stylistic preference is not an architectural failure.
