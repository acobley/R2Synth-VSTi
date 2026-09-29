//------------------------------------------------------------------------
// R2Project licence - reading and writing the shared licence file
//
// The file I/O and the clock, around the pure parts in LicenceState.h.
// SDK-free. Only ever called from the UI thread (the editor), never from
// the audio path.
//
// EVERY CALL RE-READS THE FILE, because other R2Project plug-ins - or
// another instance of this one - may have written it since: a licence
// entered in one plug-in is seen by the next editor that opens anywhere.
// Writes go to a temporary file that is then renamed over the old one, so
// a reader never sees half a file.
//
// If the file can't be written (a sandboxed host, a read-only home) the
// plug-in still works: the evaluation simply starts again next session,
// and an entered licence lasts until the editor closes.
//------------------------------------------------------------------------

#pragma once

#include "LicenceState.h"

#include <string>

namespace R2Licence {

/** This build's key and lists, from LicenceConfig.h and LicencePublicKey.h. */
BuildConfig thisBuild ();

class LicenceStore
{
public:
	/** An empty path means defaultPath(). */
	explicit LicenceStore (std::string path = std::string (), BuildConfig config = thisBuild ());

	/** ~/Library/Application Support/R2Project/licence.json and friends -
	    see LicenceState.h. */
	static std::string defaultPath ();

	/** Today, in days since 2026-01-01, local time. */
	static int today ();

	/** THE editor-open call: re-reads the file, records firstRun if this
	    is the first editor ever opened, and decides. */
	Decision editorOpened (int today);

	/** Re-reads and decides, writing nothing. */
	Decision current (int today);

	/** Checks name + key exactly as the plug-in does; on Valid, stores
	    them (keeping firstRun). `saved` says whether that reached the
	    disk. */
	Status activate (const std::string& name, const std::string& key, bool& saved);

	/** Low level - for tools/licence/r2-licence-trial and the tests. */
	bool load ();        // false: missing or corrupt (file() is then empty)
	bool save () const;  // false: couldn't write
	LicenceFile& file () { return mFile; }
	const std::string& path () const { return mPath; }

private:
	std::string mPath;
	BuildConfig mConfig;
	LicenceFile mFile;

	/** An activation that couldn't be saved still counts until this
	    store goes away. */
	LicenceFile mUnsavedLicence;
	bool mHaveUnsavedLicence = false;
	void applyUnsaved ();
};

} // namespace R2Licence
