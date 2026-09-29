//------------------------------------------------------------------------
// R2Project licence - the licence FILE and the NAG DECISION
//
// Implements "Plugin-side changes" in the Nag-ware Licensing Design doc.
// SDK-FREE and VSTGUI-free, so all of it is tested by
// tests/LicenceTests.cpp; the panel (LicenceViews.h) only draws what
// this decides.
//
// THE FILE is shared by every R2Project plug-in:
//
//   macOS    ~/Library/Application Support/R2Project/licence.json
//   Windows  %APPDATA%\R2Project\licence.json
//   Linux    $XDG_CONFIG_HOME (or ~/.config)/R2Project/licence.json
//
//   { "format": 1, "firstRun": "2026-09-29",
//     "name": "Andy Cobley", "key": "BNDL1-..." }
//
// firstRun is written the first time ANY R2Project editor opens - never
// at load, because hosts load plug-ins to scan them with no UI. A file
// that is missing or unreadable is a fresh evaluation. Nothing here ever
// touches the audio path, and nothing here ever throws.
//
// THE RULES (kEvaluationDays = 7 in LicenceConfig.h):
//
//   valid licence               no nag; the badge says "Licensed to <name>"
//   unlicensed, day 0..7        no nag; the badge says how many days are left
//   unlicensed, day 8 on        the reminder when the editor opens, at most
//                               once per session per plug-in - and
//                               "Continue evaluating" works for ever
//
// "Day" is whole calendar days since firstRun, in local time. A clock set
// back before firstRun counts as day 0.
//------------------------------------------------------------------------

#pragma once

#include "LicenceKey.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace R2Licence {

//------------------------------------------------------------------------
// The file's contents
//------------------------------------------------------------------------
struct LicenceFile
{
	int firstRunDay = -1;  // days since 2026-01-01 (LicenceKey.h); -1 = not recorded yet
	std::string name;      // as the user typed it, trimmed
	std::string key;       // canonical BNDL1-... form
};

std::string toJson (const LicenceFile& file);

/** False if the text isn't a JSON object this can read; `file` is then
    left empty, which is what a corrupt file means: a fresh evaluation. */
bool fromJson (const std::string& text, LicenceFile& file);

std::string formatDate (int day);                 // "2026-09-29"
bool parseDate (const std::string& text, int& day);

//------------------------------------------------------------------------
// What a build accepts. thisBuild() (LicenceStore.h) fills it from
// LicenceConfig.h and LicencePublicKey.h; the tests make their own.
//------------------------------------------------------------------------
struct BuildConfig
{
	const uint8_t* publicKey = nullptr;
	const uint16_t* acceptedProducts = nullptr;
	size_t acceptedCount = 0;
	const uint32_t* revokedIds = nullptr;
	size_t revokedCount = 0;
	int evaluationDays = 7;
};

//------------------------------------------------------------------------
// The decision
//------------------------------------------------------------------------
struct Decision
{
	bool licensed = false;     // a valid key for this build is stored
	bool outOfScope = false;   // a genuine key for this name, but not for this release
	bool reminderDue = false;  // past the evaluation period and not licensed
	int daysUsed = 0;          // whole days since firstRun
	int daysLeft = 0;          // evaluationDays - daysUsed, never below 0
	std::string licensedName;  // licensed or outOfScope: the name on the key
	Payload payload;           // licensed or outOfScope: what the key says
};

Decision decide (const LicenceFile& file, int today, const BuildConfig& config);

//------------------------------------------------------------------------
// The words. Here, not in the view code, so the tests can hold them to
// the space the panel gives them.
//------------------------------------------------------------------------

/** The small line in the panel's bottom-right corner. */
std::string badgeText (const Decision& d);

/** Room for the badge: kBadgeMaxChars at the panel's tiny font. The
    name in "Licensed to <name>" is shortened with "..." to fit. */
constexpr size_t kBadgeMaxChars = 40;

/** What the Enter licence dialog says for each result of verifyKey(). */
std::string activationMessage (Status status);

/** Pulls a licence out of pasted text - the whole email, or just the
    key. The key is the run of Base32 after "BNDL1", allowing dashes,
    spaces and line breaks (mail clients wrap long lines); the name is
    the rest of a line starting "Name:". Either may be absent: returns
    true if a key was found, and sets `name` only if a Name: line was. */
bool extractFromPaste (const std::string& text, std::string& name, std::string& key);

/** Cmd+V into a field when the clipboard holds no key (a name, say): the
    text on one line - line breaks become spaces, ends trimmed - added to
    what the field already holds. The dialog handles Cmd+V itself because
    some hosts (Reaper) take it for their own Edit menu first. */
std::string pasteIntoField (const std::string& current, const std::string& clipboard);

} // namespace R2Licence
