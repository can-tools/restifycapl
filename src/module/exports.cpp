// exports.cpp -- restifycapl (CAPL REST DLL)
//
// The ONLY file in this project that includes the CAPL DLL SDK headers or
// knows about CANoe/CAPL -- see CLAUDE.md and capl-export-contract.

// CAPL-visible naming convention -- DECIDED HERE, PERMANENTLY (Stage 5):
//
//     restify<VerbNoun>          e.g. restifyGetVersion
//
// lowercase "restify" prefix + UpperCamelCase verb-noun, giving an overall
// lowerCamelCase symbol matching idiomatic CAPL (sysGetVariableInt,
// dbGetSignalValue, ...). This is a one-way door: the never-rename rule
// (capl-export-contract) means changing it later is a major-version break,
// not a refactor. The CAPL_DLL_INFO table has no namespacing -- every
// loaded CAPL DLL's exports share one flat, script-global name space -- so
// this prefix is the only collision defence available at this layer.

#include "cdll.h"  // Vector CAPL DLL SDK -- include/vendor/capl-dll-sdk/

#include <windows.h>
#include <winver.h>

#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

#include "core/buffer-copy.h"
#include "core/status.h"

#pragma comment(lib, "version.lib")
// Needed for the VerQueryValue* calls below; see the Makefile's SYSLIBS comment.

namespace {

// unsigned long is 32 bits on both targets here -- see plan.md §6 CPP-16.
static_assert(sizeof(unsigned long) == sizeof(std::uint32_t),
              "restifyGetVersion assumes unsigned long is 32 bits");

// Not unit-tested: CAPL/Win32 module glue, excluded per cpp-testing-conventions.
//
// Returns:
//   Status::Ok (0)                          success -- buffer holds the null-terminated version string.
//   Status::InvalidArgument (-1)            invalid arguments (null buffer or zero size).
//   Status::BufferTooSmall (-2)             version string does not fit; buffer is left empty, never truncated.
//   Status::VersionResourceUnavailable (-3) embedded version resource could not be read.
long CopyOwnVersionString(char* buffer, unsigned long bufferSize) {
  if (buffer == nullptr || bufferSize == 0) {
    return static_cast<long>(Status::InvalidArgument);
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
    return static_cast<long>(Status::VersionResourceUnavailable);
  }

  char modulePath[MAX_PATH];
  DWORD pathLen = GetModuleFileNameA(selfModule, modulePath, MAX_PATH);
  if (pathLen == 0 || pathLen >= MAX_PATH) {
    return static_cast<long>(Status::VersionResourceUnavailable);
  }

  DWORD verInfoHandle = 0;
  DWORD verInfoSize = GetFileVersionInfoSizeA(modulePath, &verInfoHandle);
  if (verInfoSize == 0) {
    return static_cast<long>(Status::VersionResourceUnavailable);
  }

  std::vector<BYTE> verInfoBlock(verInfoSize);
  if (!GetFileVersionInfoA(modulePath, verInfoHandle, verInfoSize,
                            verInfoBlock.data())) {
    return static_cast<long>(Status::VersionResourceUnavailable);
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
    return static_cast<long>(Status::VersionResourceUnavailable);
  }

  // versionTextLen counts the terminating NUL for VerQueryValueA string
  // queries in the normal case; strnlen defends against a malformed
  // resource that omits it.
  const std::size_t textLen = strnlen(versionText, versionTextLen);
  const Status copyStatus = CopyToBuffer(std::string_view{versionText, textLen},
                                          buffer,
                                          static_cast<std::uint32_t>(bufferSize));
  return static_cast<long>(copyStatus);
}

}  // namespace

// ------------------------------------------------------------------------
// restifyGetVersion -- the project's first-ever CAPL-visible operation.
//
// CAPL signature (see the CAPL_DLL_INFO_LIST4 entry below):
//   long restifyGetVersion(char buffer[], dword bufferSize);
//
// Writes this DLL's build-version string (see CopyOwnVersionString above)
// into the caller-supplied buffer. Returns Status::Ok (0) on success, or a
// negative Status value (see CopyOwnVersionString) -- most commonly
// Status::BufferTooSmall (-2) if the caller's buffer is too small.
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
