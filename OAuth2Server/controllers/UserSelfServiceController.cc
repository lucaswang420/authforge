#include "UserSelfServiceController.h"
#include <oauth2/PasswordHasher.h>
#include <oauth2/CryptoUtils.h>
#include <oauth2/AuditLogger.h>
#include <drogon/drogon.h>
#include <chrono>

using namespace drogon;
using namespace drogon::orm;

void UserSelfServiceController::getProfile(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    std::string userId = req->getAttributes()->get<std::string>("userId");
    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    try
    {
        auto db = app().getDbClient();
        db->execSqlAsync(
          "SELECT username, email, email_verified, mfa_enabled "
          "FROM users WHERE public_sub::text = $1::text",
          [sharedCb](const Result &result) {
              if (result.empty())
              {
                  Json::Value error;
                  error["error"] = "not_found";
                  error["error_description"] = "User not found";
                  auto resp = HttpResponse::newHttpJsonResponse(error);
                  resp->setStatusCode(k404NotFound);
                  (*sharedCb)(resp);
                  return;
              }

              const auto &row = result[0];
              Json::Value json;
              json["username"] = row["username"].as<std::string>();
              json["email"] = row["email"].isNull() ? "" : row["email"].as<std::string>();
              json["email_verified"] =
                row["email_verified"].isNull() ? false : row["email_verified"].as<bool>();
              json["mfa_enabled"] =
                row["mfa_enabled"].isNull() ? false : row["mfa_enabled"].as<bool>();
              auto resp = HttpResponse::newHttpJsonResponse(json);
              (*sharedCb)(resp);
          },
          [sharedCb](const DrogonDbException &e) {
              LOG_ERROR << "getProfile failed: " << e.base().what();
              Json::Value error;
              error["error"] = "server_error";
              error["error_description"] = "Failed to fetch user profile";
              auto resp = HttpResponse::newHttpJsonResponse(error);
              resp->setStatusCode(k500InternalServerError);
              (*sharedCb)(resp);
          },
          userId
        );
    }
    catch (...)
    {
        Json::Value error;
        error["error"] = "server_error";
        error["error_description"] = "Database unavailable";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k500InternalServerError);
        (*sharedCb)(resp);
    }
}

void UserSelfServiceController::changePassword(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    std::string userId = req->getAttributes()->get<std::string>("userId");
    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    // Parse JSON body
    auto jsonBody = req->getJsonObject();
    if (!jsonBody)
    {
        Json::Value error;
        error["error"] = "invalid_request";
        error["error_description"] = "JSON body is required";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k400BadRequest);
        (*sharedCb)(resp);
        return;
    }

    std::string oldPassword = jsonBody->get("old_password", "").asString();
    std::string newPassword = jsonBody->get("new_password", "").asString();

    if (oldPassword.empty() || newPassword.empty())
    {
        Json::Value error;
        error["error"] = "invalid_request";
        error["error_description"] = "old_password and new_password are required";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k400BadRequest);
        (*sharedCb)(resp);
        return;
    }

    if (newPassword.length() < 8)
    {
        Json::Value error;
        error["error"] = "invalid_request";
        error["error_description"] = "New password must be at least 8 characters";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k400BadRequest);
        (*sharedCb)(resp);
        return;
    }

    try
    {
        auto db = app().getDbClient();
        db->execSqlAsync(
          "SELECT password_hash, salt FROM users WHERE public_sub::text = $1::text",
          [sharedCb, oldPassword, newPassword, userId, req](const Result &result) {
              if (result.empty())
              {
                  Json::Value error;
                  error["error"] = "not_found";
                  error["error_description"] = "User not found";
                  auto resp = HttpResponse::newHttpJsonResponse(error);
                  resp->setStatusCode(k404NotFound);
                  (*sharedCb)(resp);
                  return;
              }

              std::string storedHash = result[0]["password_hash"].as<std::string>();
              std::string salt =
                result[0]["salt"].isNull() ? "" : result[0]["salt"].as<std::string>();

              // Verify old password
              if (!oauth2::utils::PasswordHasher::verify(oldPassword, storedHash, salt))
              {
                  oauth2::AuditLogger::log(
                    "password_change_failed", "failure", req, userId, "user", userId
                  );
                  Json::Value error;
                  error["error"] = "invalid_credentials";
                  error["error_description"] = "Current password is incorrect";
                  auto resp = HttpResponse::newHttpJsonResponse(error);
                  resp->setStatusCode(k401Unauthorized);
                  (*sharedCb)(resp);
                  return;
              }

              // Hash new password
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

              // Update password in DB (clear salt since PBKDF2 embeds it)
              auto db2 = drogon::app().getDbClient();
              db2->execSqlAsync(
                "UPDATE users SET password_hash = $1, salt = '' "
                "WHERE public_sub::text = $2::text",
                [sharedCb, userId, req, db2](const Result &) {
                    // Revoke all access tokens for this user
                    db2->execSqlAsync(
                      "UPDATE oauth2_access_tokens SET revoked = true WHERE user_id = $1",
                      [sharedCb, userId, req, db2](const Result &) {
                          // Revoke all refresh tokens for this user
                          db2->execSqlAsync(
                            "UPDATE oauth2_refresh_tokens SET revoked = true WHERE user_id = $1",
                            [sharedCb, userId, req](const Result &) {
                                oauth2::AuditLogger::log(
                                  "password_changed", "success", req, userId, "user", userId
                                );
                                Json::Value json;
                                json["message"] = "Password changed successfully";
                                json["note"] = "All existing sessions have been revoked";
                                auto resp = HttpResponse::newHttpJsonResponse(json);
                                (*sharedCb)(resp);
                            },
                            [sharedCb, userId, req](const DrogonDbException &) {
                                // Password was changed, token revocation is best-effort
                                oauth2::AuditLogger::log(
                                  "password_changed", "success", req, userId, "user", userId
                                );
                                Json::Value json;
                                json["message"] = "Password changed successfully";
                                auto resp = HttpResponse::newHttpJsonResponse(json);
                                (*sharedCb)(resp);
                            },
                            userId
                          );
                      },
                      [sharedCb, userId, req, db2](const DrogonDbException &) {
                          // Password was changed, token revocation is best-effort
                          db2->execSqlAsync(
                            "UPDATE oauth2_refresh_tokens SET revoked = true WHERE user_id = $1",
                            [sharedCb, userId, req](const Result &) {
                                oauth2::AuditLogger::log(
                                  "password_changed", "success", req, userId, "user", userId
                                );
                                Json::Value json;
                                json["message"] = "Password changed successfully";
                                auto resp = HttpResponse::newHttpJsonResponse(json);
                                (*sharedCb)(resp);
                            },
                            [sharedCb, userId, req](const DrogonDbException &) {
                                oauth2::AuditLogger::log(
                                  "password_changed", "success", req, userId, "user", userId
                                );
                                Json::Value json;
                                json["message"] = "Password changed successfully";
                                auto resp = HttpResponse::newHttpJsonResponse(json);
                                (*sharedCb)(resp);
                            },
                            userId
                          );
                      },
                      userId
                    );
                },
                [sharedCb](const DrogonDbException &e) {
                    LOG_ERROR << "Password update failed: " << e.base().what();
                    Json::Value error;
                    error["error"] = "server_error";
                    error["error_description"] = "Failed to update password";
                    auto resp = HttpResponse::newHttpJsonResponse(error);
                    resp->setStatusCode(k500InternalServerError);
                    (*sharedCb)(resp);
                },
                newHash,
                userId
              );
          },
          [sharedCb](const DrogonDbException &e) {
              LOG_ERROR << "changePassword lookup failed: " << e.base().what();
              Json::Value error;
              error["error"] = "server_error";
              error["error_description"] = "Failed to verify credentials";
              auto resp = HttpResponse::newHttpJsonResponse(error);
              resp->setStatusCode(k500InternalServerError);
              (*sharedCb)(resp);
          },
          userId
        );
    }
    catch (...)
    {
        Json::Value error;
        error["error"] = "server_error";
        error["error_description"] = "Database unavailable";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k500InternalServerError);
        (*sharedCb)(resp);
    }
}

void UserSelfServiceController::listAuthorizedApps(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    std::string userId = req->getAttributes()->get<std::string>("userId");
    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    try
    {
        auto db = app().getDbClient();
        db->execSqlAsync(
          "SELECT DISTINCT c.client_id, c.name "
          "FROM oauth2_user_consents uc "
          "JOIN oauth2_clients c ON uc.client_id = c.client_id "
          "WHERE uc.internal_user_id = "
          "(SELECT id FROM users WHERE public_sub::text = $1::text)",
          [sharedCb](const Result &result) {
              Json::Value json;
              Json::Value apps(Json::arrayValue);

              for (const auto &row : result)
              {
                  Json::Value app;
                  app["client_id"] = row["client_id"].as<std::string>();
                  app["name"] = row["name"].isNull() ? "" : row["name"].as<std::string>();
                  apps.append(app);
              }

              json["authorized_apps"] = apps;
              json["total"] = static_cast<int>(result.size());
              auto resp = HttpResponse::newHttpJsonResponse(json);
              (*sharedCb)(resp);
          },
          [sharedCb](const DrogonDbException &e) {
              LOG_ERROR << "listAuthorizedApps failed: " << e.base().what();
              Json::Value error;
              error["error"] = "server_error";
              error["error_description"] = "Failed to fetch authorized apps";
              auto resp = HttpResponse::newHttpJsonResponse(error);
              resp->setStatusCode(k500InternalServerError);
              (*sharedCb)(resp);
          },
          userId
        );
    }
    catch (...)
    {
        Json::Value error;
        error["error"] = "server_error";
        error["error_description"] = "Database unavailable";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k500InternalServerError);
        (*sharedCb)(resp);
    }
}

void UserSelfServiceController::revokeAuthorizedApp(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback,
  const std::string &clientId
)
{
    std::string userId = req->getAttributes()->get<std::string>("userId");
    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    if (clientId.empty())
    {
        Json::Value error;
        error["error"] = "invalid_request";
        error["error_description"] = "clientId is required";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k400BadRequest);
        (*sharedCb)(resp);
        return;
    }

    try
    {
        auto db = app().getDbClient();

        // First get the internal user id
        db->execSqlAsync(
          "SELECT id FROM users WHERE public_sub::text = $1::text",
          [sharedCb, userId, clientId, req, db](const Result &result) {
              if (result.empty())
              {
                  Json::Value error;
                  error["error"] = "not_found";
                  error["error_description"] = "User not found";
                  auto resp = HttpResponse::newHttpJsonResponse(error);
                  resp->setStatusCode(k404NotFound);
                  (*sharedCb)(resp);
                  return;
              }

              int internalUserId = result[0]["id"].as<int>();

              // Delete consents
              db->execSqlAsync(
                "DELETE FROM oauth2_user_consents "
                "WHERE internal_user_id = $1 AND client_id = $2",
                [sharedCb, userId, clientId, req, db](const Result &) {
                    // Revoke access tokens for this user+client
                    db->execSqlAsync(
                      "UPDATE oauth2_access_tokens SET revoked = true "
                      "WHERE user_id = $1 AND client_id = $2",
                      [sharedCb, userId, clientId, req](const Result &) {
                          oauth2::AuditLogger::log(
                            "app_authorization_revoked", "success", req, userId, "client", clientId
                          );
                          Json::Value json;
                          json["message"] = "Authorization revoked successfully";
                          json["client_id"] = clientId;
                          auto resp = HttpResponse::newHttpJsonResponse(json);
                          (*sharedCb)(resp);
                      },
                      [sharedCb, userId, clientId, req](const DrogonDbException &) {
                          // Consent was deleted, token revocation is best-effort
                          oauth2::AuditLogger::log(
                            "app_authorization_revoked", "success", req, userId, "client", clientId
                          );
                          Json::Value json;
                          json["message"] = "Authorization revoked successfully";
                          json["client_id"] = clientId;
                          auto resp = HttpResponse::newHttpJsonResponse(json);
                          (*sharedCb)(resp);
                      },
                      userId,
                      clientId
                    );
                },
                [sharedCb](const DrogonDbException &e) {
                    LOG_ERROR << "revokeAuthorizedApp consent delete failed: " << e.base().what();
                    Json::Value error;
                    error["error"] = "server_error";
                    error["error_description"] = "Failed to revoke authorization";
                    auto resp = HttpResponse::newHttpJsonResponse(error);
                    resp->setStatusCode(k500InternalServerError);
                    (*sharedCb)(resp);
                },
                internalUserId,
                clientId
              );
          },
          [sharedCb](const DrogonDbException &e) {
              LOG_ERROR << "revokeAuthorizedApp user lookup failed: " << e.base().what();
              Json::Value error;
              error["error"] = "server_error";
              error["error_description"] = "Failed to revoke authorization";
              auto resp = HttpResponse::newHttpJsonResponse(error);
              resp->setStatusCode(k500InternalServerError);
              (*sharedCb)(resp);
          },
          userId
        );
    }
    catch (...)
    {
        Json::Value error;
        error["error"] = "server_error";
        error["error_description"] = "Database unavailable";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k500InternalServerError);
        (*sharedCb)(resp);
    }
}

void UserSelfServiceController::deleteAccount(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    std::string userId = req->getAttributes()->get<std::string>("userId");
    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    try
    {
        auto db = app().getDbClient();

        // Step 1: Revoke all access tokens
        db->execSqlAsync(
          "UPDATE oauth2_access_tokens SET revoked = true WHERE user_id = $1",
          [sharedCb, userId, req, db](const Result &) {
              // Step 2: Revoke all refresh tokens
              db->execSqlAsync(
                "UPDATE oauth2_refresh_tokens SET revoked = true WHERE user_id = $1",
                [sharedCb, userId, req, db](const Result &) {
                    // Step 3: Soft-delete user (anonymize)
                    auto timestamp = std::to_string(
                      std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()
                      )
                        .count()
                    );
                    std::string anonUsername = "deleted_" + timestamp;

                    db->execSqlAsync(
                      "UPDATE users SET username = $1, email = NULL, password_hash = 'DELETED' "
                      "WHERE public_sub::text = $2::text",
                      [sharedCb, userId, req](const Result &result) {
                          if (result.affectedRows() == 0)
                          {
                              Json::Value error;
                              error["error"] = "not_found";
                              error["error_description"] = "User not found";
                              auto resp = HttpResponse::newHttpJsonResponse(error);
                              resp->setStatusCode(k404NotFound);
                              (*sharedCb)(resp);
                              return;
                          }

                          oauth2::AuditLogger::log(
                            "account_deleted", "success", req, userId, "user", userId
                          );
                          Json::Value json;
                          json["message"] = "Account deleted successfully";
                          auto resp = HttpResponse::newHttpJsonResponse(json);
                          (*sharedCb)(resp);
                      },
                      [sharedCb](const DrogonDbException &e) {
                          LOG_ERROR << "deleteAccount user update failed: " << e.base().what();
                          Json::Value error;
                          error["error"] = "server_error";
                          error["error_description"] = "Failed to delete account";
                          auto resp = HttpResponse::newHttpJsonResponse(error);
                          resp->setStatusCode(k500InternalServerError);
                          (*sharedCb)(resp);
                      },
                      anonUsername,
                      userId
                    );
                },
                [sharedCb, userId, req, db](const DrogonDbException &) {
                    // Refresh token revocation failed, still proceed with soft-delete
                    auto timestamp = std::to_string(
                      std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()
                      )
                        .count()
                    );
                    std::string anonUsername = "deleted_" + timestamp;

                    db->execSqlAsync(
                      "UPDATE users SET username = $1, email = NULL, password_hash = 'DELETED' "
                      "WHERE public_sub::text = $2::text",
                      [sharedCb, userId, req](const Result &) {
                          oauth2::AuditLogger::log(
                            "account_deleted", "success", req, userId, "user", userId
                          );
                          Json::Value json;
                          json["message"] = "Account deleted successfully";
                          auto resp = HttpResponse::newHttpJsonResponse(json);
                          (*sharedCb)(resp);
                      },
                      [sharedCb](const DrogonDbException &e) {
                          LOG_ERROR << "deleteAccount user update failed: " << e.base().what();
                          Json::Value error;
                          error["error"] = "server_error";
                          error["error_description"] = "Failed to delete account";
                          auto resp = HttpResponse::newHttpJsonResponse(error);
                          resp->setStatusCode(k500InternalServerError);
                          (*sharedCb)(resp);
                      },
                      anonUsername,
                      userId
                    );
                },
                userId
              );
          },
          [sharedCb, userId, req, db](const DrogonDbException &) {
              // Access token revocation failed, still proceed
              auto timestamp = std::to_string(
                std::chrono::duration_cast<std::chrono::seconds>(
                  std::chrono::system_clock::now().time_since_epoch()
                )
                  .count()
              );
              std::string anonUsername = "deleted_" + timestamp;

              db->execSqlAsync(
                "UPDATE users SET username = $1, email = NULL, password_hash = 'DELETED' "
                "WHERE public_sub::text = $2::text",
                [sharedCb, userId, req](const Result &) {
                    oauth2::AuditLogger::log(
                      "account_deleted", "success", req, userId, "user", userId
                    );
                    Json::Value json;
                    json["message"] = "Account deleted successfully";
                    auto resp = HttpResponse::newHttpJsonResponse(json);
                    (*sharedCb)(resp);
                },
                [sharedCb](const DrogonDbException &e) {
                    LOG_ERROR << "deleteAccount user update failed: " << e.base().what();
                    Json::Value error;
                    error["error"] = "server_error";
                    error["error_description"] = "Failed to delete account";
                    auto resp = HttpResponse::newHttpJsonResponse(error);
                    resp->setStatusCode(k500InternalServerError);
                    (*sharedCb)(resp);
                },
                anonUsername,
                userId
              );
          },
          userId
        );
    }
    catch (...)
    {
        Json::Value error;
        error["error"] = "server_error";
        error["error_description"] = "Database unavailable";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k500InternalServerError);
        (*sharedCb)(resp);
    }
}
