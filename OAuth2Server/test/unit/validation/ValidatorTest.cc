#include <drogon/drogon_test.h>
#include <oauth2/Validator.h>
#include <vector>
#include <string>

using namespace drogon;
using namespace common::validation;

DROGON_TEST(Unit_P0_Validation_ClientId_AllScenarios)
{
    struct TestCase
    {
        std::string clientId;
        bool shouldBeValid;
    };

    std::vector<TestCase> testCases =
      {{"my-client_123.app", true},
       {"client-1", true},
       {"invalid@client!", false},
       {"", false},
       {std::string(100, 'a'), true}};

    for (const auto &tc : testCases)
    {
        auto result = Validator::validateClientId(tc.clientId);
        CHECK(result.isValid == tc.shouldBeValid);
    }
}

DROGON_TEST(Unit_P0_Validation_ClientSecret_AllScenarios)
{
    struct TestCase
    {
        std::string secret;
        bool shouldBeValid;
    };

    std::vector<TestCase> testCases =
      {{"my-secret-key-123", true},
       {"ComplexP@ssw0rd!", true},
       {"short", false},
       {"", false},
       {std::string(200, 'a'), true}};

    for (const auto &tc : testCases)
    {
        auto result = Validator::validateClientSecret(tc.secret);
        CHECK(result.isValid == tc.shouldBeValid);
    }
}

DROGON_TEST(Unit_P0_Validation_RedirectUri_AllScenarios)
{
    struct TestCase
    {
        std::string uri;
        bool shouldBeValid;
    };

    std::vector<TestCase> testCases =
      {{"https://example.com/callback", true},
       {"http://localhost:3000/auth", true},
       {"ftp://invalid.com", false},
       {"not-a-url", false},
       {"", false}};

    for (const auto &tc : testCases)
    {
        auto result = Validator::validateRedirectUri(tc.uri);
        CHECK(result.isValid == tc.shouldBeValid);
    }
}

DROGON_TEST(Unit_P0_Validation_Token_AllScenarios)
{
    struct TestCase
    {
        std::string token;
        bool shouldBeValid;
    };

    std::vector<TestCase> testCases =
      {{"abcdefghijklmnopqrstuvwxyz123456", true}, {"too-short", false}, {"", false}};

    for (const auto &tc : testCases)
    {
        auto result = Validator::validateToken(tc.token);
        CHECK(result.isValid == tc.shouldBeValid);
    }
}

DROGON_TEST(Unit_P0_Validation_Scope_AllScenarios)
{
    struct TestCase
    {
        std::string scope;
        bool shouldBeValid;
    };

    std::vector<TestCase> testCases =
      {{"read write", true},
       {"profile:read email:write", true},
       {"", false},
       {"invalid@scope!", false}};

    for (const auto &tc : testCases)
    {
        auto result = Validator::validateScope(tc.scope);
        CHECK(result.isValid == tc.shouldBeValid);
    }
}

DROGON_TEST(Unit_P0_Validation_OAuthTypes_AllScenarios)
{
    CHECK(Validator::validateResponseType("code").isValid);
    CHECK(Validator::validateResponseType("token").isValid);
    CHECK(!Validator::validateResponseType("invalid").isValid);

    CHECK(Validator::validateGrantType("authorization_code").isValid);
    CHECK(Validator::validateGrantType("refresh_token").isValid);
    CHECK(Validator::validateGrantType("client_credentials").isValid);
    CHECK(!Validator::validateGrantType("invalid_grant").isValid);
}

DROGON_TEST(Unit_P1_Validation_Primitives_BasicRules)
{
    CHECK(Validator::notEmpty("valid", "field").isValid);
    CHECK(!Validator::notEmpty("", "field").isValid);

    CHECK(Validator::length("valid", "field", 3, 10).isValid);
    CHECK(!Validator::length("ab", "field", 3, 10).isValid);
    CHECK(!Validator::length("this_is_way_too_long", "field", 3, 10).isValid);

    CHECK(Validator::regex("test123", "field", "^[a-z0-9]+$").isValid);
    CHECK(!Validator::regex("test@123", "field", "^[a-z0-9]+$").isValid);

    CHECK(Validator::numericRange(5, "field", 1, 10).isValid);
    CHECK(!Validator::numericRange(15, "field", 1, 10).isValid);
}
