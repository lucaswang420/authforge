#include "OAuth2Controller.h"
#include "../AuthService.h"
#include "EmailVerificationController.h"
#include <drogon/drogon.h>
#include <drogon/HttpClient.h>
#include <oauth2/OAuth2Metrics.h>
#include <oauth2/OAuth2Plugin.h>
#include <oauth2/JwkManager.h>
#include <oauth2/AuditLogger.h>
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
            Json::Value successExample;
            successExample["status"] = "ok";
            successExample["version"] = "1.0.0";

            common::documentation::EndpointInfo healthEndpoint;
            healthEndpoint.path = "/health";
            healthEndpoint.method = "GET";
            healthEndpoint.summary = "Health check";
            healthEndpoint.description = "Returns the health status of the service.";
            healthEndpoint.tags = {"System"};
            healthEndpoint.parameters = {};
            healthEndpoint.responses = {{200, "Service is healthy"}};
            healthEndpoint.responseExamples = {{200, successExample}};
            healthEndpoint.requiresAuth = false;
            OpenApiGenerator::addEndpoint(healthEndpoint);
        }

        // Login endpoint
        {
            Json::Value successExample;
            successExample["status"] = "success";
            successExample["redirect_uri"] = "http://127.0.0.1:5173/callback?code=xyz123";

            Json::Value errorExample;
            errorExample["error"] = "invalid_client";

            common::documentation::EndpointInfo loginEndpoint;
            loginEndpoint.path = "/oauth2/login";
            loginEndpoint.method = "POST";
            loginEndpoint.summary = "Authenticate user";
            loginEndpoint.description =
              "Authenticates user credentials and generates an authorization code. "
              "Usually called by the frontend login page during the authorization code flow.";
            loginEndpoint.tags = {"OAuth2", "Authentication"};

            common::documentation::ParameterInfo usernameParam;
            usernameParam.name = "username";
            usernameParam.description = "User's account username (required)";
            usernameParam.type = common::documentation::ParameterType::STRING;
            usernameParam.location = common::documentation::ParameterLocation::QUERY;
            usernameParam.required = true;

            common::documentation::ParameterInfo passwordParam;
            passwordParam.name = "password";
            passwordParam.description = "User's password (required)";
            passwordParam.type = common::documentation::ParameterType::STRING;
            passwordParam.location = common::documentation::ParameterLocation::QUERY;
            passwordParam.required = true;

            common::documentation::ParameterInfo clientIdParam;
            clientIdParam.name = "client_id";
            clientIdParam.description = "Client identifier matches the requesting app (required)";
            clientIdParam.type = common::documentation::ParameterType::STRING;
            clientIdParam.location = common::documentation::ParameterLocation::QUERY;
            clientIdParam.required = true;

            common::documentation::ParameterInfo redirectUriParam;
            redirectUriParam.name = "redirect_uri";
            redirectUriParam.description = "Redirect URI matching the registered client (required)";
            redirectUriParam.type = common::documentation::ParameterType::STRING;
            redirectUriParam.location = common::documentation::ParameterLocation::QUERY;
            redirectUriParam.required = true;

            common::documentation::ParameterInfo scopeParam;
            scopeParam.name = "scope";
            scopeParam.description = "Requested scope, space-separated (optional)";
            scopeParam.type = common::documentation::ParameterType::STRING;
            scopeParam.location = common::documentation::ParameterLocation::QUERY;
            scopeParam.required = false;

            common::documentation::ParameterInfo stateParam;
            stateParam.name = "state";
            stateParam.description = "Opaque value to maintain state (recommended)";
            stateParam.type = common::documentation::ParameterType::STRING;
            stateParam.location = common::documentation::ParameterLocation::QUERY;
            stateParam.required = false;

            loginEndpoint.parameters = {
              usernameParam, passwordParam, clientIdParam, redirectUriParam, scopeParam, stateParam
            };
            loginEndpoint.responses =
              {{200, "Authentication successful (JSON with redirect_uri)"},
               {302, "Redirect with authorization code (if requested via browser)"},
               {401, "Authentication failed"}};
            loginEndpoint.responseExamples = {{200, successExample}, {401, errorExample}};
            loginEndpoint.requiresAuth = false;
            OpenApiGenerator::addEndpoint(loginEndpoint);
        }

        // Register endpoint
        {
            Json::Value successExample;
            successExample["status"] = "success";
            successExample["message"] = "User registered successfully";

            common::documentation::EndpointInfo registerEndpoint;
            registerEndpoint.path = "/api/register";
            registerEndpoint.method = "POST";
            registerEndpoint.summary = "Register new user";
            registerEndpoint.description = "Registers a new user account into the system.";
            registerEndpoint.tags = {"User", "Registration"};

            common::documentation::ParameterInfo usernameParam;
            usernameParam.name = "username";
            usernameParam.description = "Desired username (required)";
            usernameParam.type = common::documentation::ParameterType::STRING;
            usernameParam.location = common::documentation::ParameterLocation::QUERY;
            usernameParam.required = true;

            common::documentation::ParameterInfo passwordParam;
            passwordParam.name = "password";
            passwordParam.description = "Strong password (required)";
            passwordParam.type = common::documentation::ParameterType::STRING;
            passwordParam.location = common::documentation::ParameterLocation::QUERY;
            passwordParam.required = true;

            common::documentation::ParameterInfo emailParam;
            emailParam.name = "email";
            emailParam.description = "Email address (optional)";
            emailParam.type = common::documentation::ParameterType::STRING;
            emailParam.location = common::documentation::ParameterLocation::QUERY;
            emailParam.required = false;

            registerEndpoint.parameters = {usernameParam, passwordParam, emailParam};
            registerEndpoint.responses =
              {{200, "User registered successfully"}, {400, "Invalid registration data"}};
            registerEndpoint.responseExamples = {{200, successExample}};
            registerEndpoint.requiresAuth = false;
            OpenApiGenerator::addEndpoint(registerEndpoint);
        }

        // Consent endpoint
        {
            common::documentation::EndpointInfo consentEndpoint;
            consentEndpoint.path = "/oauth2/consent";
            consentEndpoint.method = "POST";
            consentEndpoint.summary = "Submit user consent";
            consentEndpoint.description =
              "Submit user consent for requested scopes. Redirects back to client.";
            consentEndpoint.tags = {"OAuth2", "Consent"};

            common::documentation::ParameterInfo clientIdParam;
            clientIdParam.name = "client_id";
            clientIdParam.description = "Client identifier (required)";
            clientIdParam.type = common::documentation::ParameterType::STRING;
            clientIdParam.location = common::documentation::ParameterLocation::QUERY;
            clientIdParam.required = true;

            common::documentation::ParameterInfo userIdParam;
            userIdParam.name = "user_id";
            userIdParam.description = "User identifier (required)";
            userIdParam.type = common::documentation::ParameterType::STRING;
            userIdParam.location = common::documentation::ParameterLocation::QUERY;
            userIdParam.required = true;

            common::documentation::ParameterInfo scopeParam;
            scopeParam.name = "scope";
            scopeParam.description = "Requested scope to consent (required)";
            scopeParam.type = common::documentation::ParameterType::STRING;
            scopeParam.location = common::documentation::ParameterLocation::QUERY;
            scopeParam.required = true;

            common::documentation::ParameterInfo redirectUriParam;
            redirectUriParam.name = "redirect_uri";
            redirectUriParam.description = "Redirect URI (required)";
            redirectUriParam.type = common::documentation::ParameterType::STRING;
            redirectUriParam.location = common::documentation::ParameterLocation::QUERY;
            redirectUriParam.required = true;

            common::documentation::ParameterInfo stateParam;
            stateParam.name = "state";
            stateParam.description = "Opaque value to maintain state";
            stateParam.type = common::documentation::ParameterType::STRING;
            stateParam.location = common::documentation::ParameterLocation::QUERY;
            stateParam.required = false;

            common::documentation::ParameterInfo actionParam;
            actionParam.name = "action";
            actionParam.description = "Action to perform: 'approve' or 'deny' (required)";
            actionParam.type = common::documentation::ParameterType::STRING;
            actionParam.location = common::documentation::ParameterLocation::QUERY;
            actionParam.required = true;
            actionParam.enumValues = "approve,deny";

            consentEndpoint.parameters =
              {clientIdParam, userIdParam, scopeParam, redirectUriParam, stateParam, actionParam};
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
    std::string nonce;

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
            nonce = json->get("nonce", "").asString();
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
        nonce = params["nonce"];
    }

    AuthService::validateUser(
      username,
      password,
      [req,
       username,
       clientId,
       scope,
       redirectUri,
       state,
       nonce,
       codeChallenge,
       codeChallengeMethod,
       callback = std::move(callback)](std::optional<services::AuthResult> authResult) mutable {
          if (authResult)
          {
              req->session()->insert("userId", std::to_string(authResult->internalId));

              // Audit: login success
              oauth2::AuditLogger::log(
                "login_success",
                "success",
                req,
                authResult->publicSub,
                "user",
                authResult->publicSub
              );

              // === CHECK 1: Email verification enforcement ===
              auto customCfg = drogon::app().getCustomConfig();
              bool requireEmailVerification = false;
              if (
                customCfg.isMember("auth") &&
                customCfg["auth"].isMember("require_email_verification")
              )
              {
                  requireEmailVerification =
                    customCfg["auth"]["require_email_verification"].asBool();
              }
              if (requireEmailVerification && !authResult->emailVerified)
              {
                  Json::Value err;
                  err["error"] = "email_not_verified";
                  err["error_description"] = "Please verify your email address before logging in";
                  auto resp = HttpResponse::newHttpJsonResponse(err);
                  resp->setStatusCode(k403Forbidden);
                  callback(resp);
                  return;
              }

              // === CHECK 2: MFA enforcement ===
              if (authResult->mfaEnabled)
              {
                  // MFA is enabled - don't issue auth code yet
                  // Return mfa_required response with a temporary token
                  Json::Value mfaResp;
                  mfaResp["mfa_required"] = true;
                  mfaResp["mfa_token"] = std::to_string(authResult->internalId);
                  mfaResp["message"] =
                    "MFA verification required. Submit TOTP code to /oauth2/mfa/verify";
                  auto resp = HttpResponse::newHttpJsonResponse(mfaResp);
                  resp->setStatusCode(k200OK);
                  callback(resp);
                  return;
              }

              // === CHECK 3: PKCE enforcement for PUBLIC clients ===
              bool requirePkce = false;
              if (
                customCfg.isMember("auth") && customCfg["auth"].isMember("require_pkce_for_public")
              )
              {
                  requirePkce = customCfg["auth"]["require_pkce_for_public"].asBool();
              }
              if (requirePkce && codeChallenge.empty())
              {
                  // Check will be done after getClient - for now log warning
                  LOG_WARN << "[SECURITY] PUBLIC client " << clientId
                           << " login without PKCE (enforcement enabled)";
                  Json::Value err;
                  err["error"] = "invalid_request";
                  err["error_description"] =
                    "PKCE (code_challenge) is required for public clients. Use "
                    "code_challenge_method=S256.";
                  auto resp = HttpResponse::newHttpJsonResponse(err);
                  resp->setStatusCode(k400BadRequest);
                  callback(resp);
                  return;
              }

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
                authResult->publicSub,
                scope,
                redirectUri,
                codeChallenge,
                codeChallengeMethod,
                nonce,
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

              // Audit: login failure
              oauth2::AuditLogger::log("login_failure", "failure", req, username, "user", username);

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

    AuthService::registerUser(
      username, password, email, [callback, email](const std::string &error) {
          if (error.empty())
          {
              // Send verification email if email provided
              if (!email.empty())
              {
                  // Look up the newly created user to get their ID
                  auto db = drogon::app().getDbClient();
                  db->execSqlAsync(
                    "SELECT id FROM users WHERE email = $1 ORDER BY id DESC LIMIT 1",
                    [email](const drogon::orm::Result &r) {
                        if (!r.empty())
                        {
                            int userId = r[0]["id"].as<int>();
                            EmailVerificationController::sendVerificationEmail(userId, email);
                        }
                    },
                    [](const drogon::orm::DrogonDbException &) {},
                    email
                  );
              }

              Json::Value json;
              json["message"] = "User registered successfully";
              if (!email.empty())
                  json["note"] = "Please check your email to verify your account";
              auto resp = HttpResponse::newHttpJsonResponse(json);
              callback(resp);
          }
          else
          {
              auto resp = HttpResponse::newHttpResponse();
              resp->setStatusCode(k500InternalServerError);
              resp->setBody(error);
              callback(resp);
          }
      }
    );
}

#include <oauth2/filters/OAuth2Middleware.h>

// ...

namespace
{
/**
 * @brief Send backchannel logout notifications to all clients with a configured
 * backchannel_logout_uri for the given user.
 *
 * This is fire-and-forget: failures are logged but do not affect the logout response.
 */
void sendBackchannelLogoutNotifications(const std::string &userPublicSub)
{
    auto plugin = drogon::app().getPlugin<OAuth2Plugin>();
    if (!plugin)
        return;

    auto jwkManager = plugin->getJwkManager();
    if (!jwkManager || !jwkManager->isInitialized())
    {
        LOG_WARN << "Backchannel logout: JwkManager not initialized, skipping notifications";
        return;
    }

    // Get issuer from config
    auto customConfig = drogon::app().getCustomConfig();
    std::string issuer = "http://localhost:5555";
    if (customConfig.isMember("metadata") && customConfig["metadata"].isMember("issuer"))
    {
        issuer = customConfig["metadata"]["issuer"].asString();
    }

    // Query clients with backchannel_logout_uri that the user has authorized
    auto db = drogon::app().getDbClient();
    if (!db)
    {
        LOG_ERROR << "Backchannel logout: No DB client available";
        return;
    }

    db->execSqlAsync(
      "SELECT c.client_id, c.backchannel_logout_uri "
      "FROM oauth2_clients c "
      "JOIN oauth2_user_consents uc ON c.client_id = uc.client_id "
      "WHERE uc.internal_user_id = (SELECT id FROM users WHERE public_sub::text = $1::text) "
      "AND c.backchannel_logout_uri IS NOT NULL "
      "AND c.backchannel_logout_uri != ''",
      [jwkManager, issuer, userPublicSub](const drogon::orm::Result &result) {
          if (result.empty())
          {
              LOG_DEBUG << "Backchannel logout: No clients with backchannel_logout_uri for user "
                        << userPublicSub;
              return;
          }

          for (const auto &row : result)
          {
              std::string clientId = row["client_id"].as<std::string>();
              std::string logoutUri = row["backchannel_logout_uri"].as<std::string>();

              // Build logout_token claims
              auto now = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::system_clock::now().time_since_epoch()
              )
                           .count();

              Json::Value claims;
              claims["iss"] = issuer;
              claims["sub"] = userPublicSub;
              claims["aud"] = clientId;
              claims["iat"] = (Json::Int64)now;
              claims["jti"] = drogon::utils::getUuid();

              // OIDC backchannel logout event claim
              Json::Value events;
              events["http://schemas.openid.net/event/backchannel-logout"] = Json::objectValue;
              claims["events"] = events;

              std::string logoutToken = jwkManager->signJwt(claims);
              if (logoutToken.empty())
              {
                  LOG_ERROR << "Backchannel logout: Failed to sign logout_token for client "
                            << clientId;
                  continue;
              }

              LOG_INFO << "Backchannel logout: Sending logout_token to client " << clientId
                       << " at " << logoutUri;

              // Fire-and-forget POST to the client's backchannel_logout_uri
              try
              {
                  auto client = drogon::HttpClient::newHttpClient(logoutUri);
                  auto request = drogon::HttpRequest::newHttpRequest();
                  request->setMethod(drogon::Post);
                  request->setPath("/");  // Path is already in the full URI
                  request->setContentTypeCode(drogon::CT_APPLICATION_X_FORM);
                  request->setBody("logout_token=" + logoutToken);

                  client->sendRequest(
                    request,
                    [clientId,
                     logoutUri](drogon::ReqResult result, const drogon::HttpResponsePtr &resp) {
                        if (result == drogon::ReqResult::Ok && resp)
                        {
                            LOG_INFO << "Backchannel logout: Client " << clientId << " responded "
                                     << resp->getStatusCode();
                        }
                        else
                        {
                            LOG_WARN << "Backchannel logout: Failed to notify client " << clientId
                                     << " at " << logoutUri;
                        }
                    }
                  );
              }
              catch (const std::exception &e)
              {
                  LOG_ERROR << "Backchannel logout: Exception sending to client " << clientId
                            << ": " << e.what();
              }
          }
      },
      [userPublicSub](const drogon::orm::DrogonDbException &e) {
          LOG_ERROR << "Backchannel logout: DB query failed for user " << userPublicSub << ": "
                    << e.base().what();
      },
      userPublicSub
    );
}
}  // namespace

void OAuth2Controller::logout(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    // Check Authorization header (OAuth2Middleware normally handles this,
    // but direct calls in tests may bypass the filter)
    auto authHeader = req->getHeader("Authorization");
    if (authHeader.empty() || authHeader.length() < 8 || authHeader.substr(0, 7) != "Bearer ")
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k401Unauthorized);
        resp->setBody("Missing or invalid Authorization header");
        callback(resp);
        return;
    }

    // The OAuth2Middleware filter has already validated the token and set attributes
    auto attrs = req->getAttributes();
    std::string userId = attrs->get<std::string>("userId");
    std::string clientId = attrs->get<std::string>("clientId");

    std::string token = authHeader.substr(7);  // Remove "Bearer "

    auto plugin = drogon::app().getPlugin<OAuth2Plugin>();
    if (!plugin)
    {
        LOG_ERROR << "OAuth2 Plugin not loaded during logout";
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k500InternalServerError);
        resp->setBody("Internal Server Error: Plugin not loaded");
        callback(resp);
        return;
    }

    // Revoke the access token
    plugin->revokeAccessToken(token, clientId, [userId, callback = std::move(callback)]() mutable {
        LOG_INFO << "Logout: Token revoked for user " << userId;

        // Send backchannel logout notifications (fire-and-forget)
        sendBackchannelLogoutNotifications(userId);

        // Respond immediately without waiting for backchannel notifications
        Json::Value json;
        json["message"] = "Logged out successfully";
        auto resp = HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(k200OK);
        callback(resp);
    });
}

void OAuth2Controller::healthLive(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    // Liveness: process is running, always 200
    Json::Value json;
    json["status"] = "ok";
    auto resp = HttpResponse::newHttpJsonResponse(json);
    callback(resp);
}

void OAuth2Controller::healthReady(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    // Readiness: check DB connectivity
    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    try
    {
        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
          "SELECT 1",
          [sharedCb](const drogon::orm::Result &) {
              // DB OK - check Redis
              try
              {
                  auto redis = drogon::app().getRedisClient("default");
                  redis->execCommandAsync(
                    [sharedCb](const drogon::nosql::RedisResult &) {
                        Json::Value json;
                        json["status"] = "ok";
                        json["database"] = "connected";
                        json["redis"] = "connected";
                        (*sharedCb)(HttpResponse::newHttpJsonResponse(json));
                    },
                    [sharedCb](const std::exception &) {
                        Json::Value json;
                        json["status"] = "degraded";
                        json["database"] = "connected";
                        json["redis"] = "disconnected";
                        auto resp = HttpResponse::newHttpJsonResponse(json);
                        resp->setStatusCode(k503ServiceUnavailable);
                        (*sharedCb)(resp);
                    },
                    "PING"
                  );
              }
              catch (...)
              {
                  // Redis not configured - that's OK for some deployments
                  Json::Value json;
                  json["status"] = "ok";
                  json["database"] = "connected";
                  json["redis"] = "not_configured";
                  (*sharedCb)(HttpResponse::newHttpJsonResponse(json));
              }
          },
          [sharedCb](const drogon::orm::DrogonDbException &e) {
              Json::Value json;
              json["status"] = "unhealthy";
              json["database"] = "disconnected";
              json["error"] = e.base().what();
              auto resp = HttpResponse::newHttpJsonResponse(json);
              resp->setStatusCode(k503ServiceUnavailable);
              (*sharedCb)(resp);
          }
        );
    }
    catch (...)
    {
        Json::Value json;
        json["status"] = "unhealthy";
        json["database"] = "unavailable";
        auto resp = HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(k503ServiceUnavailable);
        (*sharedCb)(resp);
    }
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
    std::string codeChallenge = params["code_challenge"];
    std::string codeChallengeMethod = params["code_challenge_method"];
    std::string nonce = params["nonce"];

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
      [plugin,
       clientId,
       userId,
       scope,
       redirectUri,
       state,
       codeChallenge,
       codeChallengeMethod,
       nonce,
       callback = std::move(callback)](std::optional<int32_t> internalUserId) mutable {
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
                 codeChallenge,
                 codeChallengeMethod,
                 nonce,
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
                      codeChallenge,
                      codeChallengeMethod,
                      nonce,
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
                codeChallenge,
                codeChallengeMethod,
                nonce,
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

    // Build frontend register URL from config
    std::string frontendRegisterUrl;
    auto customConfig = drogon::app().getCustomConfig();
    if (customConfig.isMember("frontend"))
    {
        const auto &frontend = customConfig["frontend"];
        std::string baseUrl = frontend.get("url", "http://localhost:5173").asString();
        std::string registerPath = frontend.get("register_path", "/register").asString();
        frontendRegisterUrl = baseUrl + registerPath;
    }
    else
    {
        frontendRegisterUrl = "http://localhost:5173/register";
    }

    // Create template data
    DrTemplateData data;
    data["client_id"] = clientId;
    data["redirect_uri"] = redirectUri;
    data["scope"] = scope;
    data["state"] = state;
    data["response_type"] = responseType;
    data["code_challenge"] = codeChallenge;
    data["code_challenge_method"] = codeChallengeMethod.empty() ? "plain" : codeChallengeMethod;
    data["frontend_register_url"] = frontendRegisterUrl;

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
