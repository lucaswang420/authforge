#include <oauth2/ErrorHandler.h>
#include <drogon/utils/Utilities.h>
#include <random>
#include <sstream>
#include <iomanip>

using namespace drogon;
using namespace drogon::orm;

namespace common::error
{

int Error::toHttpStatusCode() const
{
    switch (category)
    {
        case ErrorCategory::VALIDATION:
            return 400;
        case ErrorCategory::AUTHENTICATION:
            return 401;
        case ErrorCategory::AUTHORIZATION:
            return 403;
        case ErrorCategory::NETWORK:
            return (code == ErrorCode::TIMEOUT) ? 504 : 502;
        case ErrorCategory::DATABASE:
            return 500;
        case ErrorCategory::INTERNAL:
        default:
            return 500;
    }
}

Json::Value Error::toJson() const
{
    Json::Value error;
    error["code"] = static_cast<int>(code);
    error["category"] = std::string([&]() {
        switch (category)
        {
            case ErrorCategory::NETWORK:
                return "NETWORK";
            case ErrorCategory::DATABASE:
                return "DATABASE";
            case ErrorCategory::VALIDATION:
                return "VALIDATION";
            case ErrorCategory::AUTHENTICATION:
                return "AUTHENTICATION";
            case ErrorCategory::AUTHORIZATION:
                return "AUTHORIZATION";
            case ErrorCategory::INTERNAL:
                return "INTERNAL";
            default:
                return "UNKNOWN";
        }
    }());
    error["message"] = message;
    if (!details.empty())
    {
        error["details"] = details;
    }
    if (!requestId.empty())
    {
        error["request_id"] = requestId;
    }

    Json::Value root;
    root["error"] = error;
    return root;
}

Error Error::fromException(const std::exception &e, ErrorCategory category)
{
    ErrorCode code = ErrorCode::DB_QUERY_ERROR;  // Default to DB error
    std::string message = e.what();

    // Map common exception patterns to error codes
    std::string errStr = e.what();
    if (errStr.find("connection") != std::string::npos)
    {
        code = ErrorCode::CONNECTION_FAILED;
    }
    else if (errStr.find("timeout") != std::string::npos)
    {
        code = ErrorCode::TIMEOUT;
    }

    return Error{code, category, message, "", ""};
}

void ErrorHandler::logError(const Error &error, const std::string &context)
{
    std::stringstream ss;
    ss << "[" << error.requestId << "] ";
    if (!context.empty())
    {
        ss << context << " - ";
    }
    ss << error.message;
    if (!error.details.empty())
    {
        ss << " | " << error.details;
    }

    // Use appropriate log level based on category
    switch (error.category)
    {
        case ErrorCategory::VALIDATION:
            LOG_WARN << ss.str();
            break;
        case ErrorCategory::AUTHENTICATION:
        case ErrorCategory::AUTHORIZATION:
            LOG_ERROR << ss.str();
            break;
        default:
            LOG_ERROR << ss.str();
    }
}

std::string ErrorHandler::generateRequestId()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);

    std::stringstream ss;
    ss << "req_" << std::hex << std::setw(8) << std::setfill('0') << dis(gen);
    return ss.str();
}

Error ErrorHandler::handleDbException(const DrogonDbException &e)
{
    std::string errStr = e.base().what();

    if (errStr.find("connection") != std::string::npos)
    {
        return Error{
          ErrorCode::DB_CONNECTION_ERROR,
          ErrorCategory::DATABASE,
          "Database connection failed",
          errStr,
          generateRequestId()
        };
    }
    else if (errStr.find("constraint") != std::string::npos)
    {
        return Error{
          ErrorCode::DB_CONSTRAINT_VIOLATION,
          ErrorCategory::DATABASE,
          "Database constraint violation",
          errStr,
          generateRequestId()
        };
    }
    else
    {
        return Error{
          ErrorCode::DB_QUERY_ERROR,
          ErrorCategory::DATABASE,
          "Database query error",
          errStr,
          generateRequestId()
        };
    }
}

Error ErrorHandler::handleValidationError(const std::string &field, const std::string &reason)
{
    return Error{
      ErrorCode::INVALID_INPUT,
      ErrorCategory::VALIDATION,
      reason,
      "field: " + field,
      generateRequestId()
    };
}

}  // namespace common::error
