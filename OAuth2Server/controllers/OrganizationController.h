#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

class OrganizationController : public drogon::HttpController<OrganizationController>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(
      OrganizationController::list,
      "/api/admin/organizations",
      Get,
      "AuthorizationFilter"
    );
    ADD_METHOD_TO(
      OrganizationController::create,
      "/api/admin/organizations",
      Post,
      "AuthorizationFilter"
    );
    ADD_METHOD_TO(
      OrganizationController::getBySlug,
      "/api/admin/organizations/{slug}",
      Get,
      "AuthorizationFilter"
    );
    METHOD_LIST_END

    void list(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
    void create(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
    void getBySlug(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback,
      const std::string &slug
    );
};
