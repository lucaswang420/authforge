#include <oauth2/controllers/OAuth2StandardController.h>
#include <oauth2/OAuth2Plugin.h>
#include <oauth2/OAuth2Metrics.h>
#include <oauth2/ValidatorHelper.h>
#include <oauth2/ValidationHelper.h>
#include <oauth2/OAuth2ErrorHandler.h>
#include <oauth2/OpenApiGenerator.h>
#include <drogon/drogon.h>
#include <drogon/utils/Utilities.h>
#include <algorithm>
#include <functional>
#include <sstream>

using namespace oauth2;
using namespace oauth2::controllers;
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

namespace oauth2::controllers
{

void OAuth2StandardController::initApiDocs()
{
    // Token endpoint
    common::documentation::EndpointInfo tokenEndpoint;
    tokenEndpoint.path = "/oauth2/token";
    tokenEndpoint.method = "POST";
    tokenEndpoint.summary = "Exchange authorization code for access token";
    tokenEndpoint.description =
      "OAuth2 token endpoint - exchanges authorization "
      "code or refresh token for access token";
    tokenEndpoint.tags = {"OAuth2", "Token"};
    tokenEndpoint.parameters =
      {{"grant_type",
        "Authorization code or refresh token (required)",
        common::documentation::ParameterType::STRING,
        common::documentation::ParameterLocation::QUERY,
        true},
       {"code",
        "Authorization code (required for grant_type=authorization_code)",
        common::documentation::ParameterType::STRING,
        common::documentation::ParameterLocation::QUERY,
        false},
       {"refresh_token",
        "Refresh token (required for grant_type=refresh_token)",
        common::documentation::ParameterType::STRING,
        common::documentation::ParameterLocation::QUERY,
        false},
       {"client_id",
        "Client identifier (required)",
        common::documentation::ParameterType::STRING,
        common::documentation::ParameterLocation::QUERY,
        true},
       {"client_secret",
        "Client secret (required for confidential clients)",
        common::documentation::ParameterType::STRING,
        common::documentation::ParameterLocation::QUERY,
        true},
       {"redirect_uri",
        "Redirect URI (required for authorization_code grant)",
        common::documentation::ParameterType::STRING,
        common::documentation::ParameterLocation::QUERY,
        true}};
    tokenEndpoint.responses =
      {{200, "Token response with access_token and refresh_token"},
       {400, "Invalid request"},
       {401, "Authentication failed"}};
    tokenEndpoint.requiresAuth = false;
    OpenApiGenerator::addEndpoint(tokenEndpoint);

    // Authorize endpoint
    common::documentation::EndpointInfo authorizeEndpoint;
    authorizeEndpoint.path = "/oauth2/authorize";
    authorizeEndpoint.method = "GET";
    authorizeEndpoint.summary = "Request authorization";
    authorizeEndpoint.description =
      "OAuth2 authorization endpoint - initiates authorization flow";
    authorizeEndpoint.tags = {"OAuth2", "Authorization"};
    authorizeEndpoint.parameters =
      {{"client_id",
        "Client identifier (required)",
        common::documentation::ParameterType::STRING,
        common::documentation::ParameterLocation::QUERY,
        true},
       {"redirect_uri",
        "Redirect URI (required)",
        common::documentation::ParameterType::STRING,
        common::documentation::ParameterLocation::QUERY,
        true},
       {"response_type",
        "Response type, must be 'code' (required)",
        common::documentation::ParameterType::STRING,
        common::documentation::ParameterLocation::QUERY,
        true},
       {"scope",
        "Requested scope (optional)",
        common::documentation::ParameterType::STRING,
        common::documentation::ParameterLocation::QUERY,
        false},
       {"state",
        "Opaque value to maintain state between request and callback "
        "(recommended)",
        common::documentation::ParameterType::STRING,
        common::documentation::ParameterLocation::QUERY,
        false}};
    authorizeEndpoint.responses =
      {{302, "Redirect to client with authorization code"}, {400, "Invalid request"}};
    authorizeEndpoint.requiresAuth = false;
    OpenApiGenerator::addEndpoint(authorizeEndpoint);

    // UserInfo endpoint
    {
        Json::Value successExample;
        successExample["sub"] = "1";
        successExample["name"] = "john_doe";
        successExample["email"] = "john@example.com";
        successExample["roles"] = Json::Value(Json::arrayValue);
        successExample["roles"].append("user");
        successExample["roles"].append("admin");

        Json::Value errorExample;
        errorExample["error"] = "User not found";

        common::documentation::EndpointInfo userInfoEndpoint;
        userInfoEndpoint.path = "/oauth2/userinfo";
        userInfoEndpoint.method = "GET";
        userInfoEndpoint.summary = "Get user information";
        userInfoEndpoint.description =
          "Returns information about the authenticated user. "
          "Provides user profile data including username, email, "
          "and assigned roles according to OpenID Connect "
          "standards.";
        userInfoEndpoint.tags = {"OAuth2", "User"};
        userInfoEndpoint.parameters = {};
        userInfoEndpoint.responses =
          {{200, "User information retrieved successfully"},
           {400, "Invalid User ID format"},
           {401, "Invalid or expired access token"},
           {404, "User not found"}};
        userInfoEndpoint.responseExamples = {{200, successExample}, {404, errorExample}};
        userInfoEndpoint.requiresAuth = true;
        OpenApiGenerator::addEndpoint(userInfoEndpoint);
    }
}

drogon::HttpResponsePtr OAuth2StandardController::createSuccessResponse()
{
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k200OK);
    return resp;
}

std::pair<std::string, std::string> OAuth2StandardController::extractClientCredentials(
  const drogon::HttpRequestPtr &req
)
{
    std::string clientId, clientSecret;

    // Prefer HTTP Basic Auth
    auto authHeader = req->getHeader("Authorization");
    if (!authHeader.empty() && authHeader.find("Basic ") == 0)
    {
        auto basicAuth = authHeader.substr(6);
        try
        {
            auto decoded = drogon::utils::base64Decode(basicAuth);
            auto colonPos = decoded.find(':');
            if (colonPos != std::string::npos)
            {
                clientId = decoded.substr(0, colonPos);
                clientSecret = decoded.substr(colonPos + 1);
            }
        }
        catch (...)
        {
            LOG_ERROR << "Failed to decode Basic Auth header";
        }
    }
    else
    {
        // Fallback to POST body
        clientId = req->getParameter("client_id");
        clientSecret = req->getParameter("client_secret");
    }

    return {clientId, clientSecret};
}

void OAuth2StandardController::introspect(
  const drogon::HttpRequestPtr &req,
  std::function<void(const drogon::HttpResponsePtr &)> &&callback
)
{
    LOG_DEBUG << "Token introspection requested";

    // Extract client credentials
    auto credentials = extractClientCredentials(req);
    auto clientId = credentials.first;
    auto clientSecret = credentials.second;

    if (clientId.empty() || clientSecret.empty())
    {
        common::error::OAuth2ErrorHandler::sendErrorResponse(
          std::move(callback), "invalid_client", "Client authentication required"
        );
        return;
    }

    // Get OAuth2 plugin
    auto plugin = drogon::app().getPlugin<::OAuth2Plugin>();
    if (!plugin)
    {
        common::error::OAuth2ErrorHandler::sendErrorResponse(
          std::move(callback), "server_error", "OAuth2 plugin not available"
        );
        return;
    }

    // Validate request parameters
    auto validationErrors =
      common::validation::ValidatorHelper::validateOAuth2IntrospectParams(req);
    if (!validationErrors.empty())
    {
        common::error::OAuth2ErrorHandler::sendErrorResponse(
          std::move(callback), "invalid_request", validationErrors[0]
        );
        return;
    }

    // Extract token
    std::string token = req->getParameter("token");

    // Authenticate client
    plugin->validateClient(
      clientId,
      clientSecret,
      [plugin, token, clientId, callback = std::move(callback)](bool valid) mutable {
          if (!valid)
          {
              oauth2::Metrics::incrementIntrospectErrors(clientId, "invalid_client");
              common::error::OAuth2ErrorHandler::sendErrorResponse(
                std::move(callback), "invalid_client", "Client authentication failed"
              );
              return;
          }

          // Introspect token
          plugin->introspectToken(
            token,
            [clientId, callback = std::move(callback)](
              std::optional<oauth2::TokenIntrospection> introspection
            ) mutable {
                if (!introspection)
                {
                    // Token not found or invalid
                    oauth2::Metrics::incrementIntrospectRequests(clientId);

                    Json::Value response;
                    response["active"] = false;
                    auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
                    resp->setStatusCode(drogon::k200OK);
                    callback(resp);
                    return;
                }

                // Token is active, return full metadata
                oauth2::Metrics::incrementIntrospectRequests(clientId);

                Json::Value response;
                response["active"] = introspection->active;
                response["client_id"] = introspection->clientId;
                response["token_type"] = "Bearer";

                if (introspection->exp > 0)
                {
                    response["exp"] = static_cast<Json::Int64>(introspection->exp);
                }
                if (introspection->iat > 0)
                {
                    response["iat"] = static_cast<Json::Int64>(introspection->iat);
                }
                if (introspection->nbf > 0)
                {
                    response["nbf"] = static_cast<Json::Int64>(introspection->nbf);
                }
                if (!introspection->sub.empty())
                {
                    response["sub"] = introspection->sub;
                }
                if (!introspection->aud.empty())
                {
                    response["aud"] = introspection->aud;
                }
                if (!introspection->iss.empty())
                {
                    response["iss"] = introspection->iss;
                }
                if (!introspection->scope.empty())
                {
                    response["scope"] = introspection->scope;
                }

                auto resp = drogon::HttpResponse::newHttpJsonResponse(response);
                resp->setStatusCode(drogon::k200OK);
                callback(resp);
            }
          );
      }
    );
}

void OAuth2StandardController::revoke(
  const drogon::HttpRequestPtr &req,
  std::function<void(const drogon::HttpResponsePtr &)> &&callback
)
{
    LOG_DEBUG << "Token revocation requested";

    // Extract client credentials
    auto credentials = extractClientCredentials(req);
    auto clientId = credentials.first;
    auto clientSecret = credentials.second;

    if (clientId.empty() || clientSecret.empty())
    {
        common::error::OAuth2ErrorHandler::sendErrorResponse(
          std::move(callback), "invalid_client", "Client authentication required"
        );
        return;
    }

    // Get OAuth2 plugin
    auto plugin = drogon::app().getPlugin<::OAuth2Plugin>();
    if (!plugin)
    {
        common::error::OAuth2ErrorHandler::sendErrorResponse(
          std::move(callback), "server_error", "OAuth2 plugin not available"
        );
        return;
    }

    // Validate request parameters
    auto validationErrors = common::validation::ValidatorHelper::validateOAuth2RevokeParams(req);
    if (!validationErrors.empty())
    {
        common::error::OAuth2ErrorHandler::sendErrorResponse(
          std::move(callback), "invalid_request", validationErrors[0]
        );
        return;
    }

    // Extract token
    std::string token = req->getParameter("token");

    // Authenticate client
    plugin->validateClient(
      clientId,
      clientSecret,
      [plugin, token, clientId, callback = std::move(callback)](bool valid) mutable {
          if (!valid)
          {
              oauth2::Metrics::incrementRevocationErrors(clientId, "invalid_client");
              common::error::OAuth2ErrorHandler::sendErrorResponse(
                std::move(callback), "invalid_client", "Client authentication failed"
              );
              return;
          }

          // Check token ownership (permission control)
          plugin->introspectToken(
            token,
            [plugin, clientId, callback = std::move(callback), token](
              std::optional<oauth2::TokenIntrospection> introspection
            ) mutable {
                if (!introspection || !introspection->active)
                {
                    // Token doesn't exist or inactive - return success per RFC 7009
                    // (prevents token probing attacks)
                    oauth2::Metrics::incrementRevocationRequests(clientId);
                    callback(createSuccessResponse());
                    return;
                }

                // Check permission: only token owner can revoke
                if (introspection->clientId != clientId)
                {
                    oauth2::Metrics::incrementRevocationErrors(clientId, "unauthorized_client");
                    common::error::OAuth2ErrorHandler::sendErrorResponse(
                      std::move(callback),
                      "unauthorized_client",
                      "This client is not allowed to revoke the token"
                    );
                    return;
                }

                // Has permission, execute revocation
                plugin->revokeAccessToken(
                  token, clientId, [clientId, callback = std::move(callback)]() mutable {
                      oauth2::Metrics::incrementRevocationRequests(clientId);
                      callback(createSuccessResponse());
                  }
                );
            }
          );
      }
    );
}

void OAuth2StandardController::metadata(
  const drogon::HttpRequestPtr &req,
  std::function<void(const drogon::HttpResponsePtr &)> &&callback
)
{
    LOG_DEBUG << "Metadata endpoint requested";

    // Get OAuth2 plugin
    auto plugin = drogon::app().getPlugin<::OAuth2Plugin>();
    if (!plugin)
    {
        common::error::OAuth2ErrorHandler::sendErrorResponse(
          std::move(callback), "server_error", "OAuth2 plugin not available"
        );
        return;
    }

    // In a real implementation, base URL should come from configuration.
    // For now, we construct it or use a default.
    std::string baseUrl = "http://localhost:8080";
    auto customConfig = drogon::app().getCustomConfig();
    if (customConfig.isMember("metadata") && customConfig["metadata"].isMember("issuer"))
    {
        baseUrl = customConfig["metadata"]["issuer"].asString();
    }

    Json::Value metadata;

    // Basic server info
    metadata["issuer"] = baseUrl;
    metadata["authorization_endpoint"] = baseUrl + "/oauth2/authorize";
    metadata["token_endpoint"] = baseUrl + "/oauth2/token";

    // P1 endpoints
    metadata["introspection_endpoint"] = baseUrl + "/oauth2/introspect";
    metadata["introspection_endpoint_auth_methods_supported"] = Json::Value(Json::arrayValue);
    metadata["introspection_endpoint_auth_methods_supported"].append("client_secret_post");
    metadata["introspection_endpoint_auth_methods_supported"].append("client_secret_basic");
    metadata["introspection_endpoint_auth_methods_supported"].append("none");

    metadata["revocation_endpoint"] = baseUrl + "/oauth2/revoke";
    metadata["revocation_endpoint_auth_methods_supported"] = Json::Value(Json::arrayValue);
    metadata["revocation_endpoint_auth_methods_supported"].append("client_secret_post");
    metadata["revocation_endpoint_auth_methods_supported"].append("client_secret_basic");
    metadata["revocation_endpoint_auth_methods_supported"].append("none");

    // OpenID Connect support (partial, based on what we implement)
    metadata["scopes_supported"] = Json::Value(Json::arrayValue);
    metadata["scopes_supported"].append("openid");
    metadata["scopes_supported"].append("profile");
    metadata["scopes_supported"].append("email");
    metadata["scopes_supported"].append("admin");

    metadata["response_types_supported"] = Json::Value(Json::arrayValue);
    metadata["response_types_supported"].append("code");

    metadata["response_modes_supported"] = Json::Value(Json::arrayValue);
    metadata["response_modes_supported"].append("query");

    metadata["grant_types_supported"] = Json::Value(Json::arrayValue);
    metadata["grant_types_supported"].append("authorization_code");
    metadata["grant_types_supported"].append("refresh_token");

    // PKCE support
    metadata["code_challenge_methods_supported"] = Json::Value(Json::arrayValue);
    metadata["code_challenge_methods_supported"].append("plain");
    metadata["code_challenge_methods_supported"].append("S256");

    // Client authentication methods
    metadata["token_endpoint_auth_methods_supported"] = Json::Value(Json::arrayValue);
    metadata["token_endpoint_auth_methods_supported"].append("client_secret_post");
    metadata["token_endpoint_auth_methods_supported"].append("client_secret_basic");
    metadata["token_endpoint_auth_methods_supported"].append("none");

    // Documentation and policies (if configured)
    if (customConfig.isMember("metadata"))
    {
        if (customConfig["metadata"].isMember("service_documentation"))
            metadata["service_documentation"] = customConfig["metadata"]["service_documentation"];
        if (customConfig["metadata"].isMember("op_policy_uri"))
            metadata["op_policy_uri"] = customConfig["metadata"]["op_policy_uri"];
        if (customConfig["metadata"].isMember("op_tos_uri"))
            metadata["op_tos_uri"] = customConfig["metadata"]["op_tos_uri"];
    }

    auto resp = drogon::HttpResponse::newHttpJsonResponse(metadata);
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    callback(resp);
}

void OAuth2StandardController::authorize(
  const drogon::HttpRequestPtr &req,
  std::function<void(const drogon::HttpResponsePtr &)> &&callback
)
{
    // Use ValidatorHelper for consistent validation
    auto errors = common::validation::ValidatorHelper::validateOAuth2AuthorizeParams(req);

    // Return validation errors if any
    if (
      common::validation::ValidationHelper::returnValidationErrorsIfAny(errors, std::move(callback))
    )
    {
        Metrics::incRequest("authorize", 400);
        return;
    }

    auto params = req->getParameters();
    std::string responseType = params["response_type"];
    std::string clientId = params["client_id"];
    std::string redirectUri = params["redirect_uri"];
    std::string scope = params["scope"];
    std::string state = params["state"];

    // P0-4: State Parameter Enforcement
    if (state.empty())
    {
        LOG_WARN << "Authorization request missing state parameter (CSRF vulnerability) for client: "
                 << clientId;
        Metrics::incRequest("authorize", 400);
        Metrics::incLoginFailure("missing_state_parameter");

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k400BadRequest);
        resp->setBody(
          "state parameter is required for CSRF protection. "
          "Please include a state parameter in your authorization request."
        );
        callback(resp);
        return;
    }

    if (state.length() < 8 || state.length() > 512)
    {
        LOG_WARN << "Authorization request has invalid state parameter length (must be 8-512 chars) for client: "
                 << clientId << ", state length: " << state.length();
        Metrics::incRequest("authorize", 400);
        Metrics::incLoginFailure("invalid_state_parameter");

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k400BadRequest);
        resp->setBody("state parameter must be between 8 and 512 characters.");
        callback(resp);
        return;
    }

    if (
      state.find('?') != std::string::npos || state.find('#') != std::string::npos ||
      state.find('&') != std::string::npos
    )
    {
        LOG_WARN << "Authorization request has potentially malicious state parameter (contains URL delimiters) for client: "
                 << clientId << ", state: " << state.substr(0, 20) << "...";
        Metrics::incRequest("authorize", 400);
        Metrics::incLoginFailure("suspicious_state_parameter");

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k400BadRequest);
        resp->setBody("state parameter contains invalid characters.");
        callback(resp);
        return;
    }

    LOG_DEBUG << "Authorization request with valid state parameter for client: " << clientId
              << ", state: " << state.substr(0, 8) << "...";

    auto plugin = drogon::app().getPlugin<::OAuth2Plugin>();
    if (!plugin)
    {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k500InternalServerError);
        resp->setBody("OAuth2 Plugin not loaded");
        callback(resp);
        return;
    }

    // Validate Client (Async)
    plugin->validateClient(
      clientId,
      "",
      [this, plugin, clientId, redirectUri, scope, state, responseType, req, callback = std::move(callback)](bool validClient) mutable {
          if (!validClient)
          {
              Metrics::incRequest("authorize", 400);
              Metrics::incLoginFailure("invalid_client_id");

              auto resp = drogon::HttpResponse::newHttpResponse();
              resp->setStatusCode(drogon::k400BadRequest);
              resp->setBody("Invalid client_id");
              callback(resp);
              return;
          }

          // Validate Redirect URI (Async)
          plugin->validateRedirectUri(
            clientId,
            redirectUri,
            [this, plugin, clientId, redirectUri, scope, state, responseType, req, callback = std::move(callback)](bool validUri) mutable {
                if (!validUri)
                {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k400BadRequest);
                    resp->setBody("Invalid redirect_uri");
                    callback(resp);
                    return;
                }

                std::vector<std::string> requestedScopes;
                std::stringstream ss(scope);
                std::string scopeItem;
                while (std::getline(ss, scopeItem, ' '))
                {
                    if (!scopeItem.empty())
                    {
                        requestedScopes.push_back(scopeItem);
                    }
                }

                plugin->validateClientScopes(
                  clientId,
                  requestedScopes,
                  [this, plugin, clientId, redirectUri, scope, state, responseType, req, requestedScopes, callback = std::move(callback)](bool validScopes, std::string scopeError) mutable {
                      if (!validScopes)
                      {
                          LOG_WARN << "Client scope validation failed: " << scopeError;
                          Json::Value jsonErr;
                          jsonErr["error"] = "invalid_scope";
                          jsonErr["error_description"] = scopeError;
                          auto resp = drogon::HttpResponse::newHttpJsonResponse(jsonErr);
                          resp->setStatusCode(drogon::k400BadRequest);
                          callback(resp);
                          return;
                      }

                      LOG_DEBUG << "Client scope validation successful";

                      std::string userId;
                      if (req->session())
                      {
                          userId = req->session()->get<std::string>("userId");
                      }
                      if (!userId.empty())
                      {
                          plugin->validateUserRolesForScopes(
                            userId,
                            requestedScopes,
                            [this, plugin, userId, requestedScopes, clientId, scope, redirectUri, state, callback = std::move(callback)](bool validRoles, std::string roleError) mutable {
                                if (!validRoles)
                                {
                                    LOG_WARN << "User role validation failed: " << roleError;
                                    Json::Value jsonErr;
                                    jsonErr["error"] = "unauthorized_client";
                                    jsonErr["error_description"] = roleError;
                                    auto resp = drogon::HttpResponse::newHttpJsonResponse(jsonErr);
                                    resp->setStatusCode(drogon::k403Forbidden);
                                    callback(resp);
                                    return;
                                }

                                LOG_DEBUG << "User role validation successful";

                                plugin->getInternalUserId(
                                  userId,
                                  [this, plugin, userId, clientId, scope, redirectUri, state, requestedScopes, callback = std::move(callback)](std::optional<int32_t> internalUserId) mutable {
                                      if (!internalUserId)
                                      {
                                          LOG_WARN << "No internal user ID found for subject: " << userId << ", proceeding without consent check";

                                          plugin->generateAuthorizationCode(
                                            clientId,
                                            userId,
                                            scope,
                                            redirectUri,
                                            "",  // codeChallenge
                                            "",  // codeChallengeMethod
                                            [redirectUri, state, callback = std::move(callback)](bool success, std::string code, std::string error) {
                                                if (!success)
                                                {
                                                    LOG_ERROR << "Failed to generate authorization code: " << error;
                                                    Json::Value jsonErr;
                                                    jsonErr["error"] = "server_error";
                                                    jsonErr["error_description"] = "Failed to generate authorization code";
                                                    auto resp = drogon::HttpResponse::newHttpJsonResponse(jsonErr);
                                                    resp->setStatusCode(drogon::k500InternalServerError);
                                                    callback(resp);
                                                    return;
                                                }

                                                std::string location = redirectUri + "?code=" + code;
                                                if (!state.empty())
                                                    location += "&state=" + state;
                                                auto resp = drogon::HttpResponse::newRedirectionResponse(location);
                                                Metrics::incRequest("authorize", 302);
                                                callback(resp);
                                            }
                                          );
                                          return;
                                      }

                                      checkUserConsentAndProceed(
                                        plugin,
                                        clientId,
                                        userId,
                                        *internalUserId,
                                        requestedScopes,
                                        scope,
                                        redirectUri,
                                        state,
                                        std::move(callback)
                                      );
                                  }
                                );
                            }
                          );
                          return;
                      }
                      else
                      {
                          auto customConfig = drogon::app().getCustomConfig();
                          std::string loginUrl = "/login";
                          if (customConfig.isMember("oauth2") && customConfig["oauth2"].isMember("login_url")) {
                              loginUrl = customConfig["oauth2"]["login_url"].asString();
                          }
                          std::string location = loginUrl + "?client_id=" + drogon::utils::urlEncode(clientId) + 
                                                 "&redirect_uri=" + drogon::utils::urlEncode(redirectUri) + 
                                                 "&scope=" + drogon::utils::urlEncode(scope) + 
                                                 "&state=" + drogon::utils::urlEncode(state) + 
                                                 "&response_type=" + drogon::utils::urlEncode(responseType);
                          auto resp = drogon::HttpResponse::newRedirectionResponse(location);
                          callback(resp);
                      }
                  }
                );
            }
          );
      }
    );
}

void OAuth2StandardController::token(
  const drogon::HttpRequestPtr &req,
  std::function<void(const drogon::HttpResponsePtr &)> &&callback
)
{
    // Use ValidatorHelper for consistent validation
    auto errors = common::validation::ValidatorHelper::validateOAuth2TokenParams(req);

    // Return validation errors if any
    if (
      common::validation::ValidationHelper::returnValidationErrorsIfAny(errors, std::move(callback))
    )
    {
        Metrics::incRequest("token", 400);
        return;
    }

    auto plugin = drogon::app().getPlugin<::OAuth2Plugin>();
    if (!plugin)
    {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k500InternalServerError);
        resp->setBody("OAuth2 Plugin not loaded");
        callback(resp);
        return;
    }

    std::string grantType, code, redirectUri, clientId, clientSecret;
    std::string refreshToken;
    std::string codeVerifier;

    std::string authHeader = req->getHeader("Authorization");
    if (!authHeader.empty() && authHeader.substr(0, 6) == "Basic ")
    {
        LOG_DEBUG << "Token endpoint: Attempting HTTP Basic Authentication";
        try
        {
            std::string decoded = drogon::utils::base64Decode(authHeader.substr(6));
            size_t colonPos = decoded.find(':');
            if (colonPos != std::string::npos)
            {
                clientId = decoded.substr(0, colonPos);
                clientSecret = decoded.substr(colonPos + 1);
                LOG_DEBUG << "Token endpoint: Parsed Basic Auth for client_id=" << clientId;
            }
            else
            {
                LOG_WARN << "Token endpoint: Invalid Basic Auth format (missing colon)";
            }
        }
        catch (const std::exception &e)
        {
            LOG_WARN << "Token endpoint: Base64 decode failed - " << e.what();
        }
    }

    if (clientId.empty() || req->method() == drogon::Post)
    {
        auto params = req->getParameters();
        if (clientId.empty())
            clientId = params["client_id"];
        if (clientSecret.empty())
            clientSecret = params["client_secret"];
        grantType = params["grant_type"];
        code = params["code"];
        redirectUri = params["redirect_uri"];
        refreshToken = params["refresh_token"];
        codeVerifier = params["code_verifier"];
    }
    else
    {
        if (clientId.empty())
            clientId = req->getParameter("client_id");
        if (clientSecret.empty())
            clientSecret = req->getParameter("client_secret");
        grantType = req->getParameter("grant_type");
        code = req->getParameter("code");
        redirectUri = req->getParameter("redirect_uri");
        refreshToken = req->getParameter("refresh_token");
        codeVerifier = req->getParameter("code_verifier");
    }

    if (grantType == "authorization_code")
    {
        plugin->exchangeCodeForToken(
          code,
          clientId,
          clientSecret,
          redirectUri,
          codeVerifier,
          [callback = std::move(callback)](const Json::Value &result) {
              if (result.isMember("error"))
              {
                  auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
                  std::string errorCode = result.get("error", "").asString();
                  drogon::HttpStatusCode statusCode = getHttpStatusCodeForError(errorCode);
                  resp->setStatusCode(statusCode);
                  Metrics::incRequest("token", static_cast<int>(statusCode));
                  callback(resp);
                  return;
              }

              auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
              Metrics::incRequest("token", 200);
              Metrics::updateActiveTokens(1);
              callback(resp);
          }
        );
    }
    else if (grantType == "refresh_token")
    {
        std::string refreshTokenStr = refreshToken;
        plugin->refreshAccessToken(
          refreshTokenStr, clientId, [callback = std::move(callback)](const Json::Value &result) {
              if (result.isMember("error"))
              {
                  auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
                  std::string errorCode = result.get("error", "").asString();
                  drogon::HttpStatusCode statusCode = getHttpStatusCodeForError(errorCode);
                  resp->setStatusCode(statusCode);
                  Metrics::incRequest("token", static_cast<int>(statusCode));
                  callback(resp);
                  return;
              }

              auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
              Metrics::incRequest("token", 200);
              callback(resp);
          }
        );
    }
    else
    {
        Json::Value error;
        error["error"] = "unsupported_grant_type";
        error["error_description"] = "Supported types: authorization_code, refresh_token";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(drogon::k400BadRequest);
        Metrics::incRequest("token", 400);
        callback(resp);
    }
}

void OAuth2StandardController::userInfo(
  const drogon::HttpRequestPtr &req,
  std::function<void(const drogon::HttpResponsePtr &)> &&callback
)
{
    if (req->method() == drogon::Options)
    {
        auto resp = drogon::HttpResponse::newHttpResponse();
        callback(resp);
        return;
    }

    std::string userId;
    auto attrs = req->getAttributes();
    if (!attrs->find("userId"))
    {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k401Unauthorized);
        resp->setBody("User ID not found in request attributes");
        callback(resp);
        return;
    }
    userId = attrs->get<std::string>("userId");

    auto plugin = drogon::app().getPlugin<::OAuth2Plugin>();
    if (plugin)
    {
        plugin->getUserRoles(userId, [userId, callback](std::vector<std::string> roles) {
            Json::Value userInfo;
            userInfo["sub"] = userId;
            if (!roles.empty())
            {
                userInfo["roles"] = Json::Value(Json::arrayValue);
                for (const auto& role : roles)
                {
                    userInfo["roles"].append(role);
                }
            }
            auto resp = drogon::HttpResponse::newHttpJsonResponse(userInfo);
            callback(resp);
        });
    }
    else
    {
        Json::Value userInfo;
        userInfo["sub"] = userId;
        auto resp = drogon::HttpResponse::newHttpJsonResponse(userInfo);
        callback(resp);
    }
}

void OAuth2StandardController::checkUserConsentAndProceed(
  ::OAuth2Plugin *plugin,
  const std::string &clientId,
  const std::string &userId,
  int32_t internalUserId,
  const std::vector<std::string> &requestedScopes,
  const std::string &scope,
  const std::string &redirectUri,
  const std::string &state,
  std::function<void(const drogon::HttpResponsePtr &)> &&callback
)
{
    if (requestedScopes.empty())
    {
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
                  auto resp = drogon::HttpResponse::newHttpJsonResponse(jsonErr);
                  resp->setStatusCode(drogon::k500InternalServerError);
                  callback(resp);
                  return;
              }

              std::string location = redirectUri + "?code=" + code;
              if (!state.empty())
                  location += "&state=" + state;
              auto resp = drogon::HttpResponse::newRedirectionResponse(location);
              Metrics::incRequest("authorize", 302);
              callback(resp);
          }
        );
        return;
    }

    std::string currentScope = requestedScopes[0];
    std::vector<std::string> remainingScopes(requestedScopes.begin() + 1, requestedScopes.end());

    plugin->hasUserConsent(
      internalUserId,
      clientId,
      currentScope,
      [plugin,
       clientId,
       userId,
       internalUserId,
       remainingScopes,
       scope,
       redirectUri,
       state,
       currentScope,
       callback = std::move(callback)](bool hasConsent) mutable {
          if (!hasConsent)
          {
              LOG_INFO << "User " << userId << " hasn't consented to scope " << currentScope
                       << " for client " << clientId;

              auto customConfig = drogon::app().getCustomConfig();
              std::string consentUrl = "/consent";
              if (customConfig.isMember("oauth2") && customConfig["oauth2"].isMember("consent_url")) {
                  consentUrl = customConfig["oauth2"]["consent_url"].asString();
              }
              std::string location = consentUrl + "?client_id=" + drogon::utils::urlEncode(clientId) + 
                                     "&user_id=" + drogon::utils::urlEncode(userId) +
                                     "&scope=" + drogon::utils::urlEncode(scope) + 
                                     "&redirect_uri=" + drogon::utils::urlEncode(redirectUri) + 
                                     "&state=" + drogon::utils::urlEncode(state);
              auto resp = drogon::HttpResponse::newRedirectionResponse(location);
              callback(resp);
              return;
          }

          checkUserConsentAndProceed(
            plugin,
            clientId,
            userId,
            internalUserId,
            remainingScopes,
            scope,
            redirectUri,
            state,
            std::move(callback)
          );
      }
    );
}

} // namespace oauth2::controllers
