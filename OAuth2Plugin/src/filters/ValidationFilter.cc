#include <oauth2/filters/ValidationFilter.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <string>
#include <mutex>

using namespace common::validation;

// 静态成员初始化
std::map<std::string, ValidationFilter::RouteValidationRules>
  ValidationFilter::OAUTH2_VALIDATION_RULES;

void ValidationFilter::initializeValidationRules()
{
    // /oauth2/authorize 验证规则
    OAUTH2_VALIDATION_RULES["/oauth2/authorize"] = {
      {{"client_id", "query", true, 1, 128, CLIENT_ID_PATTERN, nullptr},
       {"redirect_uri", "query", true, 10, 2048, REDIRECT_URI_PATTERN, nullptr},
       {"response_type", "query", true, 1, 10, RESPONSE_TYPE_PATTERN, nullptr}},
      true  // enabled
    };

    // /oauth2/token 验证规则
    OAUTH2_VALIDATION_RULES["/oauth2/token"] = {
      {{"grant_type", "body", true, 1, 50, GRANT_TYPE_PATTERN, nullptr}},
      true  // enabled
    };

    // /oauth2/login 验证规则
    OAUTH2_VALIDATION_RULES["/oauth2/login"] = {
      {{"username", "body", true, 1, 100, USERNAME_PATTERN, nullptr},
       {"password", "body", true, 8, 200, PASSWORD_PATTERN, nullptr}},
      true  // enabled
    };

    // /api/register 验证规则
    OAUTH2_VALIDATION_RULES["/api/register"] = {
      {{"username", "body", true, 1, 100, USERNAME_PATTERN, nullptr},
       {"password", "body", true, 8, 200, PASSWORD_PATTERN, nullptr}},
      true  // enabled
    };
}

ValidationFilter::RouteValidationRules ValidationFilter::getValidationRules(
  const std::string &path
) const
{
    // 线程安全的单次初始化
    static std::once_flag initFlag;
    std::call_once(initFlag, []() { initializeValidationRules(); });

    // 精确匹配
    auto it = OAUTH2_VALIDATION_RULES.find(path);
    if (it != OAUTH2_VALIDATION_RULES.end())
    {
        return it->second;
    }

    // 模式匹配 (支持路径参数，如 /api/user/:id)
    for (const auto &[routePattern, rules] : OAUTH2_VALIDATION_RULES)
    {
        if (path == routePattern)
        {
            return rules;
        }

        // 简单的模式匹配（可以扩展为正则表达式）
        size_t pos = routePattern.find(":");
        if (pos != std::string::npos)
        {
            std::string prefix = routePattern.substr(0, pos);
            if (path.length() > prefix.length() && path.substr(0, prefix.length()) == prefix)
            {
                return rules;
            }
        }
    }

    // 没有找到匹配规则，禁用验证
    return {{}, false};
}

void ValidationFilter::doFilter(
  const HttpRequestPtr &req,
  FilterCallback &&fcb,
  FilterChainCallback &&fccb
)
{
    // 获取请求路径
    std::string path = req->path();

    // 获取验证规则
    auto routeRules = getValidationRules(path);

    // 如果没有规则或规则被禁用，直接通过
    if (!routeRules.enabled || routeRules.rules.empty())
    {
        fccb();
        return;
    }

    // 执行验证
    auto errors = ValidatorHelper::validateRequest(req, routeRules.rules);

    // 如果有验证错误，返回错误响应
    if (!errors.empty())
    {
        ValidationHelper::returnValidationErrors(errors, std::move(fcb));
        return;
    }

    // 验证通过，继续执行
    fccb();
}
