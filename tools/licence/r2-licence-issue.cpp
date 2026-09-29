//------------------------------------------------------------------------
// r2-licence-issue - make a licence key on the Mac.
//
//   r2-licence-issue --seed licence-dev/dev-signing-seed.hex
//                    --name "Andy Cobley" [--id 1] [--product 0x0001]
//                    [--date 2026-09-29]
//
// Prints the name and key to paste into the plugin's Enter licence
// dialog. This is the design doc's "licence-issue" admin tool, before
// there's a database: the licence ID is whatever you pass (default 1).
//
// Also prints the r2-licence-check command for the key, ready to paste.
//
// Every key is checked against the PUBLIC key compiled into the plugins
// (source/LicencePublicKey.h) before it is printed, so a seed that
// doesn't match the plugins is caught here, not by a customer.
//------------------------------------------------------------------------

#include "LicenceIssue.h"
#include "LicencePublicKey.h"

#include "monocypher.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>

using namespace R2Licence;

namespace {

void usage ()
{
	std::fprintf (stderr,
	              "usage: r2-licence-issue --seed FILE --name \"Full Name\" [--id N] [--product 0x0001]\n"
	              "                        [--date YYYY-MM-DD]\n");
	std::exit (2);
}

} // namespace

int main (int argc, char** argv)
{
	std::string seedPath, name, date;
	unsigned long id = 1, product = kProductBundleV1;
	bool haveName = false;
	for (int i = 1; i < argc; ++i)
	{
		std::string a = argv[i];
		if (a == "--seed" && i + 1 < argc) seedPath = argv[++i];
		else if (a == "--name" && i + 1 < argc) { name = argv[++i]; haveName = true; }
		else if (a == "--id" && i + 1 < argc) id = std::strtoul (argv[++i], nullptr, 0);
		else if (a == "--product" && i + 1 < argc) product = std::strtoul (argv[++i], nullptr, 0);
		else if (a == "--date" && i + 1 < argc) date = argv[++i];
		else usage ();
	}
	if (seedPath.empty () || !haveName)
		usage ();
	if (normaliseName (name).empty ())
	{
		std::fprintf (stderr, "error: the name is empty\n");
		return 1;
	}
	if (id > 0xffffffffUL || product > 0xffffUL)
	{
		std::fprintf (stderr, "error: --id must fit 32 bits and --product 16 bits\n");
		return 1;
	}

	int y, m, d;
	if (date.empty ())
	{
		std::time_t now = std::time (nullptr);
		std::tm* t = std::gmtime (&now);
		y = t->tm_year + 1900; m = t->tm_mon + 1; d = t->tm_mday;
	}
	else if (std::sscanf (date.c_str (), "%d-%d-%d", &y, &m, &d) != 3)
		usage ();
	int day = daysFromCivil (y, m, d);
	if (day < 0 || day > 0xffff)
	{
		std::fprintf (stderr, "error: the date must be on or after %d-01-01\n", kEpochYear);
		return 1;
	}

	uint8_t seed[kSeedSize];
	{
		std::ifstream in (seedPath);
		std::stringstream ss;
		ss << in.rdbuf ();
		if (!in || !fromHex (ss.str (), seed, kSeedSize))
		{
			std::fprintf (stderr, "error: can't read a 64-hex-character seed from %s\n", seedPath.c_str ());
			return 1;
		}
	}

	std::string key = issueKey (name, uint16_t (product), uint32_t (id), uint16_t (day), seed);
	crypto_wipe (seed, sizeof (seed));

	const uint16_t accepted[] = {uint16_t (product)};
	Result r = verifyKey (name, key, kPublicKey, accepted, 1, nullptr, 0);
	if (r.status != Status::Valid)
	{
		std::fprintf (stderr,
		              "error: the new key does not verify against source/LicencePublicKey.h (%s).\n"
		              "This seed is not the keypair the plugins are built with.\n",
		              statusName (r.status));
		return 1;
	}

	std::printf ("Name:     %s\n", name.c_str ());
	std::printf ("Licence:  id %lu, product 0x%04lx, issued %04d-%02d-%02d%s\n", id, product, y, m, d,
	             kPublicKeyIsDevelopment ? "  [DEVELOPMENT KEY]" : "");
	std::printf ("Key:\n%s\n", key.c_str ());

	// Ready-to-paste check, name single-quoted for the shell (' -> '\'').
	std::string quoted = "'";
	for (char c : name)
		quoted += (c == '\'') ? std::string ("'\\''") : std::string (1, c);
	quoted += "'";
	std::printf ("\nCheck it (same checks as the plugin):\nbuild-licence/r2-licence-check %s %s\n",
	             quoted.c_str (), key.c_str ());
	return 0;
}
