#include "GitHubController.h"
#include <drogon/HttpClient.h>
#include <drogon/drogon.h>
#include <oauth2/OpenApiGenerator.h>
#include <oauth2/OAuth2Plugin.h>
#include <oauth2/CryptoUtils.h>

static std::string getGitHubConfig(const std::string &key)
{
    auto config = drogon::app().getCustomConfig();
    if (config.isMember("external_auth") && config["external_auth"].isMember("github"))
    {
        return config["external_auth"]["github"].get(key, "").asString();
    }
    return "";
}

namespace
{
struct GitHubControllerDocs
{
    GitHubControllerDocs()
    {
        common::documentation::EndpointInfo ep;
        ep.path = "/api/github/login";
        ep.method = "POST";
        ep.summary = "GitHub OAuth2 Login";
        ep.description = "Exchange GitHub authorization code for user information.";
        ep.tags = {"External Auth", "GitHub"};
        ep.requiresAuth = false;

        common::documentation::ParameterInfo codeParam;
        codeParam.name = "code";
        codeParam.description = "Authorization code from GitHub OAuth2 callback";
        codeParam.type = common::documentation::ParameterType::STRING;
        codeParam.location = common::documentation::ParameterLocation::QUERY;
        codeParam.required = true;
        ep.parameters = {codeParam};

        ep.responses =
          {{200, "GitHub user info retrieved successfully"},
           {400, "Invalid request (missing or invalid code)"},
           {502, "Failed to contact GitHub API"}};

        common::documentation::OpenApiGenerator::addEndpoint(ep);
    }
};

GitHubControllerDocs docs_;
}  // namespace

void GitHubController::login(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    if (req->method() == Options)
    {
        callback(HttpResponse::newHttpResponse());
        return;
    }

    // Extract code from POST body or query
    std::string code;
    auto jsonBody = req->getJsonObject();
    if (jsonBody && jsonBody->isMember("code"))
    {
        code = (*jsonBody)["code"].asString();
    }
    if (code.empty())
    {
        code = req->getParameter("code");
    }

    if (code.empty())
    {
        Json::Value err;
        err["error"] = "Missing code parameter";
        auto resp = HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    std::string clientId = getGitHubConfig("client_id");
    std::string clientSecret = getGitHubConfig("client_secret");

    if (clientId.empty() || clientSecret.empty())
    {
        Json::Value err;
        err["error"] = "GitHub OAuth not configured";
        auto resp = HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
        return;
    }

    // Step 1: Exchange code for access token
    auto client = HttpClient::newHttpClient("https://github.com");
    auto request = HttpRequest::newHttpRequest();
    request->setMethod(Post);
    request->setPath("/login/oauth/access_token");
    request->addHeader("Accept", "application/json");
    request->setParameter("client_id", clientId);
    request->setParameter("client_secret", clientSecret);
    request->setParameter("code", code);

    auto callbackPtr =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    client->sendRequest(request, [callbackPtr](ReqResult result, const HttpResponsePtr &response) {
        if (result != ReqResult::Ok || !response || response->getStatusCode() != k200OK)
        {
            Json::Value err;
            err["error"] = "Failed to contact GitHub Token API";
            auto resp = HttpResponse::newHttpJsonResponse(err);
            resp->setStatusCode(k502BadGateway);
            (*callbackPtr)(resp);
            return;
        }

        auto json = response->getJsonObject();
        if (!json || !json->isMember("access_token"))
        {
            Json::Value err;
            err["error"] = "GitHub returned invalid token response";
            if (json && json->isMember("error_description"))
                err["detail"] = (*json)["error_description"].asString();
            auto resp = HttpResponse::newHttpJsonResponse(err);
            resp->setStatusCode(k400BadRequest);
            (*callbackPtr)(resp);
            return;
        }

        std::string accessToken = (*json)["access_token"].asString();

        // Step 2: Fetch user info from GitHub API
        auto apiClient = HttpClient::newHttpClient("https://api.github.com");
        auto userReq = HttpRequest::newHttpRequest();
        userReq->setPath("/user");
        userReq->addHeader("Authorization", "Bearer " + accessToken);
        userReq->addHeader("User-Agent", "OAuth2Server");
        userReq->addHeader("Accept", "application/json");

        apiClient
          ->sendRequest(userReq, [callbackPtr](ReqResult res2, const HttpResponsePtr &resp2) {
              if (res2 != ReqResult::Ok || !resp2 || resp2->getStatusCode() != k200OK)
              {
                  Json::Value err;
                  err["error"] = "Failed to fetch GitHub user info";
                  auto resp = HttpResponse::newHttpJsonResponse(err);
                  resp->setStatusCode(k502BadGateway);
                  (*callbackPtr)(resp);
                  return;
              }

              auto githubData = resp2->getJsonObject();
              std::string githubLogin = (*githubData).get("login", "").asString();
              std::string githubEmail = (*githubData).get("email", "").asString();
              int64_t githubId = (*githubData).get("id", 0).asInt64();

              if (githubLogin.empty())
              {
                  Json::Value err;
                  err["error"] = "GitHub returned no user login";
                  auto resp = HttpResponse::newHttpJsonResponse(err);
                  resp->setStatusCode(k400BadRequest);
                  (*callbackPtr)(resp);
                  return;
              }

              // Step 3: Find or create local user linked to this GitHub account
              auto db = drogon::app().getDbClient();
              std::string provider = "github";
              std::string subject = std::to_string(githubId);

              // Check if this GitHub account is already linked
              db->execSqlAsync(
                "SELECT internal_user_id FROM oauth2_subject_mappings "
                "WHERE provider = $1 AND subject = $2",
                [callbackPtr, db, githubLogin, githubEmail, provider, subject](
                  const drogon::orm::Result &mappingResult
                ) {
                    auto issueTokens = [callbackPtr](int userId, const std::string &username) {
                        // Issue access_token and refresh_token
                        auto plugin = drogon::app().getPlugin<OAuth2Plugin>();
                        if (!plugin)
                        {
                            Json::Value err;
                            err["error"] = "OAuth2Plugin not available";
                            auto resp = HttpResponse::newHttpJsonResponse(err);
                            resp->setStatusCode(k500InternalServerError);
                            (*callbackPtr)(resp);
                            return;
                        }

                        std::string accessToken = oauth2::utils::generateSecureToken();
                        std::string refreshToken = oauth2::utils::generateSecureToken();
                        auto now = std::chrono::duration_cast<std::chrono::seconds>(
                                     std::chrono::system_clock::now().time_since_epoch()
                        )
                                     .count();

                        auto db2 = drogon::app().getDbClient();
                        db2->execSqlAsync(
                          "INSERT INTO oauth2_access_tokens (token, client_id, user_id, scope, "
                          "issued_at, expires_at) VALUES ($1, $2, $3, $4, $5, $6)",
                          [callbackPtr, accessToken, refreshToken, db2, userId](
                            const drogon::orm::Result &
                          ) {
                              // Also insert refresh token
                              auto now2 = std::chrono::duration_cast<std::chrono::seconds>(
                                            std::chrono::system_clock::now().time_since_epoch()
                              )
                                            .count();
                              db2->execSqlAsync(
                                "INSERT INTO oauth2_refresh_tokens (token, access_token, "
                                "client_id, user_id, scope, expires_at) "
                                "VALUES ($1, $2, $3, $4, $5, $6)",
                                [callbackPtr,
                                 accessToken,
                                 refreshToken](const drogon::orm::Result &) {
                                    Json::Value result;
                                    result["access_token"] = accessToken;
                                    result["refresh_token"] = refreshToken;
                                    result["token_type"] = "Bearer";
                                    result["expires_in"] = 3600;
                                    (*callbackPtr)(HttpResponse::newHttpJsonResponse(result));
                                },
                                [callbackPtr](const drogon::orm::DrogonDbException &) {
                                    Json::Value err;
                                    err["error"] = "Failed to create refresh token";
                                    auto resp = HttpResponse::newHttpJsonResponse(err);
                                    resp->setStatusCode(k500InternalServerError);
                                    (*callbackPtr)(resp);
                                },
                                refreshToken,
                                accessToken,
                                "vue-client",
                                std::to_string(userId),
                                "openid profile email",
                                now2 + 2592000  // 30 days
                              );
                          },
                          [callbackPtr](const drogon::orm::DrogonDbException &e) {
                              Json::Value err;
                              err["error"] = "Failed to create access token";
                              err["detail"] = e.base().what();
                              auto resp = HttpResponse::newHttpJsonResponse(err);
                              resp->setStatusCode(k500InternalServerError);
                              (*callbackPtr)(resp);
                          },
                          accessToken,
                          "vue-client",
                          std::to_string(userId),
                          "openid profile email",
                          now,
                          now + 3600  // 1 hour
                        );
                    };

                    if (!mappingResult.empty())
                    {
                        // Existing linked account — issue tokens
                        int userId = mappingResult[0]["internal_user_id"].as<int>();
                        // Get username
                        db->execSqlAsync(
                          "SELECT username FROM users WHERE id = $1",
                          [callbackPtr, issueTokens, userId](const drogon::orm::Result &r) {
                              std::string username =
                                r.empty() ? "user" : r[0]["username"].as<std::string>();
                              issueTokens(userId, username);
                          },
                          [callbackPtr](const drogon::orm::DrogonDbException &) {
                              Json::Value err;
                              err["error"] = "Failed to fetch user";
                              auto resp = HttpResponse::newHttpJsonResponse(err);
                              resp->setStatusCode(k500InternalServerError);
                              (*callbackPtr)(resp);
                          },
                          userId
                        );
                    }
                    else
                    {
                        // New GitHub user — create local account + link
                        std::string username = "gh_" + githubLogin;
                        std::string passwordHash =
                          oauth2::utils::generateSecureToken();  // random, user can't login with
                                                                 // password
                        db->execSqlAsync(
                          "INSERT INTO users (username, password_hash, salt, email, "
                          "email_verified) "
                          "VALUES ($1, $2, '', $3, true) "
                          "ON CONFLICT (username) DO UPDATE SET email = EXCLUDED.email, "
                          "email_verified = true "
                          "RETURNING id",
                          [callbackPtr, db, issueTokens, provider, subject, username](
                            const drogon::orm::Result &userResult
                          ) {
                              int userId = userResult[0]["id"].as<int>();
                              // Create subject mapping
                              db->execSqlAsync(
                                "INSERT INTO oauth2_subject_mappings (subject, internal_user_id, "
                                "provider) "
                                "VALUES ($1, $2, $3) ON CONFLICT DO NOTHING",
                                [callbackPtr, issueTokens, userId, username](
                                  const drogon::orm::Result &
                                ) {
                                    // Assign default 'user' role
                                    auto db3 = drogon::app().getDbClient();
                                    db3->execSqlAsync(
                                      "INSERT INTO user_roles (user_id, role_id) "
                                      "SELECT $1, id FROM roles WHERE name = 'user' "
                                      "ON CONFLICT DO NOTHING",
                                      [issueTokens, userId, username](const drogon::orm::Result &) {
                                          issueTokens(userId, username);
                                      },
                                      [issueTokens,
                                       userId,
                                       username](const drogon::orm::DrogonDbException &) {
                                          issueTokens(
                                            userId, username
                                          );  // still issue tokens even if role assignment fails
                                      },
                                      userId
                                    );
                                },
                                [callbackPtr](const drogon::orm::DrogonDbException &) {
                                    Json::Value err;
                                    err["error"] = "Failed to link GitHub account";
                                    auto resp = HttpResponse::newHttpJsonResponse(err);
                                    resp->setStatusCode(k500InternalServerError);
                                    (*callbackPtr)(resp);
                                },
                                subject,
                                userId,
                                provider
                              );
                          },
                          [callbackPtr](const drogon::orm::DrogonDbException &e) {
                              Json::Value err;
                              err["error"] = "Failed to create user account";
                              err["detail"] = e.base().what();
                              auto resp = HttpResponse::newHttpJsonResponse(err);
                              resp->setStatusCode(k500InternalServerError);
                              (*callbackPtr)(resp);
                          },
                          username,
                          passwordHash,
                          githubEmail
                        );
                    }
                },
                [callbackPtr](const drogon::orm::DrogonDbException &e) {
                    Json::Value err;
                    err["error"] = "Database error during account linking";
                    err["detail"] = e.base().what();
                    auto resp = HttpResponse::newHttpJsonResponse(err);
                    resp->setStatusCode(k500InternalServerError);
                    (*callbackPtr)(resp);
                },
                provider,
                subject
              );
          });
    });
}
