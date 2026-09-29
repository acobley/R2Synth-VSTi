#!/bin/bash
#------------------------------------------------------------------------
# tools/run-tests.sh - the suites that need no build, in one command.
#
#   DSP     tests/DspTests.cpp     - SDK-free, nothing but a compiler
#   Layout  tests/LayoutTests.cpp  - needs ONE header-only SDK file
#                                    (external/vst3sdk, or VST3_SDK_ROOT)
#   Licence tests/LicenceTests.cpp - SDK-free; Monocypher built as C first
#
# tests/ProcessorTests.cpp needs the SDK built as libraries, so it runs
# from tools/linux-validate/ alongside the validator - see the banner
# there.
#------------------------------------------------------------------------
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CXX="${CXX:-c++}"
CC="${CC:-cc}"
SDK="${VST3_SDK_ROOT:-$ROOT/external/vst3sdk}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

fail=0

run () {   # run <label> <output> <extra flags> <source...>
    local label="$1" out="$2" flags="$3"; shift 3
    printf '\n=== %s ===\n' "$label"
    if ! "$CXX" -std=c++17 -O2 $flags -I"$ROOT/source" -o "$WORK/$out" "$@" 2>"$WORK/build.err"; then
        echo "  BUILD FAILED"
        head -20 "$WORK/build.err" | sed 's/^/  /'
        fail=1
        return
    fi
    if "$WORK/$out"; then :; else fail=1; fi
}

# -ffp-contract=off: the DSP suite holds the ladder BIT-IDENTICAL to a
# verbatim copy of SpaceDub's loop, and on arm64 the compiler is free to
# fuse a*b+c into one FMA in one copy and not the other - which it did,
# in the Mac's Linux VM. Contraction changes the last bit, not the sound;
# this makes the comparison about the arithmetic rather than the codegen.
run "DSP" dsptests "-ffp-contract=off" \
    "$ROOT/tests/DspTests.cpp" "$ROOT/source/SimpleSynthDsp.cpp"

if [ -f "$SDK/pluginterfaces/vst/vsttypes.h" ]; then
    run "Layout" layouttests "-I$SDK" \
        "$ROOT/tests/LayoutTests.cpp" "$ROOT/source/SimpleSynthReadouts.cpp" \
        "$ROOT/source/SimpleSynthParams.cpp" "$ROOT/source/SimpleSynthDsp.cpp"
else
    printf '\n=== Layout ===\n  SKIPPED: no VST3 SDK at %s (run ./setup-xcode.sh once, or set VST3_SDK_ROOT)\n' "$SDK"
fi

MONO="$ROOT/third_party/monocypher"
if "$CC" -std=c99 -O2 -c -o "$WORK/monocypher.o" "$MONO/monocypher.c" 2>"$WORK/build.err" &&
   "$CC" -std=c99 -O2 -c -o "$WORK/monocypher-ed25519.o" "$MONO/monocypher-ed25519.c" 2>>"$WORK/build.err"; then
    run "Licence" licencetests "-Wall -Wextra -I$MONO -I$ROOT/tools/licence" \
        "$ROOT/tests/LicenceTests.cpp" "$ROOT/source/LicenceKey.cpp" \
        "$ROOT/source/LicenceSha256.cpp" "$ROOT/source/LicenceState.cpp" \
        "$ROOT/source/LicenceStore.cpp" "$ROOT/tools/licence/LicenceIssue.cpp" \
        "$WORK/monocypher.o" "$WORK/monocypher-ed25519.o"
else
    printf '\n=== Licence ===\n  BUILD FAILED (Monocypher)\n'; head -20 "$WORK/build.err" | sed 's/^/  /'; fail=1
fi

echo
if [ $fail -eq 0 ]; then echo "ALL SUITES PASSED"; else echo "SOMETHING FAILED"; fi
exit $fail
