# Fil-C OpenSSL Port

`FilCOpenSSL365.patch` is derived from the OpenSSL patch in
[Fil-C v0.685 `projects/openssl.projeny`](https://github.com/pizlonator/fil-c/blob/v0.685/projects/openssl.projeny).
The original patch targets OpenSSL 3.6.4. SC maintains this rebase against OpenSSL 3.6.5; it is not an upstream
Fil-C release of that port.

The only change to the original patch is rebasing the deletion of
`test/recipes/01-test_symbol_presence.t` against its 247-line 3.6.5 contents. Fil-C changes symbol names, so the
upstream port disables that ordinary-ABI symbol-name test. Production-code patch hunks are unchanged.
The original whitespace is retained, including whitespace in diff context and generated assembly comments.

Original extracted patch SHA-256:
`e87bc439d47d5c1f8b372703a321afffc80656bd1962f590515987cbb3fd657a`.

Rebased patch SHA-256:
`982c4231436064d8c79d94258b1fcdd6e82dea77e6da9794bb283bcc1d3af583`.

The patch preserves the upstream assembly bridge and safety checks; it is not a plain `no-asm` build.
See the upstream [constant-time port explanation](https://fil-c.org/constant_time_crypto). Its detailed discussion
describes the earlier x86_64 port, not an independent proof of this ARM/security-release rebase.

OpenSSL sources retain their [Apache License 2.0](FilCOpenSSL365.LICENSE.txt) notices; the adjacent license text is
copied from the pinned 3.6.5 source archive.
The patch is retained as upstream-derived source data, not relicensed as SC's MIT implementation code.
Keep upstream copyright/license notices when updating it.

Updates require hash-checked source archives, an explicit rebase review against security-release changes,
native ABI/crypto/TLS regression tests, and updated patch/cache identities. Successful patch application or
a basic API smoke alone is not a security validation.
