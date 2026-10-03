#pragma once

#include <ostream>

#include "core/status.h"

// Found by gtest through ADL because Status lives in the global namespace.
inline const char* StatusName(Status status) {
  switch (status) {
    case Status::Ok: return "Ok";
    case Status::InvalidArgument: return "InvalidArgument";
    case Status::BufferTooSmall: return "BufferTooSmall";
    case Status::VersionResourceUnavailable: return "VersionResourceUnavailable";
    case Status::MalformedHeaderBlock: return "MalformedHeaderBlock";
    case Status::UnknownHttpMethod: return "UnknownHttpMethod";
    case Status::UnterminatedInputText: return "UnterminatedInputText";
    case Status::ParseError: return "ParseError";
    case Status::PathSyntaxError: return "PathSyntaxError";
    case Status::PathNotFound: return "PathNotFound";
    case Status::IndexOutOfRange: return "IndexOutOfRange";
    case Status::TypeMismatch: return "TypeMismatch";
    case Status::NullValue: return "NullValue";
    case Status::NumericOverflow: return "NumericOverflow";
    case Status::NotIntegral: return "NotIntegral";
    case Status::NetworkError: return "NetworkError";
    case Status::Timeout: return "Timeout";
    case Status::TlsError: return "TlsError";
    case Status::TransportInitFailed: return "TransportInitFailed";
    case Status::InvalidUrl: return "InvalidUrl";
    case Status::ResponseTooLarge: return "ResponseTooLarge";
    case Status::RequestCancelled: return "RequestCancelled";
    case Status::NoFreeRequestSlot: return "NoFreeRequestSlot";
    case Status::RequestNotComplete: return "RequestNotComplete";
    case Status::UnknownRequestId: return "UnknownRequestId";
    case Status::WaitTimeout: return "WaitTimeout";
    case Status::AsyncStartFailed: return "AsyncStartFailed";
    case Status::NoFreeDocumentSlot: return "NoFreeDocumentSlot";
    case Status::UnknownDocumentId: return "UnknownDocumentId";
    case Status::DocumentTooLarge: return "DocumentTooLarge";
    case Status::NestingTooDeep: return "NestingTooDeep";
    case Status::TooManyEntries: return "TooManyEntries";
    case Status::InternalError: return "InternalError";
    case Status::KeyTextTooLarge: return "KeyTextTooLarge";
  }
  return "UnknownStatus";
}

inline void PrintTo(Status status, std::ostream* os) {
  *os << StatusName(status) << " (" << static_cast<int>(status) << ")";
}
