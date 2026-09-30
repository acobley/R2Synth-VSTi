//------------------------------------------------------------------------
// R2Project licence - key format and offline verification
//
// Implements "Licence key format" in the Nag-ware Licensing Design doc.
// SDK-FREE: plain C++17 plus Monocypher (third_party/monocypher).
//
// A key is an 81-byte blob, big-endian:
//
//     0   1  format version   (kFormatVersion)
//     1   2  product code     (kProductBundleV1, ...)
//     3   4  licence ID       (database row ID; the blocklist uses it)
//     7   2  issue day        (days since 2026-01-01)
//     9   8  name hash        (first 8 bytes of SHA-256 of normaliseName())
//    17  64  Ed25519 signature over bytes 0..16 (RFC 8032, SHA-512)
//
// written as 130 Crockford Base32 characters in groups of five after a
// "BNDL1-" prefix. The user enters NAME + KEY; the name must hash to the
// value inside the key.
//
// This header is everything a PLUGIN needs. Making keys (which needs the
// private key) lives in LicenceIssue.h, which plugins never compile.
//------------------------------------------------------------------------

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace R2Licence {

//------------------------------------------------------------------------
// Format constants - changing any of these breaks every existing key.
constexpr uint8_t kFormatVersion = 1;
constexpr size_t kPayloadSize = 17;
constexpr size_t kSignatureSize = 64;
constexpr size_t kKeyBytes = kPayloadSize + kSignatureSize;  // 81
constexpr size_t kKeyChars = (kKeyBytes * 8 + 4) / 5;        // 130
constexpr size_t kNameHashSize = 8;
constexpr size_t kPublicKeySize = 32;
constexpr const char* kKeyPrefix = "BNDL1";
constexpr int kEpochYear = 2026;  // issue day 0 = 2026-01-01

// Product codes (the `product` table). A build accepts a key only if its
// code is in that build's accepted list - see LicenceConfig.h.
constexpr uint16_t kProductBundleV1 = 0x0001;
// Free registration for SimpleSynth (the website's activate.jsp). Only
// SimpleSynth accepts it; any other plug-in needs its own code.
constexpr uint16_t kProductSimpleSynthFree = 0x0101;

//------------------------------------------------------------------------
struct Payload
{
	uint8_t version = kFormatVersion;
	uint16_t product = 0;
	uint32_t licenceId = 0;
	uint16_t issueDay = 0;
	uint8_t nameHash[kNameHashSize] = {};
};

void packPayload (const Payload& p, uint8_t out[kPayloadSize]);
Payload unpackPayload (const uint8_t in[kPayloadSize]);

//------------------------------------------------------------------------
// Name: trimmed of ASCII whitespace at both ends, ASCII letters
// lower-cased; everything else (inner spaces, UTF-8) byte for byte.
// The server MUST do exactly the same.
std::string normaliseName (const std::string& name);
void nameHash (const std::string& name, uint8_t out[kNameHashSize]);

//------------------------------------------------------------------------
// Text form. Encoding is canonical upper-case Crockford Base32; decoding
// is forgiving: case-insensitive, O->0, I/L->1, dashes and whitespace
// ignored, "BNDL1" prefix optional.
std::string base32Encode (const uint8_t* data, size_t size);
bool base32Decode (const std::string& text, std::vector<uint8_t>& out);

std::string formatKey (const uint8_t key[kKeyBytes]);
bool parseKey (const std::string& text, uint8_t key[kKeyBytes]);

//------------------------------------------------------------------------
// Dates: issue day <-> calendar date (proleptic Gregorian, UTC).
int daysFromCivil (int year, int month, int day);  // days since 2026-01-01
void civilFromDays (int days, int& year, int& month, int& day);

//------------------------------------------------------------------------
// Verification, in the order the design doc gives: format, signature,
// name, blocklist, product. The blocklist comes before the product so a
// revoked key is never offered an upgrade.
enum class Status
{
	Valid,
	Malformed,       // not 130 Base32 characters (after the prefix)
	UnknownVersion,  // decoded, but a format version this build can't read
	BadSignature,    // not made with our private key, or altered
	WrongName,       // genuine key, but for a different name
	Revoked,         // genuine, but its licence ID is on the blocklist
	WrongProduct,    // genuine and yours, but not for this release: offer the upgrade
};

const char* statusName (Status s);

struct Result
{
	Status status = Status::Malformed;
	Payload payload;  // meaningful from WrongName onwards (signature checked)
};

Result verifyKey (const std::string& name, const std::string& keyText,
                  const uint8_t publicKey[kPublicKeySize],
                  const uint16_t* acceptedProducts, size_t acceptedCount,
                  const uint32_t* revokedIds, size_t revokedCount);

} // namespace R2Licence
