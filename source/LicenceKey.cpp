//------------------------------------------------------------------------
// R2Project licence - key format and offline verification. See LicenceKey.h.
//------------------------------------------------------------------------

#include "LicenceKey.h"
#include "LicenceSha256.h"

#include "monocypher-ed25519.h"

#include <cstring>

namespace R2Licence {
namespace {

const char kAlphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

int decodeChar (char c)
{
	if (c >= 'a' && c <= 'z')
		c = char (c - 'a' + 'A');
	switch (c)
	{
		case 'O': return 0;
		case 'I':
		case 'L': return 1;
		default: break;
	}
	for (int i = 0; i < 32; ++i)
		if (kAlphabet[i] == c)
			return i;
	return -1;
}

bool isSpace (char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }

} // namespace

//------------------------------------------------------------------------
void packPayload (const Payload& p, uint8_t out[kPayloadSize])
{
	out[0] = p.version;
	out[1] = uint8_t (p.product >> 8);
	out[2] = uint8_t (p.product);
	out[3] = uint8_t (p.licenceId >> 24);
	out[4] = uint8_t (p.licenceId >> 16);
	out[5] = uint8_t (p.licenceId >> 8);
	out[6] = uint8_t (p.licenceId);
	out[7] = uint8_t (p.issueDay >> 8);
	out[8] = uint8_t (p.issueDay);
	std::memcpy (out + 9, p.nameHash, kNameHashSize);
}

Payload unpackPayload (const uint8_t in[kPayloadSize])
{
	Payload p;
	p.version = in[0];
	p.product = uint16_t ((in[1] << 8) | in[2]);
	p.licenceId = (uint32_t (in[3]) << 24) | (uint32_t (in[4]) << 16) | (uint32_t (in[5]) << 8) | in[6];
	p.issueDay = uint16_t ((in[7] << 8) | in[8]);
	std::memcpy (p.nameHash, in + 9, kNameHashSize);
	return p;
}

//------------------------------------------------------------------------
std::string normaliseName (const std::string& name)
{
	size_t b = 0, e = name.size ();
	while (b < e && isSpace (name[b]))
		++b;
	while (e > b && isSpace (name[e - 1]))
		--e;
	std::string out = name.substr (b, e - b);
	for (char& c : out)
		if (c >= 'A' && c <= 'Z')
			c = char (c - 'A' + 'a');
	return out;
}

void nameHash (const std::string& name, uint8_t out[kNameHashSize])
{
	std::string n = normaliseName (name);
	uint8_t full[32];
	sha256 (reinterpret_cast<const uint8_t*> (n.data ()), n.size (), full);
	std::memcpy (out, full, kNameHashSize);
}

//------------------------------------------------------------------------
std::string base32Encode (const uint8_t* data, size_t size)
{
	std::string out;
	out.reserve ((size * 8 + 4) / 5);
	uint32_t buffer = 0;
	int bits = 0;
	for (size_t i = 0; i < size; ++i)
	{
		buffer = (buffer << 8) | data[i];
		bits += 8;
		while (bits >= 5)
		{
			out += kAlphabet[(buffer >> (bits - 5)) & 31];
			bits -= 5;
		}
	}
	if (bits > 0)
		out += kAlphabet[(buffer << (5 - bits)) & 31];  // pad with zero bits
	return out;
}

bool base32Decode (const std::string& text, std::vector<uint8_t>& out)
{
	out.clear ();
	uint32_t buffer = 0;
	int bits = 0;
	for (char c : text)
	{
		if (c == '-' || isSpace (c))
			continue;
		int v = decodeChar (c);
		if (v < 0)
			return false;
		buffer = (buffer << 5) | uint32_t (v);
		bits += 5;
		if (bits >= 8)
		{
			out.push_back (uint8_t (buffer >> (bits - 8)));
			bits -= 8;
		}
	}
	// Leftover bits are padding and must be zero, so every key has exactly
	// one spelling (bar the forgiving letters above).
	return bits < 5 && (buffer & ((1u << bits) - 1)) == 0;
}

//------------------------------------------------------------------------
std::string formatKey (const uint8_t key[kKeyBytes])
{
	std::string body = base32Encode (key, kKeyBytes);
	std::string out = kKeyPrefix;
	for (size_t i = 0; i < body.size (); i += 5)
	{
		out += '-';
		out += body.substr (i, 5);
	}
	return out;
}

bool parseKey (const std::string& text, uint8_t key[kKeyBytes])
{
	std::string compact;
	for (char c : text)
	{
		if (c == '-' || isSpace (c))
			continue;
		compact += (c >= 'a' && c <= 'z') ? char (c - 'a' + 'A') : c;
	}
	const size_t prefixLen = std::strlen (kKeyPrefix);
	if (compact.size () == kKeyChars + prefixLen && compact.compare (0, prefixLen, kKeyPrefix) == 0)
		compact.erase (0, prefixLen);
	if (compact.size () != kKeyChars)
		return false;

	std::vector<uint8_t> bytes;
	if (!base32Decode (compact, bytes) || bytes.size () != kKeyBytes)
		return false;
	std::memcpy (key, bytes.data (), kKeyBytes);
	return true;
}

//------------------------------------------------------------------------
// Howard Hinnant's days_from_civil, rebased to 2026-01-01.
namespace {
int daysFrom1970 (int y, int m, int d)
{
	y -= m <= 2;
	const int era = (y >= 0 ? y : y - 399) / 400;
	const int yoe = y - era * 400;
	const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + doe - 719468;
}
} // namespace

int daysFromCivil (int year, int month, int day)
{
	return daysFrom1970 (year, month, day) - daysFrom1970 (kEpochYear, 1, 1);
}

void civilFromDays (int days, int& year, int& month, int& day)
{
	int z = days + daysFrom1970 (kEpochYear, 1, 1) + 719468;
	const int era = (z >= 0 ? z : z - 146096) / 146097;
	const int doe = z - era * 146097;
	const int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	const int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	const int mp = (5 * doy + 2) / 153;
	day = doy - (153 * mp + 2) / 5 + 1;
	month = mp < 10 ? mp + 3 : mp - 9;
	year = yoe + era * 400 + (month <= 2);
}

//------------------------------------------------------------------------
const char* statusName (Status s)
{
	switch (s)
	{
		case Status::Valid: return "Valid";
		case Status::Malformed: return "Malformed";
		case Status::UnknownVersion: return "UnknownVersion";
		case Status::BadSignature: return "BadSignature";
		case Status::WrongName: return "WrongName";
		case Status::Revoked: return "Revoked";
		case Status::WrongProduct: return "WrongProduct";
	}
	return "?";
}

Result verifyKey (const std::string& name, const std::string& keyText,
                  const uint8_t publicKey[kPublicKeySize],
                  const uint16_t* acceptedProducts, size_t acceptedCount,
                  const uint32_t* revokedIds, size_t revokedCount)
{
	Result r;
	uint8_t key[kKeyBytes];
	if (!parseKey (keyText, key))
		return r;  // Malformed

	if (key[0] != kFormatVersion)
	{
		r.status = Status::UnknownVersion;
		return r;
	}

	if (crypto_ed25519_check (key + kPayloadSize, publicKey, key, kPayloadSize) != 0)
	{
		r.status = Status::BadSignature;
		return r;
	}
	r.payload = unpackPayload (key);

	uint8_t hash[kNameHashSize];
	nameHash (name, hash);
	if (std::memcmp (hash, r.payload.nameHash, kNameHashSize) != 0)
	{
		r.status = Status::WrongName;
		return r;
	}

	for (size_t i = 0; i < revokedCount; ++i)
		if (revokedIds[i] == r.payload.licenceId)
		{
			r.status = Status::Revoked;
			return r;
		}

	for (size_t i = 0; i < acceptedCount; ++i)
		if (acceptedProducts[i] == r.payload.product)
		{
			r.status = Status::Valid;
			return r;
		}

	r.status = Status::WrongProduct;
	return r;
}

} // namespace R2Licence
