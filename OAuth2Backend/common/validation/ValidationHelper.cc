#include "ValidationHelper.h"
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/utils/Utilities.h>
#include <sstream>
#include <ctime>

namespace common::validation
{

drogon::HttpResponsePtr ValidationHelper::createValidationErrorResponse(
  const std::vector<std::string> &errors
)
{
    Json::Value root = createErrorJson(errors);

    auto resp = drogon::HttpResponse::newHttpJsonResponse(root);
    resp->setStatusCode(drogon::k400BadRequest);
    return resp;
}

void ValidationHelper::returnValidationError(
  const std::string &field,
  const std::string &reason,
  std::function<void(const drogon::HttpResponsePtr &)> &&callback
)
{
    std::vector<std::string> errors;
    errors.push_back(field + ": " + reason);

    auto resp = createValidationErrorResponse(errors);
    callback(resp);
}

void ValidationHelper::returnValidationErrors(
  const std::vector<std::string> &errors,
  std::function<void(const drogon::HttpResponsePtr &)> &&callback
)
{
    auto resp = createValidationErrorResponse(errors);
    callback(resp);
}

bool ValidationHelper::returnValidationErrorsIfAny(
  const std::vector<std::string> &errors,
  std::function<void(const drogon::HttpResponsePtr &)> &&callback
)
{
    if (!errors.empty())
    {
        returnValidationErrors(errors, std::move(callback));
        return true;
    }
    return false;
}

Json::Value ValidationHelper::createErrorJson(const std::vector<std::string> &errors)
{
    Json::Value error;
    error["code"] = "VALIDATION_ERROR";
    error["message"] = "Validation failed";

    // 根据环境决定详细程度
    if (shouldReturnDetailedErrors())
    {
        Json::Value details(Json::objectValue);
        for (size_t i = 0; i < errors.size(); ++i)
        {
            std::string key = "field_" + std::to_string(i + 1);
            details[key] = errors[i];
        }
        error["details"] = details;
    }
    else
    {
        // 生产环境：只返回第一个错误（保护内部信息）
        if (!errors.empty())
        {
            error["reason"] = errors[0];
        }
    }

    // 添加时间戳
    std::time_t now = std::time(nullptr);
    char timestamp[64];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
    error["timestamp"] = timestamp;

    Json::Value root;
    root["error"] = error;
    return root;
}

bool ValidationHelper::shouldReturnDetailedErrors()
{
#ifdef DEBUG
    return true;
#else
    // 检查环境变量或配置
    const char *env = std::getenv("DETAILED_VALIDATION_ERRORS");
    return env && (std::string(env) == "1" || std::string(env) == "true");
#endif
}

}  // namespace common::validation
