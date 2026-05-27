#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

class SessionController : public drogon::HttpController<SessionController>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(SessionController::showLoginPage, "/login", Get);
    ADD_METHOD_TO(SessionController::login, "/oauth2/login", Post);
    ADD_METHOD_TO(SessionController::consent, "/oauth2/consent", Post);
    ADD_METHOD_TO(
      SessionController::logout,
      "/oauth2/logout",
      Post,
      "oauth2::filters::OAuth2AuthFilter"
    );
        ADD_METHOD_TO(SessionController::registerUser, "/api/register", Post);
    METHOD_LIST_END

    void showLoginPage(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
    void login(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
    void consent(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
    void logout(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
    void registerUser(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
};
