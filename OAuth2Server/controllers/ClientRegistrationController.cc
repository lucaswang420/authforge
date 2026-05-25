#include "ClientRegistrationController.h"
#include <oauth2/CryptoUtils.h>
#include <oauth2/AuditLogger.h>
#include <oauth2/OpenApiGenerator.h>
#include <drogon/drogon.h>
#include <drogon/utils/Utilities.h>

namespace {
struct ClientRegistrationControllerDocs {
    ClientRegistrationControllerDocs() {
        common::documentation::EndpointInfo regDocs;
        regDocs.path = "/oauth2/register";
        regDocs.method = "POST";
        regDocs.summary = "Register Client";
        regDocs.description = "Dynamic client registration.";
        regDocs.tags = {"OAuth2", "Dynamic Registration"};
        regDocs.requiresAuth = true;
        common::documentation::OpenApiGenerator::addEndpoint(regDocs);
    }
};

ClientRegistrationControllerDocs docs_;
}  // namespace

void ClientRegistrationController::registerClient(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback
)
{
    auto sharedCb =
      std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    // Parse request body
    auto jsonBody = req->getJsonObject();
    if (!jsonBody)
    {
        Json::Value json;
        json["error"] = "invalid_client_metadata";
        json["error_description"] = "Request body must be valid JSON";
        auto resp = HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(k400BadRequest);
        (*sharedCb)(resp);
        return;
    }

    // Extract fields from request
    std::string clientName = (*jsonBody).get("client_name", "").asString();
    std::string clientType = (*jsonBody).get("client_type", "CONFIDENTIAL").asString();
    std::string tokenEndpointAuthMethod =
      (*jsonBody).get("token_endpoint_auth_method", "client_secret_basic").asString();

    // Validate client_name is provided
    if (clientName.empty())
    {
        Json::Value json;
        json["error"] = "invalid_client_metadata";
        json["error_description"] = "client_name is required";
        auto resp = HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(k400BadRequest);
        (*sharedCb)(resp);
        return;
    }

    // Validate client_type
    if (clientType != "CONFIDENTIAL" && clientType != "PUBLIC")
    {
        Json::Value json;
        json["error"] = "invalid_client_metadata";
        json["error_description"] = "client_type must be CONFIDENTIAL or PUBLIC";
        auto resp = HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(k400BadRequest);
        (*sharedCb)(resp);
        return;
    }

    // Parse redirect_uris (array -> comma-separated string for storage)
    std::string redirectUris;
    Json::Value redirectUrisArray(Json::arrayValue);
    if (jsonBody->isMember("redirect_uris") && (*jsonBody)["redirect_uris"].isArray())
    {
        const auto &uris = (*jsonBody)["redirect_uris"];
        for (Json::ArrayIndex i = 0; i < uris.size(); ++i)
        {
            if (i > 0)
                redirectUris += ",";
            redirectUris += uris[i].asString();
            redirectUrisArray.append(uris[i].asString());
        }
    }

    // Validate at least one redirect_uri for confidential clients
    if (redirectUris.empty() && clientType == "CONFIDENTIAL")
    {
        Json::Value json;
        json["error"] = "invalid_client_metadata";
        json["error_description"] = "redirect_uris is required for confidential clients";
        auto resp = HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(k400BadRequest);
        (*sharedCb)(resp);
        return;
    }

    // Parse grant_types (array -> comma-separated string for storage)
    std::string allowedGrantTypes;
    Json::Value grantTypesArray(Json::arrayValue);
    if (jsonBody->isMember("grant_types") && (*jsonBody)["grant_types"].isArray())
    {
        const auto &grants = (*jsonBody)["grant_types"];
        for (Json::ArrayIndex i = 0; i < grants.size(); ++i)
        {
            std::string grantType = grants[i].asString();
            // Validate grant type
            if (
              grantType != "authorization_code" && grantType != "refresh_token" &&
              grantType != "client_credentials"
            )
            {
                Json::Value json;
                json["error"] = "invalid_client_metadata";
                json["error_description"] =
                  "Unsupported grant_type: " + grantType +
                  ". Allowed: authorization_code, refresh_token, client_credentials";
                auto resp = HttpResponse::newHttpJsonResponse(json);
                resp->setStatusCode(k400BadRequest);
                (*sharedCb)(resp);
                return;
            }
            if (i > 0)
                allowedGrantTypes += ",";
            allowedGrantTypes += grantType;
            grantTypesArray.append(grantType);
        }
    }
    else
    {
        // Default grant type
        allowedGrantTypes = "authorization_code";
        grantTypesArray.append("authorization_code");
    }

    // Generate client credentials
    std::string clientId = drogon::utils::getUuid();
    std::string clientSecret = oauth2::utils::generateSecureToken();
    std::string secretHash = oauth2::utils::hashToken(clientSecret);
    std::string salt = drogon::utils::getUuid().substr(0, 36);

    // Current timestamp for client_id_issued_at
    auto now = std::chrono::system_clock::now();
    auto issuedAt =
      std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    try
    {
        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
          "INSERT INTO oauth2_clients (client_id, client_type, client_secret, salt, name, "
          "redirect_uris, allowed_grant_types) VALUES ($1, $2, $3, $4, $5, $6, $7)",
          [sharedCb,
           clientId,
           clientSecret,
           clientName,
           redirectUrisArray,
           grantTypesArray,
           issuedAt,
           tokenEndpointAuthMethod,
           req](const drogon::orm::Result &) {
              // Build RFC 7591 compliant response
              Json::Value json;
              json["client_id"] = clientId;
              json["client_secret"] = clientSecret;  // Only returned once at creation time
              json["client_name"] = clientName;
              json["redirect_uris"] = redirectUrisArray;
              json["grant_types"] = grantTypesArray;
              json["token_endpoint_auth_method"] = tokenEndpointAuthMethod;
              json["client_id_issued_at"] = static_cast<Json::Int64>(issuedAt);
              json["client_secret_expires_at"] = 0;  // Does not expire

              auto resp = HttpResponse::newHttpJsonResponse(json);
              resp->setStatusCode(k201Created);

              // Audit log the registration
              oauth2::AuditLogger::log(
                "client_registered",
                "success",
                req,
                "",  // actorId from token (handled by filter)
                "client",
                clientId
              );

              (*sharedCb)(resp);
          },
          [sharedCb, req](const drogon::orm::DrogonDbException &e) {
              Json::Value json;
              json["error"] = "server_error";
              json["error_description"] = "Failed to register client";
              auto resp = HttpResponse::newHttpJsonResponse(json);
              resp->setStatusCode(k500InternalServerError);

              // Audit log the failure
              oauth2::AuditLogger::log(
                "client_registered", "failure", req, "", "client", "", Json::Value(e.base().what())
              );

              (*sharedCb)(resp);
          },
          clientId,
          clientType,
          secretHash,
          salt,
          clientName,
          redirectUris,
          allowedGrantTypes
        );
    }
    catch (...)
    {
        Json::Value json;
        json["error"] = "server_error";
        json["error_description"] = "Database unavailable";
        auto resp = HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(k500InternalServerError);
        (*sharedCb)(resp);
    }
}
