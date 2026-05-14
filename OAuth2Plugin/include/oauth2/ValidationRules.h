#pragma once

#include <string>
#include <functional>

namespace common::validation
{

// OAuth2 validation patterns and constants
inline const char *CLIENT_ID_PATTERN = "^[a-zA-Z0-9._-]{1,128}$";
inline const size_t CLIENT_ID_MIN_LEN = 1;
inline const size_t CLIENT_ID_MAX_LEN = 128;

inline const char *REDIRECT_URI_PATTERN = "^https?://[a-zA-Z0-9.-]+(:[0-9]+)?(/[^\\s]*)?$";
inline const size_t REDIRECT_URI_MIN_LEN = 10;
inline const size_t REDIRECT_URI_MAX_LEN = 2048;

inline const char *SCOPE_PATTERN = "^[a-zA-Z0-9: ]+$";
inline const size_t SCOPE_MIN_LEN = 1;
inline const size_t SCOPE_MAX_LEN = 256;

inline const char *TOKEN_PATTERN = "^[a-zA-Z0-9._-]+$";
inline const size_t TOKEN_MIN_LEN = 32;

inline const char *RESPONSE_TYPE_PATTERN = "^[a-zA-Z0-9_]+$";
inline const char *GRANT_TYPE_PATTERN = "^[a-zA-Z0-9_]+$";
inline const char *USERNAME_PATTERN = "^[a-zA-Z0-9_]{1,100}$";
inline const char *PASSWORD_PATTERN = "^[a-zA-Z0-9!@#$%^&*()_+]{8,200}$";

enum class ValidationRuleType
{
    NOT_EMPTY,
    LENGTH_LIMIT,
    REGEX_PATTERN,
    NUMERIC_RANGE,
    URL_FORMAT,
    EMAIL_FORMAT
};

// Validation rule configuration for ValidatorHelper
struct ValidationRuleConfig
{
    std::string field;                                         // 字段名
    std::string source;                                        // "query", "body", "header"
    bool required;                                             // 是否必填
    size_t minLength = 0;                                      // 最小长度
    size_t maxLength = 0;                                      // 最大长度 (0 = 无限制)
    std::string pattern;                                       // 正则表达式
    std::function<bool(const std::string &)> customValidator;  // 自定义验证器
};

struct ValidationResult
{
    bool isValid;
    std::string fieldName;
    std::string errorMessage;

    static ValidationResult success();
    static ValidationResult failure(const std::string &field, const std::string &message);
};

}  // namespace common::validation
