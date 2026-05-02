#pragma once

#include <drogon/HttpController.h>
#include "../plugins/OAuth2Plugin.h"
#include "common/validation/Validator.h"
#include "common/error/ErrorHandler.h"

using namespace drogon;

class OAuth2Controller : public drogon::HttpController<OAuth2Controller>
{
  public:
    static void initApiDocs();

    METHOD_LIST_BEGIN
    // Authorization Endpoint
    // GET /oauth2/authorize
    ADD_METHOD_TO(OAuth2Controller::authorize, "/oauth2/authorize", Get);

    // Token Endpoint
    // POST /oauth2/token
    ADD_METHOD_TO(OAuth2Controller::token, "/oauth2/token", Post);

    // UserInfo Endpoint (Protected)
    // GET /oauth2/userinfo
    ADD_METHOD_TO(OAuth2Controller::userInfo,
                  "/oauth2/userinfo",
                  Get,
                  Options,
                  "OAuth2Middleware");

    // Login Form Submission (Internal)
    ADD_METHOD_TO(OAuth2Controller::login, "/oauth2/login", Post);

    // Register User (Helper for testing)
    // POST /api/register
    ADD_METHOD_TO(OAuth2Controller::registerUser, "/api/register", Post);

    // Logout Endpoint (Protected)
    // POST /oauth2/logout
    ADD_METHOD_TO(OAuth2Controller::logout,
                  "/oauth2/logout",
                  Post,
                  "OAuth2Middleware");

    // Health Check Endpoint (for monitoring/orchestration)
    // GET /health
    ADD_METHOD_TO(OAuth2Controller::health, "/health", Get);

    METHOD_LIST_END

    void authorize(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);

    void login(const HttpRequestPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback);

    void token(const HttpRequestPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback);

    void userInfo(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);

    void registerUser(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);

    void logout(const HttpRequestPtr &req,
                std::function<void(const HttpResponsePtr &)> &&callback);

    void health(const HttpRequestPtr &req,
                std::function<void(const HttpResponsePtr &)> &&callback);

  private:
    void errorResponse(std::function<void(const HttpResponsePtr &)> &&callback,
                       const std::string &message,
                       int statusCode = 400);
};
