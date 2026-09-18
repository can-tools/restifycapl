// ==============================================================================
// exports.cpp -- restifycapl (CAPL REST DLL)
//
// This is the ONLY file in this project that includes the Vector CAPL DLL
// SDK headers or otherwise knows anything about CANoe/CAPL. Every other
// file under src/ (core, http, registry, mapping) must stay ignorant of
// this layer -- see CLAUDE.md and the capl-export-contract skill.
//
// Stage 5 ("Hello DLL", the hard gate): exactly one trivial, self-contained
// operation, purely to prove the CAPL ABI end to end before any REST/HTTP/
// JSON logic exists. No business logic beyond a caller-buffer-safe string
// copy lives here.
// ==============================================================================

// ==============================================================================
// CAPL-visible naming convention -- DECIDED HERE, PERMANENTLY (Stage 5)
// ==============================================================================
//
// Every operation this project will ever expose to CAPL is named:
//
//     restify<VerbNoun>          e.g. restifyGetVersion
//
// i.e. a lowercase project prefix ("restify", taken from the project name
// restifycapl -- "capl" is dropped from the prefix itself, since the name
// is only ever read *inside* a CAPL script, where that suffix would be
// redundant) immediately followed by an UpperCamelCase verb-noun phrase,
// giving an overall lowerCamelCase symbol.
//
// Why this convention, and not something else -- decided deliberately
// because the never-rename rule (capl-export-contract) makes it a one-way
// door: once a later stage ships an operation under this convention,
// changing it is a major-version break, not a refactor.
//
// 1. CAPL itself, and every Vector-shipped CAPL library function visible
//    from the SDK headers and from CANoe's own built-in function library
//    (sysGetVariableInt, diagGetLastResponseCode, dbGetSignalValue, ...),
//    uses lowerCamelCase, usually further prefixed by the owning subsystem
//    ("sys", "diag", "db"). Matching that convention makes restifycapl's
//    operations read as idiomatic CAPL, not as a foreign C library bolted
//    on to the language.
//
// 2. cdll.h's own illustrative sample entry uses a bare "HelloWorld", with
//    no prefix at all. That is acceptable for a single-purpose sample DLL
//    nobody expects to load next to another CAPL DLL, but it is unsafe for
//    a general-purpose library: the CAPL_DLL_INFO table has no
//    namespacing -- every name any loaded CAPL DLL exports lives in one
//    flat, script-global name space. A CAPL script that has restifycapl
//    loaded alongside some other vendor's DLL which also happens to export
//    a generic name like "getVersion" would get an ambiguous or silently
//    wrong binding. A project-specific prefix is the only collision
//    defence available at this layer, so it is applied starting from this,
//    the very first entry.
//
// 3. Deciding it now, on the project's first-ever export, is the entire
//    point of Stage 5 being a hard gate: every operation from Stage 9
//    onward inherits this convention and can never rename it.
// ==============================================================================

#include "cdll.h"  // Vector CAPL DLL SDK -- include/vendor/capl-dll-sdk/

#include <windows.h>
#include <winver.h>

#include <cstring>
#include <vector>

#pragma comment(lib, "version.lib")
// version.lib provides GetFileVersionInfoSize/GetFileVersionInfo/
// VerQueryValue (declared in <winver.h>), used below to read this DLL's own
// embedded VERSIONINFO resource (src/module/version.rc, produced by the
// Makefile from the Git tag / commit count -- see msvc-build-conventions)
// at runtime. This is a standard Windows import library that ships with
// the OS/SDK -- the same class of thing as the seven system libs already
// listed in the Makefile's SYSLIBS, not a third-party or /MT concern.
// Linked here via #pragma comment rather than by editing the Makefile, per
// this stage's explicit instruction not to touch build files;
// build-pipeline-engineer may still want to add it to SYSLIBS for
// visibility even though this pragma alone is sufficient for the link to
// succeed.

namespace {

// ------------------------------------------------------------------------
// CopyOwnVersionString -- the one piece of non-trivial logic in this file,
// kept as a plain, separate function rather than written inline inside the
// exported CAPL wrapper below (see cpp-implementer's "thin glue" rule).
//
// It cannot be reached by the GoogleTest suite: src/module is deliberately
// excluded from `make test` (see the Makefile and cpp-testing-conventions),
// because this function's logic is inseparable from the Windows
// resource/module APIs it wraps and from this DLL's own embedded resource
// -- not because it was left untested by oversight. Verifying it is
// exactly HUM-13's job: load this DLL into real CANoe and call the
// operation from a .can script.
//
// Reads this DLL's own embedded FileVersion string -- the same
// `git describe --tags --always --dirty` value the Makefile bakes into
// version.rc's VER_STRING at build time (see msvc-build-conventions) -- and
// copies it into the caller-supplied buffer. No version number is
// hand-typed anywhere in this file; the value always comes from the
// resource the existing versioning mechanism already produces.
//
// Never returns a raw pointer to the version text (the first post-mortem
// rule from capl-export-contract / CLAUDE.md's standing constraints) -- the
// string only ever travels through a caller-owned buffer with an explicit
// size.
//
// Returns:
//    0  success -- buffer holds the null-terminated version string.
//   -1  invalid arguments (null buffer or zero size).
//   -2  the version string does not fit in the supplied buffer. The buffer
//       is left empty (not truncated) -- callers must never observe a
//       partial version string.
//   -3  the embedded version resource could not be read. Should not happen
//       in a DLL produced by this project's own Makefile, but a
//       resource-stripped or otherwise malformed binary could in principle
//       hit this path, so it is handled rather than assumed away.
// ------------------------------------------------------------------------
long CopyOwnVersionString(char* buffer, unsigned long bufferSize) {
  if (buffer == nullptr || bufferSize == 0) {
    return -1;
  }
  buffer[0] = '\0';

  // Resolve this DLL's own module handle from an address known to lie
  // inside it -- this avoids needing a DllMain hook just to remember the
  // instance handle.
  HMODULE selfModule = nullptr;
  if (!GetModuleHandleExA(
          GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
          reinterpret_cast<LPCSTR>(&CopyOwnVersionString),
          &selfModule)) {
    return -3;
  }

  char modulePath[MAX_PATH];
  DWORD pathLen = GetModuleFileNameA(selfModule, modulePath, MAX_PATH);
  if (pathLen == 0 || pathLen >= MAX_PATH) {
    return -3;
  }

  DWORD verInfoHandle = 0;
  DWORD verInfoSize = GetFileVersionInfoSizeA(modulePath, &verInfoHandle);
  if (verInfoSize == 0) {
    return -3;
  }

  std::vector<BYTE> verInfoBlock(verInfoSize);
  if (!GetFileVersionInfoA(modulePath, verInfoHandle, verInfoSize,
                            verInfoBlock.data())) {
    return -3;
  }

  // Must match version.rc's `BLOCK "040904b0"` under StringFileInfo: lang
  // 0x0409 (English, US), codepage 0x04B0 (== decimal 1200, the same 1200
  // in version.rc's `VALUE "Translation", 0x409, 1200`).
  LPSTR versionText = nullptr;
  UINT versionTextLen = 0;
  if (!VerQueryValueA(verInfoBlock.data(),
                       "\\StringFileInfo\\040904B0\\FileVersion",
                       reinterpret_cast<LPVOID*>(&versionText),
                       &versionTextLen) ||
      versionText == nullptr) {
    return -3;
  }

  // versionTextLen counts the terminating NUL for VerQueryValueA string
  // queries in the normal case; strnlen defends against a malformed
  // resource that omits it.
  size_t textLen = strnlen(versionText, versionTextLen);
  if (textLen + 1 > bufferSize) {
    return -2;
  }

  memcpy(buffer, versionText, textLen);
  buffer[textLen] = '\0';
  return 0;
}

}  // namespace

// ------------------------------------------------------------------------
// restifyGetVersion -- the project's first-ever CAPL-visible operation.
//
// CAPL signature (see the CAPL_DLL_INFO_LIST4 entry below):
//   long restifyGetVersion(char buffer[], dword bufferSize);
//
// Writes this DLL's build-version string (see CopyOwnVersionString above)
// into the caller-supplied buffer. Returns 0 on success, or a negative
// error code (see CopyOwnVersionString) -- most commonly -2 if the
// caller's buffer is too small.
//
// extern "C": avoids C++ name mangling tying this symbol to a specific
// compiler version (capl-export-contract standing rule). CAPLPASCAL:
// matches the calling convention baked into CAPL_FARCALL (cdll.h) --
// required so the call CANoe's runtime constructs through the
// CAPL_DLL_INFO4 function-pointer entry uses the same stack discipline
// this function was actually compiled with; critical on x86, where
// stdcall and cdecl disagree about who cleans the stack. Not CAPLEXPORT:
// CANoe never resolves this symbol by name (GetProcAddress) -- it only
// ever receives its address indirectly, as `adr` in the table returned by
// caplDllGetTable4 below, so this function has no need to appear in the
// DLL's own export directory or in exports.def.
// ------------------------------------------------------------------------
extern "C" long CAPLPASCAL restifyGetVersion(char* buffer,
                                              unsigned long bufferSize) {
  return CopyOwnVersionString(buffer, bufferSize);
}

// ==============================================================================
// CAPL_DLL_INFO_LIST4 -- the real API contract (capl-export-contract).
// APPEND ONLY from this point on: never rename, reorder, or remove a row.
//
// #pragma pack(push, 1) / pop below wraps this table's DEFINITION end to
// end, through and including the terminating all-zero sentinel row -- the
// second post-mortem rule this stage must apply (capl-export-contract /
// CLAUDE.md standing constraints). cdll.h already 1-byte-packs the
// CAPL_DLL_INFO4 struct *type* itself (its own pack(push,1)/pack(pop) pair
// wraps the whole header), so this struct's member layout is already fixed
// regardless of what packing is active where it is later instantiated;
// this pragma pair is applied anyway, directly around the table, so the
// rule's coverage is literally visible at the point a reviewer needs to
// check it (REV-3), rather than relying on an implication from a different
// file.
// ==============================================================================
#pragma pack(push, 1)

CAPL_DLL_INFO4 CAPL_DLL_INFO_LIST4[] = {
    // Reserved version marker -- must always be first; must never be
    // renamed, reordered, or removed (capl-export-contract). `adr` here is
    // not a real function pointer: CDLL_VERSION (2) is reinterpreted
    // through it purely as a version tag CANoe reads, and is never called.
    // The cast matches cdll.h's own documented example for this row
    // verbatim (see the "Example (in the implementation file)" comment in
    // cdll.h).
    {CDLL_VERSION_NAME, (CAPL_FARCALL)CDLL_VERSION, "", "", 0, 0, "", "",
     {""}},

    // restifyGetVersion(char buffer[], dword bufferSize) : long
    // See the naming-convention rationale and restifyGetVersion's own
    // comment above. parTypes "CD" / array "\001\000" mean: parameter 0
    // ('C', array depth 1) is a CAPL char[] buffer passed by reference;
    // parameter 1 ('D', array depth 0) is a scalar unsigned long.
    {"restifyGetVersion",
     (CAPL_FARCALL)restifyGetVersion,
     "restifycapl",
     "Writes this DLL's build version string into the caller-supplied "
     "buffer. Returns 0 on success, a negative error code otherwise.",
     'L',
     2,
     "CD",
     "\001\000",
     {"buffer", "bufferSize"}},

    // Terminating sentinel -- CANoe reads entries until the first one
    // whose name is NULL.
    {0}};

#pragma pack(pop)

// ------------------------------------------------------------------------
// caplDllGetTable4 -- the actual Windows DLL export CANoe locates by name
// (via exports.def / GetProcAddress) and calls to obtain the function
// table above. This is the only function in this file that needs to be a
// real DLL export.
// ------------------------------------------------------------------------
extern "C" CAPLEXPORT CAPL_DLL_INFO4* CAPLCDECL caplDllGetTable4(void) {
  return CAPL_DLL_INFO_LIST4;
}
