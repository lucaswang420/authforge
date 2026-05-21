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

    // User Management
    ADD_METHOD_TO(AdminApiController::listUsers, "/api/admin/users", Get, "AuthorizationFilter");

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

    void listUsers(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void listScopes(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
};
