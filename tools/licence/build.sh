#!/bin/bash
#------------------------------------------------------------------------
# tools/licence/build.sh - build the licence tools with the Mac's own
# compiler (Xcode command-line tools). No cmake, no SDK, no network.
#
#   tools/licence/build.sh   -> build-licence/r2-licence-{keygen,issue,check,trial}
#
# FIRST TIME ONLY (makes the development keypair):
#   mkdir -p licence-dev
#   build-licence/r2-licence-keygen --dev \
#       --seed licence-dev/dev-signing-seed.hex --header source/LicencePublicKey.h
#   tools/licence/build.sh          # again, so the others pick up the new key
#
# Then, any time:
#   build-licence/r2-licence-issue --seed licence-dev/dev-signing-seed.hex --name "Andy Cobley"
#   build-licence/r2-licence-check "Andy Cobley" "BNDL1-..."
#   build-licence/r2-licence-trial show | days-ago 8 | reset | unlicense
#------------------------------------------------------------------------
set -eu

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CC="${CC:-cc}"
CXX="${CXX:-c++}"
OUT="$ROOT/build-licence"
MONO="$ROOT/third_party/monocypher"
TOOLS="$ROOT/tools/licence"
mkdir -p "$OUT"

# Monocypher is C: compile it as C, then link it into the C++ tools.
"$CC" -std=c99 -O2 -c -o "$OUT/monocypher.o"         "$MONO/monocypher.c"
"$CC" -std=c99 -O2 -c -o "$OUT/monocypher-ed25519.o" "$MONO/monocypher-ed25519.c"

# LicenceIssue (signing) lives with the tools, not in source/, so the
# plug-in - which globs source/ - never contains it.
CORE=("$ROOT/source/LicenceKey.cpp" "$ROOT/source/LicenceSha256.cpp" \
      "$TOOLS/LicenceIssue.cpp" "$OUT/monocypher.o" "$OUT/monocypher-ed25519.o")
FLAGS=(-std=c++17 -O2 -Wall -Wextra -I"$ROOT/source" -I"$TOOLS" -I"$MONO")

"$CXX" "${FLAGS[@]}" -o "$OUT/r2-licence-keygen" "$TOOLS/r2-licence-keygen.cpp" "${CORE[@]}"
echo "built build-licence/r2-licence-keygen"

if [ -f "$ROOT/source/LicencePublicKey.h" ]; then
    "$CXX" "${FLAGS[@]}" -o "$OUT/r2-licence-issue" "$TOOLS/r2-licence-issue.cpp" "${CORE[@]}"
    "$CXX" "${FLAGS[@]}" -o "$OUT/r2-licence-check" "$TOOLS/r2-licence-check.cpp" "${CORE[@]}"
    "$CXX" "${FLAGS[@]}" -o "$OUT/r2-licence-trial" "$TOOLS/r2-licence-trial.cpp" \
        "$ROOT/source/LicenceState.cpp" "$ROOT/source/LicenceStore.cpp" "${CORE[@]}"
    echo "built build-licence/r2-licence-issue, r2-licence-check, r2-licence-trial"
else
    echo "source/LicencePublicKey.h not there yet: run r2-licence-keygen, then this script again"
fi
