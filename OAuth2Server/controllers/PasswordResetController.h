#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

class PasswordResetController : public drogon::HttpController<PasswordResetController>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(PasswordResetController::request, "/api/password-reset/request", Post);
    ADD_METHOD_TO(PasswordResetController::confirm, "/api/password-reset/confirm", Post);
    METHOD_LIST_END

    void request(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void confirm(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
};
