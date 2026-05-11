#pragma once

#include <drogon/HttpController.h>
#include "../plugins/OAuth2Plugin.h"
#include "common/validation/Validator.h"
#include "common/error/ErrorHandler.h"
#include "common/error/OAuth2ErrorHandler.h"

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
    ADD_METHOD_TO(OAuth2Controller::userInfo, "/oauth2/userinfo", Get, Options, "OAuth2Middleware");

    // Login Form Submission (Internal)
    ADD_METHOD_TO(OAuth2Controller::login, "/oauth2/login", Post);

    // Register User (Helper for testing)
    // POST /api/register
    ADD_METHOD_TO(OAuth2Controller::registerUser, "/api/register", Post);

    // Logout Endpoint (Protected)
    // POST /oauth2/logout
    ADD_METHOD_TO(OAuth2Controller::logout, "/oauth2/logout", Post, "OAuth2Middleware");

    // Consent Approval Endpoint (Internal)
    // POST /oauth2/consent
    ADD_METHOD_TO(OAuth2Controller::consent, "/oauth2/consent", Post);

    // Health Check Endpoint (for monitoring/orchestration)
    // GET /health
    ADD_METHOD_TO(OAuth2Controller::health, "/health", Get);

    // ========== P1: Token Introspection & Revocation Endpoints ==========

    // Token Introspection Endpoint (RFC 7662)
    // POST /oauth2/introspect
    ADD_METHOD_TO(OAuth2Controller::introspect, "/oauth2/introspect", Post);

    // Token Revocation Endpoint (RFC 7009)
    // POST /oauth2/revoke
    ADD_METHOD_TO(OAuth2Controller::revoke, "/oauth2/revoke", Post);

    METHOD_LIST_END

    void authorize(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void login(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);

    void token(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);

    void userInfo(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void registerUser(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void logout(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);

    void consent(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void health(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);

    // ========== P1: Token Introspection & Revocation Methods ==========

    void introspect(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void revoke(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);

    void metadata(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

  private:
    static drogon::HttpResponsePtr createSuccessResponse();

    // ========== P1: Helper Methods ==========

    // Extract client credentials from Basic Auth or POST body
    static std::pair<std::string, std::string> extractClientCredentials(
      const drogon::HttpRequestPtr &req
    );

    // P0-2: Helper function to check user consent and proceed with
    // authorization
    static void checkUserConsentAndProceed(
      OAuth2Plugin *plugin,
      const std::string &clientId,
      const std::string &userId,
      int32_t internalUserId,
      const std::vector<std::string> &requestedScopes,
      const std::string &scope,
      const std::string &redirectUri,
      const std::string &state,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
};
ate,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
};
