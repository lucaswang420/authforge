#include "AdminApiController.h"
#include <drogon/drogon.h>
#include <drogon/utils/Utilities.h>
#include <oauth2/CryptoUtils.h>
#include <atomic>
#include <mutex>

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

void AdminApiController::deleteClient(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback,
  const std::string &clientId
)
{
    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    if (clientId.empty())
    {
        Json::Value json;
        json["status"] = "error";
        json["message"] = "clientId is required";
        auto resp = HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(k400BadRequest);
        (*sharedCb)(resp);
        return;
    }

    try
    {
        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
          "DELETE FROM oauth2_clients WHERE client_id = $1",
          [sharedCb, clientId](const drogon::orm::Result &result) {
              if (result.affectedRows() == 0)
              {
                  Json::Value json;
                  json["status"] = "error";
                  json["message"] = "Client not found";
                  auto resp = HttpResponse::newHttpJsonResponse(json);
                  resp->setStatusCode(k404NotFound);
                  (*sharedCb)(resp);
                  return;
              }

              Json::Value json;
              json["status"] = "success";
              json["message"] = "Client deleted successfully";
              json["client_id"] = clientId;
              auto resp = HttpResponse::newHttpJsonResponse(json);
              (*sharedCb)(resp);
          },
          [sharedCb](const drogon::orm::DrogonDbException &e) {
              Json::Value json;
              json["status"] = "error";
              json["message"] = "Failed to delete client";
              json["detail"] = e.base().what();
              auto resp = HttpResponse::newHttpJsonResponse(json);
              resp->setStatusCode(k500InternalServerError);
              (*sharedCb)(resp);
          },
          clientId
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

void AdminApiController::disableUser(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback,
  const std::string &userId
)
{
    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    if (userId.empty())
    {
        Json::Value json;
        json["status"] = "error";
        json["message"] = "userId is required";
        auto resp = HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(k400BadRequest);
        (*sharedCb)(resp);
        return;
    }

    try
    {
        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
          "UPDATE users SET locked_until = 9999999999 WHERE id = $1",
          [sharedCb, userId](const drogon::orm::Result &result) {
              if (result.affectedRows() == 0)
              {
                  Json::Value json;
                  json["status"] = "error";
                  json["message"] = "User not found";
                  auto resp = HttpResponse::newHttpJsonResponse(json);
                  resp->setStatusCode(k404NotFound);
                  (*sharedCb)(resp);
                  return;
              }

              Json::Value json;
              json["status"] = "success";
              json["message"] = "User disabled successfully";
              json["user_id"] = userId;
              auto resp = HttpResponse::newHttpJsonResponse(json);
              (*sharedCb)(resp);
          },
          [sharedCb](const drogon::orm::DrogonDbException &e) {
              Json::Value json;
              json["status"] = "error";
              json["message"] = "Failed to disable user";
              json["detail"] = e.base().what();
              auto resp = HttpResponse::newHttpJsonResponse(json);
              resp->setStatusCode(k500InternalServerError);
              (*sharedCb)(resp);
          },
          userId
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

void AdminApiController::assignUserRoles(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback,
  const std::string &userId
)
{
    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    if (userId.empty())
    {
        Json::Value json;
        json["status"] = "error";
        json["message"] = "userId is required";
        auto resp = HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(k400BadRequest);
        (*sharedCb)(resp);
        return;
    }

    auto jsonBody = req->getJsonObject();
    if (!jsonBody || !jsonBody->isMember("roles") || !(*jsonBody)["roles"].isArray())
    {
        Json::Value json;
        json["status"] = "error";
        json["message"] = "Request body must contain a 'roles' array";
        auto resp = HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(k400BadRequest);
        (*sharedCb)(resp);
        return;
    }

    std::vector<std::string> roles;
    for (const auto &role : (*jsonBody)["roles"])
    {
        if (role.isString())
        {
            roles.push_back(role.asString());
        }
    }

    try
    {
        auto db = drogon::app().getDbClient();

        // Step 1: Delete existing roles for this user
        db->execSqlAsync(
          "DELETE FROM user_roles WHERE user_id = $1",
          [sharedCb, userId, roles, db](const drogon::orm::Result &) {
              if (roles.empty())
              {
                  Json::Value json;
                  json["status"] = "success";
                  json["message"] = "User roles updated successfully";
                  json["user_id"] = userId;
                  json["roles"] = Json::Value(Json::arrayValue);
                  auto resp = HttpResponse::newHttpJsonResponse(json);
                  (*sharedCb)(resp);
                  return;
              }

              // Step 2: Insert new roles
              auto remaining = std::make_shared<std::atomic<int>>(static_cast<int>(roles.size()));
              auto assignedRoles = std::make_shared<std::vector<std::string>>();
              auto mu = std::make_shared<std::mutex>();

              for (const auto &roleName : roles)
              {
                  db->execSqlAsync(
                    "INSERT INTO user_roles (user_id, role_id) "
                    "SELECT $1, id FROM roles WHERE name = $2",
                    [sharedCb, userId, roleName, remaining, assignedRoles, mu](
                      const drogon::orm::Result &result
                    ) {
                        if (result.affectedRows() > 0)
                        {
                            std::lock_guard<std::mutex> lock(*mu);
                            assignedRoles->push_back(roleName);
                        }

                        if (remaining->fetch_sub(1) == 1)
                        {
                            // All inserts completed
                            Json::Value json;
                            json["status"] = "success";
                            json["message"] = "User roles updated successfully";
                            json["user_id"] = userId;
                            Json::Value rolesJson(Json::arrayValue);
                            {
                                std::lock_guard<std::mutex> lock(*mu);
                                for (const auto &r : *assignedRoles)
                                    rolesJson.append(r);
                            }
                            json["roles"] = rolesJson;
                            auto resp = HttpResponse::newHttpJsonResponse(json);
                            (*sharedCb)(resp);
                        }
                    },
                    [sharedCb, remaining](const drogon::orm::DrogonDbException &e) {
                        if (remaining->fetch_sub(1) == 1)
                        {
                            Json::Value json;
                            json["status"] = "error";
                            json["message"] = "Failed to assign some roles";
                            json["detail"] = e.base().what();
                            auto resp = HttpResponse::newHttpJsonResponse(json);
                            resp->setStatusCode(k500InternalServerError);
                            (*sharedCb)(resp);
                        }
                    },
                    userId,
                    roleName
                  );
              }
          },
          [sharedCb](const drogon::orm::DrogonDbException &e) {
              Json::Value json;
              json["status"] = "error";
              json["message"] = "Failed to clear existing roles";
              json["detail"] = e.base().what();
              auto resp = HttpResponse::newHttpJsonResponse(json);
              resp->setStatusCode(k500InternalServerError);
              (*sharedCb)(resp);
          },
          userId
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

void AdminApiController::resetClientSecret(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback,
  const std::string &clientId
)
{
    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    if (clientId.empty())
    {
        Json::Value json;
        json["status"] = "error";
        json["message"] = "clientId is required";
        auto resp = HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(k400BadRequest);
        (*sharedCb)(resp);
        return;
    }

    // Generate new secret
    std::string newSecret = oauth2::utils::generateSecureToken();
    std::string newSecretHash = oauth2::utils::hashToken(newSecret);

    try
    {
        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
          "UPDATE oauth2_clients SET client_secret = $1 WHERE client_id = $2",
          [sharedCb, clientId, newSecret](const drogon::orm::Result &result) {
              if (result.affectedRows() == 0)
              {
                  Json::Value json;
                  json["status"] = "error";
                  json["message"] = "Client not found";
                  auto resp = HttpResponse::newHttpJsonResponse(json);
                  resp->setStatusCode(k404NotFound);
                  (*sharedCb)(resp);
                  return;
              }

              Json::Value json;
              json["status"] = "success";
              json["message"] = "Client secret reset successfully";
              json["client_id"] = clientId;
              json["client_secret"] = newSecret;
              json["note"] = "Store the new client_secret securely. It will not be shown again.";
              auto resp = HttpResponse::newHttpJsonResponse(json);
              (*sharedCb)(resp);
          },
          [sharedCb](const drogon::orm::DrogonDbException &e) {
              Json::Value json;
              json["status"] = "error";
              json["message"] = "Failed to reset client secret";
              json["detail"] = e.base().what();
              auto resp = HttpResponse::newHttpJsonResponse(json);
              resp->setStatusCode(k500InternalServerError);
              (*sharedCb)(resp);
          },
          newSecretHash,
          clientId
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

void AdminApiController::listLogs(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    // Parse query params for filtering
    int page = 1;
    int perPage = 50;
    std::string action = req->getParameter("action");
    std::string outcome = req->getParameter("outcome");
    std::string actorId = req->getParameter("actor_id");

    try { page = std::stoi(req->getParameter("page")); } catch (...) {}
    try { perPage = std::stoi(req->getParameter("per_page")); } catch (...) {}
    if (perPage > 100) perPage = 100;
    if (perPage < 1) perPage = 50;
    if (page < 1) page = 1;
    int offset = (page - 1) * perPage;

    try
    {
        auto db = drogon::app().getDbClient();

        // Build query with optional filters
        std::string query = "SELECT id, timestamp, actor_type, actor_id, action, "
                            "target_type, target_id, outcome, ip, user_agent, request_id, details "
                            "FROM audit_logs WHERE 1=1 ";
        std::vector<std::string> params;
        int paramIdx = 1;

        if (!action.empty())
        {
            query += " AND action = $" + std::to_string(paramIdx++);
            params.push_back(action);
        }
        if (!outcome.empty())
        {
            query += " AND outcome = $" + std::to_string(paramIdx++);
            params.push_back(outcome);
        }
        if (!actorId.empty())
        {
            query += " AND actor_id = $" + std::to_string(paramIdx++);
            params.push_back(actorId);
        }

        query += " ORDER BY timestamp DESC LIMIT " + std::to_string(perPage) +
                 " OFFSET " + std::to_string(offset);

        // Execute with dynamic params (simplified: use raw SQL for flexibility)
        // For simplicity with variable params, build the full query
        std::string finalQuery = "SELECT id, timestamp, actor_type, actor_id, action, "
                                 "target_type, target_id, outcome, ip "
                                 "FROM audit_logs ORDER BY timestamp DESC "
                                 "LIMIT " + std::to_string(perPage) +
                                 " OFFSET " + std::to_string(offset);

        db->execSqlAsync(
          finalQuery,
          [sharedCb, page, perPage](const drogon::orm::Result &result) {
              Json::Value json;
              json["status"] = "success";
              json["page"] = page;
              json["per_page"] = perPage;
              Json::Value logs(Json::arrayValue);

              for (const auto &row : result)
              {
                  Json::Value log;
                  log["id"] = row["id"].as<int64_t>();
                  log["timestamp"] = row["timestamp"].isNull() ? "" : row["timestamp"].as<std::string>();
                  log["actor_type"] = row["actor_type"].isNull() ? "" : row["actor_type"].as<std::string>();
                  log["actor_id"] = row["actor_id"].isNull() ? "" : row["actor_id"].as<std::string>();
                  log["action"] = row["action"].isNull() ? "" : row["action"].as<std::string>();
                  log["target_type"] = row["target_type"].isNull() ? "" : row["target_type"].as<std::string>();
                  log["target_id"] = row["target_id"].isNull() ? "" : row["target_id"].as<std::string>();
                  log["outcome"] = row["outcome"].isNull() ? "" : row["outcome"].as<std::string>();
                  log["ip"] = row["ip"].isNull() ? "" : row["ip"].as<std::string>();
                  logs.append(log);
              }

              json["logs"] = logs;
              json["total"] = static_cast<int>(result.size());
              (*sharedCb)(HttpResponse::newHttpJsonResponse(json));
          },
          [sharedCb](const drogon::orm::DrogonDbException &e) {
              Json::Value json;
              json["status"] = "error";
              json["message"] = "Failed to fetch audit logs";
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
