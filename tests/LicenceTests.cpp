//------------------------------------------------------------------------
// R2Project licence - key format tests
//
// SDK-FREE. Monocypher is C, so it's compiled separately (tools/run-tests.sh
// does all of this):
//
//   cc  -std=c99 -O2 -c ../third_party/monocypher/monocypher.c
//   cc  -std=c99 -O2 -c ../third_party/monocypher/monocypher-ed25519.c
//   c++ -std=c++17 -O2 -I../source -I../third_party/monocypher LicenceTests.cpp
//       ../source/LicenceKey.cpp ../source/LicenceSha256.cpp
//       ../source/LicenceIssue.cpp monocypher.o monocypher-ed25519.o
//
// Signing here uses RFC 8032's published TEST seed, never the dev or
// production key, so the suite needs no secrets. The "golden key" was
// made by tools/licence/reference_key.py - an independent Python
// implementation of the doc's format, standing in for the Java server -
// so both ends are held to the same bytes.
//------------------------------------------------------------------------

#include "LicenceConfig.h"
#include "LicenceIssue.h"
#include "LicenceKey.h"
#include "LicenceSha256.h"

#include "monocypher-ed25519.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if __has_include("LicencePublicKey.h")
#include "LicencePublicKey.h"
#define HAVE_PUBLIC_KEY 1
#endif

using namespace R2Licence;

namespace {

int gChecks = 0;
int gFailures = 0;

void check (bool condition, const std::string& what)
{
	++gChecks;
	if (!condition)
	{
		++gFailures;
		std::printf ("  FAIL  %s\n", what.c_str ());
	}
}

std::vector<uint8_t> hex (const char* s)
{
	std::vector<uint8_t> out (std::strlen (s) / 2);
	fromHex (s, out.data (), out.size ());
	return out;
}

std::string sha256Hex (const std::string& s)
{
	uint8_t h[32];
	sha256 (reinterpret_cast<const uint8_t*> (s.data ()), s.size (), h);
	return toHex (h, 32);
}

// RFC 8032 section 7.1, TEST 1 and TEST 2.
const char* kTestSeed = "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60";
const char* kTestPub = "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a";

const char* kGoldenName = "Andy Cobley";
const char* kGoldenKey =  // reference_key.py SEED "Andy Cobley" 42 0x0001 2026-09-29
	"BNDL1-04002-00000-N023T-H9GWM-YZJ8R-96R2J-R98Q1-VKYFV-2BW2G-1QPMS-8GW74-G9VGY-FAJDC-"
	"PVJZY-FTC2K-7NRG9-6R9M5-2N3RF-F3CMF-8RTHX-JXN82-QEB7H-AT1MZ-9DWFW-7ZNBJ-7B410";

const uint16_t kV1[] = {kProductBundleV1};

Status verify (const std::string& name, const std::string& key, const uint8_t* pub,
               const uint16_t* accepted = kV1, size_t nAccepted = 1,
               const uint32_t* revoked = nullptr, size_t nRevoked = 0)
{
	return verifyKey (name, key, pub, accepted, nAccepted, revoked, nRevoked).status;
}

//------------------------------------------------------------------------
void testSha256 ()
{
	std::printf ("SHA-256\n");
	check (sha256Hex ("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "empty");
	check (sha256Hex ("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "abc");
	check (sha256Hex ("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
	           "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
	       "FIPS 448-bit message");
	// Padding boundaries: 55 fits one block, 56..63 spill into a second.
	struct { size_t n; const char* h; } lens[] = {
		{55, "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318"},
		{56, "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"},
		{63, "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34"},
		{64, "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"},
		{65, "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0"},
		{119, "31eba51c313a5c08226adf18d4a359cfdfd8d2e816b13f4af952f7ea6584dcfb"},
		{120, "2f3d335432c70b580af0e8e1b3674a7c020d683aa5f73aaaedfdc55af904c21c"},
		{1000000, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"},
	};
	for (auto& l : lens)
		check (sha256Hex (std::string (l.n, 'a')) == l.h, "sha256 of " + std::to_string (l.n) + " x 'a'");
}

void testEd25519 ()
{
	std::printf ("Ed25519 (RFC 8032 vectors: standard SHA-512 Ed25519, not Monocypher's BLAKE2b EdDSA)\n");
	auto seed1 = hex (kTestSeed);
	uint8_t pub[32];
	publicKeyFromSeed (seed1.data (), pub);
	check (toHex (pub, 32) == kTestPub, "TEST 1 public key");

	uint8_t sk[64], pk[32], s[32], sig[64];
	std::memcpy (s, seed1.data (), 32);
	crypto_ed25519_key_pair (sk, pk, s);
	crypto_ed25519_sign (sig, sk, nullptr, 0);
	check (toHex (sig, 64) ==
	           "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b",
	       "TEST 1 signature");

	auto seed2 = hex ("4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb");
	std::memcpy (s, seed2.data (), 32);
	crypto_ed25519_key_pair (sk, pk, s);
	const uint8_t msg = 0x72;
	crypto_ed25519_sign (sig, sk, &msg, 1);
	check (toHex (pk, 32) == "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c", "TEST 2 public key");
	check (toHex (sig, 64) ==
	           "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00",
	       "TEST 2 signature");
	check (crypto_ed25519_check (sig, pk, &msg, 1) == 0, "TEST 2 verifies");
}

void testFormat ()
{
	std::printf ("Format\n");
	check (kKeyBytes == 81 && kKeyChars == 130, "81 bytes -> 130 characters");

	Payload p;
	p.product = 0x0102;
	p.licenceId = 0x03040506;
	p.issueDay = 0x0708;
	for (int i = 0; i < 8; ++i)
		p.nameHash[i] = uint8_t (0x10 + i);
	uint8_t b[kPayloadSize];
	packPayload (p, b);
	check (toHex (b, kPayloadSize) == "0101020304050607081011121314151617", "payload packs big-endian");
	Payload q = unpackPayload (b);
	check (q.version == 1 && q.product == 0x0102 && q.licenceId == 0x03040506 && q.issueDay == 0x0708 &&
	           std::memcmp (q.nameHash, p.nameHash, 8) == 0,
	       "payload round-trips");

	check (base32Encode (reinterpret_cast<const uint8_t*> ("f"), 1) == "CR", "base32 'f'");
	check (base32Encode (reinterpret_cast<const uint8_t*> ("foobar"), 6) == "CSQPYRK1E8", "base32 'foobar'");
	std::vector<uint8_t> all (256);
	for (int i = 0; i < 256; ++i)
		all[i] = uint8_t (i);
	std::vector<uint8_t> back;
	check (base32Decode (base32Encode (all.data (), all.size ()), back) && back == all, "base32 round-trips 0..255");
	check (base32Decode ("csqpyrk1e8", back) && std::string (back.begin (), back.end ()) == "foobar", "lower case");
	check (base32Decode ("CSQPYRKIE8", back) && std::string (back.begin (), back.end ()) == "foobar", "I reads as 1");
	check (base32Decode ("CSQPYRKLE8", back) && std::string (back.begin (), back.end ()) == "foobar", "L reads as 1");
	check (base32Decode ("CT", back) == false, "non-zero padding bits rejected");
	check (base32Decode ("CU", back) == false, "U is not in the alphabet");

	check (normaliseName ("  Andy Cobley\t\n") == "andy cobley", "name trimmed and lower-cased");
	check (normaliseName ("ANDY  COBLEY") == "andy  cobley", "inner spaces kept (the server does the same)");
	check (normaliseName ("Zo\xc3\xab") == "zo\xc3\xab", "non-ASCII bytes untouched");

	check (daysFromCivil (2026, 1, 1) == 0, "day 0 = 2026-01-01");
	check (daysFromCivil (2026, 9, 29) == 271, "2026-09-29 = day 271");
	check (daysFromCivil (2028, 3, 1) - daysFromCivil (2028, 2, 28) == 2, "2028 is a leap year");
	bool roundTrip = true;
	for (int d = 0; d <= 0xffff; ++d)
	{
		int y, m, dd;
		civilFromDays (d, y, m, dd);
		roundTrip = roundTrip && daysFromCivil (y, m, dd) == d;
	}
	check (roundTrip, "every issue day round-trips through a calendar date");
}

void testKeys ()
{
	std::printf ("Keys\n");
	auto seed = hex (kTestSeed);
	auto pub = hex (kTestPub);

	std::string key = issueKey (kGoldenName, kProductBundleV1, 42, uint16_t (daysFromCivil (2026, 9, 29)), seed.data ());
	check (key == kGoldenKey, "C++ key == Python reference key, byte for byte");
	check (key.size () == 5 + 26 * 6 && key.compare (0, 6, "BNDL1-") == 0, "BNDL1- then 26 groups of five");

	check (verify (kGoldenName, kGoldenKey, pub.data ()) == Status::Valid, "valid key accepted");
	check (verify ("  andy COBLEY ", kGoldenKey, pub.data ()) == Status::Valid, "name case and outer spaces ignored");

	Result r = verifyKey (kGoldenName, kGoldenKey, pub.data (), kV1, 1, nullptr, 0);
	check (r.payload.licenceId == 42 && r.payload.product == kProductBundleV1 && r.payload.issueDay == 271,
	       "payload read back from the key");

	// Forgiving entry.
	std::string compact;
	for (char c : std::string (kGoldenKey))
		if (c != '-')
			compact += char (c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
	check (verify (kGoldenName, compact, pub.data ()) == Status::Valid, "lower case, no dashes");
	check (verify (kGoldenName, compact.substr (5), pub.data ()) == Status::Valid, "prefix omitted");
	check (verify (kGoldenName, std::string ("  ") + kGoldenKey + "\n", pub.data ()) == Status::Valid,
	       "pasted with whitespace");

	// Rejections.
	check (verify ("Andy Cobly", kGoldenKey, pub.data ()) == Status::WrongName, "wrong name rejected");
	check (verify ("", kGoldenKey, pub.data ()) == Status::WrongName, "empty name rejected");

	const uint16_t v2only[] = {0x0002};
	check (verify (kGoldenName, kGoldenKey, pub.data (), v2only, 1) == Status::WrongProduct,
	       "v1 key in a v2-only build: WrongProduct (offer upgrade)");
	const uint16_t v1v2[] = {0x0001, 0x0002};
	check (verify (kGoldenName, kGoldenKey, pub.data (), v1v2, 2) == Status::Valid, "v2 free to registered users");

	const uint32_t revoked[] = {7, 42};
	check (verify (kGoldenName, kGoldenKey, pub.data (), kV1, 1, revoked, 2) == Status::Revoked, "blocklisted ID rejected");
	check (verify (kGoldenName, kGoldenKey, pub.data (), v2only, 1, revoked, 2) == Status::Revoked,
	       "revoked beats out-of-scope (no upgrade offer)");

	uint8_t otherPub[32];
	auto otherSeed = hex ("4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb");
	publicKeyFromSeed (otherSeed.data (), otherPub);
	check (verify (kGoldenName, kGoldenKey, otherPub) == Status::BadSignature, "key from another keypair rejected");

	// Every single-byte change must fail. Byte 0 is the version, which is
	// read before the signature is checked.
	uint8_t raw[kKeyBytes];
	check (parseKey (kGoldenKey, raw), "golden key parses");
	bool allRejected = true;
	for (size_t i = 0; i < kKeyBytes; ++i)
		for (uint8_t bit = 1; bit; bit = uint8_t (bit << 1))
		{
			uint8_t t[kKeyBytes];
			std::memcpy (t, raw, kKeyBytes);
			t[i] ^= bit;
			Status s = verify (kGoldenName, formatKey (t), pub.data ());
			Status want = (i == 0) ? Status::UnknownVersion : Status::BadSignature;
			if (s != want)
			{
				allRejected = false;
				std::printf ("    byte %zu bit %02x -> %s\n", i, bit, statusName (s));
			}
		}
	check (allRejected, "every one of the 648 single-bit changes rejected");

	check (verify (kGoldenName, std::string (kGoldenKey).substr (0, 100), pub.data ()) == Status::Malformed, "truncated key");
	check (verify (kGoldenName, std::string (kGoldenKey) + "0", pub.data ()) == Status::Malformed, "extra character");
	check (verify (kGoldenName, "", pub.data ()) == Status::Malformed, "empty key");
	check (verify (kGoldenName, "hello", pub.data ()) == Status::Malformed, "garbage");
	std::string withU = kGoldenKey;
	withU.back () = 'U';
	check (verify (kGoldenName, withU, pub.data ()) == Status::Malformed, "character outside the alphabet");
}

void testThisBuild ()
{
	std::printf ("This build (LicenceConfig.h / LicencePublicKey.h)\n");
	check (kEvaluationDays > 0, "evaluation period is positive");
	bool hasV1 = false;
	for (auto p : kAcceptedProducts)
		hasV1 = hasV1 || p == kProductBundleV1;
	check (hasV1, "a v1.x build accepts Bundle v1 keys");
#ifdef HAVE_PUBLIC_KEY
	bool nonZero = false;
	for (auto b : kPublicKey)
		nonZero = nonZero || b != 0;
	check (nonZero, "public key present");
	check (std::memcmp (kPublicKey, hex (kTestPub).data (), 32) != 0, "public key is not the RFC test key");
	if (kPublicKeyIsDevelopment)
		std::printf ("  note: built with the DEVELOPMENT public key\n");
#else
	std::printf ("  note: no source/LicencePublicKey.h yet (run r2-licence-keygen)\n");
#endif
}

} // namespace

int main ()
{
	testSha256 ();
	testEd25519 ();
	testFormat ();
	testKeys ();
	testThisBuild ();
	std::printf ("%d checks, %d failures\n", gChecks, gFailures);
	return gFailures == 0 ? 0 : 1;
}
