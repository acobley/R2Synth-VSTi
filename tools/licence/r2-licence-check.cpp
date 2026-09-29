//------------------------------------------------------------------------
// r2-licence-check - check a name + key exactly as the plugin will.
//
//   r2-licence-check "Andy Cobley" "BNDL1-XXXXX-..."
//
// Uses the same public key (source/LicencePublicKey.h) and the same
// accepted-products list and blocklist (source/LicenceConfig.h) as the
// plugin build. Needs no private key: safe to give to anyone.
//------------------------------------------------------------------------

#include "LicenceConfig.h"
#include "LicenceKey.h"
#include "LicencePublicKey.h"

#include <cstdio>

using namespace R2Licence;

int main (int argc, char** argv)
{
	if (argc != 3)
	{
		std::fprintf (stderr, "usage: r2-licence-check \"Full Name\" \"BNDL1-...\"\n");
		return 2;
	}
	Result r = verifyKey (argv[1], argv[2], kPublicKey, kAcceptedProducts.data (), kAcceptedProducts.size (),
	                      kRevokedLicenceIds.data (), kRevokedLicenceIds.size ());
	std::printf ("%s\n", statusName (r.status));
	if (r.status != Status::Malformed && r.status != Status::UnknownVersion && r.status != Status::BadSignature)
	{
		int y, m, d;
		civilFromDays (r.payload.issueDay, y, m, d);
		std::printf ("  licence id %u, product 0x%04x, issued %04d-%02d-%02d\n", r.payload.licenceId,
		             r.payload.product, y, m, d);
	}
	return r.status == Status::Valid ? 0 : 1;
}
