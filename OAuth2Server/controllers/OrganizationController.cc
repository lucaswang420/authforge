#include "OrganizationController.h"
#include <oauth2/AuditLogger.h>
#include <oauth2/OpenApiGenerator.h>
#include <drogon/drogon.h>
#include <regex>

namespace
{
struct OrganizationControllerDocs
{
    OrganizationControllerDocs()
    {
        common::documentation::EndpointInfo getOrgsDocs;
        getOrgsDocs.path = "/api/orgs";
        getOrgsDocs.method = "GET";
        getOrgsDocs.summary = "List Organizations";
        getOrgsDocs.description = "List all organizations.";
        getOrgsDocs.tags = {"Organization"};
        getOrgsDocs.requiresAuth = true;
        common::documentation::OpenApiGenerator::addEndpoint(getOrgsDocs);

        common::documentation::EndpointInfo postOrgsDocs;
        postOrgsDocs.path = "/api/orgs";
        postOrgsDocs.method = "POST";
        postOrgsDocs.summary = "Create Organization";
        postOrgsDocs.description = "Create a new organization.";
        postOrgsDocs.tags = {"Organization"};
        postOrgsDocs.requiresAuth = true;
        common::documentation::OpenApiGenerator::addEndpoint(postOrgsDocs);

        common::documentation::EndpointInfo postOrgUsersDocs;
        postOrgUsersDocs.path = "/api/orgs/{orgId}/users";
        postOrgUsersDocs.method = "POST";
        postOrgUsersDocs.summary = "Add User to Organization";
        postOrgUsersDocs.description = "Add a user to an organization.";
        postOrgUsersDocs.tags = {"Organization"};
        postOrgUsersDocs.requiresAuth = true;
        common::documentation::OpenApiGenerator::addEndpoint(postOrgUsersDocs);
    }
};

OrganizationControllerDocs docs_;
}  // namespace

void OrganizationController::list(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
      "SELECT id, slug, name, logo_uri, primary_color, issuer_override, created_at "
      "FROM organizations ORDER BY id",
      [sharedCb](const drogon::orm::Result &r) {
          Json::Value json;
          Json::Value orgs(Json::arrayValue);
          for (const auto &row : r)
          {
              Json::Value org;
              org["id"] = row["id"].as<int>();
              org["slug"] = row["slug"].as<std::string>();
              org["name"] = row["name"].as<std::string>();
              org["logo_uri"] = row["logo_uri"].isNull() ? "" : row["logo_uri"].as<std::string>();
              org["primary_color"] =
                row["primary_color"].isNull() ? "" : row["primary_color"].as<std::string>();
              org["issuer_override"] =
                row["issuer_override"].isNull() ? "" : row["issuer_override"].as<std::string>();
              orgs.append(org);
          }
          json["organizations"] = orgs;
          json["total"] = static_cast<int>(r.size());
          (*sharedCb)(HttpResponse::newHttpJsonResponse(json));
      },
      [sharedCb](const drogon::orm::DrogonDbException &e) {
          Json::Value err;
          err["error"] = "server_error";
          err["message"] = e.base().what();
          auto resp = HttpResponse::newHttpJsonResponse(err);
          resp->setStatusCode(k500InternalServerError);
          (*sharedCb)(resp);
      }
    );
}

void OrganizationController::create(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));
    auto jsonBody = req->getJsonObject();
    if (!jsonBody)
    {
        Json::Value err;
        err["error"] = "invalid_request";
        err["error_description"] = "JSON body required";
        auto resp = HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(k400BadRequest);
        (*sharedCb)(resp);
        return;
    }

    std::string slug = (*jsonBody).get("slug", "").asString();
    std::string name = (*jsonBody).get("name", "").asString();
    std::string logoUri = (*jsonBody).get("logo_uri", "").asString();
    std::string primaryColor = (*jsonBody).get("primary_color", "").asString();
    std::string issuerOverride = (*jsonBody).get("issuer_override", "").asString();

    // Validate slug: lowercase alphanumeric + hyphens, 3-50 chars
    std::regex slugPattern("^[a-z0-9][a-z0-9-]{1,48}[a-z0-9]$");
    if (!std::regex_match(slug, slugPattern))
    {
        Json::Value err;
        err["error"] = "invalid_request";
        err["error_description"] = "slug must be 3-50 chars, lowercase alphanumeric + hyphens";
        auto resp = HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(k400BadRequest);
        (*sharedCb)(resp);
        return;
    }

    if (name.empty())
    {
        Json::Value err;
        err["error"] = "invalid_request";
        err["error_description"] = "name is required";
        auto resp = HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(k400BadRequest);
        (*sharedCb)(resp);
        return;
    }

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
      "INSERT INTO organizations (slug, name, logo_uri, primary_color, issuer_override) "
      "VALUES ($1, $2, $3, $4, $5) RETURNING id",
      [sharedCb, slug, name, req](const drogon::orm::Result &r) {
          int id = r[0]["id"].as<int>();
          oauth2::AuditLogger::log(
            "organization_created", "success", req, "", "organization", slug
          );
          Json::Value json;
          json["id"] = id;
          json["slug"] = slug;
          json["name"] = name;
          json["message"] = "Organization created";
          auto resp = HttpResponse::newHttpJsonResponse(json);
          resp->setStatusCode(k201Created);
          (*sharedCb)(resp);
      },
      [sharedCb](const drogon::orm::DrogonDbException &e) {
          Json::Value err;
          err["error"] = "conflict";
          err["error_description"] = "Organization slug already exists or DB error";
          auto resp = HttpResponse::newHttpJsonResponse(err);
          resp->setStatusCode(k409Conflict);
          (*sharedCb)(resp);
      },
      slug,
      name,
      logoUri,
      primaryColor,
      issuerOverride
    );
}

void OrganizationController::getBySlug(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback,
  const std::string &slug
)
{
    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
      "SELECT id, slug, name, logo_uri, primary_color, issuer_override, created_at "
      "FROM organizations WHERE slug = $1",
      [sharedCb](const drogon::orm::Result &r) {
          if (r.empty())
          {
              Json::Value err;
              err["error"] = "not_found";
              auto resp = HttpResponse::newHttpJsonResponse(err);
              resp->setStatusCode(k404NotFound);
              (*sharedCb)(resp);
              return;
          }
          auto row = r[0];
          Json::Value json;
          json["id"] = row["id"].as<int>();
          json["slug"] = row["slug"].as<std::string>();
          json["name"] = row["name"].as<std::string>();
          json["logo_uri"] = row["logo_uri"].isNull() ? "" : row["logo_uri"].as<std::string>();
          json["primary_color"] =
            row["primary_color"].isNull() ? "" : row["primary_color"].as<std::string>();
          json["issuer_override"] =
            row["issuer_override"].isNull() ? "" : row["issuer_override"].as<std::string>();
          (*sharedCb)(HttpResponse::newHttpJsonResponse(json));
      },
      [sharedCb](const drogon::orm::DrogonDbException &e) {
          Json::Value err;
          err["error"] = "server_error";
          auto resp = HttpResponse::newHttpJsonResponse(err);
          resp->setStatusCode(k500InternalServerError);
          (*sharedCb)(resp);
      },
      slug
    );
}
