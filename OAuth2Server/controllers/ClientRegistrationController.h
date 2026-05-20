#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

/**
 * @brief Dynamic Client Registration Controller (RFC 7591)
 *
 * Provides a REST API for programmatic OAuth2 client registration.
 * Endpoint: POST /oauth2/register
 *
 * Access control: Requires Bearer token with admin role (AuthorizationFilter).
 */
class ClientRegistrationController : public drogon::HttpController<ClientRegistrationController>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(
      ClientRegistrationController::registerClient,
      "/oauth2/register",
      Post,
      "AuthorizationFilter"
    );
    METHOD_LIST_END

    void registerClient(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
};
