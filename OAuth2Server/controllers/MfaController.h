#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

class MfaController : public drogon::HttpController<MfaController>
{
  public:
    METHOD_LIST_BEGIN
    // Setup MFA (requires auth)
    ADD_METHOD_TO(
      MfaController::setup,
      "/api/me/mfa/setup",
      Post,
      "oauth2::filters::OAuth2Middleware"
    );
    // Confirm MFA setup with TOTP code
    ADD_METHOD_TO(
      MfaController::verifySetup,
      "/api/me/mfa/verify",
      Post,
      "oauth2::filters::OAuth2Middleware"
    );
    // Disable MFA (requires auth + password)
    ADD_METHOD_TO(
      MfaController::disable,
      "/api/me/mfa/disable",
      Post,
      "oauth2::filters::OAuth2Middleware"
    );
    // Verify TOTP during login (second factor)
    ADD_METHOD_TO(MfaController::verifyLogin, "/oauth2/mfa/verify", Post);
    METHOD_LIST_END

    void setup(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
    void verifySetup(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
    void disable(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
    void verifyLogin(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
};
