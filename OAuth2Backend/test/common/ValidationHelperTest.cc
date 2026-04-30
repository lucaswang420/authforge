#include <drogon/drogon_test.h>
#include <drogon/drogon.h>
#include "common/validation/ValidatorHelper.h"
#include "common/validation/ValidationHelper.h"

using namespace drogon;
using namespace common::validation;

DROGON_TEST(ValidatorHelper_ValidateClientId_Valid)
{
    auto result = ValidatorHelper::validateClientId("my-client_123.app");
    CHECK(!result.has_value());
}

DROGON_TEST(ValidatorHelper_ValidateClientId_Invalid)
{
    auto result = ValidatorHelper::validateClientId("invalid@client!");
    CHECK(result.has_value());
    CHECK(result->find("alphanumeric") != std::string::npos);
}

DROGON_TEST(ValidatorHelper_ValidateClientId_Empty)
{
    auto result = ValidatorHelper::validateClientId("");
    CHECK(result.has_value());
    CHECK(result->find("required") != std::string::npos);
}

DROGON_TEST(ValidatorHelper_ValidateRedirectUri_Valid)
{
    auto result = ValidatorHelper::validateRedirectUri("https://example.com/callback");
    CHECK(!result.has_value());

    result = ValidatorHelper::validateRedirectUri("http://localhost:3000/auth");
    CHECK(!result.has_value());
}

DROGON_TEST(ValidatorHelper_ValidateRedirectUri_Invalid)
{
    auto result = ValidatorHelper::validateRedirectUri("ftp://invalid.com");
    CHECK(result.has_value());

    result = ValidatorHelper::validateRedirectUri("not-a-url");
    CHECK(result.has_value());
}

DROGON_TEST(ValidatorHelper_ValidateResponseType_Valid)
{
    auto result = ValidatorHelper::validateResponseType("code");
    CHECK(!result.has_value());
}

DROGON_TEST(ValidatorHelper_ValidateResponseType_Invalid)
{
    auto result = ValidatorHelper::validateResponseType("invalid");
    CHECK(result.has_value());
}

DROGON_TEST(ValidatorHelper_ValidateGrantType_Valid)
{
    auto result = ValidatorHelper::validateGrantType("authorization_code");
    CHECK(!result.has_value());

    result = ValidatorHelper::validateGrantType("refresh_token");
    CHECK(!result.has_value());
}

DROGON_TEST(ValidatorHelper_ValidateGrantType_Invalid)
{
    auto result = ValidatorHelper::validateGrantType("invalid_grant");
    CHECK(result.has_value());
}

DROGON_TEST(ValidatorHelper_ValidateToken_Valid)
{
    auto result = ValidatorHelper::validateToken("abcdefghijklmnopqrstuvwxyz123456");
    CHECK(!result.has_value());
}

DROGON_TEST(ValidatorHelper_ValidateToken_Invalid)
{
    auto result = ValidatorHelper::validateToken("too-short");
    CHECK(result.has_value());
}

DROGON_TEST(ValidatorHelper_ValidateScope_Valid)
{
    auto result = ValidatorHelper::validateScope("read write");
    CHECK(!result.has_value());

    result = ValidatorHelper::validateScope("profile:read email:write");
    CHECK(!result.has_value());
}

DROGON_TEST(ValidatorHelper_ValidateScope_Empty)
{
    auto result = ValidatorHelper::validateScope("");
    CHECK(!result.has_value());  // scope is optional
}

DROGON_TEST(ValidationHelper_CreateErrorResponse)
{
    std::vector<std::string> errors = {"field1: error1", "field2: error2"};
    auto response = ValidationHelper::createValidationErrorResponse(errors);

    CHECK(response->statusCode() == k400BadRequest);
}

DROGON_TEST(ValidatorHelper_ValidateField_Config)
{
    ValidationRuleConfig config;
    config.field = "test_field";
    config.required = true;
    config.minLength = 3;
    config.maxLength = 10;
    config.pattern = "^[a-zA-Z0-9]+$";

    auto result = ValidatorHelper::validateField("valid123", "test_field", config);
    CHECK(!result.has_value());

    result = ValidatorHelper::validateField("ab", "test_field", config);
    CHECK(result.has_value());
    CHECK(result->find("at least") != std::string::npos);

    result = ValidatorHelper::validateField("way_too_long_field", "test_field", config);
    CHECK(result.has_value());
    CHECK(result->find("at most") != std::string::npos);

    result = ValidatorHelper::validateField("invalid@chars", "test_field", config);
    CHECK(result.has_value());  // Should fail pattern validation
}

DROGON_TEST(ValidatorHelper_ValidateField_EmptyRequired)
{
    ValidationRuleConfig config;
    config.field = "required_field";
    config.required = true;

    auto result = ValidatorHelper::validateField("", "required_field", config);
    CHECK(result.has_value());
    CHECK(result->find("required") != std::string::npos);
}

DROGON_TEST(ValidatorHelper_ValidateField_EmptyOptional)
{
    ValidationRuleConfig config;
    config.field = "optional_field";
    config.required = false;

    auto result = ValidatorHelper::validateField("", "optional_field", config);
    CHECK(!result.has_value());
}

DROGON_TEST(ValidatorHelper_ValidateFields_Multiple)
{
    std::vector<std::pair<std::string, std::string>> fields = {
        {"username", "valid_user"},
        {"password", "pass123"},
        {"email", "invalid@email"}  // will fail pattern validation if pattern is set
    };

    std::vector<ValidationRuleConfig> rules = {
        {"username", "body", true, 3, 20, "^[a-zA-Z0-9_]+$", nullptr},
        {"password", "body", true, 6, 50, "^[a-zA-Z0-9]+$", nullptr},
        {"email", "body", false, 0, 100, "", nullptr}
    };

    auto errors = ValidatorHelper::validateFields(fields, rules);
    CHECK(errors.empty());  // All should pass with these rules
}