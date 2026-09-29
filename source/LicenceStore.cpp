//------------------------------------------------------------------------
// R2Project licence - reading and writing the shared licence file.
// See LicenceStore.h.
//------------------------------------------------------------------------

#include "LicenceStore.h"
#include "LicenceConfig.h"
#include "LicencePublicKey.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
#include <direct.h>
#include <process.h>
#include <windows.h>
#else
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace R2Licence {

//------------------------------------------------------------------------
BuildConfig thisBuild ()
{
	BuildConfig c;
	c.publicKey = kPublicKey;
	c.acceptedProducts = kAcceptedProducts.data ();
	c.acceptedCount = kAcceptedProducts.size ();
	c.revokedIds = kRevokedLicenceIds.data ();
	c.revokedCount = kRevokedLicenceIds.size ();
	c.evaluationDays = kEvaluationDays;
	return c;
}

//------------------------------------------------------------------------
namespace {

#if defined(_WIN32)
const char kSep = '\\';
#else
const char kSep = '/';
#endif

std::string homeDirectory ()
{
#if defined(_WIN32)
	const char* appData = std::getenv ("APPDATA");
	return appData ? appData : "";
#else
	// $HOME first: in a sandboxed Audio Unit host it is the container,
	// which is the one place the plug-in can write.
	const char* home = std::getenv ("HOME");
	if (home && *home)
		return home;
	if (const passwd* pw = getpwuid (getuid ()))
		return pw->pw_dir ? pw->pw_dir : "";
	return "";
#endif
}

bool makeDirectories (const std::string& path)
{
	// mkdir -p on the parent folders, one component at a time.
	for (size_t i = 1; i <= path.size (); ++i)
	{
		if (i != path.size () && path[i] != kSep)
			continue;
		const std::string part = path.substr (0, i);
#if defined(_WIN32)
		if (part.size () == 2 && part[1] == ':')
			continue;  // "C:"
		if (_mkdir (part.c_str ()) != 0 && errno != EEXIST)
			return false;
#else
		if (mkdir (part.c_str (), 0755) != 0 && errno != EEXIST)
			return false;
#endif
	}
	return true;
}

} // namespace

//------------------------------------------------------------------------
LicenceStore::LicenceStore (std::string path, BuildConfig config)
: mPath (path.empty () ? defaultPath () : std::move (path))
, mConfig (config)
{
}

std::string LicenceStore::defaultPath ()
{
	const std::string home = homeDirectory ();
	if (home.empty ())
		return std::string ();
#if defined(__APPLE__)
	return home + "/Library/Application Support/R2Project/licence.json";
#elif defined(_WIN32)
	return home + "\\R2Project\\licence.json";
#else
	const char* xdg = std::getenv ("XDG_CONFIG_HOME");
	const std::string base = (xdg && *xdg) ? std::string (xdg) : home + "/.config";
	return base + "/R2Project/licence.json";
#endif
}

int LicenceStore::today ()
{
	std::time_t now = std::time (nullptr);
	std::tm local {};
#if defined(_WIN32)
	localtime_s (&local, &now);
#else
	localtime_r (&now, &local);
#endif
	return daysFromCivil (local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
}

//------------------------------------------------------------------------
bool LicenceStore::load ()
{
	mFile = LicenceFile ();
	if (mPath.empty ())
		return false;
	std::ifstream in (mPath, std::ios::binary);
	if (!in)
		return false;
	std::stringstream text;
	text << in.rdbuf ();
	if (text.str ().size () > 64 * 1024)
		return false;  // not ours
	return fromJson (text.str (), mFile);
}

bool LicenceStore::save () const
{
	if (mPath.empty ())
		return false;
	const size_t slash = mPath.find_last_of (kSep);
	if (slash != std::string::npos && !makeDirectories (mPath.substr (0, slash)))
		return false;

#if defined(_WIN32)
	const std::string tmp = mPath + ".tmp" + std::to_string (_getpid ());
#else
	const std::string tmp = mPath + ".tmp" + std::to_string (getpid ());
#endif
	{
		std::ofstream out (tmp, std::ios::binary | std::ios::trunc);
		out << toJson (mFile);
		out.close ();
		if (!out)
		{
			std::remove (tmp.c_str ());
			return false;
		}
	}
#if defined(_WIN32)
	if (!MoveFileExA (tmp.c_str (), mPath.c_str (), MOVEFILE_REPLACE_EXISTING))
#else
	if (std::rename (tmp.c_str (), mPath.c_str ()) != 0)
#endif
	{
		std::remove (tmp.c_str ());
		return false;
	}
	return true;
}

void LicenceStore::applyUnsaved ()
{
	if (mHaveUnsavedLicence && mFile.key.empty ())
	{
		mFile.name = mUnsavedLicence.name;
		mFile.key = mUnsavedLicence.key;
	}
}

//------------------------------------------------------------------------
Decision LicenceStore::editorOpened (int todayDay)
{
	load ();
	applyUnsaved ();
	if (mFile.firstRunDay < 0)
	{
		mFile.firstRunDay = todayDay;
		save ();  // if this fails, the evaluation restarts next session - never an error
	}
	return decide (mFile, todayDay, mConfig);
}

Decision LicenceStore::current (int todayDay)
{
	load ();
	applyUnsaved ();
	return decide (mFile, todayDay, mConfig);
}

Status LicenceStore::activate (const std::string& name, const std::string& key, bool& saved)
{
	saved = false;
	Result r = verifyKey (name, key, mConfig.publicKey, mConfig.acceptedProducts, mConfig.acceptedCount,
	                      mConfig.revokedIds, mConfig.revokedCount);
	if (r.status != Status::Valid)
		return r.status;

	// Stored trimmed, and the key in its canonical spelling.
	std::string trimmed = name;
	const char* space = " \t\r\n\f\v";
	trimmed.erase (0, trimmed.find_first_not_of (space));
	trimmed.erase (trimmed.find_last_not_of (space) + 1);
	uint8_t raw[kKeyBytes];
	parseKey (key, raw);

	load ();  // keep firstRun, and anything another plug-in wrote
	mFile.name = trimmed;
	mFile.key = formatKey (raw);
	saved = save ();
	if (!saved)
	{
		mUnsavedLicence = mFile;
		mHaveUnsavedLicence = true;
	}
	return Status::Valid;
}

} // namespace R2Licence
