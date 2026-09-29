//------------------------------------------------------------------------
// R2Project licence - MAKING keys (needs the private key)
//
// For tools/licence/ and the tests only. PLUGINS NEVER COMPILE THIS: a
// plugin holds only the public key (LicencePublicKey.h).
//
// The private key is kept as its 32-byte Ed25519 SEED (RFC 8032's
// "private key"), stored as 64 hex characters. Java's Ed25519 and
// OpenSSL can both load that seed, so the server can sign with the same
// key later.
//------------------------------------------------------------------------

#pragma once

#include "LicenceKey.h"

#include <string>

namespace R2Licence {

constexpr size_t kSeedSize = 32;

void publicKeyFromSeed (const uint8_t seed[kSeedSize], uint8_t publicKey[kPublicKeySize]);

// Builds and signs the 81 bytes, returned in text form (formatKey).
std::string issueKey (const std::string& name, uint16_t product, uint32_t licenceId,
                      uint16_t issueDay, const uint8_t seed[kSeedSize]);

// Hex helpers for the seed file and the public-key header.
std::string toHex (const uint8_t* data, size_t size);
bool fromHex (const std::string& text, uint8_t* out, size_t size);  // ignores whitespace

} // namespace R2Licence
