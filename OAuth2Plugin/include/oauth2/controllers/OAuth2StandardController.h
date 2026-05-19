#pragma once

#include <drogon/HttpController.h>
#include <oauth2/OAuth2Plugin.h>

namespace oauth2::controllers
{

class OAuth2StandardController : public drogon::HttpController<OAuth2StandardController>
{
  public:
    static void initApiDocs();

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(OAuth2StandardController::authorize, "/oauth2/authorize", drogon::Get);
    ADD_METHOD_TO(OAuth2StandardController::token, "/oauth2/token", drogon::Post);
    ADD_METHOD_TO(
      OAuth2StandardController::userInfo,
      "/oauth2/userinfo",
      drogon::Get,
      "oauth2::filters::OAuth2Middleware"
    );
    ADD_METHOD_TO(
      OAuth2StandardController::introspect,
      "/oauth2/introspect",
      drogon::Post,
      "oauth2::filters::OAuth2Middleware"
    );
    ADD_METHOD_TO(
      OAuth2StandardController::revoke,
      "/oauth2/revoke",
      drogon::Post,
      "oauth2::filters::OAuth2Middleware"
    );
    ADD_METHOD_TO(
      OAuth2StandardController::metadata,
      "/.well-known/oauth-authorization-server",
      drogon::Get
    );
    ADD_METHOD_TO(
      OAuth2StandardController::oidcDiscovery,
      "/.well-known/openid-configuration",
      drogon::Get
    );
    ADD_METHOD_TO(
      OAuth2StandardController::jwks,
      "/.well-known/jwks.json",
      drogon::Get
    );
    METHOD_LIST_END

    void authorize(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback
    );
    void token(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback
    );
    void userInfo(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback
    );
    void introspect(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback
    );
    void revoke(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback
    );
    void metadata(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback
    );
    void oidcDiscovery(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback
    );
    void jwks(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback
    );

  private:
    static drogon::HttpResponsePtr createSuccessResponse();

    static std::pair<std::string, std::string> extractClientCredentials(
      const drogon::HttpRequestPtr &req
    );

    static void checkUserConsentAndProceed(
      ::OAuth2Plugin *plugin,
      const std::string &clientId,
      const std::string &userId,
      int32_t internalUserId,
      const std::vector<std::string> &requestedScopes,
      const std::string &scope,
      const std::string &redirectUri,
      const std::string &state,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback
    );
};

}  // namespace oauth2::controllers
