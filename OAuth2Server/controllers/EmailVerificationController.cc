#include "EmailVerificationController.h"
#include <oauth2/CryptoUtils.h>
#include <oauth2/EmailService.h>
#include <oauth2/observability/openapi/OpenApiGenerator.h>
#include <drogon/drogon.h>
#include <chrono>

using namespace drogon;
using namespace drogon::orm;

namespace
{
struct EmailVerificationControllerDocs
{
    EmailVerificationControllerDocs()
    {
        oauth2::observability::openapi::EndpointInfo verifyEmail;
        verifyEmail.path = "/api/verify-email";
        verifyEmail.method = "GET";
        verifyEmail.summary = "Verify Email";
        verifyEmail.description = "Verify an email address using a token.";
        verifyEmail.tags = {"User Verification"};
        verifyEmail.requiresAuth = false;
        oauth2::observability::openapi::OpenApiGenerator::addEndpoint(verifyEmail);

        oauth2::observability::openapi::EndpointInfo resendEmail;
        resendEmail.path = "/api/verify-email/resend";
        resendEmail.method = "POST";
        resendEmail.summary = "Resend Verification Email";
        resendEmail.description = "Resend the email verification link.";
        resendEmail.tags = {"User Verification"};
        resendEmail.requiresAuth = false;
        oauth2::observability::openapi::OpenApiGenerator::addEndpoint(resendEmail);
    }
};

EmailVerificationControllerDocs docs_;
}  // namespace

// Lazy accessor — avoids static init order crash (see P5 bugfix).
static oauth2::IEmailService &getEmailSvc() { return oauth2::getEmailService(); }

void EmailVerificationController::sendVerificationEmail(int userId, const std::string &email)
{
    if (email.empty())
        return;

    std::string rawToken = oauth2::utils::generateSecureToken();
    std::string tokenHash = oauth2::utils::hashToken(rawToken);

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
                 std::chrono::system_clock::now().time_since_epoch()
    )
                 .count();
    int64_t expiresAt = now + 86400;  // 24 hours

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
      "INSERT INTO email_verification_tokens (token_hash, user_id, email, expires_at) "
      "VALUES ($1, $2, $3, $4) "
      "ON CONFLICT (token_hash) DO NOTHING",
      [rawToken, email](const Result &) {
          // Build verification link using frontend URL
          auto customConfig = drogon::app().getCustomConfig();
          std::string frontendUrl = "http://localhost:5173";
          if (customConfig.isMember("frontend") && customConfig["frontend"].isMember("url"))
          {
              frontendUrl = customConfig["frontend"]["url"].asString();
          }
          std::string verifyLink = frontendUrl + "/verify-email?token=" + rawToken;
          std::string body = "Please verify your email by clicking:\n\n" + verifyLink +
                             "\n\nThis link expires in 24 hours.";

          oauth2::getEmailService().sendEmail(email, "Verify Your Email", body, [](bool) {});
      },
      [](const DrogonDbException &e) {
          LOG_ERROR << "Failed to store verification token: " << e.base().what();
      },
      tokenHash,
      userId,
      email,
      expiresAt
    );
}

void EmailVerificationController::verify(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    std::string token = req->getParameter("token");
    if (token.empty())
    {
        Json::Value error;
        error["error"] = "invalid_request";
        error["error_description"] = "token parameter is required";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    std::string tokenHash = oauth2::utils::hashToken(token);
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
                 std::chrono::system_clock::now().time_since_epoch()
    )
                 .count();

    auto db = app().getDbClient();

    // Atomic consume and get user info
    db->execSqlAsync(
      "DELETE FROM email_verification_tokens "
      "WHERE token_hash = $1 AND expires_at > $2 "
      "RETURNING user_id, email",
      [sharedCb, db](const Result &r) {
          if (r.empty())
          {
              Json::Value error;
              error["error"] = "invalid_grant";
              error["error_description"] = "Token is invalid or expired";
              auto resp = HttpResponse::newHttpJsonResponse(error);
              resp->setStatusCode(k400BadRequest);
              (*sharedCb)(resp);
              return;
          }

          int userId = r[0]["user_id"].as<int>();

          // Mark email as verified
          db->execSqlAsync(
            "UPDATE users SET email_verified = true WHERE id = $1",
            [sharedCb](const Result &) {
                Json::Value json;
                json["message"] = "Email verified successfully";
                auto resp = HttpResponse::newHttpJsonResponse(json);
                (*sharedCb)(resp);
            },
            [sharedCb](const DrogonDbException &e) {
                LOG_ERROR << "Failed to update email_verified: " << e.base().what();
                Json::Value error;
                error["error"] = "server_error";
                auto resp = HttpResponse::newHttpJsonResponse(error);
                resp->setStatusCode(k500InternalServerError);
                (*sharedCb)(resp);
            },
            userId
          );
      },
      [sharedCb](const DrogonDbException &e) {
          LOG_ERROR << "Email verification failed: " << e.base().what();
          Json::Value error;
          error["error"] = "server_error";
          auto resp = HttpResponse::newHttpJsonResponse(error);
          resp->setStatusCode(k500InternalServerError);
          (*sharedCb)(resp);
      },
      tokenHash,
      now
    );
}

void EmailVerificationController::resend(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    // Get userId from OAuth2Middleware attributes
    std::string userId = req->getAttributes()->get<std::string>("userId");
    if (userId.empty())
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    auto db = app().getDbClient();
    db->execSqlAsync(
      "SELECT id, email, email_verified FROM users WHERE public_sub::text = $1::text",
      [sharedCb](const Result &r) {
          if (r.empty())
          {
              Json::Value error;
              error["error"] = "not_found";
              auto resp = HttpResponse::newHttpJsonResponse(error);
              resp->setStatusCode(k404NotFound);
              (*sharedCb)(resp);
              return;
          }

          if (r[0]["email_verified"].as<bool>())
          {
              Json::Value json;
              json["message"] = "Email is already verified";
              auto resp = HttpResponse::newHttpJsonResponse(json);
              (*sharedCb)(resp);
              return;
          }

          int internalId = r[0]["id"].as<int>();
          std::string email = r[0]["email"].isNull() ? "" : r[0]["email"].as<std::string>();

          if (email.empty())
          {
              Json::Value error;
              error["error"] = "invalid_request";
              error["error_description"] = "No email address on file";
              auto resp = HttpResponse::newHttpJsonResponse(error);
              resp->setStatusCode(k400BadRequest);
              (*sharedCb)(resp);
              return;
          }

          sendVerificationEmail(internalId, email);

          Json::Value json;
          json["message"] = "Verification email sent";
          auto resp = HttpResponse::newHttpJsonResponse(json);
          (*sharedCb)(resp);
      },
      [sharedCb](const DrogonDbException &e) {
          LOG_ERROR << "Resend verification failed: " << e.base().what();
          Json::Value error;
          error["error"] = "server_error";
          auto resp = HttpResponse::newHttpJsonResponse(error);
          resp->setStatusCode(k500InternalServerError);
          (*sharedCb)(resp);
      },
      userId
    );
}
