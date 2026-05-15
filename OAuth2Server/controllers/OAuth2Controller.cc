#include "OAuth2Controller.h"
#include "../AuthService.h"
#include <drogon/drogon.h>
#include <oauth2/OAuth2Metrics.h>
#include <oauth2/OAuth2Plugin.h>
#include <drogon/utils/Utilities.h>
#include <algorithm>
#include <functional>
#include <oauth2/OpenApiGenerator.h>
#include <oauth2/ValidatorHelper.h>
#include <oauth2/ValidationHelper.h>
#include <oauth2/OAuth2Types.h>
#include <oauth2/IOAuth2Storage.h>

using namespace oauth2;
using namespace services;
using namespace common::documentation;

namespace
{
/**
 * @brief Get HTTP status code for OAuth2 error
 */
drogon::HttpStatusCode getHttpStatusCodeForError(const std::string &errorCode)
{
    if (errorCode == "invalid_client" || errorCode == "unauthorized_client")
    {
        return drogon::k401Unauthorized;  // 401
    }
    return drogon::k400BadRequest;  // 400
}
}  // namespace

// API documentation initialization
namespace
{
struct OAuth2ControllerDocs
{
    OAuth2ControllerDocs()
    {
        // Health endpoint
        {
            common::documentation::EndpointInfo healthEndpoint;
            healthEndpoint.path = "/health";
            healthEndpoint.method = "GET";
            healthEndpoint.summary = "Health check";
            healthEndpoint.description = "Returns the health status of the service";
            healthEndpoint.tags = {"System"};
            healthEndpoint.parameters = {};
            healthEndpoint.responses = {{200, "Service is healthy"}};
            healthEndpoint.requiresAuth = false;
            OpenApiGenerator::addEndpoint(healthEndpoint);
        }

        // Login endpoint
        {
            common::documentation::EndpointInfo loginEndpoint;
            loginEndpoint.path = "/oauth2/login";
            loginEndpoint.method = "POST";
            loginEndpoint.summary = "Authenticate user";
            loginEndpoint.description =
              "Authenticates user credentials and generates "
              "authorization code";
            loginEndpoint.tags = {"OAuth2", "Authentication"};
            loginEndpoint.parameters =
              {{"username",
                "Username (required)",
                common::documentation::ParameterType::STRING,
                common::documentation::ParameterLocation::QUERY,
                true},
               {"password",
                "Password (required)",
                common::documentation::ParameterType::STRING,
                common::documentation::ParameterLocation::QUERY,
                true},
               {"client_id",
                "Client identifier (required)",
                common::documentation::ParameterType::STRING,
                common::documentation::ParameterLocation::QUERY,
                true},
               {"redirect_uri",
                "Redirect URI (required)",
                common::documentation::ParameterType::STRING,
                common::documentation::ParameterLocation::QUERY,
                true},
               {"scope",
                "Requested scope (optional)",
                common::documentation::ParameterType::STRING,
                common::documentation::ParameterLocation::QUERY,
                false},
               {"state",
                "Opaque value to maintain state (recommended)",
                common::documentation::ParameterType::STRING,
                common::documentation::ParameterLocation::QUERY,
                false}};
            loginEndpoint.responses =
              {{200, "Authentication successful"},
               {302, "Redirect with authorization code"},
               {401, "Authentication failed"}};
            loginEndpoint.requiresAuth = false;
            OpenApiGenerator::addEndpoint(loginEndpoint);
        }

        // Register endpoint
        {
            common::documentation::EndpointInfo registerEndpoint;
            registerEndpoint.path = "/api/register";
            registerEndpoint.method = "POST";
            registerEndpoint.summary = "Register new user";
            registerEndpoint.description = "Registers a new user account (for testing purposes)";
            registerEndpoint.tags = {"User", "Registration"};
            registerEndpoint.parameters =
              {{"username",
                "Username (required)",
                common::documentation::ParameterType::STRING,
                common::documentation::ParameterLocation::QUERY,
                true},
               {"password",
                "Password (required)",
                common::documentation::ParameterType::STRING,
                common::documentation::ParameterLocation::QUERY,
                true},
               {"email",
                "Email address (optional)",
                common::documentation::ParameterType::STRING,
                common::documentation::ParameterLocation::QUERY,
                false}};
            registerEndpoint.responses =
              {{200, "User registered successfully"}, {400, "Invalid registration data"}};
            registerEndpoint.requiresAuth = false;
            OpenApiGenerator::addEndpoint(registerEndpoint);
        }

        // Consent endpoint
        {
            common::documentation::EndpointInfo consentEndpoint;
            consentEndpoint.path = "/oauth2/consent";
            consentEndpoint.method = "POST";
            consentEndpoint.summary = "Submit user consent";
            consentEndpoint.description = "Submit user consent for requested scopes";
            consentEndpoint.tags = {"OAuth2", "Consent"};
            consentEndpoint.parameters =
              {{"client_id",
                "Client identifier (required)",
                common::documentation::ParameterType::STRING,
                common::documentation::ParameterLocation::QUERY,
                true},
               {"user_id",
                "User identifier (required)",
                common::documentation::ParameterType::STRING,
                common::documentation::ParameterLocation::QUERY,
                true},
               {"scope",
                "Requested scope (required)",
                common::documentation::ParameterType::STRING,
                common::documentation::ParameterLocation::QUERY,
                true},
               {"redirect_uri",
                "Redirect URI (required)",
                common::documentation::ParameterType::STRING,
                common::documentation::ParameterLocation::QUERY,
                true},
               {"state",
                "Opaque value to maintain state",
                common::documentation::ParameterType::STRING,
                common::documentation::ParameterLocation::QUERY,
                false},
               {"action",
                "Action to perform: 'approve' or 'deny' (required)",
                common::documentation::ParameterType::STRING,
                common::documentation::ParameterLocation::QUERY,
                true}};
            consentEndpoint.responses = {
              {302, "Redirect to client with authorization code or error"}
            };
            consentEndpoint.requiresAuth = false;
            OpenApiGenerator::addEndpoint(consentEndpoint);
        }
    }
};

OAuth2ControllerDocs docs_;
}  // namespace

void OAuth2Controller::initApiDocs()
{
    // Implementation is handled by the static instantiation of OAuth2ControllerDocs
}

void OAuth2Controller::login(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    // Use ValidatorHelper for consistent validation
    auto errors = common::validation::ValidatorHelper::validateLoginParams(req);

    // Return validation errors if any
    if (
      common::validation::ValidationHelper::returnValidationErrorsIfAny(errors, std::move(callback))
    )
    {
        Metrics::incLoginFailure("validation_failed");
        return;
    }

    // Prefer POST body (JSON or form data) over URL parameters for security
    std::string username, password;
    std::string clientId, redirectUri, scope, state;
    std::string codeChallenge, codeChallengeMethod;

    // Try JSON body first
    if (req->contentType() == CT_APPLICATION_JSON)
    {
        auto json = req->getJsonObject();
        if (json)
        {
            username = json->get("username", "").asString();
            password = json->get("password", "").asString();
            clientId = json->get("client_id", "").asString();
            redirectUri = json->get("redirect_uri", "").asString();
            scope = json->get("scope", "").asString();
            state = json->get("state", "").asString();
            codeChallenge = json->get("code_challenge", "").asString();
            codeChallengeMethod = json->get("code_challenge_method", "").asString();
        }
    }
    // Fallback to form data (Drogon automatically parses form-urlencoded)
    else
    {
        auto params = req->getParameters();
        username = params["username"];
        password = params["password"];
        clientId = params["client_id"];
        redirectUri = params["redirect_uri"];
        scope = params["scope"];
        state = params["state"];
        codeChallenge = params["code_challenge"];
        codeChallengeMethod = params["code_challenge_method"];
    }

    AuthService::validateUser(
      username,
      password,
      [req,
       clientId,
       scope,
       redirectUri,
       state,
       codeChallenge,
       codeChallengeMethod,
       callback = std::move(callback)](std::optional<int> userId) mutable {
          if (userId)
          {
              req->session()->insert("userId", std::to_string(*userId));
              auto plugin = drogon::app().getPlugin<OAuth2Plugin>();
              if (!plugin)
              {
                  LOG_ERROR << "OAuth2 Plugin not loaded during login";
                  auto resp = HttpResponse::newHttpResponse();
                  resp->setStatusCode(k500InternalServerError);
                  resp->setBody("Internal Server Error: Plugin not loaded");
                  callback(resp);
                  return;
              }

              plugin->generateAuthorizationCode(
                clientId,
                std::to_string(*userId),  // Subject
                scope,
                redirectUri,          // CRITICAL: Pass redirect_uri for RFC 6749
                                      // Section 4.1.3 validation
                codeChallenge,        // PKCE code challenge
                codeChallengeMethod,  // PKCE code challenge method
                [req,
                 redirectUri,
                 state,
                 codeChallenge,
                 codeChallengeMethod,
                 callback =
                   std::move(callback)](bool success, std::string code, std::string error) mutable {
                    if (!success)
                    {
                        LOG_ERROR << "Failed to generate authorization code: " << error;
                        Json::Value jsonErr;
                        jsonErr["error"] = "server_error";
                        jsonErr["error_description"] = "Failed to generate authorization code";
                        auto resp = HttpResponse::newHttpJsonResponse(jsonErr);
                        resp->setStatusCode(k500InternalServerError);
                        callback(resp);
                        return;
                    }

                    std::string location = redirectUri + "?code=" + code;
                    if (!state.empty())
                        location += "&state=" + state;
                    if (req->getParameter("json") == "true")
                    {
                        Json::Value ret;
                        ret["code"] = code;
                        ret["location"] = location;
                        auto resp = HttpResponse::newHttpJsonResponse(ret);
                        callback(resp);
                        return;
                    }
                    auto resp = HttpResponse::newRedirectionResponse(location);
                    callback(resp);
                }
              );
          }
          else
          {
              // Fail (Bad Password or User Not Found)
              Metrics::incLoginFailure("bad_credentials");

              auto resp = HttpResponse::newHttpResponse();
              resp->setStatusCode(k401Unauthorized);
              resp->setBody("Login Failed: Invalid Credentials");
              callback(resp);
          }
      }
    );
}

void OAuth2Controller::registerUser(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    // Use ValidatorHelper for consistent validation
    auto errors = common::validation::ValidatorHelper::validateLoginParams(req);

    // Return validation errors if any
    if (
      common::validation::ValidationHelper::returnValidationErrorsIfAny(errors, std::move(callback))
    )
    {
        return;
    }

    // Extract parameters (validation ensures they exist and are valid)
    auto params = req->getParameters();
    std::string username = params["username"];
    std::string password = params["password"];
    std::string email = params["email"];

    AuthService::registerUser(username, password, email, [callback](const std::string &error) {
        if (error.empty())
        {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody("User Registered");
            callback(resp);
        }
        else
        {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k500InternalServerError);
            resp->setBody(error);
            callback(resp);
        }
    });
}

#include <oauth2/filters/OAuth2Middleware.h>

// ...

void OAuth2Controller::logout(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    auto middleware = std::make_shared<oauth2::filters::OAuth2Middleware>();
    middleware->doFilter(
      req,
      [&](const HttpResponsePtr &resp) { callback(resp); },  // Filter 拦截失败（返�?401/error�?
      [&]() {                                                // Filter 校验通过
          auto resp = HttpResponse::newHttpResponse();
          resp->setStatusCode(k200OK);
          callback(resp);
      }
    );
}

void OAuth2Controller::health(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    // Health check endpoint for monitoring/orchestration systems
    // Returns 200 OK if service is healthy
    Json::Value json;
    json["status"] = "ok";
    json["service"] = "OAuth2 Server";
    json["timestamp"] = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                               std::chrono::system_clock::now().time_since_epoch()
    )
                                               .count());
    auto statusCode = k200OK;

    // Check database connectivity (optional - can be expensive)
    try
    {
        auto plugin = drogon::app().getPlugin<OAuth2Plugin>();
        if (plugin)
        {
            json["storage_type"] = plugin->getStorageType();
            json["database"] = "connected";
        }
        else
        {
            json["status"] = "unhealthy";
            json["database"] = "unknown";
            statusCode = k503ServiceUnavailable;
        }
    }
    catch (...)
    {
        json["status"] = "unhealthy";
        json["database"] = "disconnected";
        statusCode = k503ServiceUnavailable;
    }

    auto resp = HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(statusCode);
    callback(resp);
}

void OAuth2Controller::consent(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    // P0-2: Handle user consent approval
    auto params = req->getParameters();
    std::string clientId = params["client_id"];
    std::string userId = params["user_id"];
    std::string scope = params["scope"];
    std::string redirectUri = params["redirect_uri"];
    std::string state = params["state"];
    std::string action = params["action"];  // "approve" or "deny"

    if (action == "deny")
    {
        // User denied consent, redirect back with error
        std::string location =
          redirectUri + "?error=access_denied&error_description=User+denied+consent";
        if (!state.empty())
            location += "&state=" + state;
        auto resp = HttpResponse::newRedirectionResponse(location);
        callback(resp);
        return;
    }

    // User approved consent, save it and proceed with authorization
    auto plugin = drogon::app().getPlugin<OAuth2Plugin>();
    if (!plugin)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k500InternalServerError);
        resp->setBody("OAuth2 Plugin not loaded");
        callback(resp);
        return;
    }

    // Get internal user ID
    plugin->getInternalUserId(
      userId,
      [plugin, clientId, userId, scope, redirectUri, state, callback = std::move(callback)](
        std::optional<int32_t> internalUserId
      ) mutable {
          if (!internalUserId)
          {
              auto resp = HttpResponse::newHttpResponse();
              resp->setStatusCode(k500InternalServerError);
              resp->setBody("Failed to get user mapping");
              callback(resp);
              return;
          }

          // Parse scopes and save consent for each
          std::vector<std::string> scopes;
          std::stringstream ss(scope);
          std::string scopeItem;
          while (std::getline(ss, scopeItem, ' '))
          {
              if (!scopeItem.empty())
              {
                  scopes.push_back(scopeItem);
              }
          }

          // Save consent for all scopes (use first scope as callback trigger)
          if (!scopes.empty())
          {
              std::string firstScope = scopes[0];
              int32_t uid = *internalUserId;
              plugin->saveUserConsent(
                uid,
                clientId,
                firstScope,
                [plugin,
                 uid,
                 clientId,
                 userId,
                 scope,
                 redirectUri,
                 state,
                 firstScope,
                 scopes,
                 callback = std::move(callback)](bool success) mutable {
                    if (!success)
                    {
                        LOG_ERROR << "Failed to save user consent for scope: " << firstScope;
                        auto resp = HttpResponse::newHttpResponse();
                        resp->setStatusCode(k500InternalServerError);
                        resp->setBody("Failed to save consent");
                        callback(resp);
                        return;
                    }

                    // Save consent for remaining scopes (fire and forget
                    // for simplicity)
                    for (size_t i = 1; i < scopes.size(); ++i)
                    {
                        plugin->saveUserConsent(uid, clientId, scopes[i], [](bool) {});
                    }

                    // Proceed with authorization code generation
                    plugin->generateAuthorizationCode(
                      clientId,
                      userId,
                      scope,
                      redirectUri,
                      "",  // codeChallenge (empty for now)
                      "",  // codeChallengeMethod (empty for now)
                      [clientId, redirectUri, state, callback = std::move(callback)](
                        bool success, std::string code, std::string error
                      ) mutable {
                          if (!success)
                          {
                              LOG_ERROR << "Failed to generate "
                                           "authorization code: "
                                        << error;
                              Json::Value jsonErr;
                              jsonErr["error"] = "server_error";
                              jsonErr["error_description"] =
                                "Failed to generate authorization code";
                              auto resp = HttpResponse::newHttpJsonResponse(jsonErr);
                              resp->setStatusCode(k500InternalServerError);
                              callback(resp);
                              return;
                          }

                          std::string location = redirectUri + "?code=" + code;
                          if (!state.empty())
                              location += "&state=" + state;
                          auto resp = HttpResponse::newRedirectionResponse(location);
                          Metrics::incRequest("authorize", 302);
                          callback(resp);
                      }
                    );
                }
              );
          }
          else
          {
              // No scopes to save consent for, proceed directly
              plugin->generateAuthorizationCode(
                clientId,
                userId,
                scope,
                redirectUri,
                "",  // codeChallenge
                "",  // codeChallengeMethod
                [clientId, redirectUri, state, callback = std::move(callback)](
                  bool success, std::string code, std::string error
                ) mutable {
                    if (!success)
                    {
                        LOG_ERROR << "Failed to generate authorization code: " << error;
                        Json::Value jsonErr;
                        jsonErr["error"] = "server_error";
                        jsonErr["error_description"] = "Failed to generate authorization code";
                        auto resp = HttpResponse::newHttpJsonResponse(jsonErr);
                        resp->setStatusCode(k500InternalServerError);
                        callback(resp);
                        return;
                    }

                    std::string location = redirectUri + "?code=" + code;
                    if (!state.empty())
                        location += "&state=" + state;
                    auto resp = HttpResponse::newRedirectionResponse(location);
                    Metrics::incRequest("authorize", 302);
                    callback(resp);
                }
              );
          }
      }
    );
}

void OAuth2Controller::showLoginPage(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    // Get OAuth2 parameters from URL
    auto params = req->getParameters();
    std::string clientId = params["client_id"];
    std::string redirectUri = params["redirect_uri"];
    std::string scope = params["scope"];
    std::string state = params["state"];
    std::string responseType = params["response_type"];
    std::string codeChallenge = params["code_challenge"];
    std::string codeChallengeMethod = params["code_challenge_method"];

    LOG_INFO << "Showing login page with OAuth2 parameters: client_id=" << clientId
             << ", code_challenge=" << (codeChallenge.empty() ? "not provided" : "provided");

    // Create template data
    DrTemplateData data;
    data["client_id"] = clientId;
    data["redirect_uri"] = redirectUri;
    data["scope"] = scope;
    data["state"] = state;
    data["response_type"] = responseType;
    data["code_challenge"] = codeChallenge;
    data["code_challenge_method"] = codeChallengeMethod.empty() ? "plain" : codeChallengeMethod;

    // Render login.csp template
    try
    {
        auto resp = HttpResponse::newHttpViewResponse("login", data);
        callback(resp);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to render login page: " << e.what();
        Json::Value jsonErr;
        jsonErr["error"] = "server_error";
        jsonErr["error_description"] = "Failed to render login page";
        auto resp = HttpResponse::newHttpJsonResponse(jsonErr);
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
    }
}
