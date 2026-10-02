# Fil-C TLS Test Certificates

These are public, test-only credentials for the loopback HTTP/2 fixture. The private server key is intentionally
committed and must never be used for a real service. Never install either test CA into a system trust store.

The server certificate is signed by `test-ca.pem` and names only `DNS:localhost`. `bad-ca.pem` is an unrelated root;
using it must fail certificate verification. Connecting to `127.0.0.1` with the trusted root must fail hostname
verification. Root private keys are not retained. The certificates expire on 2036-09-30 and must be regenerated
before then, retaining these extensions and relationships.

Run `Support/Scripts/RunFilCTLSFixture.sh Debug` or `Release` on native Linux after installing normal host build
prerequisites. The script installs compatible dependencies, compiles SCTest and the fixture, runs the suite with
mandatory OpenSSL capabilities and TLS fixture variables, and checks the server's three-connection summary.
Evidence is retained under ignored `_Build/_FilCTLSFixture/`. The fixture is not a general-purpose HTTPS server.
