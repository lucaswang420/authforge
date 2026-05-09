#include "OAuth2Controller.h"
#include "../services/AuthService.h"
#include <drogon/drogon.h>
#include "../plugins/OAuth2Metrics.h"
#include <drogon/utils/Utilities.h>
#include <algorithm>
#include "../common/documentation/OpenApiGenerator.h"
#include "../common/validation/ValidatorHelper.h"
#include "../common/validation/ValidationHelper.h"
#include "../common/types/OAuth2Types.h"

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
        // Token endpoint
        common::documentation::EndpointInfo tokenEndpoint;
        tokenEndpoint.path = "/oauth2/token";
        tokenEndpoint.method = "POST";
        tokenEndpoint.summary = "Exchange authorization code for access token";
        tokenEndpoint.description =
            "OAuth2 token endpoint - exchanges authorization "
            "code or refresh token for access token";
        tokenEndpoint.tags = {"OAuth2", "Token"};
        tokenEndpoint.parameters = {
            {"grant_type",
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
        tokenEndpoint.responses = {
            {200, "Token response with access_token and refresh_token"},
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
        authorizeEndpoint.parameters = {
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
        authorizeEndpoint.responses = {
            {302, "Redirect to client with authorization code"},
            {400, "Invalid request"}};
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
            userInfoEndpoint.responses = {
                {200, "User information retrieved successfully"},
                {400, "Invalid User ID format"},
                {401, "Invalid or expired access token"},
                {404, "User not found"}};
            userInfoEndpoint.responseExamples = {{200, successExample},
                                                 {404, errorExample}};
            userInfoEndpoint.requiresAuth = true;
            OpenApiGenerator::addEndpoint(userInfoEndpoint);
        }

        // Health endpoint
        {
            common::documentation::EndpointInfo healthEndpoint;
            healthEndpoint.path = "/health";
            healthEndpoint.method = "GET";
            healthEndpoint.summary = "Health check";
            healthEndpoint.description =
                "Returns the health status of the service";
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
            loginEndpoint.parameters = {
                {"username",
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
            loginEndpoint.responses = {{200, "Authentication successful"},
                                       {302,
                                        "Redirect with authorization code"},
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
            registerEndpoint.description =
                "Registers a new user account (for testing purposes)";
            registerEndpoint.tags = {"User", "Registration"};
            registerEndpoint.parameters = {
                {"username",
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
            registerEndpoint.responses = {{200, "User registered successfully"},
                                          {400, "Invalid registration data"}};
            registerEndpoint.requiresAuth = false;
            OpenApiGenerator::addEndpoint(registerEndpoint);
        }
    }
};

OAuth2ControllerDocs docs_;
}  // namespace

void OAuth2Controller::errorResponse(
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &message,
    int statusCode)
{
    Json::Value error;
    error["error"] = message;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(static_cast<drogon::HttpStatusCode>(statusCode));
    callback(resp);
}

void OAuth2Controller::authorize(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    // Use ValidatorHelper for consistent validation
    auto errors =
        common::validation::ValidatorHelper::validateOAuth2AuthorizeParams(req);

    // Return validation errors if any
    if (common::validation::ValidationHelper::returnValidationErrorsIfAny(
            errors, std::move(callback)))
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
    auto plugin = drogon::app().getPlugin<OAuth2Plugin>();
    if (!plugin)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k500InternalServerError);
        resp->setBody("OAuth2Plugin not loaded");
        callback(resp);
        return;
    }

    // Validate Client (Async)
    plugin->validateClient(
        clientId,
        "",
        [=, callback = std::move(callback)](bool validClient) mutable {
            if (!validClient)
            {
                Metrics::incRequest("authorize", 400);
                Metrics::incLoginFailure("invalid_client_id");

                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k400BadRequest);
                resp->setBody("Invalid client_id");
                callback(resp);
                return;
            }

            // Validate Redirect URI (Async)
            plugin->validateRedirectUri(
                clientId,
                redirectUri,
                [=, callback = std::move(callback)](bool validUri) mutable {
                    if (!validUri)
                    {
                        auto resp = HttpResponse::newHttpResponse();
                        resp->setStatusCode(k400BadRequest);
                        resp->setBody("Invalid redirect_uri");
                        callback(resp);
                        return;
                    }

                    // Check Session
                    std::string userId;
                    if (req->session())
                    {
                        userId = req->session()->get<std::string>("userId");
                    }
                    if (!userId.empty())
                    {
                        // P0-2: Check user consent for requested scopes
                        // Parse requested scopes
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

                        // Get internal user ID for consent checking
                        plugin->getInternalUserId(
                            userId,
                            [=, callback = std::move(callback)](
                                std::optional<int32_t> internalUserId) mutable {
                                if (!internalUserId)
                                {
                                    // User mapping not found, this might be a first-time login
                                    // For now, we'll proceed without consent checking for unmapped users
                                    // In production, you would want to handle this differently
                                    LOG_WARN
                                        << "No internal user ID found for subject: "
                                        << userId << ", proceeding without consent check";

                                    // Proceed with authorization code generation
                                    plugin->generateAuthorizationCode(
                                        clientId,
                                        userId,
                                        scope,
                                        redirectUri,
                                        "",  // codeChallenge (empty for now)
                                        "",  // codeChallengeMethod (empty for now)
                                        [=,
                                         callback =
                                             std::move(callback)](bool success,
                                                                  std::string code,
                                                                  std::string error) {
                                            if (!success)
                                            {
                                                LOG_ERROR
                                                    << "Failed to generate authorization code: "
                                                    << error;
                                                Json::Value jsonErr;
                                                jsonErr["error"] = "server_error";
                                                jsonErr["error_description"] =
                                                    "Failed to generate authorization code";
                                                auto resp =
                                                    HttpResponse::newHttpJsonResponse(
                                                        jsonErr);
                                                resp->setStatusCode(
                                                    k500InternalServerError);
                                                callback(resp);
                                                return;
                                            }

                                            std::string location =
                                                redirectUri + "?code=" + code;
                                            if (!state.empty())
                                                location += "&state=" + state;
                                            auto resp =
                                                HttpResponse::newRedirectionResponse(
                                                    location);
                                            Metrics::incRequest("authorize", 302);
                                            callback(resp);
                                        });
                                    return;
                                }

                                // Check consent for all requested scopes
                                checkUserConsentAndProceed(
                                    plugin,
                                    clientId,
                                    userId,
                                    *internalUserId,
                                    requestedScopes,
                                    scope,
                                    redirectUri,
                                    state,
                                    std::move(callback));
                            });
                        return;
                    }

                    // Render Login Page
                    HttpViewData data;
                    data.insert("client_id", clientId);
                    data.insert("redirect_uri", redirectUri);
                    data.insert("scope", scope);
                    data.insert("state", state);
                    data.insert("response_type", responseType);

                    // Get frontend configuration for register link
                    auto customConfig = drogon::app().getCustomConfig();
                    std::string frontendUrl = "http://localhost:5173";
                    std::string registerPath = "/register";

                    if (customConfig.isMember("frontend") &&
                        customConfig["frontend"].isMember("url") &&
                        customConfig["frontend"]["url"].isString())
                    {
                        frontendUrl =
                            customConfig["frontend"]["url"].asString();
                    }

                    if (customConfig.isMember("frontend") &&
                        customConfig["frontend"].isMember("register_path") &&
                        customConfig["frontend"]["register_path"].isString())
                    {
                        registerPath = customConfig["frontend"]["register_path"]
                                           .asString();
                    }

                    std::string frontendRegisterUrl =
                        frontendUrl + registerPath;
                    data.insert("frontend_register_url", frontendRegisterUrl);

                    auto resp =
                        HttpResponse::newHttpViewResponse("login.csp", data);
                    callback(resp);
                });
        });
}

void OAuth2Controller::login(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    // Use ValidatorHelper for consistent validation
    auto errors = common::validation::ValidatorHelper::validateLoginParams(req);

    // Return validation errors if any
    if (common::validation::ValidationHelper::returnValidationErrorsIfAny(
            errors, std::move(callback)))
    {
        Metrics::incLoginFailure("validation_failed");
        return;
    }

    // Prefer POST body (JSON or form data) over URL parameters for security
    std::string username, password;
    std::string clientId, redirectUri, scope, state;

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
    }

    AuthService::validateUser(
        username,
        password,
        [=, callback = std::move(callback)](std::optional<int> userId) {
            if (userId)
            {
                req->session()->insert("userId", std::to_string(*userId));
                auto plugin = drogon::app().getPlugin<OAuth2Plugin>();
                if (!plugin)
                {
                    LOG_ERROR << "OAuth2Plugin not loaded during login";
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
                    redirectUri,  // CRITICAL: Pass redirect_uri for RFC 6749
                                  // Section 4.1.3 validation
                    "",           // codeChallenge (empty for now)
                    "",           // codeChallengeMethod (empty for now)
                    [=, callback = std::move(callback)](bool success,
                                                        std::string code,
                                                        std::string error) {
                        if (!success)
                        {
                            LOG_ERROR
                                << "Failed to generate authorization code: "
                                << error;
                            Json::Value jsonErr;
                            jsonErr["error"] = "server_error";
                            jsonErr["error_description"] =
                                "Failed to generate authorization code";
                            auto resp =
                                HttpResponse::newHttpJsonResponse(jsonErr);
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
                        auto resp =
                            HttpResponse::newRedirectionResponse(location);
                        callback(resp);
                    });
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
        });
}

void OAuth2Controller::registerUser(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    // Use ValidatorHelper for consistent validation
    auto errors = common::validation::ValidatorHelper::validateLoginParams(req);

    // Return validation errors if any
    if (common::validation::ValidationHelper::returnValidationErrorsIfAny(
            errors, std::move(callback)))
    {
        return;
    }

    // Extract parameters (validation ensures they exist and are valid)
    auto params = req->getParameters();
    std::string username = params["username"];
    std::string password = params["password"];
    std::string email = params["email"];

    AuthService::registerUser(
        username, password, email, [callback](const std::string &error) {
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

void OAuth2Controller::token(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    // Use ValidatorHelper for consistent validation
    auto errors =
        common::validation::ValidatorHelper::validateOAuth2TokenParams(req);

    // Return validation errors if any
    if (common::validation::ValidationHelper::returnValidationErrorsIfAny(
            errors, std::move(callback)))
    {
        Metrics::incRequest("token", 400);
        return;
    }

    auto plugin = drogon::app().getPlugin<OAuth2Plugin>();
    if (!plugin)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k500InternalServerError);
        resp->setBody("OAuth2Plugin not loaded");
        callback(resp);
        return;
    }

    // OAuth2 spec requires POST body (form-urlencoded) for token endpoint
    // Do NOT use URL parameters for sensitive data like client_secret
    std::string grantType, code, redirectUri, clientId, clientSecret;
    std::string refreshToken;

    // Try HTTP Basic Authentication first (RFC 6749 Section 2.3.1)
    std::string authHeader = req->getHeader("Authorization");
    if (!authHeader.empty() && authHeader.substr(0, 6) == "Basic ")
    {
        LOG_DEBUG << "Token endpoint: Attempting HTTP Basic Authentication";
        try
        {
            std::string decoded =
                drogon::utils::base64Decode(authHeader.substr(6));
            size_t colonPos = decoded.find(':');
            if (colonPos != std::string::npos)
            {
                clientId = decoded.substr(0, colonPos);
                clientSecret = decoded.substr(colonPos + 1);
                LOG_DEBUG << "Token endpoint: Parsed Basic Auth for client_id="
                          << clientId;
            }
            else
            {
                LOG_WARN << "Token endpoint: Invalid Basic Auth format "
                            "(missing colon)";
            }
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Token endpoint: Base64 decode failed - " << e.what();
        }
    }

    // Fallback to POST body (form-urlencoded) if Basic Auth not provided or
    // incomplete
    if (clientId.empty() || req->method() == Post)
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
    }
    else
    {
        // Fallback to query parameters (not recommended, but for compatibility)
        if (clientId.empty())
            clientId = req->getParameter("client_id");
        if (clientSecret.empty())
            clientSecret = req->getParameter("client_secret");
        grantType = req->getParameter("grant_type");
        code = req->getParameter("code");
        redirectUri = req->getParameter("redirect_uri");
        refreshToken = req->getParameter("refresh_token");
    }

    // Process grant types
    if (grantType == "authorization_code")
    {
        plugin->exchangeCodeForToken(
            code,
            clientId,
            clientSecret,  // CRITICAL: Pass client_secret for validation
            redirectUri,   // CRITICAL: Pass redirect_uri for validation per RFC
                           // 6749 Section 4.1.3
            [callback = std::move(callback)](const Json::Value &result) {
                if (result.isMember("error"))
                {
                    auto resp = HttpResponse::newHttpJsonResponse(result);
                    // CRITICAL: Use correct HTTP status code
                    std::string errorCode = result.get("error", "").asString();
                    drogon::HttpStatusCode statusCode =
                        getHttpStatusCodeForError(errorCode);
                    resp->setStatusCode(statusCode);
                    Metrics::incRequest("token", static_cast<int>(statusCode));
                    callback(resp);
                    return;
                }

                auto resp = HttpResponse::newHttpJsonResponse(result);
                Metrics::incRequest("token", 200);
                Metrics::updateActiveTokens(1);
                callback(resp);
            });
    }
    else if (grantType == "refresh_token")
    {
        std::string refreshTokenStr = req->getParameter("refresh_token");
        plugin->refreshAccessToken(
            refreshTokenStr,
            clientId,
            [callback = std::move(callback)](const Json::Value &result) {
                if (result.isMember("error"))
                {
                    auto resp = HttpResponse::newHttpJsonResponse(result);
                    // CRITICAL: Use correct HTTP status code
                    std::string errorCode = result.get("error", "").asString();
                    drogon::HttpStatusCode statusCode =
                        getHttpStatusCodeForError(errorCode);
                    resp->setStatusCode(statusCode);
                    Metrics::incRequest("token", static_cast<int>(statusCode));
                    callback(resp);
                    return;
                }

                auto resp = HttpResponse::newHttpJsonResponse(result);
                Metrics::incRequest("token", 200);
                callback(resp);
            });
    }
    else
    {
        Json::Value error;
        error["error"] = "unsupported_grant_type";
        error["error_description"] =
            "Supported types: authorization_code, refresh_token";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k400BadRequest);
        Metrics::incRequest("token", 400);
        callback(resp);
    }
}

void OAuth2Controller::userInfo(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    if (req->method() == Options)
    {
        auto resp = HttpResponse::newHttpResponse();
        callback(resp);
        return;
    }

    // This endpoint is protected by OAuth2Middleware.
    // If we are here, we have a valid token.

    // Attributes set by OAuth2Middleware
    std::string userId;
    auto attrs = req->getAttributes();
    if (!attrs->find("userId"))
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k401Unauthorized);
        resp->setBody("User ID not found in request attributes");
        callback(resp);
        return;
    }
    userId = attrs->get<std::string>("userId");

    int uid = -1;
    try
    {
        uid = std::stoi(userId);
    }
    catch (...)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Invalid User ID format");
        callback(resp);
        return;
    }

    AuthService::getUserInfo(
        uid, [callback](std::optional<Json::Value> userInfo) {
            if (userInfo)
            {
                auto resp = HttpResponse::newHttpJsonResponse(*userInfo);
                callback(resp);
            }
            else
            {
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k404NotFound);
                resp->setBody("User not found");
                callback(resp);
            }
        });
}

void OAuth2Controller::logout(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    // This endpoint is protected by OAuth2Middleware
    // The middleware validates the access token and sets userId in request
    // attributes

    std::string userId;
    auto attrs = req->getAttributes();
    if (!attrs->find("userId"))
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k401Unauthorized);
        resp->setBody("User ID not found in request attributes");
        callback(resp);
        return;
    }
    userId = attrs->get<std::string>("userId");

    // Get the access token from the Authorization header
    std::string authHeader = req->getHeader("Authorization");
    if (authHeader.empty() || authHeader.substr(0, 7) != "Bearer ")
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k401Unauthorized);
        resp->setBody("Invalid or missing Authorization header");
        callback(resp);
        return;
    }
    std::string accessToken = authHeader.substr(7);

    // Revoke the access token
    auto plugin = drogon::app().getPlugin<OAuth2Plugin>();
    if (!plugin)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k500InternalServerError);
        resp->setBody("OAuth2Plugin not loaded");
        callback(resp);
        return;
    }

    auto storage = plugin->getStorage();
    if (!storage)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k500InternalServerError);
        resp->setBody("Storage not available");
        callback(resp);
        return;
    }

    // Mark token as revoked
    storage->getAccessToken(
        accessToken,
        [storage, userId, callback, req](
            const std::optional<oauth2::OAuth2AccessToken> &token) {
            if (!token)
            {
                // Token not found (already expired or invalid)
                // CRITICAL: Clear session anyway to ensure complete logout
                req->session()->erase("userId");
                req->session()->clear();

                // Still return success for idempotency
                Json::Value json;
                json["message"] = "Logged out successfully";
                json["userId"] = userId;
                auto resp = HttpResponse::newHttpJsonResponse(json);
                resp->setStatusCode(k200OK);
                callback(resp);
                return;
            }

            // Verify token belongs to the user
            if (token->userId != userId)
            {
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k403Forbidden);
                resp->setBody("Token does not belong to this user");
                callback(resp);
                return;
            }

            // Revoke the token
            oauth2::OAuth2AccessToken revokedToken = *token;
            revokedToken.revoked = true;

            storage->saveAccessToken(revokedToken, [userId, callback, req]() {
                if (req->session())
                {
                    req->session()->erase("userId");
                    req->session()->clear();
                }

                Json::Value json;
                json["message"] = "Logged out successfully";
                json["userId"] = userId;
                auto resp = HttpResponse::newHttpJsonResponse(json);
                resp->setStatusCode(k200OK);
                callback(resp);
            });
        });
}

void OAuth2Controller::health(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    // Health check endpoint for monitoring/orchestration systems
    // Returns 200 OK if service is healthy
    Json::Value json;
    json["status"] = "ok";
    json["service"] = "OAuth2Server";
    json["timestamp"] = static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
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

// P0-2: Helper function to check user consent and proceed with authorization
void OAuth2Controller::checkUserConsentAndProceed(
    OAuth2Plugin *plugin,
    const std::string &clientId,
    const std::string &userId,
    int32_t internalUserId,
    const std::vector<std::string> &requestedScopes,
    const std::string &scope,
    const std::string &redirectUri,
    const std::string &state,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    if (requestedScopes.empty())
    {
        // No scopes requested, proceed with authorization
        plugin->generateAuthorizationCode(
            clientId,
            userId,
            scope,
            redirectUri,
            "",  // codeChallenge (empty for now)
            "",  // codeChallengeMethod (empty for now)
            [=, callback = std::move(callback)](bool success,
                                                std::string code,
                                                std::string error) {
                if (!success)
                {
                    LOG_ERROR << "Failed to generate authorization code: "
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
            });
        return;
    }

    // Check consent for the first scope, then recursively check the rest
    std::string currentScope = requestedScopes[0];
    std::vector<std::string> remainingScopes(requestedScopes.begin() + 1,
                                             requestedScopes.end());

    plugin->hasUserConsent(
        internalUserId,
        clientId,
        currentScope,
        [=, callback = std::move(callback)](bool hasConsent) mutable {
            if (!hasConsent)
            {
                // User hasn't consented to this scope, redirect to consent page
                LOG_INFO << "User " << userId << " hasn't consented to scope "
                         << currentScope << " for client " << clientId;

                // Store authorization request parameters in session for consent
                // approval
                auto req = HttpRequest::newHttpRequest();
                // Note: In a real implementation, you would store these in the
                // user session or a temporary transaction

                HttpViewData data;
                data.insert("client_id", clientId);
                data.insert("user_id", userId);
                data.insert("requested_scope", scope);
                data.insert("redirect_uri", redirectUri);
                data.insert("state", state);

                auto resp = HttpResponse::newHttpViewResponse("consent.csp",
                                                              data);
                callback(resp);
                return;
            }

            // User has consented to this scope, check the remaining scopes
            checkUserConsentAndProceed(plugin,
                                       clientId,
                                       userId,
                                       internalUserId,
                                       remainingScopes,
                                       scope,
                                       redirectUri,
                                       state,
                                       std::move(callback));
        });
}

void OAuth2Controller::consent(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
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
        resp->setBody("OAuth2Plugin not loaded");
        callback(resp);
        return;
    }

    // Get internal user ID
    plugin->getInternalUserId(
        userId,
        [=, callback = std::move(callback)](
            std::optional<int32_t> internalUserId) mutable {
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
                plugin->saveUserConsent(
                    *internalUserId,
                    clientId,
                    firstScope,
                    [=, callback = std::move(callback)](bool success) mutable {
                        if (!success)
                        {
                            LOG_ERROR << "Failed to save user consent for scope: "
                                      << firstScope;
                            auto resp = HttpResponse::newHttpResponse();
                            resp->setStatusCode(k500InternalServerError);
                            resp->setBody("Failed to save consent");
                            callback(resp);
                            return;
                        }

                        // Save consent for remaining scopes (fire and forget for
                        // simplicity)
                        for (size_t i = 1; i < scopes.size(); ++i)
                        {
                            plugin->saveUserConsent(
                                *internalUserId, clientId, scopes[i], [](bool) {
                                });
                        }

                        // Proceed with authorization code generation
                        plugin->generateAuthorizationCode(
                            clientId,
                            userId,
                            scope,
                            redirectUri,
                            "",  // codeChallenge (empty for now)
                            "",  // codeChallengeMethod (empty for now)
                            [=, callback = std::move(callback)](
                                bool success,
                                std::string code,
                                std::string error) mutable {
                                if (!success)
                                {
                                    LOG_ERROR
                                        << "Failed to generate authorization code: "
                                        << error;
                                    Json::Value jsonErr;
                                    jsonErr["error"] = "server_error";
                                    jsonErr["error_description"] =
                                        "Failed to generate authorization code";
                                    auto resp =
                                        HttpResponse::newHttpJsonResponse(jsonErr);
                                    resp->setStatusCode(k500InternalServerError);
                                    callback(resp);
                                    return;
                                }

                                std::string location =
                                    redirectUri + "?code=" + code;
                                if (!state.empty())
                                    location += "&state=" + state;
                                auto resp =
                                    HttpResponse::newRedirectionResponse(location);
                                Metrics::incRequest("authorize", 302);
                                callback(resp);
                            });
                    });
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
                    [=, callback = std::move(callback)](bool success,
                                                        std::string code,
                                                        std::string error) {
                        if (!success)
                        {
                            LOG_ERROR << "Failed to generate authorization code: "
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
                    });
            }
        });
}
