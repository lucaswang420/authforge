#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

class OAuth2Controller : public drogon::HttpController<OAuth2Controller>
{
  public:
    static void initApiDocs();

    METHOD_LIST_BEGIN
    // Login Form Submission (Internal)
    ADD_METHOD_TO(OAuth2Controller::login, "/oauth2/login", Post);

    // Register User (Helper for testing)
    // POST /api/register
    ADD_METHOD_TO(OAuth2Controller::registerUser, "/api/register", Post);

    // Consent Approval Endpoint (Internal)
    // POST /oauth2/consent
    ADD_METHOD_TO(OAuth2Controller::consent, "/oauth2/consent", Post);

    // Logout Endpoint (Protected)
    // POST /oauth2/logout
    ADD_METHOD_TO(OAuth2Controller::logout, "/oauth2/logout", Post, "oauth2::filters::OAuth2Middleware");

    // Health Check Endpoint (for monitoring/orchestration)
    // GET /health
    ADD_METHOD_TO(OAuth2Controller::health, "/health", Get);
    METHOD_LIST_END

    void login(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);

    void registerUser(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void consent(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void logout(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);

    void health(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
};
