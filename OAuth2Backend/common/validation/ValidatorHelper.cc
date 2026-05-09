#include "ValidatorHelper.h"
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <regex>
#include <algorithm>

namespace common::validation
{

std::optional<std::string> ValidatorHelper::validateField(
  const std::string &value,
  const std::string &fieldName,
  const ValidationRuleConfig &rule
)
{
    // 检查必填字段
    if (rule.required && value.empty())
    {
        return fieldName + " is required";
    }

    // 如果字段为空且非必填，跳过其他验证
    if (value.empty())
    {
        return std::nullopt;
    }

    // 长度验证
    if (value.length() < rule.minLength)
    {
        return fieldName + " must be at least " + std::to_string(rule.minLength) + " characters";
    }

    if (rule.maxLength > 0 && value.length() > rule.maxLength)
    {
        return fieldName + " must be at most " + std::to_string(rule.maxLength) + " characters";
    }

    // 正则表达式验证
    if (!rule.pattern.empty())
    {
        try
        {
            std::regex pattern(rule.pattern);
            if (!std::regex_match(value, pattern))
            {
                return fieldName + " format is invalid";
            }
        }
        catch (const std::regex_error &e)
        {
            // 如果正则表达式无效，记录警告但不阻止验证
            // 这样可以避免配置错误导致整个验证失败
        }
    }

    // 自定义验证器
    if (rule.customValidator && !rule.customValidator(value))
    {
        return fieldName + " validation failed";
    }

    return std::nullopt;
}

std::vector<std::string> ValidatorHelper::validateFields(
  const std::vector<std::pair<std::string, std::string>> &fieldsAndValues,
  const std::vector<ValidationRuleConfig> &rules
)
{
    std::vector<std::string> errors;

    for (const auto &rule : rules)
    {
        // 查找对应的字段值
        std::string value;
        bool found = false;
        for (const auto &[fieldName, fieldValue] : fieldsAndValues)
        {
            if (fieldName == rule.field)
            {
                value = fieldValue;
                found = true;
                break;
            }
        }

        // 如果字段不存在且非必填，跳过
        if (!found && !rule.required)
        {
            continue;
        }

        // 执行验证
        auto error = validateField(value, rule.field, rule);
        if (error)
        {
            errors.push_back(*error);
        }
    }

    return errors;
}

std::string ValidatorHelper::extractFieldValue(
  const drogon::HttpRequestPtr &req,
  const std::string &field,
  const std::string &source
)
{
    if (source == "query")
    {
        return req->getParameter(field);
    }
    else if (source == "body")
    {
        // 尝试从 JSON body 获取
        if (req->contentType() == drogon::CT_APPLICATION_JSON)
        {
            auto json = req->getJsonObject();
            if (json)
            {
                return json->get(field, "").asString();
            }
        }
        // 回退到 form 数据
        return req->getParameter(field);
    }
    else if (source == "header")
    {
        return req->getHeader(field);
    }

    return "";
}

std::vector<std::string> ValidatorHelper::validateRequest(
  const drogon::HttpRequestPtr &req,
  const std::vector<ValidationRuleConfig> &rules
)
{
    std::vector<std::pair<std::string, std::string>> fieldsAndValues;

    // 提取所有需要的字段值
    for (const auto &rule : rules)
    {
        std::string value = extractFieldValue(req, rule.field, rule.source);
        fieldsAndValues.push_back({rule.field, value});
    }

    return validateFields(fieldsAndValues, rules);
}

// OAuth2 专用验证方法实现

std::optional<std::string> ValidatorHelper::validateClientId(const std::string &clientId)
{
    if (clientId.empty())
    {
        return "client_id is required";
    }

    if (clientId.length() > 128)
    {
        return "client_id exceeds maximum length of 128 characters";
    }

    // 使用现有的 Validator
    auto result = Validator::validateClientId(clientId);
    if (!result.isValid)
    {
        return result.errorMessage;
    }

    return std::nullopt;
}

std::optional<std::string> ValidatorHelper::validateClientSecret(const std::string &secret)
{
    if (secret.empty())
    {
        // client_secret 可以为空（公共客户端）
        return std::nullopt;
    }

    if (secret.length() > 256)
    {
        return "client_secret exceeds maximum length of 256 characters";
    }

    auto result = Validator::validateClientSecret(secret);
    if (!result.isValid)
    {
        return result.errorMessage;
    }

    return std::nullopt;
}

std::optional<std::string> ValidatorHelper::validateRedirectUri(const std::string &uri)
{
    if (uri.empty())
    {
        return "redirect_uri is required";
    }

    auto result = Validator::validateRedirectUri(uri);
    if (!result.isValid)
    {
        return result.errorMessage;
    }

    return std::nullopt;
}

std::optional<std::string> ValidatorHelper::validateScope(const std::string &scope)
{
    // scope 是可选的
    if (scope.empty())
    {
        return std::nullopt;
    }

    auto result = Validator::validateScope(scope);
    if (!result.isValid)
    {
        return result.errorMessage;
    }

    return std::nullopt;
}

std::optional<std::string> ValidatorHelper::validateResponseType(const std::string &type)
{
    if (type.empty())
    {
        return "response_type is required";
    }

    if (type != "code")
    {
        return "response_type must be 'code'";
    }

    return std::nullopt;
}

std::optional<std::string> ValidatorHelper::validateGrantType(const std::string &type)
{
    if (type.empty())
    {
        return "grant_type is required";
    }

    if (type != "authorization_code" && type != "refresh_token")
    {
        return "grant_type must be 'authorization_code' or 'refresh_token'";
    }

    return std::nullopt;
}

std::optional<std::string> ValidatorHelper::validateToken(const std::string &token)
{
    if (token.empty())
    {
        return "token is required";
    }

    auto result = Validator::validateToken(token);
    if (!result.isValid)
    {
        return result.errorMessage;
    }

    return std::nullopt;
}

// 便捷验证组合方法

std::vector<std::string> ValidatorHelper::validateOAuth2AuthorizeParams(
  const drogon::HttpRequestPtr &req
)
{
    std::vector<std::string> errors;

    // 验证 client_id
    auto clientId = req->getParameter("client_id");
    auto error1 = validateClientId(clientId);
    if (error1)
    {
        errors.push_back(*error1);
    }

    // 验证 redirect_uri
    auto redirectUri = req->getParameter("redirect_uri");
    auto error2 = validateRedirectUri(redirectUri);
    if (error2)
    {
        errors.push_back(*error2);
    }

    // 验证 response_type
    auto responseType = req->getParameter("response_type");
    auto error3 = validateResponseType(responseType);
    if (error3)
    {
        errors.push_back(*error3);
    }

    // 验证 scope (可选)
    auto scope = req->getParameter("scope");
    if (!scope.empty())
    {
        auto error4 = validateScope(scope);
        if (error4)
        {
            errors.push_back(*error4);
        }
    }

    return errors;
}

std::vector<std::string> ValidatorHelper::validateOAuth2TokenParams(
  const drogon::HttpRequestPtr &req
)
{
    std::vector<std::string> errors;

    // 优先从 POST body 获取参数
    std::string grantType, code, clientId, redirectUri, refreshToken;

    if (req->method() == drogon::Post)
    {
        auto params = req->getParameters();
        grantType = params["grant_type"];
        code = params["code"];
        clientId = params["client_id"];
        redirectUri = params["redirect_uri"];
        refreshToken = params["refresh_token"];
    }
    else
    {
        grantType = req->getParameter("grant_type");
        code = req->getParameter("code");
        clientId = req->getParameter("client_id");
        redirectUri = req->getParameter("redirect_uri");
        refreshToken = req->getParameter("refresh_token");
    }

    // 验证 grant_type
    auto error1 = validateGrantType(grantType);
    if (error1)
    {
        errors.push_back(*error1);
    }

    // 根据 grant_type 验证其他参数
    if (grantType == "authorization_code")
    {
        auto error2 = validateToken(code);
        if (error2)
        {
            errors.push_back("code: " + *error2);
        }

        if (!clientId.empty())
        {
            auto error3 = validateClientId(clientId);
            if (error3)
            {
                errors.push_back(*error3);
            }
        }
    }
    else if (grantType == "refresh_token")
    {
        auto error4 = validateToken(refreshToken);
        if (error4)
        {
            errors.push_back("refresh_token: " + *error4);
        }
    }

    return errors;
}

std::vector<std::string> ValidatorHelper::validateLoginParams(const drogon::HttpRequestPtr &req)
{
    std::vector<std::string> errors;

    // 优先从 POST body 获取参数
    std::string username, password;

    if (req->contentType() == drogon::CT_APPLICATION_JSON)
    {
        auto json = req->getJsonObject();
        if (json)
        {
            username = json->get("username", "").asString();
            password = json->get("password", "").asString();
        }
    }
    else
    {
        auto params = req->getParameters();
        username = params["username"];
        password = params["password"];
    }

    // 验证 username
    if (username.empty())
    {
        errors.push_back("username is required");
    }
    else if (username.length() > 100)
    {
        errors.push_back("username exceeds maximum length of 100 characters");
    }

    // 验证 password
    if (password.empty())
    {
        errors.push_back("password is required");
    }
    else if (password.length() > 200)
    {
        errors.push_back("password exceeds maximum length of 200 characters");
    }

    return errors;
}

}  // namespace common::validation
