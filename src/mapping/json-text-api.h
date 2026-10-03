// json-text-api.h -- pointer+size CAPL parameters to JsonDocumentStore calls; see docs/capl-json-surface.md.
#pragma once

#include <cstdint>

#include "core/status.h"
#include "mapping/json-document-store.h"

// Out-parameters are zeroed on entry and written only on Ok; an unexpected internal failure is reported as InternalError.
Status ParseJsonDocument(JsonDocumentStore& store, const char* jsonText, std::uint32_t jsonSize,
                         std::uint32_t& documentId);

Status CountJsonEntries(JsonDocumentStore& store, std::uint32_t documentId,
                        std::uint32_t& entryCount);

Status ReadJsonEntry(JsonDocumentStore& store, std::uint32_t documentId, std::uint32_t entryIndex,
                     char* key, std::uint32_t keySize, char* value, std::uint32_t valueSize,
                     std::int32_t& valueType);

// The path text is checked before the document id.
Status ReadJsonValue(JsonDocumentStore& store, std::uint32_t documentId, const char* pathText,
                     std::uint32_t pathSize, char* value, std::uint32_t valueSize,
                     std::int32_t& valueType);

Status DiscardJsonDocument(JsonDocumentStore& store, std::uint32_t documentId);

Status DiscardAllJsonDocuments(JsonDocumentStore& store, std::uint32_t& discardedCount);
