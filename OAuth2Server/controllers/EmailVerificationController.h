#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

class EmailVerificationController : public drogon::HttpController<EmailVerificationController>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(EmailVerificationController::verify, "/api/verify-email", Get);
    ADD_METHOD_TO(
      EmailVerificationController::resend,
      "/api/verify-email/resend",
      Post,
      "oauth2::filters::OAuth2AuthFilter"
    );
    METHOD_LIST_END

    void verify(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);

    void resend(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);

    // Helper: send verification email for a user
    static void sendVerificationEmail(int userId, const std::string &email);
};
