//------------------------------------------------------------------------
// R2Synth - build the factory presets
//
//   tools/make-presets.sh              after editing the patch table below
//
// Writes installer/presets/<name>.vstpreset and <name>.aupreset for every
// patch in kPatches. The patches are written here in the units the panel
// shows (Hz, ms, %, semitones, octaves), so they can be read and tuned
// without a host; anything a patch doesn't mention is the parameter's
// default, exactly as a fresh instance has it.
//
// BOTH FILES CARRY THE SAME BYTES: the stream R2SynthProcessor::getState
// writes (version, count, every parameter normalised, bypass). A .vstpreset
// holds it as its Comp chunk, written by Steinberg's own PresetFile so the
// format can't be wrong; an .aupreset holds it under "Processor State",
// which is where Steinberg's AU wrapper reads it from (auwrapper.mm,
// restoreState). The wrapper also insists on version 0 and on R2Synth's
// subtype and manufacturer codes, and only updates the panel when a
// "Controller State" key is present - so an empty one is written, as Logic
// does for ForTran.
//
// A .vstpreset carries the PROCESSOR CLASS UID and a host silently ignores
// one whose UID does not match, so these files are married to the UID in
// R2SynthIDs.h for good. Likewise the AU codes below and resource/au-info.plist.
//------------------------------------------------------------------------

#include "R2SynthIDs.h"
#include "R2SynthParams.h"

#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/vstpresetfile.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace R2Synth;

namespace {

//------------------------------------------------------------------------
// THE PATCHES. Plain units; Enum parameters take the position number
// (VCO wave 0 Saw / 1 Square; LFO output 0 Shape / 1 Square / 2 Sine;
// VCF type 0 Moog / 1 MS-20 I / 2 MS-20 II).
//------------------------------------------------------------------------
struct Setting { Param id; double plain; };
struct Patch   { const char* name; std::vector<Setting> settings; };

const std::vector<Patch> kPatches = {
	{ "Init", {} },   // every parameter at its default: the fresh-instance sound

	{ "Fat Bass", {
		{ kVco1Wave, 0 }, { kVco1Tune, 0 },     { kVco1Level, 100 },
		{ kVco2Wave, 1 }, { kVco2Tune, -12 },   { kVco2Level, 70 },
		{ kVco3Wave, 0 }, { kVco3Tune, 0.08 },  { kVco3Level, 60 },
		{ kCutoff, 300 }, { kResonance, 25 }, { kKeyTrack, 40 }, { kVcfEnvAmount, 3.5 },
		{ kVcfAttack, 1 }, { kVcfDecay, 220 }, { kVcfSustain, 10 }, { kVcfRelease, 120 },
		{ kVcaAttack, 1 }, { kVcaDecay, 400 }, { kVcaSustain, 85 }, { kVcaRelease, 80 },
		{ kOutputLevel, -3 } } },

	{ "Acid Squelch", {
		{ kVco1Wave, 0 }, { kVco1Level, 100 }, { kVco2Level, 0 }, { kVco3Level, 0 },
		{ kCutoff, 180 }, { kResonance, 78 }, { kKeyTrack, 30 }, { kVcfEnvAmount, 4.5 },
		{ kVcfAttack, 0.5 }, { kVcfDecay, 180 }, { kVcfSustain, 0 }, { kVcfRelease, 100 },
		{ kVcaAttack, 1 }, { kVcaDecay, 300 }, { kVcaSustain, 70 }, { kVcaRelease, 60 },
		{ kPortamento, 60 }, { kOutputLevel, -6 } } },

	{ "Sharp Lead", {
		{ kVco1Wave, 0 }, { kVco1Level, 100 },
		{ kVco2Wave, 0 }, { kVco2Tune, 12 },   { kVco2Level, 50 },
		{ kVco3Wave, 0 }, { kVco3Tune, 0.12 }, { kVco3Level, 60 },
		{ kCutoff, 2500 }, { kResonance, 30 }, { kKeyTrack, 60 }, { kVcfEnvAmount, 2 },
		{ kVcfAttack, 2 }, { kVcfDecay, 400 }, { kVcfSustain, 50 }, { kVcfRelease, 300 },
		{ kVcaAttack, 5 }, { kVcaDecay, 500 }, { kVcaSustain, 90 }, { kVcaRelease, 250 },
		{ kLfoRate, 5.5 }, { kLfoOutput, 2 }, { kLfoPitch, 0.15 },
		{ kPortamento, 30 }, { kOutputLevel, -4 } } },

	{ "Slow Pad", {
		{ kVco1Wave, 0 }, { kVco1Level, 100 },
		{ kVco2Wave, 0 }, { kVco2Tune, 0.1 },  { kVco2Level, 80 },
		{ kVco3Wave, 0 }, { kVco3Tune, -0.1 }, { kVco3Level, 80 },
		{ kCutoff, 900 }, { kResonance, 15 }, { kVcfEnvAmount, 1.5 },
		{ kVcfAttack, 1200 }, { kVcfDecay, 2000 }, { kVcfSustain, 60 }, { kVcfRelease, 1500 },
		{ kVcaAttack, 800 }, { kVcaDecay, 1500 }, { kVcaSustain, 85 }, { kVcaRelease, 2000 },
		{ kLfoRate, 0.3 }, { kLfoOutput, 2 }, { kVcfLfo, 0.3 },
		{ kLfo2Rate, 0.1 }, { kLfo2Level, 40 }, { kOutputLevel, -6 } } },

	{ "Wobble Bass", {
		{ kVco1Wave, 0 }, { kVco1Level, 100 },
		{ kVco2Wave, 1 }, { kVco2Tune, -12 }, { kVco2Level, 80 }, { kVco3Level, 0 },
		{ kCutoff, 250 }, { kResonance, 55 }, { kVcfEnvAmount, 1 },
		{ kVcfSustain, 100 },
		{ kVcaAttack, 2 }, { kVcaSustain, 100 }, { kVcaRelease, 80 },
		{ kLfoRate, 4 }, { kLfoOutput, 2 }, { kVcfLfo, 2.5 }, { kOutputLevel, -5 } } },

	{ "Brass", {
		{ kVco1Wave, 0 }, { kVco1Level, 100 },
		{ kVco2Wave, 0 }, { kVco2Tune, 0.08 }, { kVco2Level, 90 }, { kVco3Level, 0 },
		{ kCutoff, 600 }, { kResonance, 10 }, { kKeyTrack, 60 }, { kVcfEnvAmount, 3 },
		{ kVcfAttack, 60 }, { kVcfDecay, 500 }, { kVcfSustain, 45 }, { kVcfRelease, 200 },
		{ kVcaAttack, 30 }, { kVcaDecay, 300 }, { kVcaSustain, 90 }, { kVcaRelease, 200 },
		{ kLfoRate, 5 }, { kLfoOutput, 2 }, { kLfoPitch, 0.1 }, { kOutputLevel, -4 } } },

	{ "Pluck", {
		{ kVco1Wave, 1 }, { kVco1Level, 100 },
		{ kVco2Wave, 0 }, { kVco2Tune, 12 }, { kVco2Level, 40 }, { kVco3Level, 0 },
		{ kCutoff, 400 }, { kResonance, 35 }, { kKeyTrack, 70 }, { kVcfEnvAmount, 4.5 },
		{ kVcfAttack, 0.5 }, { kVcfDecay, 150 }, { kVcfSustain, 0 }, { kVcfRelease, 150 },
		{ kVcaAttack, 0.5 }, { kVcaDecay, 350 }, { kVcaSustain, 0 }, { kVcaRelease, 300 },
		{ kOutputLevel, -3 } } },

	{ "Sub Bass", {
		{ kVco1Wave, 1 }, { kVco1Level, 100 },
		{ kVco2Wave, 1 }, { kVco2Tune, -12 }, { kVco2Level, 80 }, { kVco3Level, 0 },
		{ kCutoff, 150 }, { kResonance, 0 }, { kKeyTrack, 50 }, { kVcfEnvAmount, 0.5 },
		{ kVcfSustain, 100 },
		{ kVcaAttack, 2 }, { kVcaDecay, 100 }, { kVcaSustain, 100 }, { kVcaRelease, 60 } } },

	{ "MS-20 Scream", {
		{ kVcfType, 1 },
		{ kVco1Wave, 0 }, { kVco1Level, 100 },
		{ kVco2Wave, 0 }, { kVco2Tune, -0.1 }, { kVco2Level, 70 }, { kVco3Level, 0 },
		{ kCutoff, 700 }, { kResonance, 85 }, { kKeyTrack, 50 }, { kVcfEnvAmount, 3 },
		{ kVcfAttack, 5 }, { kVcfDecay, 600 }, { kVcfSustain, 30 }, { kVcfRelease, 300 },
		{ kVcaAttack, 3 }, { kVcaSustain, 90 }, { kVcaRelease, 200 },
		{ kLfoRate, 6 }, { kLfoOutput, 2 }, { kLfoPitch, 0.2 },
		{ kPortamento, 120 }, { kOutputLevel, -8 } } },

	{ "Siren", {
		{ kVco1Wave, 1 }, { kVco1Level, 100 }, { kVco2Level, 0 }, { kVco3Level, 0 },
		{ kCutoff, 3000 }, { kResonance, 20 }, { kVcfEnvAmount, 0 }, { kVcfSustain, 100 },
		{ kVcaAttack, 5 }, { kVcaSustain, 100 }, { kVcaRelease, 400 },
		{ kLfoRate, 0.5 }, { kLfoOutput, 0 }, { kLfoShape, 100 }, { kLfoPitch, 12 },
		{ kOutputLevel, -8 } } },

	{ "Tremolo Organ", {
		{ kVco1Wave, 1 }, { kVco1Level, 100 },
		{ kVco2Wave, 1 }, { kVco2Tune, 12 }, { kVco2Level, 60 },
		{ kVco3Wave, 1 }, { kVco3Tune, 7 },  { kVco3Level, 40 },
		{ kCutoff, 4000 }, { kResonance, 0 }, { kVcfEnvAmount, 0 }, { kVcfSustain, 100 },
		{ kVcaAttack, 5 }, { kVcaSustain, 100 }, { kVcaRelease, 80 },
		{ kLfoRate, 6 }, { kLfoOutput, 2 }, { kVcaLfo, 40 }, { kOutputLevel, -6 } } },
};

//------------------------------------------------------------------------
// The AU identity, which must match resource/au-info.plist.
//------------------------------------------------------------------------
constexpr uint32 fourCC (const char* s)
{
	return (uint32 (uint8 (s[0])) << 24) | (uint32 (uint8 (s[1])) << 16) |
	       (uint32 (uint8 (s[2])) << 8)  |  uint32 (uint8 (s[3]));
}

constexpr uint32 kAuType         = fourCC ("aumu");   // instrument
constexpr uint32 kAuSubtype      = fourCC ("SiSy");
constexpr uint32 kAuManufacturer = fourCC ("AECo");

// R2SynthProcessor.cpp: static const int32 kStateVersion = 1;
constexpr int32 kStateVersion = 1;

//------------------------------------------------------------------------
void putInt32 (std::string& out, int32 v)
{
	const uint32 u = (uint32) v;
	for (int i = 0; i < 4; ++i)
		out.push_back ((char) ((u >> (8 * i)) & 0xff));   // little-endian, as IBStreamer (kLittleEndian)
}

void putDouble (std::string& out, double v)
{
	uint64 u;
	std::memcpy (&u, &v, 8);
	for (int i = 0; i < 8; ++i)
		out.push_back ((char) ((u >> (8 * i)) & 0xff));
}

/** The processor's getState stream for a patch, or "" if a setting is out of range. */
std::string stateFor (const Patch& p)
{
	double normalized[kNumParams];
	for (int i = 0; i < kParamCount; ++i)
		normalized[i] = kParams[i].defaultNormalized ();

	for (const auto& s : p.settings)
	{
		const ParamDef& d = paramDef (s.id);
		if (s.plain < d.plainMin - 1e-9 || s.plain > d.plainMax + 1e-9)
		{
			std::fprintf (stderr, "  %-16s %s = %g is outside %g .. %g %s\n", p.name, d.title, s.plain,
			              d.plainMin, d.plainMax, d.units);
			return {};
		}
		normalized[s.id] = d.toNormalized (s.plain);
	}

	std::string out;
	putInt32 (out, kStateVersion);
	putInt32 (out, (int32) kNumParams);
	for (int i = 0; i < kParamCount; ++i)
		putDouble (out, normalized[i]);
	putInt32 (out, 0);   // not bypassed
	return out;
}

//------------------------------------------------------------------------
std::string base64 (const std::string& in)
{
	static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string out;
	size_t i = 0;
	for (; i + 2 < in.size (); i += 3)
	{
		const uint32 n = (uint32 (uint8 (in[i])) << 16) | (uint32 (uint8 (in[i + 1])) << 8) | uint8 (in[i + 2]);
		out += t[(n >> 18) & 63]; out += t[(n >> 12) & 63]; out += t[(n >> 6) & 63]; out += t[n & 63];
	}
	if (i + 1 == in.size ())
	{
		const uint32 n = uint32 (uint8 (in[i])) << 16;
		out += t[(n >> 18) & 63]; out += t[(n >> 12) & 63]; out += "==";
	}
	else if (i + 2 == in.size ())
	{
		const uint32 n = (uint32 (uint8 (in[i])) << 16) | (uint32 (uint8 (in[i + 1])) << 8);
		out += t[(n >> 18) & 63]; out += t[(n >> 12) & 63]; out += t[(n >> 6) & 63]; out += "=";
	}

	std::string wrapped;   // 60 a line, tab-indented, as Logic writes them
	for (size_t j = 0; j < out.size (); j += 60)
		wrapped += "\t" + out.substr (j, 60) + "\n";
	return wrapped;
}

std::string xmlEscape (const std::string& s)
{
	std::string out;
	for (char c : s)
	{
		if (c == '&') out += "&amp;";
		else if (c == '<') out += "&lt;";
		else if (c == '>') out += "&gt;";
		else out += c;
	}
	return out;
}

std::string auPreset (const std::string& name, const std::string& state)
{
	std::string x;
	x += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
	x += "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
	x += "<plist version=\"1.0\">\n<dict>\n";
	x += "\t<key>Controller State</key>\n\t<data>\n\t</data>\n";
	x += "\t<key>Processor State</key>\n\t<data>\n" + base64 (state) + "\t</data>\n";
	x += "\t<key>manufacturer</key>\n\t<integer>" + std::to_string (kAuManufacturer) + "</integer>\n";
	x += "\t<key>name</key>\n\t<string>" + xmlEscape (name) + "</string>\n";
	x += "\t<key>subtype</key>\n\t<integer>" + std::to_string (kAuSubtype) + "</integer>\n";
	x += "\t<key>type</key>\n\t<integer>" + std::to_string (kAuType) + "</integer>\n";
	x += "\t<key>version</key>\n\t<integer>0</integer>\n";
	x += "</dict>\n</plist>\n";
	return x;
}

bool writeFile (const std::string& path, const void* data, size_t size)
{
	FILE* f = std::fopen (path.c_str (), "wb");
	if (!f)
	{
		std::fprintf (stderr, "make-presets: cannot write %s\n", path.c_str ());
		return false;
	}
	const bool ok = size == 0 || std::fwrite (data, 1, size, f) == size;
	std::fclose (f);
	return ok;
}

} // namespace

//------------------------------------------------------------------------
int main (int argc, char** argv)
{
	const std::string dir = (argc > 1) ? argv[1] : "installer/presets";
	int failed = 0;

	for (const auto& p : kPatches)
	{
		const std::string state = stateFor (p);
		if (state.empty ())
		{
			++failed;
			continue;
		}

		MemoryStream component;
		component.write ((void*) state.data (), (int32) state.size (), nullptr);
		component.seek (0, IBStream::kIBSeekSet, nullptr);

		MemoryStream preset;
		if (!PresetFile::savePreset (&preset, kR2SynthProcessorUID, &component))
		{
			std::fprintf (stderr, "  %-16s savePreset failed\n", p.name);
			++failed;
			continue;
		}

		const std::string base = dir + "/" + p.name;
		const std::string au = auPreset (p.name, state);
		if (!writeFile (base + ".vstpreset", preset.getData (), (size_t) preset.getSize ())
		    || !writeFile (base + ".aupreset", au.data (), au.size ()))
		{
			++failed;
			continue;
		}

		std::printf ("  %-16s %2d settings -> %s.vstpreset, %s.aupreset\n", p.name, (int) p.settings.size (),
		             p.name, p.name);
	}

	std::printf ("\n%d presets, %d written, %d failed\n", (int) kPatches.size (),
	             (int) kPatches.size () - failed, failed);
	return failed == 0 ? 0 : 1;
}
