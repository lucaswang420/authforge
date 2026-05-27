#pragma once

#include <string>
#include <json/json.h>

namespace common::error
{

enum class ErrorCategory
{
    NETWORK,         // Network-related errors
    DATABASE,        // Database errors
    VALIDATION,      // Input validation errors
    AUTHENTICATION,  // Authentication errors
    AUTHORIZATION,   // Authorization errors
    INTERNAL,        // Internal system errors
    UNKNOWN          // Unknown errors
};

enum class ErrorCode
{
    // Network errors (1000-1099)
    CONNECTION_FAILED = 1001,
    TIMEOUT = 1002,

    // Database errors (2000-2099)
    DB_CONNECTION_ERROR = 2001,
    DB_QUERY_ERROR = 2002,
    DB_CONSTRAINT_VIOLATION = 2003,

    // Validation errors (3000-3099)
    INVALID_INPUT = 3001,
    MISSING_REQUIRED_FIELD = 3002,
    FORMAT_ERROR = 3003,

    // Authentication errors (4000-4099)
    INVALID_CREDENTIALS = 4001,
    TOKEN_EXPIRED = 4002,
    TOKEN_INVALID = 4003,

    // Authorization errors (5000-5099)
    ACCESS_DENIED = 5001,
    INSUFFICIENT_PERMISSIONS = 5002,

    // Internal errors (6000-6099)
    INTERNAL = 6001
};

struct Error
{
    ErrorCode code;
    ErrorCategory category;
    std::string message;
    std::string details;
    std::string requestId;

    // Convert error code to HTTP status code
    int toHttpStatusCode() const;

    // Convert error to JSON response
    Json::Value toJson() const;

    // Create error from exception
    static Error fromException(const std::exception &e, ErrorCategory category);
};

}  // namespace common::error
