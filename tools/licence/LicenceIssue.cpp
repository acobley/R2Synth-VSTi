//------------------------------------------------------------------------
// R2Project licence - making keys. See LicenceIssue.h.
//------------------------------------------------------------------------

#include "LicenceIssue.h"

#include "monocypher-ed25519.h"
#include "monocypher.h"

#include <cstring>

namespace R2Licence {

void publicKeyFromSeed (const uint8_t seed[kSeedSize], uint8_t publicKey[kPublicKeySize])
{
	uint8_t seedCopy[kSeedSize];  // Monocypher wipes the seed it is given
	uint8_t secret[64];
	std::memcpy (seedCopy, seed, kSeedSize);
	crypto_ed25519_key_pair (secret, publicKey, seedCopy);
	crypto_wipe (secret, sizeof (secret));
}

std::string issueKey (const std::string& name, uint16_t product, uint32_t licenceId,
                      uint16_t issueDay, const uint8_t seed[kSeedSize])
{
	Payload p;
	p.product = product;
	p.licenceId = licenceId;
	p.issueDay = issueDay;
	nameHash (name, p.nameHash);

	uint8_t key[kKeyBytes];
	packPayload (p, key);

	uint8_t seedCopy[kSeedSize];
	uint8_t secret[64];
	uint8_t pub[kPublicKeySize];
	std::memcpy (seedCopy, seed, kSeedSize);
	crypto_ed25519_key_pair (secret, pub, seedCopy);
	crypto_ed25519_sign (key + kPayloadSize, secret, key, kPayloadSize);
	crypto_wipe (secret, sizeof (secret));

	return formatKey (key);
}

std::string toHex (const uint8_t* data, size_t size)
{
	static const char digits[] = "0123456789abcdef";
	std::string out;
	for (size_t i = 0; i < size; ++i)
	{
		out += digits[data[i] >> 4];
		out += digits[data[i] & 15];
	}
	return out;
}

bool fromHex (const std::string& text, uint8_t* out, size_t size)
{
	size_t n = 0;
	int hi = -1;
	for (char c : text)
	{
		int v;
		if (c >= '0' && c <= '9') v = c - '0';
		else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
		else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
		else return false;
		if (hi < 0)
			hi = v;
		else
		{
			if (n == size)
				return false;
			out[n++] = uint8_t ((hi << 4) | v);
			hi = -1;
		}
	}
	return n == size && hi < 0;
}

} // namespace R2Licence
