#include "OAuth2Controller.h"
#include "../services/AuthService.h"
#include <drogon/drogon.h>
#include "../plugins/OAuth2Metrics.h"
#include <drogon/utils/Utilities.h>
#include <algorithm>
#include "../common/documentation/OpenApiGenerator.h"
#include "../common/validation/ValidatorHelper.h"
#include "../common/validation/ValidationHelper.h"

using namespace oauth2;
using namespace services;
using namespace common::documentation;

// API documentation initialization
namespace
{
struct OAuth2ControllerDocs
{
    OAuth2ControllerDocs()
    {
        // Token endpoint
        OpenApiGenerator::addEndpoint(
            {.path = "/oauth2/token",
             .method = "POST",
             .summary = "Exchange authorization code for access token",
             .description = "OAuth2 token endpoint - exchanges authorization "
                            "code or refresh token for access token",
             .tags = {"OAuth2", "Token"},
             .parameters =
                 {{"grant_type",
                   "Authorization code or refresh token (required)"},
                  {"code",
                   "Authorization code (required for "
                   "grant_type=authorization_code)"},
                  {"refresh_token",
                   "Refresh token (required for grant_type=refresh_token)"},
                  {"client_id", "Client identifier (required)"},
                  {"client_secret",
                   "Client secret (required for confidential clients)"},
                  {"redirect_uri",
                   "Redirect URI (required for authorization_code grant)"}},
             .responses =
                 {{200, "Token response with access_token and refresh_token"},
                  {400, "Invalid request"},
                  {401, "Authentication failed"}},
             .requiresAuth = false});

        // Authorize endpoint
        OpenApiGenerator::addEndpoint(
            {.path = "/oauth2/authorize",
             .method = "GET",
             .summary = "Request authorization",
             .description =
                 "OAuth2 authorization endpoint - initiates authorization flow",
             .tags = {"OAuth2", "Authorization"},
             .parameters = {{"client_id", "Client identifier (required)"},
                            {"redirect_uri", "Redirect URI (required)"},
                            {"response_type",
                             "Response type, must be 'code' (required)"},
                            {"scope", "Requested scope (optional)"},
                            {"state",
                             "Opaque value to maintain state between request "
                             "and callback (recommended)"}},
             .responses = {{302, "Redirect to client with authorization code"},
                           {400, "Invalid request"}},
             .requiresAuth = false});

        // UserInfo endpoint
        OpenApiGenerator::addEndpoint(
            {.path = "/oauth2/userinfo",
             .method = "GET",
             .summary = "Get user information",
             .description = "Returns information about the authenticated user",
             .tags = {"OAuth2", "User"},
             .parameters = {},
             .responses = {{200, "User information"},
                           {401, "Invalid or expired access token"}},
             .requiresAuth = true});

        // Health endpoint
        OpenApiGenerator::addEndpoint(
            {.path = "/health",
             .method = "GET",
             .summary = "Health check",
             .description = "Returns the health status of the service",
             .tags = {"System"},
             .parameters = {},
             .responses = {{200, "Service is healthy"}},
             .requiresAuth = false});

        // Login endpoint
        OpenApiGenerator::addEndpoint(
            {.path = "/oauth2/login",
             .method = "POST",
             .summary = "Authenticate user",
             .description = "Authenticates user credentials and generates "
                            "authorization code",
             .tags = {"OAuth2", "Authentication"},
             .parameters = {{"username", "Username (required)"},
                            {"password", "Password (required)"},
                            {"client_id", "Client identifier (required)"},
                            {"redirect_uri", "Redirect URI (required)"},
                            {"scope", "Requested scope (optional)"},
                            {"state",
                             "Opaque value to maintain state (recommended)"}},
             .responses = {{200, "Authentication successful"},
                           {302, "Redirect with authorization code"},
                           {401, "Authentication failed"}},
             .requiresAuth = false});

        // Register endpoint
        OpenApiGenerator::addEndpoint(
            {.path = "/api/register",
             .method = "POST",
             .summary = "Register new user",
             .description =
                 "Registers a new user account (for testing purposes)",
             .tags = {"User", "Registration"},
             .parameters = {{"username", "Username (required)"},
                            {"password", "Password (required)"},
                            {"email", "Email address (optional)"}},
             .responses = {{200, "User registered successfully"},
                           {400, "Invalid registration data"}},
             .requiresAuth = false});
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
                    auto userId = req->session()->get<std::string>("userId");
                    if (!userId.empty())
                    {
                        // Generate Code (Async)
                        plugin->generateAuthorizationCode(
                            clientId,
                            userId,
                            scope,
                            [=,
                             callback = std::move(callback)](std::string code) {
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

                    // Render Login Page
                    HttpViewData data;
                    data.insert("client_id", clientId);
                    data.insert("redirect_uri", redirectUri);
                    data.insert("scope", scope);
                    data.insert("state", state);
                    data.insert("response_type", responseType);
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
                    std::to_string(*userId),
                    scope,
                    [=, callback = std::move(callback)](std::string code) {
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

    // Prefer POST body (form-urlencoded) - Drogon auto-parses to
    // getParameters()
    if (req->method() == Post)
    {
        auto params = req->getParameters();
        grantType = params["grant_type"];
        code = params["code"];
        redirectUri = params["redirect_uri"];
        clientId = params["client_id"];
        clientSecret = params["client_secret"];
        refreshToken = params["refresh_token"];
    }
    else
    {
        // Fallback to query parameters (not recommended, but for compatibility)
        grantType = req->getParameter("grant_type");
        code = req->getParameter("code");
        redirectUri = req->getParameter("redirect_uri");
        clientId = req->getParameter("client_id");
        clientSecret = req->getParameter("client_secret");
        refreshToken = req->getParameter("refresh_token");
    }

    // Process grant types
    if (grantType == "authorization_code")
    {
        plugin->exchangeCodeForToken(
            code,
            clientId,
            [callback = std::move(callback)](const Json::Value &result) {
                if (result.isMember("error"))
                {
                    auto resp = HttpResponse::newHttpJsonResponse(result);
                    Metrics::incRequest("token", 400);
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
                    Metrics::incRequest("token", 400);
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

    // TODO: Replace with actual user data from database
    // This is placeholder data for demonstration
    // Ideally should query users table and return real email, name, etc.
    Json::Value json;
    json["sub"] = userId;
    json["name"] = userId;
    json["email"] = userId + "@local";

    auto resp = HttpResponse::newHttpJsonResponse(json);
    callback(resp);
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
            json["database"] = "unknown";
        }
    }
    catch (...)
    {
        json["database"] = "disconnected";
    }

    auto resp = HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(k200OK);
    callback(resp);
}
