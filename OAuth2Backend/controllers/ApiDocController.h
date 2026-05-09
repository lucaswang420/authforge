#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <functional>

namespace api
{

/**
 * @brief API Documentation Controller
 *
 * Serves OpenAPI specification and Swagger UI for API documentation
 */
class ApiDocController : public drogon::HttpController<ApiDocController>
{
  public:
    METHOD_LIST_BEGIN
    // Serve OpenAPI JSON specification
    ADD_METHOD_TO(ApiDocController::openApiSpec, "/docs/api/openapi.json", drogon::Get);

    // Serve Swagger UI main page
    ADD_METHOD_TO(ApiDocController::swaggerUi, "/docs/api", drogon::Get);
    ADD_METHOD_TO(ApiDocController::swaggerUi, "/docs/api/", drogon::Get);
    METHOD_LIST_END

    /**
     * @brief Serve OpenAPI JSON specification
     * @param req HTTP request
     * @param callback Response callback
     */
    void openApiSpec(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback
    );

    /**
     * @brief Serve Swagger UI HTML page
     * @param req HTTP request
     * @param callback Response callback
     */
    void swaggerUi(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback
    );
};

}  // namespace api
