#!/bin/sh
# Runs inside rdr2-msvc-build: compiles src/**/*.cpp, generates the ScriptHookRDR2 import library from the symbols the
# objects import, links out/$NAME.asi. Env: NAME, DEFS.
set -e
OBJ=/tmp/obj; rm -rf $OBJ; mkdir -p $OBJ
FLAGS="--target=x86_64-pc-windows-msvc -fms-compatibility-version=19.29 /nologo /c /std:c++17 /O2 /MT /EHsc /fp:fast /W3 \
 /D WIN32 /D _CRT_SECURE_NO_WARNINGS /D NDEBUG /D _WINDOWS /D _USRDLL $DEFS /I src \
 /imsvc /xwin/crt/include /imsvc /xwin/sdk/include/ucrt /imsvc /xwin/sdk/include/um /imsvc /xwin/sdk/include/shared \
 -Wno-unused-parameter -Wno-missing-field-initializers -Wno-microsoft-cast"
export FLAGS OBJ
find src -name '*.cpp' | sort > /tmp/srcs
: > /tmp/failed
compile_one() {
  o="$OBJ/$(echo "$1" | tr '/' '_' | sed 's/\.cpp$/.obj/')"
  clang-cl $FLAGS "/Fo$o" "$1" 2>&1 | head -60
  [ -f "$o" ] || echo "$1" >> /tmp/failed
}
n=0
while read -r f; do
  ( compile_one "$f" ) &
  n=$((n+1)); if [ $n -ge 8 ]; then wait; n=0; fi
done < /tmp/srcs
wait
if [ -s /tmp/failed ]; then echo "FAILED:"; cat /tmp/failed; exit 1; fi
NAMES=$(paste -sd'|' tools/scripthook.names)
{
  echo "LIBRARY ScriptHookRDR2.dll"
  echo "EXPORTS"
  llvm-nm --undefined-only $OBJ/*.obj | awk '{print $NF}' | sed 's/^__imp_//' | grep -E "^\?($NAMES)@@" | sort -u
} > /tmp/sh.def
llvm-lib /nologo /def:/tmp/sh.def /machine:x64 /out:/tmp/ScriptHookRDR2.lib
lld-link /nologo /DLL /OUT:out/$NAME.asi /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /MACHINE:X64 \
  /LIBPATH:/xwin/crt/lib/x86_64 /LIBPATH:/xwin/sdk/lib/um/x86_64 /LIBPATH:/xwin/sdk/lib/ucrt/x86_64 \
  $OBJ/*.obj /tmp/ScriptHookRDR2.lib kernel32.lib user32.lib shell32.lib
echo "ScriptHookRDR2 imports: $(tail -n +3 /tmp/sh.def | wc -l)"
