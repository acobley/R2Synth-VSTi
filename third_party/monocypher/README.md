# Monocypher 4.0.2 (vendored)

Source: https://github.com/LoupVaillant/Monocypher, tag `4.0.2`
(commit 0d85f98c9d9b0227e42cf795cb527dff372b40a4). The only change is the
version line at the top of each file (`__git__` -> `4.0.2`), which the
upstream release tarball stamps the same way.

Licence: dual CC0 / 2-clause BSD - see LICENCE.md.

Files:

- `monocypher.c/.h` - core library (only `crypto_wipe` and `crypto_verify*` are used directly)
- `monocypher-ed25519.c/.h` - from `src/optional/`: **standard Ed25519
  (RFC 8032, SHA-512)**. This is the one we use. Monocypher's default
  `crypto_eddsa_*` uses BLAKE2b instead of SHA-512 and is NOT compatible with
  Java's / OpenSSL's Ed25519, which the licence server will use.

These are C files: compile them with a C compiler (`cc`), not `c++`.
