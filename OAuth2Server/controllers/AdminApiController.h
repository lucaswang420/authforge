#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

class AdminApiController : public drogon::HttpController<AdminApiController>
{
  public:
    METHOD_LIST_BEGIN
    // Client Management
    ADD_METHOD_TO(
      AdminApiController::listClients,
      "/api/admin/clients",
      Get,
      "AuthorizationFilter"
    );
    ADD_METHOD_TO(
      AdminApiController::createClient,
      "/api/admin/clients",
      Post,
      "AuthorizationFilter"
    );
    ADD_METHOD_TO(
      AdminApiController::deleteClient,
      "/api/admin/clients/{clientId}",
      Delete,
      "AuthorizationFilter"
    );
    ADD_METHOD_TO(
      AdminApiController::resetClientSecret,
      "/api/admin/clients/{clientId}/reset-secret",
      Post,
      "AuthorizationFilter"
    );

    // User Management
    ADD_METHOD_TO(AdminApiController::listUsers, "/api/admin/users", Get, "AuthorizationFilter");
    ADD_METHOD_TO(
      AdminApiController::disableUser,
      "/api/admin/users/{userId}/disable",
      Put,
      "AuthorizationFilter"
    );
    ADD_METHOD_TO(
      AdminApiController::assignUserRoles,
      "/api/admin/users/{userId}/roles",
      Put,
      "AuthorizationFilter"
    );

    // Scope Management
    ADD_METHOD_TO(AdminApiController::listScopes, "/api/admin/scopes", Get, "AuthorizationFilter");
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
};
