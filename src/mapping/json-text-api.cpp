#include "mapping/json-text-api.h"

#include <string_view>
#include <utility>

#include "core/input-text.h"
#include "mapping/json-flatten.h"

Status ParseJsonDocument(JsonDocumentStore& store, const char* jsonText, std::uint32_t jsonSize,
                         std::uint32_t& documentId) {
  documentId = 0;
  try {
    std::string_view textView;
    Status status = BoundedText(jsonText, jsonSize, textView);
    if (status != Status::Ok) {
      return status;
    }

    FlattenResult flattened;
    status = FlattenJson(textView, flattened);
    if (status != Status::Ok) {
      return status;
    }

    std::uint32_t newId = 0;
    status = store.Insert(std::move(flattened), newId);
    if (status == Status::Ok) {
      documentId = newId;
    }
    return status;
  } catch (...) {
    return Status::InternalError;
  }
}

Status CountJsonEntries(JsonDocumentStore& store, std::uint32_t documentId,
                        std::uint32_t& entryCount) {
  entryCount = 0;
  try {
    return store.Count(documentId, entryCount);
  } catch (...) {
    return Status::InternalError;
  }
}

Status ReadJsonEntry(JsonDocumentStore& store, std::uint32_t documentId, std::uint32_t entryIndex,
                     char* key, std::uint32_t keySize, char* value, std::uint32_t valueSize,
                     std::int32_t& valueType) {
  valueType = 0;
  try {
    JsonEntryType type = JsonEntryType::None;
    const Status status =
        store.ReadEntry(documentId, entryIndex, key, keySize, value, valueSize, type);
    if (status == Status::Ok) {
      valueType = static_cast<std::int32_t>(type);
    }
    return status;
  } catch (...) {
    return Status::InternalError;
  }
}

Status ReadJsonValue(JsonDocumentStore& store, std::uint32_t documentId, const char* pathText,
                     std::uint32_t pathSize, char* value, std::uint32_t valueSize,
                     std::int32_t& valueType) {
  valueType = 0;
  try {
    std::string_view pathView;
    Status status = BoundedText(pathText, pathSize, pathView);
    if (status != Status::Ok) {
      return status;
    }

    JsonEntryType type = JsonEntryType::None;
    status = store.ReadValue(documentId, pathView, value, valueSize, type);
    if (status == Status::Ok) {
      valueType = static_cast<std::int32_t>(type);
    }
    return status;
  } catch (...) {
    return Status::InternalError;
  }
}

Status DiscardJsonDocument(JsonDocumentStore& store, std::uint32_t documentId) {
  try {
    return store.Discard(documentId);
  } catch (...) {
    return Status::InternalError;
  }
}

Status DiscardAllJsonDocuments(JsonDocumentStore& store, std::uint32_t& discardedCount) {
  discardedCount = 0;
  try {
    return store.DiscardAll(discardedCount);
  } catch (...) {
    return Status::InternalError;
  }
}
