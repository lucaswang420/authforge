#include "MfaController.h"
#include <oauth2/TotpUtils.h>
#include <oauth2/CryptoUtils.h>
#include <oauth2/OAuth2Plugin.h>
#include <oauth2/AuditLogger.h>
#include <oauth2/OpenApiGenerator.h>
#include <drogon/drogon.h>
#include <chrono>

using namespace drogon;
using namespace drogon::orm;

namespace {
struct MfaControllerDocs {
    MfaControllerDocs() {
        common::documentation::EndpointInfo setupDocs;
        setupDocs.path = "/oauth2/mfa/setup";
        setupDocs.method = "POST";
        setupDocs.summary = "Setup MFA";
        setupDocs.description = "Initiate MFA setup by generating a TOTP secret.";
        setupDocs.tags = {"MFA"};
        setupDocs.requiresAuth = true;
        common::documentation::OpenApiGenerator::addEndpoint(setupDocs);

        common::documentation::EndpointInfo verifySetupDocs;
        verifySetupDocs.path = "/oauth2/mfa/setup/verify";
        verifySetupDocs.method = "POST";
        verifySetupDocs.summary = "Verify MFA Setup";
        verifySetupDocs.description = "Verify a TOTP code to finalize MFA setup.";
        verifySetupDocs.tags = {"MFA"};
        verifySetupDocs.requiresAuth = true;
        common::documentation::OpenApiGenerator::addEndpoint(verifySetupDocs);

        common::documentation::EndpointInfo disableDocs;
        disableDocs.path = "/oauth2/mfa/disable";
        disableDocs.method = "POST";
        disableDocs.summary = "Disable MFA";
        disableDocs.description = "Disable MFA for the authenticated user.";
        disableDocs.tags = {"MFA"};
        disableDocs.requiresAuth = true;
        common::documentation::OpenApiGenerator::addEndpoint(disableDocs);

        common::documentation::EndpointInfo verifyDocs;
        verifyDocs.path = "/oauth2/mfa/verify";
        verifyDocs.method = "POST";
        verifyDocs.summary = "Verify MFA Code (Login)";
        verifyDocs.description = "Verify MFA code during login.";
        verifyDocs.tags = {"MFA"};
        verifyDocs.requiresAuth = false;
        common::documentation::OpenApiGenerator::addEndpoint(verifyDocs);
    }
};

MfaControllerDocs docs_;
}  // namespace

void MfaController::setup(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    std::string userId = req->getAttributes()->get<std::string>("userId");
    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    // Generate TOTP secret
    std::string secret = oauth2::utils::TotpUtils::generateSecret();

    // Store secret temporarily (not enabled until verified)
    auto db = app().getDbClient();
    db->execSqlAsync(
      "UPDATE users SET mfa_secret = $1 WHERE public_sub::text = $2::text",
      [sharedCb, secret, userId](const Result &) {
          std::string otpUri =
            oauth2::utils::TotpUtils::generateOtpAuthUri(secret, userId, "OAuth2Server");

          Json::Value json;
          json["secret"] = secret;
          json["otpauth_uri"] = otpUri;
          json["message"] = "Scan the QR code with your authenticator app, then verify with a code";
          auto resp = HttpResponse::newHttpJsonResponse(json);
          (*sharedCb)(resp);
      },
      [sharedCb](const DrogonDbException &e) {
          LOG_ERROR << "MFA setup failed: " << e.base().what();
          Json::Value error;
          error["error"] = "server_error";
          auto resp = HttpResponse::newHttpJsonResponse(error);
          resp->setStatusCode(k500InternalServerError);
          (*sharedCb)(resp);
      },
      secret,
      userId
    );
}

void MfaController::verifySetup(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    std::string userId = req->getAttributes()->get<std::string>("userId");
    std::string code;
    if (req->contentType() == CT_APPLICATION_JSON)
    {
        auto json = req->getJsonObject();
        if (json)
            code = json->get("code", "").asString();
    }
    else
    {
        code = req->getParameter("code");
    }

    if (code.empty() || code.length() != 6)
    {
        Json::Value error;
        error["error"] = "invalid_request";
        error["error_description"] = "6-digit TOTP code is required";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    auto db = app().getDbClient();
    db->execSqlAsync(
      "SELECT mfa_secret FROM users WHERE public_sub::text = $1::text",
      [sharedCb, code, userId, db, req](const Result &r) {
          if (r.empty() || r[0]["mfa_secret"].isNull())
          {
              Json::Value error;
              error["error"] = "invalid_request";
              error["error_description"] = "MFA not set up. Call /api/me/mfa/setup first";
              auto resp = HttpResponse::newHttpJsonResponse(error);
              resp->setStatusCode(k400BadRequest);
              (*sharedCb)(resp);
              return;
          }

          std::string secret = r[0]["mfa_secret"].as<std::string>();

          // Verify the TOTP code
          if (!oauth2::utils::TotpUtils::verifyCode(secret, code))
          {
              Json::Value error;
              error["error"] = "invalid_code";
              error["error_description"] = "TOTP code is incorrect";
              auto resp = HttpResponse::newHttpJsonResponse(error);
              resp->setStatusCode(k401Unauthorized);
              (*sharedCb)(resp);
              return;
          }

          // Generate backup codes
          auto backupCodes = oauth2::utils::TotpUtils::generateBackupCodes(10);
          Json::Value codesJson(Json::arrayValue);
          Json::Value hashedCodesJson(Json::arrayValue);
          for (const auto &bc : backupCodes)
          {
              codesJson.append(bc);
              hashedCodesJson.append(oauth2::utils::hashToken(bc));
          }

          // Enable MFA
          Json::StreamWriterBuilder writer;
          writer["indentation"] = "";
          std::string hashedCodesStr = Json::writeString(writer, hashedCodesJson);

          db->execSqlAsync(
            "UPDATE users SET mfa_enabled = true, mfa_backup_codes = $1 "
            "WHERE public_sub::text = $2::text",
            [sharedCb, codesJson, userId, req](const Result &) {
                oauth2::AuditLogger::log("mfa_enabled", "success", req, userId, "user", userId);
                Json::Value json;
                json["message"] = "MFA enabled successfully";
                json["backup_codes"] = codesJson;
                json["warning"] = "Save these backup codes securely. They cannot be shown again.";
                auto resp = HttpResponse::newHttpJsonResponse(json);
                (*sharedCb)(resp);
            },
            [sharedCb](const DrogonDbException &e) {
                LOG_ERROR << "MFA enable failed: " << e.base().what();
                Json::Value error;
                error["error"] = "server_error";
                auto resp = HttpResponse::newHttpJsonResponse(error);
                resp->setStatusCode(k500InternalServerError);
                (*sharedCb)(resp);
            },
            hashedCodesStr,
            userId
          );
      },
      [sharedCb](const DrogonDbException &e) {
          LOG_ERROR << "MFA verify setup failed: " << e.base().what();
          Json::Value error;
          error["error"] = "server_error";
          auto resp = HttpResponse::newHttpJsonResponse(error);
          resp->setStatusCode(k500InternalServerError);
          (*sharedCb)(resp);
      },
      userId
    );
}

void MfaController::disable(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    std::string userId = req->getAttributes()->get<std::string>("userId");

    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    auto db = app().getDbClient();
    db->execSqlAsync(
      "UPDATE users SET mfa_enabled = false, mfa_secret = NULL, mfa_backup_codes = NULL "
      "WHERE public_sub::text = $1::text",
      [sharedCb](const Result &) {
          Json::Value json;
          json["message"] = "MFA disabled successfully";
          auto resp = HttpResponse::newHttpJsonResponse(json);
          (*sharedCb)(resp);
      },
      [sharedCb](const DrogonDbException &e) {
          LOG_ERROR << "MFA disable failed: " << e.base().what();
          Json::Value error;
          error["error"] = "server_error";
          auto resp = HttpResponse::newHttpJsonResponse(error);
          resp->setStatusCode(k500InternalServerError);
          (*sharedCb)(resp);
      },
      userId
    );
}

void MfaController::verifyLogin(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    // This endpoint is called during login when MFA is required
    // The mfa_token is a short-lived session token from the login step
    std::string mfaToken, code;
    if (req->contentType() == CT_APPLICATION_JSON)
    {
        auto json = req->getJsonObject();
        if (json)
        {
            mfaToken = json->get("mfa_token", "").asString();
            code = json->get("code", "").asString();
        }
    }
    else
    {
        mfaToken = req->getParameter("mfa_token");
        code = req->getParameter("code");
    }

    if (mfaToken.empty() || code.empty())
    {
        Json::Value error;
        error["error"] = "invalid_request";
        error["error_description"] = "mfa_token and code are required";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // For now, MFA login verification uses session-based approach
    // The mfa_token is the userId stored in session during first login step
    // In production, this should be a signed short-lived JWT
    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    auto db = app().getDbClient();
    db->execSqlAsync(
      "SELECT id, public_sub, mfa_secret, mfa_backup_codes FROM users WHERE id = $1",
      [sharedCb, code, mfaToken, req](const Result &r) {
          if (r.empty())
          {
              Json::Value error;
              error["error"] = "invalid_grant";
              error["error_description"] = "Invalid MFA session";
              auto resp = HttpResponse::newHttpJsonResponse(error);
              resp->setStatusCode(k401Unauthorized);
              (*sharedCb)(resp);
              return;
          }

          std::string secret =
            r[0]["mfa_secret"].isNull() ? "" : r[0]["mfa_secret"].as<std::string>();
          std::string publicSub = r[0]["public_sub"].as<std::string>();

          // Try TOTP code first
          if (oauth2::utils::TotpUtils::verifyCode(secret, code))
          {
              // MFA verified - store in session and return success
              if (req->session())
              {
                  req->session()->insert("mfa_verified", true);
                  req->session()->insert("userId", publicSub);
              }

              Json::Value json;
              json["message"] = "MFA verification successful";
              json["mfa_verified"] = true;
              auto resp = HttpResponse::newHttpJsonResponse(json);
              (*sharedCb)(resp);
              return;
          }

          // Try backup code
          // (simplified: in production, parse JSON array and check each hashed code)
          Json::Value error;
          error["error"] = "invalid_code";
          error["error_description"] = "TOTP code is incorrect";
          auto resp = HttpResponse::newHttpJsonResponse(error);
          resp->setStatusCode(k401Unauthorized);
          (*sharedCb)(resp);
      },
      [sharedCb](const DrogonDbException &e) {
          LOG_ERROR << "MFA login verify failed: " << e.base().what();
          Json::Value error;
          error["error"] = "server_error";
          auto resp = HttpResponse::newHttpJsonResponse(error);
          resp->setStatusCode(k500InternalServerError);
          (*sharedCb)(resp);
      },
      mfaToken
    );
}
