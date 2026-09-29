//------------------------------------------------------------------------
// r2-licence-trial - look at, or wind, the shared licence file.
//
// For TESTING the evaluation on your own Mac - nothing a customer needs.
//
//   r2-licence-trial show              what the plug-ins will decide today
//   r2-licence-trial days-ago 8        pretend the first editor opened 8 days ago
//   r2-licence-trial reset             forget firstRun: the next editor open is day 0
//   r2-licence-trial unlicense         remove the stored name and key
//
//   --file PATH   another licence file (default: the one the plug-ins use,
//                 ~/Library/Application Support/R2Project/licence.json)
//
// The reminder shows at most once per SESSION: after changing the file,
// quit and reopen the host (or rescan) before opening the editor again.
//------------------------------------------------------------------------

#include "LicenceConfig.h"
#include "LicenceStore.h"

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace R2Licence;

namespace {

void usage ()
{
	std::fprintf (stderr, "usage: r2-licence-trial [--file PATH] show | days-ago N | reset | unlicense\n");
	std::exit (2);
}

void show (LicenceStore& store)
{
	const int today = LicenceStore::today ();
	const bool readable = store.load ();
	const LicenceFile f = store.file ();
	std::printf ("File:       %s%s\n", store.path ().c_str (), readable ? "" : "  (missing or unreadable)");
	std::printf ("First run:  %s\n", f.firstRunDay >= 0 ? formatDate (f.firstRunDay).c_str () : "(not yet)");
	std::printf ("Licence:    %s\n", f.key.empty () ? "(none)" : f.name.c_str ());

	const Decision d = decide (f, today, thisBuild ());
	std::printf ("Today:      %s, evaluation %d days\n", formatDate (today).c_str (), kEvaluationDays);
	std::printf ("Decision:   %s\n", d.licensed      ? "licensed - no reminder"
	                                 : d.reminderDue ? "evaluation over - reminder when the editor opens"
	                                                 : "evaluating - no reminder");
	if (!d.licensed)
		std::printf ("            day %d%s\n", d.daysUsed, d.outOfScope ? ", licence is for another version" : "");
	std::printf ("Badge:      %s\n", badgeText (d).c_str ());
}

} // namespace

int main (int argc, char** argv)
{
	std::string path;
	int i = 1;
	if (i + 1 < argc && std::string (argv[i]) == "--file")
	{
		path = argv[i + 1];
		i += 2;
	}
	if (i >= argc)
		usage ();
	const std::string command = argv[i++];

	LicenceStore store (path);
	if (store.path ().empty ())
	{
		std::fprintf (stderr, "error: can't work out where the licence file is (no HOME?)\n");
		return 1;
	}

	if (command == "show" && i == argc)
	{
		show (store);
		return 0;
	}

	store.load ();
	if (command == "days-ago" && i + 1 == argc)
	{
		char* end = nullptr;
		long n = std::strtol (argv[i], &end, 10);
		if (!end || *end || n < 0 || n > 10000)
			usage ();
		const int day = LicenceStore::today () - int (n);
		if (day < 0)
		{
			std::fprintf (stderr, "error: that's before 2026-01-01, which the file can't hold\n");
			return 1;
		}
		store.file ().firstRunDay = day;
	}
	else if (command == "reset" && i == argc)
		store.file ().firstRunDay = -1;
	else if (command == "unlicense" && i == argc)
	{
		store.file ().name.clear ();
		store.file ().key.clear ();
	}
	else
		usage ();

	if (!store.save ())
	{
		std::fprintf (stderr, "error: couldn't write %s\n", store.path ().c_str ());
		return 1;
	}
	show (store);
	return 0;
}
