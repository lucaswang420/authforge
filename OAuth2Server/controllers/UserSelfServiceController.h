#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

class UserSelfServiceController : public drogon::HttpController<UserSelfServiceController>
{
  public:
    METHOD_LIST_BEGIN
    // Get current user profile
    ADD_METHOD_TO(
      UserSelfServiceController::getProfile,
      "/api/me",
      Get,
      "oauth2::filters::OAuth2Middleware"
    );
    // Change password
    ADD_METHOD_TO(
      UserSelfServiceController::changePassword,
      "/api/me/password",
      Put,
      "oauth2::filters::OAuth2Middleware"
    );
    // List authorized OAuth2 clients
    ADD_METHOD_TO(
      UserSelfServiceController::listAuthorizedApps,
      "/api/me/authorized-apps",
      Get,
      "oauth2::filters::OAuth2Middleware"
    );
    // Revoke authorization for a specific client
    ADD_METHOD_TO(
      UserSelfServiceController::revokeAuthorizedApp,
      "/api/me/authorized-apps/{clientId}",
      Delete,
      "oauth2::filters::OAuth2Middleware"
    );
    // Delete account (soft-delete)
    ADD_METHOD_TO(
      UserSelfServiceController::deleteAccount,
      "/api/me",
      Delete,
      "oauth2::filters::OAuth2Middleware"
    );
    METHOD_LIST_END

    void getProfile(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
    void changePassword(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
    void listAuthorizedApps(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
    void revokeAuthorizedApp(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback,
      const std::string &clientId
    );
    void deleteAccount(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
};
