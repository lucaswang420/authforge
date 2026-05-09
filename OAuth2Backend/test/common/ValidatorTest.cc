#include <drogon/drogon_test.h>
#include <drogon/drogon.h>
#include "common/validation/Validator.h"

using namespace drogon;
using namespace common::validation;

DROGON_TEST(ValidateValidClientId)
{
    auto result = Validator::validateClientId("my-client_123.app");
    CHECK(result.isValid);
}

DROGON_TEST(ValidateInvalidClientId)
{
    auto result = Validator::validateClientId("invalid@client!");
    CHECK(!result.isValid);
    CHECK(result.errorMessage.find("alphanumeric") != std::string::npos);
}

DROGON_TEST(ValidateRedirectUri)
{
    CHECK(Validator::validateRedirectUri("https://example.com/callback").isValid);
    CHECK(Validator::validateRedirectUri("http://localhost:3000/auth").isValid);
    CHECK(!Validator::validateRedirectUri("ftp://invalid.com").isValid);
    CHECK(!Validator::validateRedirectUri("not-a-url").isValid);
}

DROGON_TEST(ValidateToken)
{
    CHECK(Validator::validateToken("abcdefghijklmnopqrstuvwxyz123456").isValid);
    CHECK(!Validator::validateToken("too-short").isValid);
}

DROGON_TEST(ValidateScope)
{
    CHECK(Validator::validateScope("read write").isValid);
    CHECK(Validator::validateScope("profile:read email:write").isValid);
    CHECK(!Validator::validateScope("invalid@scope!").isValid);
}

DROGON_TEST(ValidateResponseType)
{
    CHECK(Validator::validateResponseType("code").isValid);
    CHECK(Validator::validateResponseType("token").isValid);
    CHECK(!Validator::validateResponseType("invalid").isValid);
}

DROGON_TEST(ValidateGrantType)
{
    CHECK(Validator::validateGrantType("authorization_code").isValid);
    CHECK(Validator::validateGrantType("refresh_token").isValid);
    CHECK(!Validator::validateGrantType("invalid_grant").isValid);
}

DROGON_TEST(ValidateClientSecret)
{
    CHECK(Validator::validateClientSecret("my-secret-key-123").isValid);
    CHECK(!Validator::validateClientSecret("short").isValid);
}

DROGON_TEST(BasicValidation_NotEmpty)
{
    auto result = Validator::notEmpty("valid", "field");
    CHECK(result.isValid);

    auto emptyResult = Validator::notEmpty("", "field");
    CHECK(!emptyResult.isValid);
    CHECK(emptyResult.errorMessage.find("empty") != std::string::npos);
}

DROGON_TEST(BasicValidation_Length)
{
    auto result = Validator::length("valid", "field", 3, 10);
    CHECK(result.isValid);

    auto tooShort = Validator::length("ab", "field", 3, 10);
    CHECK(!tooShort.isValid);

    auto tooLong = Validator::length("this_is_way_too_long", "field", 3, 10);
    CHECK(!tooLong.isValid);
}

DROGON_TEST(BasicValidation_Regex)
{
    auto result = Validator::regex("test123", "field", "^[a-z0-9]+$");
    CHECK(result.isValid);

    auto invalid = Validator::regex("test@123", "field", "^[a-z0-9]+$");
    CHECK(!invalid.isValid);
}

DROGON_TEST(BasicValidation_NumericRange)
{
    auto result = Validator::numericRange(5, "field", 1, 10);
    CHECK(result.isValid);

    auto outOfRange = Validator::numericRange(15, "field", 1, 10);
    CHECK(!outOfRange.isValid);
}
