// exports.cpp -- restifycapl (CAPL REST DLL)
//
// The ONLY file in this project that includes the CAPL DLL SDK headers or
// knows about CANoe/CAPL -- see CLAUDE.md and capl-export-contract.

// CAPL-visible naming convention -- DECIDED HERE, PERMANENTLY:
//
//     restify<VerbNoun>          e.g. restifyReadVersion
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
#include "http/async-operations.h"
#include "http/async-text-api.h"
#include "http/http-client.h"
#include "http/sync-text-api.h"
#include "mapping/json-text-api.h"

#pragma comment(lib, "version.lib")
// Needed for the VerQueryValue* calls below; see the Makefile's SYSLIBS comment.

namespace {

// unsigned long is 32 bits on both targets here.
static_assert(sizeof(unsigned long) == sizeof(std::uint32_t),
              "restifyReadVersion assumes unsigned long is 32 bits");
// The shims move CAPL long values through std::int32_t.
static_assert(sizeof(long) == sizeof(std::int32_t),
              "the export shims assume CAPL long maps to a 32-bit long");

// type - 128 marks a CAPL by-reference parameter (docs/capl-sync-surface.md).
// Spelled via named constants, not inline arithmetic, to avoid a /W4
// narrowing diagnostic in the table's braced parTypes initializers below.
constexpr char kRefLong = static_cast<char>('L' - 128);
constexpr char kRefDword = static_cast<char>('D' - 128);
constexpr char kRefDouble = static_cast<char>('F' - 128);

HttpClient& SyncClient() {
  static HttpClient instance;
  return instance;
}

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
// restifyReadVersion -- the project's first-ever CAPL-visible operation.
//
// CAPL signature (see the CAPL_DLL_INFO_LIST4 entry below):
//   long restifyReadVersion(char buffer[], dword bufferSize);
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
extern "C" long CAPLPASCAL restifyReadVersion(char* buffer,
                                               unsigned long bufferSize) {
  return CopyOwnVersionString(buffer, bufferSize);
}

// ------------------------------------------------------------------------
// Six synchronous REST shims -- each is a direct forwarding call into the
// matching Execute*Sync function (src/http/sync-text-api.h), which owns
// all parsing, request assembly and status mapping. Signature table,
// header-block grammar and status codes: docs/capl-sync-surface.md.
// ------------------------------------------------------------------------
extern "C" long CAPLPASCAL restifyGetSync(char* url, unsigned long urlSize,
                                           char* requestHeaders,
                                           unsigned long requestHeadersSize,
                                           char* responseBody,
                                           unsigned long responseBodySize,
                                           long* httpStatusCode,
                                           unsigned long* responseBodyLength) {
  std::int32_t status = *httpStatusCode;
  std::uint32_t length = *responseBodyLength;
  Status result = ExecuteGetSync(SyncClient(), url, urlSize, requestHeaders,
                                  requestHeadersSize, responseBody, responseBodySize,
                                  status, length);
  *httpStatusCode = status;
  *responseBodyLength = length;
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyDeleteSync(char* url, unsigned long urlSize,
                                              char* requestHeaders,
                                              unsigned long requestHeadersSize,
                                              char* responseBody,
                                              unsigned long responseBodySize,
                                              long* httpStatusCode,
                                              unsigned long* responseBodyLength) {
  std::int32_t status = *httpStatusCode;
  std::uint32_t length = *responseBodyLength;
  Status result = ExecuteDeleteSync(SyncClient(), url, urlSize, requestHeaders,
                                     requestHeadersSize, responseBody, responseBodySize,
                                     status, length);
  *httpStatusCode = status;
  *responseBodyLength = length;
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyPostSync(char* url, unsigned long urlSize,
                                            char* requestHeaders,
                                            unsigned long requestHeadersSize,
                                            char* requestBody, unsigned long requestBodySize,
                                            char* responseBody, unsigned long responseBodySize,
                                            long* httpStatusCode,
                                            unsigned long* responseBodyLength) {
  std::int32_t status = *httpStatusCode;
  std::uint32_t length = *responseBodyLength;
  Status result = ExecutePostSync(SyncClient(), url, urlSize, requestHeaders,
                                   requestHeadersSize, requestBody, requestBodySize,
                                   responseBody, responseBodySize, status, length);
  *httpStatusCode = status;
  *responseBodyLength = length;
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyPutSync(char* url, unsigned long urlSize,
                                           char* requestHeaders,
                                           unsigned long requestHeadersSize,
                                           char* requestBody, unsigned long requestBodySize,
                                           char* responseBody, unsigned long responseBodySize,
                                           long* httpStatusCode,
                                           unsigned long* responseBodyLength) {
  std::int32_t status = *httpStatusCode;
  std::uint32_t length = *responseBodyLength;
  Status result = ExecutePutSync(SyncClient(), url, urlSize, requestHeaders,
                                  requestHeadersSize, requestBody, requestBodySize,
                                  responseBody, responseBodySize, status, length);
  *httpStatusCode = status;
  *responseBodyLength = length;
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyPatchSync(char* url, unsigned long urlSize,
                                             char* requestHeaders,
                                             unsigned long requestHeadersSize,
                                             char* requestBody, unsigned long requestBodySize,
                                             char* responseBody, unsigned long responseBodySize,
                                             long* httpStatusCode,
                                             unsigned long* responseBodyLength) {
  std::int32_t status = *httpStatusCode;
  std::uint32_t length = *responseBodyLength;
  Status result = ExecutePatchSync(SyncClient(), url, urlSize, requestHeaders,
                                    requestHeadersSize, requestBody, requestBodySize,
                                    responseBody, responseBodySize, status, length);
  *httpStatusCode = status;
  *responseBodyLength = length;
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyRequestSync(char* method, unsigned long methodSize, char* url,
                                               unsigned long urlSize, char* requestHeaders,
                                               unsigned long requestHeadersSize,
                                               char* requestBody, unsigned long requestBodySize,
                                               char* responseBody, unsigned long responseBodySize,
                                               unsigned long connectTimeoutMs,
                                               unsigned long totalTimeoutMs,
                                               unsigned long maxResponseBytes,
                                               long* httpStatusCode,
                                               unsigned long* responseBodyLength) {
  std::int32_t status = *httpStatusCode;
  std::uint32_t length = *responseBodyLength;
  Status result = ExecuteRequestSync(SyncClient(), method, methodSize, url, urlSize,
                                      requestHeaders, requestHeadersSize, requestBody,
                                      requestBodySize, connectTimeoutMs, totalTimeoutMs,
                                      maxResponseBytes, responseBody, responseBodySize,
                                      status, length);
  *httpStatusCode = status;
  *responseBodyLength = length;
  return static_cast<long>(result);
}

// ------------------------------------------------------------------------
// Eleven asynchronous REST shims -- six dispatch shims and five lifecycle
// shims, each a direct forwarding call into the matching Dispatch*Async /
// Poll/Await/Read/Discard function (src/http/async-text-api.h), which owns
// all parsing, slot management and status mapping. Signature table,
// state-machine semantics and status codes: docs/capl-async-surface.md.
// ------------------------------------------------------------------------
extern "C" long CAPLPASCAL restifyGetAsync(char* url, unsigned long urlSize,
                                            char* requestHeaders,
                                            unsigned long requestHeadersSize,
                                            std::uint32_t* requestId) {
  Status result = DispatchGetAsync(SyncClient(), DefaultAsyncEngine(), url, urlSize,
                                    requestHeaders, requestHeadersSize, *requestId);
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyDeleteAsync(char* url, unsigned long urlSize,
                                               char* requestHeaders,
                                               unsigned long requestHeadersSize,
                                               std::uint32_t* requestId) {
  Status result = DispatchDeleteAsync(SyncClient(), DefaultAsyncEngine(), url, urlSize,
                                       requestHeaders, requestHeadersSize, *requestId);
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyPostAsync(char* url, unsigned long urlSize,
                                             char* requestHeaders,
                                             unsigned long requestHeadersSize,
                                             char* requestBody, unsigned long requestBodySize,
                                             std::uint32_t* requestId) {
  Status result = DispatchPostAsync(SyncClient(), DefaultAsyncEngine(), url, urlSize,
                                     requestHeaders, requestHeadersSize, requestBody,
                                     requestBodySize, *requestId);
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyPutAsync(char* url, unsigned long urlSize,
                                            char* requestHeaders,
                                            unsigned long requestHeadersSize,
                                            char* requestBody, unsigned long requestBodySize,
                                            std::uint32_t* requestId) {
  Status result = DispatchPutAsync(SyncClient(), DefaultAsyncEngine(), url, urlSize,
                                    requestHeaders, requestHeadersSize, requestBody,
                                    requestBodySize, *requestId);
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyPatchAsync(char* url, unsigned long urlSize,
                                              char* requestHeaders,
                                              unsigned long requestHeadersSize,
                                              char* requestBody, unsigned long requestBodySize,
                                              std::uint32_t* requestId) {
  Status result = DispatchPatchAsync(SyncClient(), DefaultAsyncEngine(), url, urlSize,
                                      requestHeaders, requestHeadersSize, requestBody,
                                      requestBodySize, *requestId);
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyRequestAsync(char* method, unsigned long methodSize,
                                                char* url, unsigned long urlSize,
                                                char* requestHeaders,
                                                unsigned long requestHeadersSize,
                                                char* requestBody, unsigned long requestBodySize,
                                                unsigned long connectTimeoutMs,
                                                unsigned long totalTimeoutMs,
                                                unsigned long maxResponseBytes,
                                                std::uint32_t* requestId) {
  Status result = DispatchRequestAsync(SyncClient(), DefaultAsyncEngine(), method, methodSize,
                                        url, urlSize, requestHeaders, requestHeadersSize,
                                        requestBody, requestBodySize, connectTimeoutMs,
                                        totalTimeoutMs, maxResponseBytes, *requestId);
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyPollResponse(unsigned long requestId, std::int32_t* state) {
  Status result = PollAsyncResponse(DefaultAsyncEngine(), requestId, *state);
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyAwaitResponse(unsigned long requestId,
                                                 unsigned long waitTimeoutMs) {
  Status result = AwaitAsyncResponse(DefaultAsyncEngine(), requestId, waitTimeoutMs);
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyReadResponse(unsigned long requestId, char* responseBody,
                                                unsigned long responseBodySize,
                                                std::int32_t* requestStatus,
                                                std::int32_t* httpStatusCode,
                                                std::uint32_t* responseBodyLength) {
  Status result = ReadAsyncResponse(DefaultAsyncEngine(), requestId, responseBody,
                                     responseBodySize, *requestStatus, *httpStatusCode,
                                     *responseBodyLength);
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyDiscardResponse(unsigned long requestId) {
  Status result = DiscardAsyncResponse(DefaultAsyncEngine(), requestId);
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyDiscardAllResponses(std::uint32_t* stillRunning) {
  Status result = DiscardAllAsyncResponses(DefaultAsyncEngine(), *stillRunning);
  return static_cast<long>(result);
}

// JSON shims -- signatures and status codes: docs/capl-json-surface.md.
extern "C" long CAPLPASCAL restifyJsonParse(char* json, unsigned long jsonSize,
                                             std::uint32_t* documentId) {
  Status result = ParseJsonDocument(DefaultJsonDocumentStore(), json, jsonSize, *documentId);
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyJsonCountEntries(unsigned long documentId,
                                                    std::uint32_t* entryCount) {
  Status result = CountJsonEntries(DefaultJsonDocumentStore(), documentId, *entryCount);
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyJsonReadEntry(unsigned long documentId,
                                                 unsigned long entryIndex, char* key,
                                                 unsigned long keySize, char* value,
                                                 unsigned long valueSize,
                                                 std::int32_t* valueType) {
  Status result = ReadJsonEntry(DefaultJsonDocumentStore(), documentId, entryIndex, key,
                                 keySize, value, valueSize, *valueType);
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyJsonReadValue(unsigned long documentId, char* path,
                                                 unsigned long pathSize, char* value,
                                                 unsigned long valueSize,
                                                 std::int32_t* valueType) {
  Status result = ReadJsonValue(DefaultJsonDocumentStore(), documentId, path, pathSize, value,
                                 valueSize, *valueType);
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyJsonDiscardDocument(unsigned long documentId) {
  Status result = DiscardJsonDocument(DefaultJsonDocumentStore(), documentId);
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyJsonDiscardAllDocuments(std::uint32_t* discardedCount) {
  Status result = DiscardAllJsonDocuments(DefaultJsonDocumentStore(), *discardedCount);
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyJsonNormalize(char* json, unsigned long jsonSize,
                                                 char* normalized,
                                                 unsigned long normalizedSize) {
  Status result = NormalizeJsonText(json, jsonSize, normalized, normalizedSize);
  return static_cast<long>(result);
}

// Typed JSON read shims -- signatures and status codes: docs/capl-json-surface.md.
extern "C" long CAPLPASCAL restifyJsonReadLong(unsigned long documentId, char* path,
                                                unsigned long pathSize, std::int32_t* value) {
  Status result = ReadJsonLong(DefaultJsonDocumentStore(), documentId, path, pathSize, *value);
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyJsonReadDouble(unsigned long documentId, char* path,
                                                  unsigned long pathSize, double* value) {
  Status result = ReadJsonDouble(DefaultJsonDocumentStore(), documentId, path, pathSize, *value);
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyJsonReadBool(unsigned long documentId, char* path,
                                                unsigned long pathSize, std::int32_t* value) {
  Status result = ReadJsonBool(DefaultJsonDocumentStore(), documentId, path, pathSize, *value);
  return static_cast<long>(result);
}

extern "C" long CAPLPASCAL restifyJsonCountElements(unsigned long documentId, char* path,
                                                     unsigned long pathSize,
                                                     std::uint32_t* elementCount) {
  Status result = CountJsonElements(DefaultJsonDocumentStore(), documentId, path, pathSize,
                                     *elementCount);
  return static_cast<long>(result);
}

// ==============================================================================
// CAPL_DLL_INFO_LIST4 -- the real API contract (capl-export-contract).
// APPEND ONLY from this point on: never rename, reorder, or remove a row.
//
// #pragma pack(push, 1) / pop below wraps this table's DEFINITION end to
// end, including the terminating all-zero sentinel row. cdll.h already
// 1-byte-packs the CAPL_DLL_INFO4 struct *type* itself, so this struct's
// member layout is already fixed regardless of packing state elsewhere --
// this pragma pair is applied anyway, directly around the table, so the
// packing requirement stays visible at the point it matters rather than
// relying on an implication from a different file.
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

    // restifyReadVersion(char buffer[], dword bufferSize) : long
    // See the naming-convention rationale and restifyReadVersion's own
    // comment above. parTypes "CD" / array "\001\000" mean: parameter 0
    // ('C', array depth 1) is a CAPL char[] buffer passed by reference;
    // parameter 1 ('D', array depth 0) is a scalar unsigned long.
    {"restifyReadVersion",
     (CAPL_FARCALL)restifyReadVersion,
     "Common",
     "Writes this DLL's build version string into the caller-supplied "
     "buffer. Returns 0 on success, a negative error code otherwise.",
     'L',
     2,
     "CD",
     "\001\000",
     {"buffer", "bufferSize"}},

    // Trap: parCount below must equal the number of entries actually
    // present in that row's parTypes/array/parNames -- nothing checks this
    // at compile time or at runtime, and a mismatch corrupts the CAPL
    // stack (worst on x86) with no diagnostic. Trap: a `type - 128` entry
    // (`kRefLong`/`kRefDword`/`kRefDouble`) marks a CAPL reference parameter;
    // the matching C++ shim parameter must be a pointer, never a plain
    // value, or the write-back targets the wrong memory.

    {"restifyGetSync",
     (CAPL_FARCALL)restifyGetSync,
     "Sync",
     "Performs a blocking HTTP GET and copies the response body into the "
     "caller's buffer, returning 0 on success or a negative error code "
     "otherwise. Blocking call \xe2\x80\x94 use from Measurement Setup or a "
     "test node only, never from a Simulation Setup node.",
     'L',
     8,
     {'C', 'D', 'C', 'D', 'C', 'D', kRefLong, kRefDword},
     {1, 0, 1, 0, 1, 0, 0, 0},
     {"url", "urlSize", "requestHeaders", "requestHeadersSize", "responseBody",
      "responseBodySize", "httpStatusCode", "responseBodyLength"}},

    {"restifyDeleteSync",
     (CAPL_FARCALL)restifyDeleteSync,
     "Sync",
     "Performs a blocking HTTP DELETE and copies the response body into the "
     "caller's buffer, returning 0 on success or a negative error code "
     "otherwise. Blocking call \xe2\x80\x94 use from Measurement Setup or a "
     "test node only, never from a Simulation Setup node.",
     'L',
     8,
     {'C', 'D', 'C', 'D', 'C', 'D', kRefLong, kRefDword},
     {1, 0, 1, 0, 1, 0, 0, 0},
     {"url", "urlSize", "requestHeaders", "requestHeadersSize", "responseBody",
      "responseBodySize", "httpStatusCode", "responseBodyLength"}},

    {"restifyPostSync",
     (CAPL_FARCALL)restifyPostSync,
     "Sync",
     "Performs a blocking HTTP POST with the given request body and copies "
     "the response body into the caller's buffer, returning 0 on success or "
     "a negative error code otherwise. Blocking call \xe2\x80\x94 use from "
     "Measurement Setup or a test node only, never from a Simulation Setup "
     "node.",
     'L',
     10,
     {'C', 'D', 'C', 'D', 'C', 'D', 'C', 'D', kRefLong, kRefDword},
     {1, 0, 1, 0, 1, 0, 1, 0, 0, 0},
     {"url", "urlSize", "requestHeaders", "requestHeadersSize", "requestBody",
      "requestBodySize", "responseBody", "responseBodySize", "httpStatusCode",
      "responseBodyLength"}},

    {"restifyPutSync",
     (CAPL_FARCALL)restifyPutSync,
     "Sync",
     "Performs a blocking HTTP PUT with the given request body and copies "
     "the response body into the caller's buffer, returning 0 on success or "
     "a negative error code otherwise. Blocking call \xe2\x80\x94 use from "
     "Measurement Setup or a test node only, never from a Simulation Setup "
     "node.",
     'L',
     10,
     {'C', 'D', 'C', 'D', 'C', 'D', 'C', 'D', kRefLong, kRefDword},
     {1, 0, 1, 0, 1, 0, 1, 0, 0, 0},
     {"url", "urlSize", "requestHeaders", "requestHeadersSize", "requestBody",
      "requestBodySize", "responseBody", "responseBodySize", "httpStatusCode",
      "responseBodyLength"}},

    {"restifyPatchSync",
     (CAPL_FARCALL)restifyPatchSync,
     "Sync",
     "Performs a blocking HTTP PATCH with the given request body and copies "
     "the response body into the caller's buffer, returning 0 on success or "
     "a negative error code otherwise. Blocking call \xe2\x80\x94 use from "
     "Measurement Setup or a test node only, never from a Simulation Setup "
     "node.",
     'L',
     10,
     {'C', 'D', 'C', 'D', 'C', 'D', 'C', 'D', kRefLong, kRefDword},
     {1, 0, 1, 0, 1, 0, 1, 0, 0, 0},
     {"url", "urlSize", "requestHeaders", "requestHeadersSize", "requestBody",
      "requestBodySize", "responseBody", "responseBodySize", "httpStatusCode",
      "responseBodyLength"}},

    {"restifyRequestSync",
     (CAPL_FARCALL)restifyRequestSync,
     "Sync",
     "Performs a blocking HTTP request using the given method, request body "
     "and timeouts, copying the response body into the caller's buffer and "
     "returning 0 on success or a negative error code otherwise. Blocking "
     "call \xe2\x80\x94 use from Measurement Setup or a test node only, "
     "never from a Simulation Setup node.",
     'L',
     15,
     {'C', 'D', 'C', 'D', 'C', 'D', 'C', 'D', 'C', 'D', 'D', 'D', 'D', kRefLong, kRefDword},
     {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0},
     {"method", "methodSize", "url", "urlSize", "requestHeaders",
      "requestHeadersSize", "requestBody", "requestBodySize", "responseBody",
      "responseBodySize", "connectTimeoutMs", "totalTimeoutMs",
      "maxResponseBytes", "httpStatusCode", "responseBodyLength"}},

    // Eleven asynchronous rows -- six dispatch rows and five lifecycle
    // rows. Same parCount / kRefLong / kRefDword trap as above; signature
    // table and state-machine semantics: docs/capl-async-surface.md.

    {"restifyGetAsync",
     (CAPL_FARCALL)restifyGetAsync,
     "Async",
     "Dispatches a non-blocking HTTP GET, returning 0 on success with a "
     "request id or a negative error code otherwise. Non-blocking \xe2\x80\x94 "
     "safe to call from a Simulation Setup node.",
     'L',
     5,
     {'C', 'D', 'C', 'D', kRefDword},
     {1, 0, 1, 0, 0},
     {"url", "urlSize", "requestHeaders", "requestHeadersSize", "requestId"}},

    {"restifyDeleteAsync",
     (CAPL_FARCALL)restifyDeleteAsync,
     "Async",
     "Dispatches a non-blocking HTTP DELETE, returning 0 on success with a "
     "request id or a negative error code otherwise. Non-blocking \xe2\x80\x94 "
     "safe to call from a Simulation Setup node.",
     'L',
     5,
     {'C', 'D', 'C', 'D', kRefDword},
     {1, 0, 1, 0, 0},
     {"url", "urlSize", "requestHeaders", "requestHeadersSize", "requestId"}},

    {"restifyPostAsync",
     (CAPL_FARCALL)restifyPostAsync,
     "Async",
     "Dispatches a non-blocking HTTP POST with the given request body, "
     "returning 0 on success with a request id or a negative error code "
     "otherwise. Non-blocking \xe2\x80\x94 safe to call from a Simulation "
     "Setup node.",
     'L',
     7,
     {'C', 'D', 'C', 'D', 'C', 'D', kRefDword},
     {1, 0, 1, 0, 1, 0, 0},
     {"url", "urlSize", "requestHeaders", "requestHeadersSize", "requestBody",
      "requestBodySize", "requestId"}},

    {"restifyPutAsync",
     (CAPL_FARCALL)restifyPutAsync,
     "Async",
     "Dispatches a non-blocking HTTP PUT with the given request body, "
     "returning 0 on success with a request id or a negative error code "
     "otherwise. Non-blocking \xe2\x80\x94 safe to call from a Simulation "
     "Setup node.",
     'L',
     7,
     {'C', 'D', 'C', 'D', 'C', 'D', kRefDword},
     {1, 0, 1, 0, 1, 0, 0},
     {"url", "urlSize", "requestHeaders", "requestHeadersSize", "requestBody",
      "requestBodySize", "requestId"}},

    {"restifyPatchAsync",
     (CAPL_FARCALL)restifyPatchAsync,
     "Async",
     "Dispatches a non-blocking HTTP PATCH with the given request body, "
     "returning 0 on success with a request id or a negative error code "
     "otherwise. Non-blocking \xe2\x80\x94 safe to call from a Simulation "
     "Setup node.",
     'L',
     7,
     {'C', 'D', 'C', 'D', 'C', 'D', kRefDword},
     {1, 0, 1, 0, 1, 0, 0},
     {"url", "urlSize", "requestHeaders", "requestHeadersSize", "requestBody",
      "requestBodySize", "requestId"}},

    {"restifyRequestAsync",
     (CAPL_FARCALL)restifyRequestAsync,
     "Async",
     "Dispatches a non-blocking HTTP request using the given method, "
     "request body and timeouts, returning 0 on success with a request id "
     "or a negative error code otherwise. Non-blocking \xe2\x80\x94 safe to "
     "call from a Simulation Setup node.",
     'L',
     12,
     {'C', 'D', 'C', 'D', 'C', 'D', 'C', 'D', 'D', 'D', 'D', kRefDword},
     {1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0},
     {"method", "methodSize", "url", "urlSize", "requestHeaders",
      "requestHeadersSize", "requestBody", "requestBodySize", "connectTimeoutMs",
      "totalTimeoutMs", "maxResponseBytes", "requestId"}},

    {"restifyPollResponse",
     (CAPL_FARCALL)restifyPollResponse,
     "Async",
     "Checks whether a dispatched request is still in flight or has "
     "completed, without blocking. Safe to call from a Simulation Setup "
     "timer.",
     'L',
     2,
     {'D', kRefLong},
     {0, 0},
     {"requestId", "state"}},

    {"restifyAwaitResponse",
     (CAPL_FARCALL)restifyAwaitResponse,
     "Async",
     "Blocks until a dispatched request completes or the given timeout "
     "elapses. Blocking call \xe2\x80\x94 use from Measurement Setup or a "
     "test node only, never from a Simulation Setup node.",
     'L',
     2,
     {'D', 'D'},
     {0, 0},
     {"requestId", "waitTimeoutMs"}},

    {"restifyReadResponse",
     (CAPL_FARCALL)restifyReadResponse,
     "Async",
     "Copies a completed response's body into the caller's buffer without "
     "blocking, returning 0 on success or a negative error code otherwise. "
     "Safe to call from a Simulation Setup timer.",
     'L',
     6,
     {'D', 'C', 'D', kRefLong, kRefLong, kRefDword},
     {0, 1, 0, 0, 0, 0},
     {"requestId", "responseBody", "responseBodySize", "requestStatus",
      "httpStatusCode", "responseBodyLength"}},

    {"restifyDiscardResponse",
     (CAPL_FARCALL)restifyDiscardResponse,
     "Async",
     "Releases a single response's slot without blocking, returning 0 on "
     "success or a negative error code otherwise. Safe to call from a "
     "Simulation Setup timer.",
     'L',
     1,
     {'D'},
     {0},
     {"requestId"}},

    {"restifyDiscardAllResponses",
     (CAPL_FARCALL)restifyDiscardAllResponses,
     "Async",
     "Releases every response's memory at once, returning 0 and a count of "
     "requests still running. Frees memory \xe2\x80\x94 call from on "
     "stopMeasurement or a test node.",
     'L',
     1,
     {kRefDword},
     {0},
     {"stillRunning"}},

    // JSON rows -- see docs/capl-json-surface.md.

    {"restifyJsonParse",
     (CAPL_FARCALL)restifyJsonParse,
     "Json",
     "Parses JSON text into a stored document, returning 0 on success with a "
     "document id or a negative error code otherwise.",
     'L',
     3,
     {'C', 'D', kRefDword},
     {1, 0, 0},
     {"json", "jsonSize", "documentId"}},

    {"restifyJsonCountEntries",
     (CAPL_FARCALL)restifyJsonCountEntries,
     "Json",
     "Writes the number of flattened entries of a parsed document to "
     "entryCount, returning 0 on success or a negative error code otherwise.",
     'L',
     2,
     {'D', kRefDword},
     {0, 0},
     {"documentId", "entryCount"}},

    {"restifyJsonReadEntry",
     (CAPL_FARCALL)restifyJsonReadEntry,
     "Json",
     "Copies the key and value text of one entry of a parsed document into "
     "the caller's buffers and reports the value type, returning 0 on "
     "success or a negative error code otherwise.",
     'L',
     7,
     {'D', 'D', 'C', 'D', 'C', 'D', kRefLong},
     {0, 0, 1, 0, 1, 0, 0},
     {"documentId", "entryIndex", "key", "keySize", "value", "valueSize",
      "valueType"}},

    {"restifyJsonReadValue",
     (CAPL_FARCALL)restifyJsonReadValue,
     "Json",
     "Copies the value text found at a JSON Pointer path in a parsed "
     "document into the caller's buffer and reports the value type, "
     "returning 0 on success or a negative error code otherwise.",
     'L',
     6,
     {'D', 'C', 'D', 'C', 'D', kRefLong},
     {0, 1, 0, 1, 0, 0},
     {"documentId", "path", "pathSize", "value", "valueSize", "valueType"}},

    {"restifyJsonDiscardDocument",
     (CAPL_FARCALL)restifyJsonDiscardDocument,
     "Json",
     "Releases a single parsed document, returning 0 on success or a "
     "negative error code otherwise.",
     'L',
     1,
     {'D'},
     {0},
     {"documentId"}},

    {"restifyJsonDiscardAllDocuments",
     (CAPL_FARCALL)restifyJsonDiscardAllDocuments,
     "Json",
     "Releases every parsed document at once, returning 0 and the number of "
     "documents released.",
     'L',
     1,
     {kRefDword},
     {0},
     {"discardedCount"}},

    {"restifyJsonNormalize",
     (CAPL_FARCALL)restifyJsonNormalize,
     "Json",
     "Converts apostrophe-quoted JSON strings to standard JSON and checks the "
     "result, returning 0 and the normalized text on success or a negative "
     "error code otherwise.",
     'L',
     4,
     {'C', 'D', 'C', 'D'},
     {1, 0, 1, 0},
     {"json", "jsonSize", "normalized", "normalizedSize"}},

    // Typed JSON read rows -- see docs/capl-json-surface.md.

    {"restifyJsonReadLong",
     (CAPL_FARCALL)restifyJsonReadLong,
     "Json",
     "Writes the integer value found at a JSON Pointer path in a parsed "
     "document to value, returning 0 on success or a negative error code "
     "otherwise.",
     'L',
     4,
     {'D', 'C', 'D', kRefLong},
     {0, 1, 0, 0},
     {"documentId", "path", "pathSize", "value"}},

    {"restifyJsonReadDouble",
     (CAPL_FARCALL)restifyJsonReadDouble,
     "Json",
     "Writes the numeric value found at a JSON Pointer path in a parsed "
     "document to value, returning 0 on success or a negative error code "
     "otherwise.",
     'L',
     4,
     {'D', 'C', 'D', kRefDouble},
     {0, 1, 0, 0},
     {"documentId", "path", "pathSize", "value"}},

    {"restifyJsonReadBool",
     (CAPL_FARCALL)restifyJsonReadBool,
     "Json",
     "Writes the boolean value (1 or 0) found at a JSON Pointer path in a "
     "parsed document to value, returning 0 on success or a negative error "
     "code otherwise.",
     'L',
     4,
     {'D', 'C', 'D', kRefLong},
     {0, 1, 0, 0},
     {"documentId", "path", "pathSize", "value"}},

    {"restifyJsonCountElements",
     (CAPL_FARCALL)restifyJsonCountElements,
     "Json",
     "Writes the number of elements of the array found at a JSON Pointer "
     "path in a parsed document to elementCount, returning 0 on success or "
     "a negative error code otherwise.",
     'L',
     4,
     {'D', 'C', 'D', kRefDword},
     {0, 1, 0, 0},
     {"documentId", "path", "pathSize", "elementCount"}},

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
