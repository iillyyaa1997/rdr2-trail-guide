#!/bin/bash
# Builds TrailGuide.asi with clang-cl + lld-link in the rdr2-msvc-build Docker image (MS CRT/SDK from xwin).
# usage: ./build.sh            -> out/TrailGuide.asi
#        TEST=1 ./build.sh     -> out/TrailGuide-test.asi (test hooks: commands from TrailGuide.test)
set -euo pipefail
cd "$(dirname "$0")"
here="$PWD"
tc="${XWIN_DIR:-$here/../.toolchains/xwin}"
[ -d "$tc/crt" ] || { echo "xwin toolchain not found at $tc (set XWIN_DIR)"; exit 1; }
name=TrailGuide; defs=""
if [ "${TEST:-0}" = 1 ]; then name=TrailGuide-test; defs="/D TG_TEST"; fi
mkdir -p out
docker run --rm -v "$here":/w -v "$tc":/xwin:ro -w /w -e NAME="$name" -e DEFS="$defs" rdr2-msvc-build sh tools/build-in-docker.sh
ls -l "out/$name.asi" | awk '{print $5, $NF}'
shasum -a 256 "out/$name.asi" | cut -c1-8 | tr a-f A-F
