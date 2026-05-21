#include "AdminApiController.h"
#include <drogon/drogon.h>
#include <drogon/utils/Utilities.h>
#include <oauth2/CryptoUtils.h>

void AdminApiController::listClients(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    try
    {
        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
          "SELECT client_id, client_type, name, redirect_uris, allowed_grant_types "
          "FROM oauth2_clients ORDER BY client_id",
          [sharedCb](const drogon::orm::Result &result) {
              Json::Value json;
              json["status"] = "success";
              Json::Value clients(Json::arrayValue);

              for (const auto &row : result)
              {
                  Json::Value client;
                  client["client_id"] = row["client_id"].as<std::string>();
                  client["client_type"] = row["client_type"].as<std::string>();
                  client["name"] = row["name"].isNull() ? "" : row["name"].as<std::string>();
                  client["redirect_uris"] =
                    row["redirect_uris"].isNull() ? "" : row["redirect_uris"].as<std::string>();
                  client["allowed_grant_types"] = row["allowed_grant_types"].isNull()
                                                    ? ""
                                                    : row["allowed_grant_types"].as<std::string>();
                  clients.append(client);
              }

              json["clients"] = clients;
              json["total"] = static_cast<int>(result.size());
              (*sharedCb)(HttpResponse::newHttpJsonResponse(json));
          },
          [sharedCb](const drogon::orm::DrogonDbException &e) {
              Json::Value json;
              json["status"] = "error";
              json["message"] = "Failed to fetch clients";
              json["detail"] = e.base().what();
              auto resp = HttpResponse::newHttpJsonResponse(json);
              resp->setStatusCode(k500InternalServerError);
              (*sharedCb)(resp);
          }
        );
    }
    catch (...)
    {
        Json::Value json;
        json["status"] = "error";
        json["message"] = "Database unavailable";
        auto resp = HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(k500InternalServerError);
        (*sharedCb)(resp);
    }
}

void AdminApiController::createClient(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    // Parse request body
    std::string name;
    std::string redirectUris;
    std::string allowedGrantTypes = "authorization_code";
    std::string clientType = "CONFIDENTIAL";

    auto jsonBody = req->getJsonObject();
    if (jsonBody)
    {
        name = jsonBody->get("name", "").asString();
        redirectUris = jsonBody->get("redirect_uris", "").asString();
        allowedGrantTypes = jsonBody->get("allowed_grant_types", "authorization_code").asString();
        clientType = jsonBody->get("client_type", "CONFIDENTIAL").asString();
    }

    // Generate client_id (UUID) and client_secret
    std::string clientId = drogon::utils::getUuid();
    std::string clientSecret = oauth2::utils::generateSecureToken();
    std::string secretHash = oauth2::utils::hashToken(clientSecret);
    std::string salt = drogon::utils::getUuid().substr(0, 36);

    try
    {
        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
          "INSERT INTO oauth2_clients (client_id, client_type, client_secret, salt, name, "
          "redirect_uris, allowed_grant_types) VALUES ($1, $2, $3, $4, $5, $6, $7)",
          [sharedCb, clientId, clientSecret](const drogon::orm::Result &) {
              Json::Value json;
              json["status"] = "success";
              json["message"] = "Client created successfully";
              json["client_id"] = clientId;
              json["client_secret"] = clientSecret;  // Only returned once at creation time
              json["note"] = "Store the client_secret securely. It will not be shown again.";
              auto resp = HttpResponse::newHttpJsonResponse(json);
              resp->setStatusCode(k201Created);
              (*sharedCb)(resp);
          },
          [sharedCb](const drogon::orm::DrogonDbException &e) {
              Json::Value json;
              json["status"] = "error";
              json["message"] = "Failed to create client";
              json["detail"] = e.base().what();
              auto resp = HttpResponse::newHttpJsonResponse(json);
              resp->setStatusCode(k500InternalServerError);
              (*sharedCb)(resp);
          },
          clientId,
          clientType,
          secretHash,
          salt,
          name,
          redirectUris,
          allowedGrantTypes
        );
    }
    catch (...)
    {
        Json::Value json;
        json["status"] = "error";
        json["message"] = "Database unavailable";
        auto resp = HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(k500InternalServerError);
        (*sharedCb)(resp);
    }
}

void AdminApiController::listUsers(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    try
    {
        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
          "SELECT id, username, email, email_verified, mfa_enabled "
          "FROM users ORDER BY id",
          [sharedCb](const drogon::orm::Result &result) {
              Json::Value json;
              json["status"] = "success";
              Json::Value users(Json::arrayValue);

              for (const auto &row : result)
              {
                  Json::Value user;
                  user["id"] = row["id"].as<int>();
                  user["username"] = row["username"].as<std::string>();
                  user["email"] = row["email"].isNull() ? "" : row["email"].as<std::string>();
                  user["email_verified"] =
                    row["email_verified"].isNull() ? false : row["email_verified"].as<bool>();
                  user["mfa_enabled"] =
                    row["mfa_enabled"].isNull() ? false : row["mfa_enabled"].as<bool>();
                  users.append(user);
              }

              json["users"] = users;
              json["total"] = static_cast<int>(result.size());
              (*sharedCb)(HttpResponse::newHttpJsonResponse(json));
          },
          [sharedCb](const drogon::orm::DrogonDbException &e) {
              Json::Value json;
              json["status"] = "error";
              json["message"] = "Failed to fetch users";
              json["detail"] = e.base().what();
              auto resp = HttpResponse::newHttpJsonResponse(json);
              resp->setStatusCode(k500InternalServerError);
              (*sharedCb)(resp);
          }
        );
    }
    catch (...)
    {
        Json::Value json;
        json["status"] = "error";
        json["message"] = "Database unavailable";
        auto resp = HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(k500InternalServerError);
        (*sharedCb)(resp);
    }
}

void AdminApiController::listScopes(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    try
    {
        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
          "SELECT id, name, description, mapped_role, is_default, requires_admin_role "
          "FROM oauth2_scopes ORDER BY id",
          [sharedCb](const drogon::orm::Result &result) {
              Json::Value json;
              json["status"] = "success";
              Json::Value scopes(Json::arrayValue);

              for (const auto &row : result)
              {
                  Json::Value scope;
                  scope["id"] = row["id"].as<int>();
                  scope["name"] = row["name"].as<std::string>();
                  scope["description"] =
                    row["description"].isNull() ? "" : row["description"].as<std::string>();
                  scope["mapped_role"] =
                    row["mapped_role"].isNull() ? "" : row["mapped_role"].as<std::string>();
                  scope["is_default"] =
                    row["is_default"].isNull() ? false : row["is_default"].as<bool>();
                  scope["requires_admin_role"] = row["requires_admin_role"].isNull()
                                                   ? false
                                                   : row["requires_admin_role"].as<bool>();
                  scopes.append(scope);
              }

              json["scopes"] = scopes;
              json["total"] = static_cast<int>(result.size());
              (*sharedCb)(HttpResponse::newHttpJsonResponse(json));
          },
          [sharedCb](const drogon::orm::DrogonDbException &e) {
              Json::Value json;
              json["status"] = "error";
              json["message"] = "Failed to fetch scopes";
              json["detail"] = e.base().what();
              auto resp = HttpResponse::newHttpJsonResponse(json);
              resp->setStatusCode(k500InternalServerError);
              (*sharedCb)(resp);
          }
        );
    }
    catch (...)
    {
        Json::Value json;
        json["status"] = "error";
        json["message"] = "Database unavailable";
        auto resp = HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(k500InternalServerError);
        (*sharedCb)(resp);
    }
}
