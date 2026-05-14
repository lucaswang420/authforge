#pragma once

#include <drogon/HttpFilter.h>
#include <drogon/drogon.h>
#include <string>
#include <vector>
#include <map>
#include <oauth2/ValidationHelper.h>
#include <oauth2/ValidatorHelper.h>

using namespace drogon;

/**
 * @brief ValidationFilter - 自动验证HTTP请求的Filter
 *
 * 在Controller执行前自动验证请求参数，处理基础、通用的验证规则：
 * - 格式验证（长度、字符集）
 * - 必填字段检查
 * - OAuth2专用验证
 */
class ValidationFilter : public HttpFilter<ValidationFilter>
{
  public:
    ValidationFilter() = default;
    ~ValidationFilter() override = default;

    void doFilter(
      const HttpRequestPtr &req,
      FilterCallback &&fcb,
      FilterChainCallback &&fccb
    ) override;

  private:
    // 定义路由验证规则
    struct RouteValidationRules
    {
        std::vector<common::validation::ValidationRuleConfig> rules;
        bool enabled;
    };

    // 获取路由的验证规则
    RouteValidationRules getValidationRules(const std::string &path) const;

    // OAuth2 路由验证规则
    static std::map<std::string, RouteValidationRules> OAUTH2_VALIDATION_RULES;

    // 初始化验证规则
    static void initializeValidationRules();
};
