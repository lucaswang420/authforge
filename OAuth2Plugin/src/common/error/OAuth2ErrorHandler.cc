#include <oauth2/OAuth2ErrorHandler.h>
#include <json/json.h>

namespace common::error
{

void OAuth2ErrorHandler::sendErrorResponse(
  std::function<void(const drogon::HttpResponsePtr &)> &&callback,
  const std::string &errorCode,
  const std::string &description,
  const std::string &errorUri
)
{
    Json::Value error;
    error["error"] = errorCode;

    if (!description.empty())
    {
        error["error_description"] = description;
    }

    if (!errorUri.empty())
    {
        error["error_uri"] = errorUri;
    }

    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);

    // Set appropriate headers (RFC 6749 Section 5.2 specifies caching behavior)
    resp->addHeader("Cache-Control", "no-store");
    resp->addHeader("Pragma", "no-cache");

    // Determine HTTP status code based on error
    resp->setStatusCode(getHttpStatusCode(errorCode));

    callback(resp);
}

drogon::HttpStatusCode OAuth2ErrorHandler::getHttpStatusCode(const std::string &errorCode)
{
    // RFC 6749 Section 5.2 specifies:
    // invalid_client: HTTP 401 Unauthorized
    if (errorCode == INVALID_CLIENT)
    {
        return drogon::k401Unauthorized;
    }

    // server_error or temporarily_unavailable: HTTP 500 or 503
    if (errorCode == SERVER_ERROR)
    {
        return drogon::k500InternalServerError;
    }
    if (errorCode == TEMPORARILY_UNAVAILABLE)
    {
        return drogon::k503ServiceUnavailable;
    }

    // unauthorized_client can be 400 or 403, we use 400 for token endpoint and 403 for
    // resource/revocation By default RFC 6749 token endpoint errors return HTTP 400 Bad Request
    return drogon::k400BadRequest;
}

}  // namespace common::error
