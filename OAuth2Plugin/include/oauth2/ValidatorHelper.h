#pragma once

#include "Validator.h"
#include "ValidationRules.h"
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

namespace common::validation
{

class ValidatorHelper
{
  public:
    // 快速验证单个字段，返回错误信息（如果有）
    static std::optional<std::string> validateField(
      const std::string &value,
      const std::string &fieldName,
      const ValidationRuleConfig &rule
    );

    // 批量验证多个字段，返回所有错误信息
    static std::vector<std::string> validateFields(
      const std::vector<std::pair<std::string, std::string>> &fieldsAndValues,
      const std::vector<ValidationRuleConfig> &rules
    );

    // 从 HttpRequest 提取并验证字段
    static std::vector<std::string> validateRequest(
      const drogon::HttpRequestPtr &req,
      const std::vector<ValidationRuleConfig> &rules
    );

    // OAuth2 专用快速验证方法
    static std::optional<std::string> validateClientId(const std::string &clientId);
    static std::optional<std::string> validateClientSecret(const std::string &secret);
    static std::optional<std::string> validateRedirectUri(const std::string &uri);
    static std::optional<std::string> validateScope(const std::string &scope);
    static std::optional<std::string> validateResponseType(const std::string &type);
    static std::optional<std::string> validateGrantType(const std::string &type);
    static std::optional<std::string> validateToken(const std::string &token);

    // 便捷验证组合
    static std::vector<std::string> validateOAuth2AuthorizeParams(
      const drogon::HttpRequestPtr &req
    );
    static std::vector<std::string> validateOAuth2TokenParams(const drogon::HttpRequestPtr &req);
    static std::vector<std::string> validateLoginParams(const drogon::HttpRequestPtr &req);

    // ========== P1: Token Introspection & Revocation Validation ==========

    static std::vector<std::string> validateOAuth2IntrospectParams(
      const drogon::HttpRequestPtr &req
    );
    static std::vector<std::string> validateOAuth2RevokeParams(const drogon::HttpRequestPtr &req);

  private:
    // 从 HttpRequest 提取字段值
    static std::string extractFieldValue(
      const drogon::HttpRequestPtr &req,
      const std::string &field,
      const std::string &source
    );

    // 根据源位置获取字段值
    static std::string getValueFromSource(
      const drogon::HttpRequestPtr &req,
      const std::string &field,
      const std::string &source
    );
};

}  // namespace common::validation
