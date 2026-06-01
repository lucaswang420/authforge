#include <oauth2/validation/HttpResponder.h>
#include <oauth2/error/ErrorTypes.h>
#include <oauth2/error/ErrorContext.h>
#include <oauth2/error/ErrorHandler.h>
#include <oauth2/error/RequestId.h>

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <sstream>

namespace oauth2::validation
{

using common::error::Error;
using common::error::ErrorContext;
using common::error::ErrorHandler;
using common::error::RequestId;

namespace
{

// Join the raw validation error strings into a single Internal/diagnostic
// detail string. Each entry already embeds a field name and failure reason
// (e.g. "username is required", "code: invalid"), so this preserves the field
// names and reasons required in `details` for non-Production_Mode
// (Requirement 7.6).
std::string joinFieldErrors(const std::vector<std::string> &errors)
{
    std::ostringstream oss;
    bool first = true;
    for (const auto &e : errors)
    {
        if (!first)
        {
            oss << "; ";
        }
        first = false;
        oss << e;
    }
    return oss.str();
}

}  // namespace

Json::Value HttpResponder::buildErrorJson(const std::vector<std::string> &errors)
{
    // VALIDATION-class Error Envelope: code VALIDATION_INVALID_INPUT, category
    // VALIDATION, HTTP 400 (Requirement 7.4). The message is the catalog default
    // Client_Safe_Message; the legacy aliases (`VALIDATION_ERROR`, `reason`,
    // `error_description`, `timestamp`) are intentionally gone (Requirement 7.5).
    Error error = Error::fromCode("VALIDATION_INVALID_INPUT", RequestId::generate());

    // Field names + failure reasons are diagnostic detail. They are surfaced in
    // the Envelope `details` only when detailed errors are allowed
    // (non-Production_Mode, Requirement 7.6) and are always recorded in the log.
    error.details = joinFieldErrors(errors);

    // Server-side log carries the Internal_Detail and Request_ID
    // (Requirement 5.2 / 6.2).
    ErrorHandler::logError(error, "validation::HttpResponder");

    // ErrorContext is the single Production_Mode decision: in Production_Mode the
    // `details` key is fully omitted (Requirement 5.1); otherwise it lists the
    // field names and failure reasons (Requirement 7.6).
    const bool includeDetails = ErrorContext::detailedErrorsAllowed();
    return error.toJson(includeDetails);
}

drogon::HttpResponsePtr HttpResponder::buildErrorResponse(
  const std::vector<std::string> &errors
)
{
    Json::Value root = buildErrorJson(errors);

    auto resp = drogon::HttpResponse::newHttpJsonResponse(root);
    // HTTP 400 for the VALIDATION category (Requirement 7.4). Content-Type is set
    // to application/json by newHttpJsonResponse; make it explicit for clarity.
    resp->setStatusCode(drogon::k400BadRequest);
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    return resp;
}

void HttpResponder::respondWithError(
  const std::string &field,
  const std::string &reason,
  std::function<void(const drogon::HttpResponsePtr &)> &&callback
)
{
    std::vector<std::string> errors;
    errors.push_back(field + ": " + reason);

    auto resp = buildErrorResponse(errors);
    callback(resp);
}

void HttpResponder::respondWithErrors(
  const std::vector<std::string> &errors,
  std::function<void(const drogon::HttpResponsePtr &)> &&callback
)
{
    auto resp = buildErrorResponse(errors);
    callback(resp);
}

bool HttpResponder::respondIfErrors(
  const std::vector<std::string> &errors,
  std::function<void(const drogon::HttpResponsePtr &)> &&callback
)
{
    if (!errors.empty())
    {
        respondWithErrors(errors, std::move(callback));
        return true;
    }
    return false;
}

}  // namespace oauth2::validation
