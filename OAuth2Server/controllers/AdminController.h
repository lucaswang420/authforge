#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

class AdminController : public drogon::HttpController<AdminController>
{
  public:
    METHOD_LIST_BEGIN
    // Dashboard (merged from old AdminController)
    ADD_METHOD_TO(AdminController::dashboard, "/api/admin/dashboard", Get, "oauth2::filters::AuthorizationFilter");
    // Client Management
    ADD_METHOD_TO(
      AdminController::listClients,
      "/api/admin/clients",
      Get,
      "oauth2::filters::AuthorizationFilter"
    );
    ADD_METHOD_TO(
      AdminController::createClient,
      "/api/admin/clients",
      Post,
      "oauth2::filters::AuthorizationFilter"
    );
    ADD_METHOD_TO(
      AdminController::getClient,
      "/api/admin/clients/{clientId}",
      Get,
      "oauth2::filters::AuthorizationFilter"
    );
    ADD_METHOD_TO(
      AdminController::updateClient,
      "/api/admin/clients/{clientId}",
      Put,
      "oauth2::filters::AuthorizationFilter"
    );
    ADD_METHOD_TO(
      AdminController::deleteClient,
      "/api/admin/clients/{clientId}",
      Delete,
      "oauth2::filters::AuthorizationFilter"
    );
    ADD_METHOD_TO(
      AdminController::resetClientSecret,
      "/api/admin/clients/{clientId}/reset-secret",
      Post,
      "oauth2::filters::AuthorizationFilter"
    );
    ADD_METHOD_TO(
      AdminController::getClientScopes,
      "/api/admin/clients/{clientId}/scopes",
      Get,
      "oauth2::filters::AuthorizationFilter"
    );
    ADD_METHOD_TO(
      AdminController::updateClientScopes,
      "/api/admin/clients/{clientId}/scopes",
      Put,
      "oauth2::filters::AuthorizationFilter"
    );

    // User Management
    ADD_METHOD_TO(AdminController::listUsers, "/api/admin/users", Get, "oauth2::filters::AuthorizationFilter");
    ADD_METHOD_TO(
      AdminController::disableUser,
      "/api/admin/users/{userId}/disable",
      Put,
      "oauth2::filters::AuthorizationFilter"
    );
    ADD_METHOD_TO(
      AdminController::assignUserRoles,
      "/api/admin/users/{userId}/roles",
      Put,
      "oauth2::filters::AuthorizationFilter"
    );

    // Scope Management
    ADD_METHOD_TO(AdminController::listScopes, "/api/admin/scopes", Get, "oauth2::filters::AuthorizationFilter");

    // Audit Logs
    ADD_METHOD_TO(AdminController::listLogs, "/api/admin/logs", Get, "oauth2::filters::AuthorizationFilter");

    // Token Management
    ADD_METHOD_TO(AdminController::listTokens, "/api/admin/tokens", Get, "oauth2::filters::AuthorizationFilter");
    ADD_METHOD_TO(
      AdminController::revokeTokensByClient,
      "/api/admin/tokens/revoke-by-client",
      Post,
      "oauth2::filters::AuthorizationFilter"
    );
    ADD_METHOD_TO(
      AdminController::revokeTokensByUser,
      "/api/admin/tokens/revoke-by-user",
      Post,
      "oauth2::filters::AuthorizationFilter"
    );
    ADD_METHOD_TO(
      AdminController::revokeToken,
      "/api/admin/tokens/{tokenPrefix}",
      Delete,
      "oauth2::filters::AuthorizationFilter"
    );

    // User Detail & Management
    ADD_METHOD_TO(
      AdminController::getUser,
      "/api/admin/users/{userId}",
      Get,
      "oauth2::filters::AuthorizationFilter"
    );
    ADD_METHOD_TO(
      AdminController::updateUser,
      "/api/admin/users/{userId}",
      Put,
      "oauth2::filters::AuthorizationFilter"
    );
    ADD_METHOD_TO(
      AdminController::enableUser,
      "/api/admin/users/{userId}/enable",
      Post,
      "oauth2::filters::AuthorizationFilter"
    );
    ADD_METHOD_TO(
      AdminController::getUserRoles,
      "/api/admin/users/{userId}/roles",
      Get,
      "oauth2::filters::AuthorizationFilter"
    );

    // Role Management
    ADD_METHOD_TO(AdminController::listRoles, "/api/admin/roles", Get, "oauth2::filters::AuthorizationFilter");
    ADD_METHOD_TO(AdminController::createRole, "/api/admin/roles", Post, "oauth2::filters::AuthorizationFilter");
    ADD_METHOD_TO(
      AdminController::updateRole,
      "/api/admin/roles/{roleId}",
      Put,
      "oauth2::filters::AuthorizationFilter"
    );
    ADD_METHOD_TO(
      AdminController::deleteRole,
      "/api/admin/roles/{roleId}",
      Delete,
      "oauth2::filters::AuthorizationFilter"
    );

    // Scope Management (CRUD)
    ADD_METHOD_TO(
      AdminController::createScope,
      "/api/admin/scopes",
      Post,
      "oauth2::filters::AuthorizationFilter"
    );
    ADD_METHOD_TO(
      AdminController::updateScope,
      "/api/admin/scopes/{scopeId}",
      Put,
      "oauth2::filters::AuthorizationFilter"
    );
    ADD_METHOD_TO(
      AdminController::deleteScope,
      "/api/admin/scopes/{scopeId}",
      Delete,
      "oauth2::filters::AuthorizationFilter"
    );

    // Dashboard Stats
    ADD_METHOD_TO(
      AdminController::getDashboardStats,
      "/api/admin/dashboard/stats",
      Get,
      "oauth2::filters::AuthorizationFilter"
    );

    // OIDC Key Info
    ADD_METHOD_TO(
      AdminController::getOidcKeys,
      "/api/admin/oidc/keys",
      Get,
      "oauth2::filters::AuthorizationFilter"
    );
    METHOD_LIST_END

    void listClients(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void createClient(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void deleteClient(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback,
      const std::string &clientId
    );

    void getClient(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback,
      const std::string &clientId
    );

    void updateClient(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback,
      const std::string &clientId
    );

    void getClientScopes(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback,
      const std::string &clientId
    );

    void updateClientScopes(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback,
      const std::string &clientId
    );

    void resetClientSecret(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback,
      const std::string &clientId
    );

    void listUsers(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void disableUser(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback,
      const std::string &userId
    );

    void assignUserRoles(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback,
      const std::string &userId
    );

    void listScopes(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void listLogs(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void listTokens(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void revokeToken(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback,
      const std::string &tokenPrefix
    );

    void revokeTokensByClient(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void revokeTokensByUser(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void getOidcKeys(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    // User Detail & Management
    void getUser(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback,
      const std::string &userId
    );

    void updateUser(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback,
      const std::string &userId
    );

    void enableUser(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback,
      const std::string &userId
    );

    void getUserRoles(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback,
      const std::string &userId
    );

    // Role Management
    void listRoles(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void createRole(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void updateRole(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback,
      const std::string &roleId
    );

    void deleteRole(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback,
      const std::string &roleId
    );

    // Scope Management (CRUD)
    void createScope(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void updateScope(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback,
      const std::string &scopeId
    );

    void deleteScope(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback,
      const std::string &scopeId
    );

    // Dashboard Stats
    void getDashboardStats(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
    void dashboard(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
};
