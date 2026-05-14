#include "AdminController.h"

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
