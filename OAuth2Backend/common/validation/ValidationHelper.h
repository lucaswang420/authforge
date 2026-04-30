#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <vector>
#include <string>
#include <functional>
#include <json/json.h>

namespace common::validation
{

class ValidationHelper
{
  public:
    // 创建标准验证错误响应
    static drogon::HttpResponsePtr createValidationErrorResponse(
        const std::vector<std::string> &errors);

    // 快速返回单个验证错误
    static void returnValidationError(
        const std::string &field,
        const std::string &reason,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback);

    // 快速返回多个验证错误
    static void returnValidationErrors(
        const std::vector<std::string> &errors,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback);

    // 检查验证结果并返回错误（如果有）
    static bool returnValidationErrorsIfAny(
        const std::vector<std::string> &errors,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  private:
    // 创建标准错误JSON
    static Json::Value createErrorJson(const std::vector<std::string> &errors);

    // 根据环境决定错误详细程度
    static bool shouldReturnDetailedErrors();
};

}  // namespace common::validation