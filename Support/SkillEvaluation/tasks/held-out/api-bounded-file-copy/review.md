# Predeclared source-review obligations

- The copy uses Sane `FileDescriptor` and caller-owned `Span` storage, not an STL stream, whole-file read, heap allocation, or system file API for the data path.
- The requested buffer bound is honored for every read, including the smallest valid capacity.
- A successful zero-length read is treated as EOF; read failures are not mistaken for EOF.
- Source open and input validation happen before destination creation or truncation.
- Open, read, and write failures produce distinct diagnostics and nonzero exits; no partial copy is reported as success.
- Both descriptors are closed on all paths, including destination-open, read, and write failures.
