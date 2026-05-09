#include <drogon/drogon_test.h>
#include "common/validation/Validator.h"

using namespace common::validation;

DROGON_TEST(SecurityValidationTest_RejectInvalidClientId)
{
    auto result = Validator::validateClientId("invalid@client!");
    CHECK(!result.isValid);
}

DROGON_TEST(SecurityValidationTest_RejectInvalidRedirectUri)
{
    auto result = Validator::validateRedirectUri("ftp://malicious.com");
    CHECK(!result.isValid);
}

DROGON_TEST(SecurityValidationTest_AcceptValidClientId)
{
    auto result = Validator::validateClientId("my-client_123.app");
    CHECK(result.isValid);
}

DROGON_TEST(SecurityValidationTest_AcceptValidHttpsRedirectUri)
{
    auto result = Validator::validateRedirectUri("https://example.com/callback");
    CHECK(result.isValid);
}

DROGON_TEST(SecurityValidationTest_AcceptValidHttpRedirectUri)
{
    auto result = Validator::validateRedirectUri("http://localhost:8080/callback");
    CHECK(result.isValid);
}

DROGON_TEST(SecurityValidationTest_AcceptValidScope)
{
    auto result = Validator::validateScope("read write");
    CHECK(result.isValid);
}

DROGON_TEST(SecurityValidationTest_ValidateResponseType_Code)
{
    auto result = Validator::validateResponseType("code");
    CHECK(result.isValid);
}

DROGON_TEST(SecurityValidationTest_ValidateResponseType_Token)
{
    auto result = Validator::validateResponseType("token");
    CHECK(result.isValid);
}

DROGON_TEST(SecurityValidationTest_ValidateResponseType_Invalid)
{
    auto result = Validator::validateResponseType("invalid");
    CHECK(!result.isValid);
}

DROGON_TEST(SecurityValidationTest_ValidateGrantType_AuthCode)
{
    auto result = Validator::validateGrantType("authorization_code");
    CHECK(result.isValid);
}

DROGON_TEST(SecurityValidationTest_ValidateGrantType_RefreshToken)
{
    auto result = Validator::validateGrantType("refresh_token");
    CHECK(result.isValid);
}

DROGON_TEST(SecurityValidationTest_ValidateGrantType_Invalid)
{
    auto result = Validator::validateGrantType("invalid");
    CHECK(!result.isValid);
}

DROGON_TEST(SecurityValidationTest_ValidateToken_Valid)
{
    auto result = Validator::validateToken("abcdefghijklmnopqrstuvwxyz123456");
    CHECK(result.isValid);
}

DROGON_TEST(SecurityValidationTest_ValidateToken_Empty)
{
    auto result = Validator::validateToken("");
    CHECK(!result.isValid);
}

DROGON_TEST(SecurityValidationTest_ValidateClientSecret_Valid)
{
    auto result = Validator::validateClientSecret("valid_secret_123");
    CHECK(result.isValid);
}

DROGON_TEST(SecurityValidationTest_ValidateClientSecret_Short)
{
    auto result = Validator::validateClientSecret("short");
    CHECK(!result.isValid);
}
