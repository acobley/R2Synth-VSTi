#!/bin/sh
#-----------------------------------------------------------------------------
# Build and run tools/make-presets.cpp, writing installer/presets/.
#
#   tools/make-presets.sh              after editing the patch table
#
# Needs the VST3 SDK: the .vstpreset is written by Steinberg's own
# PresetFile, so the format cannot be wrong. The SDK sources it needs are
# listed in tools/sdk-preset-sources.txt.
#-----------------------------------------------------------------------------
set -e

cd "$(dirname "$0")/.."
ROOT="$(pwd)"
SDK="${VST3_SDK_ROOT:-$ROOT/external/vst3sdk}"
CXX="${CXX:-c++}"

if [ ! -d "$SDK" ]; then
	echo "make-presets: no VST3 SDK at $SDK - run ./setup-xcode.sh once," >&2
	echo "              or set VST3_SDK_ROOT." >&2
	exit 1
fi

SDK_SOURCES=""
while IFS= read -r line; do
	case "$line" in ''|\#*) continue ;; esac
	SDK_SOURCES="$SDK_SOURCES $SDK/$line"
done < "$ROOT/tools/sdk-preset-sources.txt"

# On macOS two of those sources (funknown.cpp, fstring.cpp) need
# CoreFoundation; on Linux they compile without it. See ForTran's script.
SDK_LIBS=""
if [ "$(uname -s)" = "Darwin" ]; then
	SDK_LIBS="-framework CoreFoundation"
fi

OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

mkdir -p installer/presets

# shellcheck disable=SC2086
$CXX -std=c++17 -O2 -DRELEASE=1 -DNDEBUG -I"$SDK" -I"$ROOT/source" \
	"$ROOT/source/R2SynthParams.cpp" \
	"$ROOT/tools/make-presets.cpp" $SDK_SOURCES $SDK_LIBS -o "$OUT/make-presets"

"$OUT/make-presets" "$ROOT/installer/presets"
