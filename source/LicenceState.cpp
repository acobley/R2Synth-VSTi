//------------------------------------------------------------------------
// R2Project licence - the licence file and the nag decision.
// See LicenceState.h.
//------------------------------------------------------------------------

#include "LicenceState.h"

#include <cstdio>
#include <cstring>

namespace R2Licence {

//------------------------------------------------------------------------
// Dates
//------------------------------------------------------------------------
std::string formatDate (int day)
{
	int y, m, d;
	civilFromDays (day, y, m, d);
	char text[16];
	std::snprintf (text, sizeof (text), "%04d-%02d-%02d", y, m, d);
	return text;
}

bool parseDate (const std::string& text, int& day)
{
	if (text.size () != 10 || text[4] != '-' || text[7] != '-')
		return false;
	for (size_t i : {0u, 1u, 2u, 3u, 5u, 6u, 8u, 9u})
		if (text[i] < '0' || text[i] > '9')
			return false;
	const int y = std::stoi (text.substr (0, 4));
	const int m = std::stoi (text.substr (5, 2));
	const int d = std::stoi (text.substr (8, 2));
	if (m < 1 || m > 12 || d < 1 || d > 31)
		return false;
	day = daysFromCivil (y, m, d);
	int y2, m2, d2;
	civilFromDays (day, y2, m2, d2);
	return y2 == y && m2 == m && d2 == d;  // rejects 2026-02-30
}

//------------------------------------------------------------------------
// JSON - just enough for a flat object of strings and numbers, which is
// all this file is. Anything else is "corrupt", i.e. a fresh evaluation.
//------------------------------------------------------------------------
namespace {

void appendEscaped (std::string& out, const std::string& s)
{
	out += '"';
	for (unsigned char c : s)
	{
		switch (c)
		{
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (c < 0x20)
				{
					char u[8];
					std::snprintf (u, sizeof (u), "\\u%04x", c);
					out += u;
				}
				else
					out += char (c);  // UTF-8 passes straight through
		}
	}
	out += '"';
}

void appendUtf8 (std::string& out, uint32_t cp)
{
	if (cp < 0x80)
		out += char (cp);
	else if (cp < 0x800)
	{
		out += char (0xC0 | (cp >> 6));
		out += char (0x80 | (cp & 0x3F));
	}
	else if (cp < 0x10000)
	{
		out += char (0xE0 | (cp >> 12));
		out += char (0x80 | ((cp >> 6) & 0x3F));
		out += char (0x80 | (cp & 0x3F));
	}
	else
	{
		out += char (0xF0 | (cp >> 18));
		out += char (0x80 | ((cp >> 12) & 0x3F));
		out += char (0x80 | ((cp >> 6) & 0x3F));
		out += char (0x80 | (cp & 0x3F));
	}
}

struct Parser
{
	const std::string& s;
	size_t i = 0;

	void skipSpace ()
	{
		while (i < s.size () && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
			++i;
	}

	bool expect (char c)
	{
		skipSpace ();
		if (i < s.size () && s[i] == c)
		{
			++i;
			return true;
		}
		return false;
	}

	bool hex4 (uint32_t& v)
	{
		if (i + 4 > s.size ())
			return false;
		v = 0;
		for (int k = 0; k < 4; ++k)
		{
			char c = s[i++];
			v <<= 4;
			if (c >= '0' && c <= '9') v |= uint32_t (c - '0');
			else if (c >= 'a' && c <= 'f') v |= uint32_t (c - 'a' + 10);
			else if (c >= 'A' && c <= 'F') v |= uint32_t (c - 'A' + 10);
			else return false;
		}
		return true;
	}

	bool string (std::string& out)
	{
		out.clear ();
		if (!expect ('"'))
			return false;
		while (i < s.size ())
		{
			char c = s[i++];
			if (c == '"')
				return true;
			if (static_cast<unsigned char> (c) < 0x20)
				return false;
			if (c != '\\')
			{
				out += c;
				continue;
			}
			if (i >= s.size ())
				return false;
			char e = s[i++];
			switch (e)
			{
				case '"': out += '"'; break;
				case '\\': out += '\\'; break;
				case '/': out += '/'; break;
				case 'b': out += '\b'; break;
				case 'f': out += '\f'; break;
				case 'n': out += '\n'; break;
				case 'r': out += '\r'; break;
				case 't': out += '\t'; break;
				case 'u':
				{
					uint32_t cp;
					if (!hex4 (cp))
						return false;
					if (cp >= 0xD800 && cp < 0xDC00)  // a surrogate pair
					{
						uint32_t lo;
						if (i + 2 > s.size () || s[i] != '\\' || s[i + 1] != 'u')
							return false;
						i += 2;
						if (!hex4 (lo) || lo < 0xDC00 || lo >= 0xE000)
							return false;
						cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
					}
					else if (cp >= 0xDC00 && cp < 0xE000)
						return false;
					appendUtf8 (out, cp);
					break;
				}
				default: return false;
			}
		}
		return false;
	}

	/** A number, true, false or null - kept as text. */
	bool scalar (std::string& out)
	{
		skipSpace ();
		size_t start = i;
		while (i < s.size () && (s[i] == '-' || s[i] == '+' || s[i] == '.' || s[i] == 'e' || s[i] == 'E' ||
		                         (s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'z')))
			++i;
		out = s.substr (start, i - start);
		return !out.empty ();
	}
};

} // namespace

std::string toJson (const LicenceFile& file)
{
	std::string out = "{\n  \"format\": 1";
	if (file.firstRunDay >= 0)
	{
		out += ",\n  \"firstRun\": ";
		appendEscaped (out, formatDate (file.firstRunDay));
	}
	if (!file.key.empty ())
	{
		out += ",\n  \"name\": ";
		appendEscaped (out, file.name);
		out += ",\n  \"key\": ";
		appendEscaped (out, file.key);
	}
	out += "\n}\n";
	return out;
}

bool fromJson (const std::string& text, LicenceFile& file)
{
	file = LicenceFile ();
	LicenceFile parsed;
	Parser p {text};
	if (!p.expect ('{'))
		return false;
	if (!p.expect ('}'))
	{
		for (;;)
		{
			std::string key, value;
			if (!p.string (key) || !p.expect (':'))
				return false;
			p.skipSpace ();
			bool isString = p.i < text.size () && text[p.i] == '"';
			if (isString ? !p.string (value) : !p.scalar (value))
				return false;

			if (key == "firstRun")
			{
				int day;
				if (isString && parseDate (value, day))
					parsed.firstRunDay = day;
			}
			else if (key == "name" && isString)
				parsed.name = value;
			else if (key == "key" && isString)
				parsed.key = value;
			// "format" and anything unknown: ignored, so a later format
			// that only adds fields is still read.

			if (p.expect (','))
				continue;
			if (p.expect ('}'))
				break;
			return false;
		}
	}
	p.skipSpace ();
	if (p.i != text.size ())
		return false;
	file = parsed;
	return true;
}

//------------------------------------------------------------------------
// The decision
//------------------------------------------------------------------------
Decision decide (const LicenceFile& file, int today, const BuildConfig& config)
{
	Decision d;

	if (!file.key.empty () && config.publicKey)
	{
		Result r = verifyKey (file.name, file.key, config.publicKey, config.acceptedProducts,
		                      config.acceptedCount, config.revokedIds, config.revokedCount);
		if (r.status == Status::Valid || r.status == Status::WrongProduct)
		{
			d.licensedName = file.name;
			d.payload = r.payload;
			if (r.status == Status::Valid)
			{
				d.licensed = true;
				return d;
			}
			d.outOfScope = true;
		}
		// Anything else stored (tampered, revoked, another keypair) is
		// simply not a licence: the plug-in carries on as unlicensed.
	}

	const int firstRun = (file.firstRunDay >= 0) ? file.firstRunDay : today;
	d.daysUsed = (today > firstRun) ? today - firstRun : 0;
	d.daysLeft = (config.evaluationDays > d.daysUsed) ? config.evaluationDays - d.daysUsed : 0;
	d.reminderDue = d.daysUsed > config.evaluationDays;
	return d;
}

//------------------------------------------------------------------------
// The words
//------------------------------------------------------------------------
std::string badgeText (const Decision& d)
{
	if (d.licensed)
	{
		const std::string prefix = "Licensed to ";
		std::string name = d.licensedName;
		if (prefix.size () + name.size () > kBadgeMaxChars)
		{
			// Cut on a UTF-8 character boundary, then mark the cut.
			size_t keep = kBadgeMaxChars - prefix.size () - 3;
			while (keep > 0 && (static_cast<unsigned char> (name[keep]) & 0xC0) == 0x80)
				--keep;
			name = name.substr (0, keep) + "...";
		}
		return prefix + name;
	}
	if (d.reminderDue)
		return "Evaluation - activate...";
	if (d.daysLeft == 0)
		return "Evaluation - last day - activate...";
	if (d.daysLeft == 1)
		return "Evaluation - 1 day left - activate...";
	return "Evaluation - " + std::to_string (d.daysLeft) + " days left - activate...";
}

std::string activationMessage (Status status)
{
	switch (status)
	{
		case Status::Valid:
			return "Thank you - SimpleSynth is activated.";
		case Status::Malformed:
			return "That isn't a complete code. Paste the whole BNDL1-... code.";
		case Status::UnknownVersion:
			return "That code needs a newer version of SimpleSynth.";
		case Status::BadSignature:
			return "That code isn't valid. Check it was copied in full, exactly as shown.";
		case Status::WrongName:
			return "That code is for a different name. Enter the name exactly as shown with it.";
		case Status::Revoked:
			return "That code has been withdrawn. Please get in touch.";
		case Status::WrongProduct:
			return "That code is genuine but doesn't cover this version of SimpleSynth.";
	}
	return "";
}

//------------------------------------------------------------------------
std::string pasteIntoField (const std::string& current, const std::string& clipboard)
{
	std::string line;
	for (char c : clipboard)
	{
		if (c == '\r' || c == '\n' || c == '\t')
			c = ' ';
		if (c == ' ' && (line.empty () || line.back () == ' '))
			continue;  // no leading or doubled spaces from line breaks
		if (static_cast<unsigned char> (c) < 0x20)
			continue;
		line += c;
	}
	while (!line.empty () && line.back () == ' ')
		line.pop_back ();
	return current + line;
}

bool extractFromPaste (const std::string& text, std::string& name, std::string& key)
{
	auto upper = [] (char c) { return (c >= 'a' && c <= 'z') ? char (c - 'a' + 'A') : c; };
	auto isSpace = [] (char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v'; };

	// The name: "Name: Andy Cobley" on a line of its own.
	for (size_t start = 0; start < text.size ();)
	{
		size_t end = text.find ('\n', start);
		if (end == std::string::npos)
			end = text.size ();
		std::string line = text.substr (start, end - start);
		size_t b = line.find_first_not_of (" \t");
		if (b != std::string::npos && line.size () - b >= 5)
		{
			std::string head = line.substr (b, 5);
			for (char& c : head)
				c = upper (c);
			if (head == "NAME:")
			{
				std::string value = line.substr (b + 5);
				value.erase (0, value.find_first_not_of (" \t"));
				value.erase (value.find_last_not_of (" \t\r") + 1);
				if (!value.empty ())
					name = value;
			}
		}
		start = end + 1;
	}

	// The key: from "BNDL1", Base32 with dashes and whitespace between,
	// until kKeyChars characters or something that can't be in a key.
	const size_t prefixLen = std::strlen (kKeyPrefix);
	for (size_t i = 0; i + prefixLen <= text.size (); ++i)
	{
		bool match = true;
		for (size_t k = 0; k < prefixLen && match; ++k)
			match = upper (text[i + k]) == kKeyPrefix[k];
		if (!match)
			continue;

		std::string body;
		size_t j = i + prefixLen;
		for (; j < text.size () && body.size () < kKeyChars; ++j)
		{
			char c = text[j];
			if (c == '-' || isSpace (c))
				continue;
			const char u = upper (c);
			if ((u >= '0' && u <= '9') || (u >= 'A' && u <= 'Z'))
				body += u;
			else
				break;
		}
		if (body.size () != kKeyChars)
			continue;

		uint8_t raw[kKeyBytes];
		if (!parseKey (body, raw))
			continue;
		key = formatKey (raw);
		return true;
	}
	return false;
}

} // namespace R2Licence
