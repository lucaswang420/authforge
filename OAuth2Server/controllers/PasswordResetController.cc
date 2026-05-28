#include "PasswordResetController.h"
#include <oauth2/CryptoUtils.h>
#include <oauth2/PasswordHasher.h>
#include <oauth2/EmailService.h>
#include <oauth2/OAuth2Plugin.h>
#include <oauth2/observability/AuditLogger.h>
#include <oauth2/observability/openapi/OpenApiGenerator.h>
#include <drogon/drogon.h>
#include <drogon/utils/Utilities.h>
#include <chrono>

using namespace drogon;
using namespace drogon::orm;

namespace
{
struct PasswordResetControllerDocs
{
    PasswordResetControllerDocs()
    {
        oauth2::observability::openapi::EndpointInfo requestDocs;
        requestDocs.path = "/api/password-reset/request";
        requestDocs.method = "POST";
        requestDocs.summary = "Request Password Reset";
        requestDocs.description = "Request a password reset link to be sent via email.";
        requestDocs.tags = {"User Verification"};
        requestDocs.requiresAuth = false;
        oauth2::observability::openapi::OpenApiGenerator::addEndpoint(requestDocs);

        oauth2::observability::openapi::EndpointInfo confirmDocs;
        confirmDocs.path = "/api/password-reset/confirm";
        confirmDocs.method = "POST";
        confirmDocs.summary = "Confirm Password Reset";
        confirmDocs.description = "Confirm a password reset using the token sent via email.";
        confirmDocs.tags = {"User Verification"};
        confirmDocs.requiresAuth = false;
        oauth2::observability::openapi::OpenApiGenerator::addEndpoint(confirmDocs);
    }
};

PasswordResetControllerDocs docs_;
}  // namespace

// Lazy accessor — avoids static init order crash (see P5 bugfix).
static oauth2::IEmailService &getEmailSvc() { return oauth2::getEmailService(); }

void PasswordResetController::request(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    // Extract email from request
    std::string email;
    if (req->contentType() == CT_APPLICATION_JSON)
    {
        auto json = req->getJsonObject();
        if (json)
            email = json->get("email", "").asString();
    }
    else
    {
        email = req->getParameter("email");
    }

    if (email.empty())
    {
        Json::Value error;
        error["error"] = "invalid_request";
        error["error_description"] = "email is required";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Always return 200 regardless of whether email exists (prevent enumeration)
    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    auto db = app().getDbClient();
    db->execSqlAsync(
      "SELECT id, email FROM users WHERE email = $1",
      [sharedCb, email](const Result &r) {
          // Always respond 200 to prevent email enumeration
          Json::Value json;
          json["message"] = "If the email exists, a reset link has been sent";

          if (r.empty())
          {
              // User not found - still return 200
              auto resp = HttpResponse::newHttpJsonResponse(json);
              (*sharedCb)(resp);
              return;
          }

          int userId = r[0]["id"].as<int>();

          // Generate secure reset token
          std::string rawToken = oauth2::utils::generateSecureToken();
          std::string tokenHash = oauth2::utils::hashToken(rawToken);

          // Token expires in 15 minutes
          auto now = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch()
          )
                       .count();
          int64_t expiresAt = now + 900;  // 15 minutes

          // Store hashed token
          auto db2 = drogon::app().getDbClient();
          db2->execSqlAsync(
            "INSERT INTO password_reset_tokens (token_hash, user_id, expires_at) "
            "VALUES ($1, $2, $3)",
            [sharedCb, json, rawToken, email](const Result &) {
                // Send email with reset link
                auto customConfig = drogon::app().getCustomConfig();
                std::string frontendUrl = "http://localhost:5173";
                if (customConfig.isMember("frontend") && customConfig["frontend"].isMember("url"))
                    frontendUrl = customConfig["frontend"]["url"].asString();
                std::string resetLink = frontendUrl + "/reset-password?token=" + rawToken;
                std::string emailBody = "Click the following link to reset your password:\n\n" +
                                        resetLink + "\n\nThis link expires in 15 minutes.";

                getEmailSvc().sendEmail(
                  email, "Password Reset Request", emailBody, [](bool) {}  // fire-and-forget
                );

                auto resp = HttpResponse::newHttpJsonResponse(json);
                (*sharedCb)(resp);
            },
            [sharedCb, json](const DrogonDbException &e) {
                LOG_ERROR << "Failed to store reset token: " << e.base().what();
                auto resp = HttpResponse::newHttpJsonResponse(json);
                (*sharedCb)(resp);
            },
            tokenHash,
            userId,
            expiresAt
          );
      },
      [sharedCb](const DrogonDbException &e) {
          LOG_ERROR << "Password reset lookup failed: " << e.base().what();
          Json::Value json;
          json["message"] = "If the email exists, a reset link has been sent";
          auto resp = HttpResponse::newHttpJsonResponse(json);
          (*sharedCb)(resp);
      },
      email
    );
}

void PasswordResetController::confirm(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    // Extract token and new password
    std::string token, newPassword;
    if (req->contentType() == CT_APPLICATION_JSON)
    {
        auto json = req->getJsonObject();
        if (json)
        {
            token = json->get("token", "").asString();
            newPassword = json->get("new_password", "").asString();
        }
    }
    else
    {
        token = req->getParameter("token");
        newPassword = req->getParameter("new_password");
    }

    if (token.empty() || newPassword.empty())
    {
        Json::Value error;
        error["error"] = "invalid_request";
        error["error_description"] = "token and new_password are required";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    if (newPassword.length() < 8)
    {
        Json::Value error;
        error["error"] = "invalid_request";
        error["error_description"] = "Password must be at least 8 characters";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    // Hash the token and look up
    std::string tokenHash = oauth2::utils::hashToken(token);
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
                 std::chrono::system_clock::now().time_since_epoch()
    )
                 .count();

    auto db = app().getDbClient();

    // Atomic consume: mark as used only if not already used and not expired
    db->execSqlAsync(
      "UPDATE password_reset_tokens SET used = true "
      "WHERE token_hash = $1 AND used = false AND expires_at > $2 "
      "RETURNING user_id",
      [sharedCb, newPassword, db, req](const Result &r) {
          if (r.empty())
          {
              Json::Value error;
              error["error"] = "invalid_grant";
              error["error_description"] = "Token is invalid, expired, or already used";
              auto resp = HttpResponse::newHttpJsonResponse(error);
              resp->setStatusCode(k400BadRequest);
              (*sharedCb)(resp);
              return;
          }

          int userId = r[0]["user_id"].as<int>();

          // Hash new password with PBKDF2
          std::string newHash;
          try
          {
              newHash = oauth2::utils::PasswordHasher::hash(newPassword);
          }
          catch (const std::exception &e)
          {
              LOG_ERROR << "Password hashing failed: " << e.what();
              Json::Value error;
              error["error"] = "server_error";
              auto resp = HttpResponse::newHttpJsonResponse(error);
              resp->setStatusCode(k500InternalServerError);
              (*sharedCb)(resp);
              return;
          }

          // Update password
          db->execSqlAsync(
            "UPDATE users SET password_hash = $1, salt = '' WHERE id = $2",
            [sharedCb, userId, db, req](const Result &) {
                // Revoke all tokens for this user
                std::string userIdStr = std::to_string(userId);
                db->execSqlAsync(
                  "UPDATE oauth2_access_tokens SET revoked = true WHERE user_id = $1",
                  [sharedCb, userId, db, userIdStr, req](const Result &) {
                      db->execSqlAsync(
                        "UPDATE oauth2_refresh_tokens SET revoked = true WHERE user_id = $1",
                        [sharedCb, userId, req](const Result &) {
                            oauth2::observability::AuditLogger::log(
                              "password_reset",
                              "success",
                              req,
                              std::to_string(userId),
                              "user",
                              std::to_string(userId)
                            );
                            Json::Value json;
                            json["message"] = "Password reset successful";
                            json["note"] = "All existing sessions have been revoked";
                            auto resp = HttpResponse::newHttpJsonResponse(json);
                            (*sharedCb)(resp);
                        },
                        [sharedCb, userId, req](const DrogonDbException &) {
                            oauth2::observability::AuditLogger::log(
                              "password_reset",
                              "success",
                              req,
                              std::to_string(userId),
                              "user",
                              std::to_string(userId)
                            );
                            Json::Value json;
                            json["message"] = "Password reset successful";
                            auto resp = HttpResponse::newHttpJsonResponse(json);
                            (*sharedCb)(resp);
                        },
                        userIdStr
                      );
                  },
                  [sharedCb, userId, req](const DrogonDbException &) {
                      oauth2::observability::AuditLogger::log(
                        "password_reset",
                        "success",
                        req,
                        std::to_string(userId),
                        "user",
                        std::to_string(userId)
                      );
                      Json::Value json;
                      json["message"] = "Password reset successful";
                      auto resp = HttpResponse::newHttpJsonResponse(json);
                      (*sharedCb)(resp);
                  },
                  userIdStr
                );
            },
            [sharedCb](const DrogonDbException &e) {
                LOG_ERROR << "Failed to update password: " << e.base().what();
                Json::Value error;
                error["error"] = "server_error";
                auto resp = HttpResponse::newHttpJsonResponse(error);
                resp->setStatusCode(k500InternalServerError);
                (*sharedCb)(resp);
            },
            newHash,
            userId
          );
      },
      [sharedCb](const DrogonDbException &e) {
          LOG_ERROR << "Reset token lookup failed: " << e.base().what();
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
