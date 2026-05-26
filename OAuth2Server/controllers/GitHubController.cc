#include "GitHubController.h"
#include <drogon/HttpClient.h>
#include <oauth2/OpenApiGenerator.h>

static std::string getGitHubConfig(const std::string &key)
{
    auto config = drogon::app().getCustomConfig();
    if (config.isMember("external_auth") && config["external_auth"].isMember("github"))
    {
        return config["external_auth"]["github"].get(key, "").asString();
    }
    return "";
}

namespace {
struct GitHubControllerDocs {
    GitHubControllerDocs() {
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

        ep.responses = {
          {200, "GitHub user info retrieved successfully"},
          {400, "Invalid request (missing or invalid code)"},
          {502, "Failed to contact GitHub API"}
        };

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

    client->sendRequest(
      request, [callbackPtr](ReqResult result, const HttpResponsePtr &response) {
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

          apiClient->sendRequest(
            userReq, [callbackPtr](ReqResult res2, const HttpResponsePtr &resp2) {
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
                Json::Value result;
                result["id"] = (*githubData).get("id", 0).asInt64();
                result["login"] = (*githubData).get("login", "").asString();
                result["name"] = (*githubData).get("name", "").asString();
                result["email"] = (*githubData).get("email", "").asString();
                result["avatar_url"] = (*githubData).get("avatar_url", "").asString();

                (*callbackPtr)(HttpResponse::newHttpJsonResponse(result));
            }
          );
      }
    );
}
