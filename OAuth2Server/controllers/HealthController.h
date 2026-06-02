#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

class HealthController : public drogon::HttpController<HealthController>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(HealthController::healthLive, "/health/live", Get);
    ADD_METHOD_TO(HealthController::healthReady, "/health/ready", Get);
    ADD_METHOD_TO(HealthController::health, "/health", Get);
    METHOD_LIST_END

    void health(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
    void healthLive(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
    void healthReady(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );
};
