#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

/**
 * @brief WebAuthn / Passkey Controller
 *
 * Provides endpoints for passwordless authentication using WebAuthn.
 * Registration: user registers a new credential (passkey)
 * Authentication: user authenticates with an existing credential
 */
class WebAuthnController : public drogon::HttpController<WebAuthnController>
{
  public:
    METHOD_LIST_BEGIN
    // Registration flow (requires existing auth)
    ADD_METHOD_TO(
      WebAuthnController::registerBegin,
      "/api/me/webauthn/register/begin",
      Post,
      "oauth2::filters::OAuth2Middleware"
    );
    ADD_METHOD_TO(
      WebAuthnController::registerFinish,
      "/api/me/webauthn/register/finish",
      Post,
      "oauth2::filters::OAuth2Middleware"
    );
    // Authentication flow (no auth required - this IS the auth)
    ADD_METHOD_TO(
      WebAuthnController::authenticateBegin,
      "/oauth2/webauthn/authenticate/begin",
      Post
    );
    ADD_METHOD_TO(
      WebAuthnController::authenticateFinish,
      "/oauth2/webauthn/authenticate/finish",
      Post
    );
    // List credentials (requires auth)
    ADD_METHOD_TO(
      WebAuthnController::listCredentials,
      "/api/me/webauthn/credentials",
      Get,
      "oauth2::filters::OAuth2Middleware"
    );
    METHOD_LIST_END

    void registerBegin(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
    void registerFinish(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
    void authenticateBegin(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
    void authenticateFinish(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
    void listCredentials(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
};
