//------------------------------------------------------------------------
// R2Project licence - SHA-256 (FIPS 180-4)
//
// Used only for the 8-byte NAME HASH in a licence key. Monocypher ships
// SHA-512 (for Ed25519) but not SHA-256, and the key format specifies
// SHA-256 so the Java server can use MessageDigest.getInstance("SHA-256").
//
// Not constant-time, and doesn't need to be: it hashes a person's name.
//------------------------------------------------------------------------

#pragma once

#include <cstddef>
#include <cstdint>

namespace R2Licence {

void sha256 (const uint8_t* data, size_t size, uint8_t out[32]);

} // namespace R2Licence
