#include "ApiDocController.h"
#include <drogon/utils/Utilities.h>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace api;

void ApiDocController::openApiSpec(
  const drogon::HttpRequestPtr &req,
  std::function<void(const drogon::HttpResponsePtr &)> &&callback
)
{
    try
    {
        // Use document root from Drogon config as base, fallback to current
        // directory
        std::filesystem::path baseDir = drogon::app().getDocumentRoot();
        if (baseDir.empty() || baseDir == "./" || baseDir == ".")
        {
            baseDir = std::filesystem::current_path();
        }
        std::string filePath = (baseDir / "docs" / "api" / "openapi.json").string();

        // Read the OpenAPI specification file
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeString("application/json");
            resp->setBody(
              "{\"error\": \"OpenAPI specification not found\", \"path\": "
              "\"" +
              filePath + "\"}"
            );
            callback(resp);
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k200OK);
        resp->setContentTypeString("application/json");
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->setBody(content);
        callback(resp);
    }
    catch (const std::exception &e)
    {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k500InternalServerError);
        resp->setContentTypeString("application/json");
        resp->setBody("{\"error\": \"" + std::string(e.what()) + "\"}");
        callback(resp);
    }
}

void ApiDocController::swaggerUi(
  const drogon::HttpRequestPtr &req,
  std::function<void(const drogon::HttpResponsePtr &)> &&callback
)
{
    try
    {
        // Use document root from Drogon config as base, fallback to current
        // directory
        std::filesystem::path baseDir = drogon::app().getDocumentRoot();
        if (baseDir.empty() || baseDir == "./" || baseDir == ".")
        {
            baseDir = std::filesystem::current_path();
        }
        std::string filePath = (baseDir / "docs" / "api" / "swagger-ui" / "index.html").string();

        // Read the Swagger UI HTML file
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeString("text/html");
            resp->setBody(
              "<h1>Swagger UI not found</h1><p>Attempted to read from: " + filePath + "</p>"
            );
            callback(resp);
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k200OK);
        resp->setContentTypeString("text/html");
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->setBody(content);
        callback(resp);
    }
    catch (const std::exception &e)
    {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k500InternalServerError);
        resp->setContentTypeString("text/html");
        resp->setBody("<h1>Error</h1><p>" + std::string(e.what()) + "</p>");
        callback(resp);
    }
}
