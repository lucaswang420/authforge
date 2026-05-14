#include "GoogleController.h"
#include <drogon/HttpClient.h>
#include <oauth2/OpenApiGenerator.h>

// TODO: REPLACE WITH YOUR REAL GOOGLE CREDENTIALS
const std::string GOOGLE_CLIENT_ID_KEY = "client_id";
const std::string GOOGLE_CLIENT_SECRET_KEY = "client_secret";
const std::string GOOGLE_REDIRECT_URI_KEY = "redirect_uri";

std::string getGoogleConfig(const std::string &key)
{
    auto config = drogon::app().getCustomConfig();
    if (config.isMember("external_auth") && config["external_auth"].isMember("google"))
    {
        return config["external_auth"]["google"].get(key, "").asString();
    }
    return "";
}

// Register OpenAPI documentation (executed once at startup)
namespace
{
struct GoogleControllerDocs
{
    GoogleControllerDocs()
    {
        using namespace common::documentation;

        Json::Value successExample;
        successExample["sub"] = "123456789012345678901";
        successExample["name"] = "John Doe";
        successExample["email"] = "john.doe@gmail.com";
        successExample["picture"] = "https://lh3.googleusercontent.com/...";

        Json::Value errorExample;
        errorExample["error"] = "Missing code parameter";

        // C++17 compatible initialization (avoid designated initializers)
        common::documentation::EndpointInfo googleEndpoint;
        googleEndpoint.path = "/api/google/login";
        googleEndpoint.method = "POST";
        googleEndpoint.summary = "Google OAuth2 Login";
        googleEndpoint.description =
          "Exchange Google authorization code for user information. "
          "This endpoint handles the server-side OAuth2 flow with "
          "Google "
          "Identity Platform.";
        googleEndpoint.tags = {"External Auth", "Google"};

        // Initialize parameters
        common::documentation::ParameterInfo codeParam;
        codeParam.name = "code";
        codeParam.description = "Authorization code from Google OAuth2 callback (required)";
        codeParam.type = common::documentation::ParameterType::STRING;
        codeParam.location = common::documentation::ParameterLocation::QUERY;
        codeParam.required = true;
        googleEndpoint.parameters = {codeParam};

        // Initialize responses
        googleEndpoint.responses =
          {{200, "Google user info retrieved successfully"},
           {400, "Invalid request (missing or invalid code)"},
           {502, "Failed to contact Google API"}};

        // Initialize response examples
        googleEndpoint.responseExamples = {{200, successExample}, {400, errorExample}};

        googleEndpoint.requiresAuth = false;

        OpenApiGenerator::addEndpoint(googleEndpoint);
    }
};

GoogleControllerDocs docs_;
}  // namespace

void GoogleController::login(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    // Handle OPTIONS for CORS
    if (req->method() == Options)
    {
        auto resp = HttpResponse::newHttpResponse();
        callback(resp);
        return;
    }

    // Try to get code from POST body first, then fallback to query parameter
    std::string code;

    // Check Content-Type and parse accordingly
    auto contentType = req->getHeader("Content-Type");
    if (contentType.find("application/x-www-form-urlencoded") != std::string::npos)
    {
        // Parse from POST body
        auto body = req->getBody();
        // Simple parsing for "code=xxx" format
        size_t codePos = body.find("code=");
        if (codePos != std::string::npos)
        {
            size_t valueStart = codePos + 5;  // "code=" length
            size_t valueEnd = body.find("&", valueStart);
            if (valueEnd == std::string::npos)
            {
                valueEnd = body.length();
            }
            code = body.substr(valueStart, valueEnd - valueStart);
        }
    }

    // Fallback to query parameter
    if (code.empty())
    {
        code = req->getParameter("code");
    }

    if (code.empty())
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Missing code parameter");
        callback(resp);
        return;
    }

    // 1. Exchange Code for Access Token
    // API: https://oauth2.googleapis.com/token
    auto client = HttpClient::newHttpClient("https://oauth2.googleapis.com");
    auto request = HttpRequest::newHttpRequest();
    request->setMethod(Post);
    request->setPath("/token");
    request->setParameter("code", code);
    request->setParameter("client_id", getGoogleConfig(GOOGLE_CLIENT_ID_KEY));
    request->setParameter("client_secret", getGoogleConfig(GOOGLE_CLIENT_SECRET_KEY));
    request->setParameter("redirect_uri", getGoogleConfig(GOOGLE_REDIRECT_URI_KEY));
    request->setParameter("grant_type", "authorization_code");

    auto callbackPtr =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    client->sendRequest(
      request, [callbackPtr, client](ReqResult result, const HttpResponsePtr &response) {
          if (result != ReqResult::Ok || !response || response->getStatusCode() != k200OK)
          {
              auto resp = HttpResponse::newHttpResponse();
              resp->setStatusCode(k502BadGateway);
              resp->setBody("Failed to contact Google Token API");
              (*callbackPtr)(resp);
              return;
          }

          auto json = response->getJsonObject();
          if (!json || !json->isMember("access_token"))
          {
              auto resp = HttpResponse::newHttpResponse();
              resp->setStatusCode(k400BadRequest);
              resp->setBody("Google Error: Invalid token response");
              (*callbackPtr)(resp);
              return;
          }

          std::string accessToken = (*json)["access_token"].asString();

          // 2. Fetch User Info
          // API: https://www.googleapis.com/oauth2/v3/userinfo
          auto client2 = HttpClient::newHttpClient("https://www.googleapis.com");
          auto req2 = HttpRequest::newHttpRequest();
          req2->setPath("/oauth2/v3/userinfo");
          req2->addHeader("Authorization", "Bearer " + accessToken);

          client2->sendRequest(req2, [callbackPtr](ReqResult res2, const HttpResponsePtr &resp2) {
              if (res2 != ReqResult::Ok || !resp2)
              {
                  auto errResp = HttpResponse::newHttpResponse();
                  errResp->setStatusCode(k502BadGateway);
                  errResp->setBody("Failed to fetch Google UserInfo");
                  (*callbackPtr)(errResp);
                  return;
              }

              // Filter response to only include necessary fields
              // (security best practice)
              auto googleData = resp2->getJsonObject();
              Json::Value filteredJson;
              filteredJson["sub"] = (*googleData).get("sub", "").asString();
              filteredJson["name"] = (*googleData).get("name", "").asString();
              filteredJson["email"] = (*googleData).get("email", "").asString();
              filteredJson["picture"] = (*googleData).get("picture", "").asString();

              auto finalResp = HttpResponse::newHttpJsonResponse(filteredJson);
              (*callbackPtr)(finalResp);
          });
      }
    );
}
