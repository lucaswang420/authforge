#include "AdminController.h"
#include <oauth2/OpenApiGenerator.h>

namespace
{
struct AdminControllerDocs
{
    AdminControllerDocs()
    {
        common::documentation::EndpointInfo dashboard;
        dashboard.path = "/api/admin/dashboard";
        dashboard.method = "GET";
        dashboard.summary = "Admin Dashboard Data";
        dashboard.description =
          "Get summary data for the admin dashboard. Requires admin permissions.";
        dashboard.tags = {"Admin"};
        dashboard.requiresAuth = true;
        common::documentation::OpenApiGenerator::addEndpoint(dashboard);
    }
};

AdminControllerDocs docs_;
}  // namespace

void AdminController::dashboard(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    // This endpoint is protected by AuthorizationFilter
    // If we are here, the user has admin permissions

    Json::Value json;
    json["message"] = "Welcome to Admin Dashboard";
    json["status"] = "success";

    auto resp = HttpResponse::newHttpJsonResponse(json);
    callback(resp);
}
