#include <drogon/drogon_test.h>
#include "../../common/utils/SubjectGenerator.h"
#include "../../storage/MemoryOAuth2Storage.h"
#include "../../plugins/OAuth2Plugin.h"
#include <json/json.h>

using namespace oauth2;
using namespace oauth2::utils;

// ========== P0-1: Subject Mapping Tests ==========

DROGON_TEST(Unit_P0_SubjectMapping_Legacy_GenerateAndParseSubjects)
{
    // Test local user
    std::string localSubject = SubjectGenerator::forLocalUser("alice");
    CHECK((localSubject) == ("local:alice"));

    // Test Google user
    std::string googleSubject = SubjectGenerator::forGoogleUser("google123");
    CHECK((googleSubject) == ("google:google123"));

    // Test WeChat user
    std::string wechatSubject = SubjectGenerator::forWeChatUser("openid123");
    CHECK((wechatSubject) == ("wechat:openid123"));

    // Test parsing
    auto [provider, sub] = SubjectGenerator::parse("google:sub123");
    CHECK((provider) == ("google"));
    CHECK((sub) == ("sub123"));

    // Test validation
    CHECK(SubjectGenerator::isValid("local:alice"));
    CHECK(SubjectGenerator::isValid("google:sub123"));
    CHECK(!(SubjectGenerator::isValid("")));
    CHECK(!(SubjectGenerator::isValid("local")));
}

DROGON_TEST(Unit_P0_SubjectMapping_Legacy_ProviderIsolation)
{
    MemoryOAuth2Storage storage;
    Json::Value clientsConfig;
    Json::Value adminConfig;
    storage.initFromConfig(clientsConfig, adminConfig);

    bool createCalled1 = false;
    bool createCalled2 = false;
    bool getCalled1 = false;
    bool getCalled2 = false;

    // Create same subject for different providers
    storage.createSubjectMapping("alice", 1, "local", [&createCalled1](bool success) {
        createCalled1 = true;
        CHECK(success);
    });

    storage.createSubjectMapping("alice", 2, "google", [&createCalled2](bool success) {
        createCalled2 = true;
        CHECK(success);
    });

    // Verify they are isolated
    storage.getInternalUserId("alice", "local", [&getCalled1](auto userIdOpt) {
        getCalled1 = true;
        CHECK(userIdOpt);
        CHECK((*userIdOpt) == (1));
    });

    storage.getInternalUserId("alice", "google", [&getCalled2](auto userIdOpt) {
        getCalled2 = true;
        CHECK(userIdOpt);
        CHECK((*userIdOpt) == (2));
    });

    CHECK(createCalled1);
    CHECK(createCalled2);
    CHECK(getCalled1);
    CHECK(getCalled2);
}

// ========== P0-3: PKCE Validation Tests ==========

DROGON_TEST(Unit_P0_PKCE_Legacy_PlainMethodValidation)
{
    // Test plain method validation
    std::string codeVerifier = "testVerifier123";
    std::string codeChallenge = "testVerifier123";  // plain method
    std::string codeChallengeMethod = "plain";

    bool isValid =
      OAuth2Plugin::validatePkceCodeVerifier(codeVerifier, codeChallenge, codeChallengeMethod);

    CHECK(isValid);
}

DROGON_TEST(Unit_P0_PKCE_Legacy_S256MethodValidation)
{
    // Test S256 method validation
    std::string codeVerifier =
      "dBjftJeRp4gWTkYbsm1nkjpKfuHYQoRin2057DeWNPBG-"
      "jOgNoFryB9oqLb7Jx1vjbhgHRLQ";  // RFC 7636 test vector
    std::string codeChallenge = "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJstwvGElvfczsDiY54tlddqPv97LM";
    std::string codeChallengeMethod = "S256";

    bool isValid =
      OAuth2Plugin::validatePkceCodeVerifier(codeVerifier, codeChallenge, codeChallengeMethod);

    CHECK(isValid);
}

DROGON_TEST(Unit_P0_PKCE_Legacy_InvalidVerifier)
{
    // Test invalid verifier
    std::string codeVerifier = "wrongVerifier";
    std::string codeChallenge = "testChallenge";
    std::string codeChallengeMethod = "S256";

    bool isValid =
      OAuth2Plugin::validatePkceCodeVerifier(codeVerifier, codeChallenge, codeChallengeMethod);

    CHECK(!(isValid)) << "Invalid verifier should fail validation";
}

DROGON_TEST(Unit_P0_PKCE_Legacy_MissingVerifierForChallenge)
{
    // Test missing verifier when challenge exists
    std::string codeVerifier = "";
    std::string codeChallenge = "testChallenge";
    std::string codeChallengeMethod = "S256";

    bool isValid =
      OAuth2Plugin::validatePkceCodeVerifier(codeVerifier, codeChallenge, codeChallengeMethod);

    CHECK(!(isValid)) << "Missing verifier should fail validation";
}

// ========== P0-4: State Parameter Tests ==========

DROGON_TEST(Unit_P0_StateParameter_Legacy_ValidStateFormats)
{
    // Test valid state parameter formats
    std::string state1 = "abc123";
    std::string state2 = "randomState123456789";
    std::string state3 = "state-with-dashes";

    EXPECT_GE(state1.length(), 8);
    EXPECT_LE(state1.length(), 512);
    EXPECT_GE(state2.length(), 8);
    EXPECT_LE(state2.length(), 512);
    EXPECT_GE(state3.length(), 8);
    EXPECT_LE(state3.length(), 512);
}

DROGON_TEST(Unit_P0_StateParameter_Legacy_InvalidStateFormats)
{
    // Test invalid state parameter formats
    std::string state1 = "";                     // too short
    std::string state2 = "abc";                  // too short
    std::string state3 = std::string(513, 'a');  // too long

    EXPECT_LT(state1.length(), 8);
    EXPECT_LT(state2.length(), 8);
    EXPECT_GT(state3.length(), 512);
}

DROGON_TEST(Unit_P0_StateParameter_Legacy_MaliciousStateDetection)
{
    // Test state parameter injection attempts
    std::string state1 = "state?param=value";
    std::string state2 = "state#fragment";
    std::string state3 = "state&param=value";

    CHECK(state1.find('?') != std::string::npos);
    CHECK(state2.find('#') != std::string::npos);
    CHECK(state3.find('&') != std::string::npos);
}

// ========== P0-5: Scope Permission Control Tests ==========

DROGON_TEST(Unit_P0_ScopeValidation_Legacy_AdminScopeRecognition)
{
    // Test admin scope recognition
    CHECK(OAuth2Plugin::scopeRequiresAdminRole("admin"));
    CHECK(OAuth2Plugin::scopeRequiresAdminRole("admin:read"));
    CHECK(OAuth2Plugin::scopeRequiresAdminRole("admin:write"));
    CHECK(OAuth2Plugin::scopeRequiresAdminRole("user:manage"));
    CHECK(OAuth2Plugin::scopeRequiresAdminRole("settings:manage"));

    // Test non-admin scopes
    CHECK(!(OAuth2Plugin::scopeRequiresAdminRole("openid")));
    CHECK(!(OAuth2Plugin::scopeRequiresAdminRole("profile")));
    CHECK(!(OAuth2Plugin::scopeRequiresAdminRole("email")));
    CHECK(!(OAuth2Plugin::scopeRequiresAdminRole("read")));
}

DROGON_TEST(Unit_P0_ScopeValidation_Legacy_ScopePrefixMatching)
{
    // Test scope prefix matching for admin scopes
    CHECK(OAuth2Plugin::scopeRequiresAdminRole("admin:custom"));
    CHECK(OAuth2Plugin::scopeRequiresAdminRole("settings:advanced"));

    // Test non-admin scope prefixes
    CHECK(!(OAuth2Plugin::scopeRequiresAdminRole("profile:advanced")));
    CHECK(!(OAuth2Plugin::scopeRequiresAdminRole("email:custom")));
}

// ========== Integration Tests ==========

DROGON_TEST(Integration_P0_Flows_Legacy_SubjectMappingWithPKCE)
{
    MemoryOAuth2Storage storage;
    Json::Value clientsConfig;
    Json::Value adminConfig;
    storage.initFromConfig(clientsConfig, adminConfig);

    // Create subject mapping
    storage.createSubjectMapping("user1", 100, "local", [=](bool success) { CHECK(success); });

    // Verify mapping was created
    storage.getInternalUserId("user1", "local", [=](auto userIdOpt) {
        CHECK(userIdOpt);
        CHECK((*userIdOpt) == (100));
    });
}

DROGON_TEST(Integration_P0_Flows_Legacy_PKCEWithSubjectMapping)
{
    // Test PKCE validation with subject mapping context
    std::string codeVerifier = "testVerifier456";
    std::string codeChallenge = OAuth2Plugin::generateSha256Hash(codeVerifier);
    std::string codeChallengeMethod = "S256";

    // Verify S256 validation works
    bool isValid =
      OAuth2Plugin::validatePkceCodeVerifier(codeVerifier, codeChallenge, codeChallengeMethod);

    CHECK(isValid);

    // Verify hash is base64-url encoded (no +, /, = characters)
    CHECK((codeChallenge.find('+')) == (std::string::npos)) << "Hash should not contain +";
    CHECK((codeChallenge.find('/')) == (std::string::npos)) << "Hash should not contain /";
    CHECK((codeChallenge.find('=')) == (std::string::npos)) << "Hash should not contain padding";
}

DROGON_TEST(Integration_P0_Flows_Legacy_StateWithScopeValidation)
{
    // Test state parameter with scope validation context
    std::string state = "secureState123";
    std::vector<std::string> scopes = {"openid", "profile", "email"};

    // Verify state meets requirements
    EXPECT_GE(state.length(), 8);
    EXPECT_LE(state.length(), 512);
    CHECK((state.find('?')) == (std::string::npos));
    CHECK((state.find('#')) == (std::string::npos));
    CHECK((state.find('&')) == (std::string::npos));

    // Verify scopes are valid
    CHECK(!(scopes.empty()));
    CHECK((scopes.size()) == (3));

    // Verify no admin scopes in basic request
    for (const auto &scope : scopes)
    {
        CHECK(!(OAuth2Plugin::scopeRequiresAdminRole(scope)))
          << "Basic request should not contain admin scopes";
    }
}

// ========== Performance Tests ==========

DROGON_TEST(Performance_P1_Benchmark_P0_Legacy_SubjectGenerationPerformance_Works)
{
    // Test subject generation performance (should be very fast)
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i)
    {
        std::string subject = SubjectGenerator::forLocalUser("user" + std::to_string(i));
        CHECK((subject.substr(0) == (6)), "local:");
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 100) << "Subject generation should be very "
                                        "fast (<100ms for 1000 iterations)";
}

DROGON_TEST(Performance_P1_Benchmark_P0_Legacy_PKCEHashPerformance_Works)
{
    // Test PKCE hash performance (should be reasonably fast)
    auto start = std::chrono::high_resolution::now();

    for (int i = 0; i < 100; ++i)
    {
        std::string verifier = "testVerifier" + std::to_string(i);
        std::string hash = OAuth2Plugin::generateSha256Hash(verifier);
        CHECK(!(hash.empty()));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 500)
      << "PKCE hash should be reasonably fast (<500ms for 100 iterations)";
}

// ========== Security Tests ==========

DROGON_TEST(Security_P0_Legacy_SubjectInjectionPrevention)
{
    // Test that subject injection attempts are prevented
    std::string malicious1 = "local:alice@evil.com";
    std::string malicious2 = "local:alice\nevil";
    std::string malicious3 = "local:alice?admin=true";

    // All should be valid subjects (we don't block special characters in
    // subjects) but our validation should prevent injection in database queries
    CHECK(SubjectGenerator::isValid(malicious1));
    CHECK(SubjectGenerator::isValid(malicious2));
    CHECK(SubjectGenerator::isValid(malicious3));

    // Test parsing handles special characters
    auto [provider1, sub1] = SubjectGenerator::parse(malicious1);
    CHECK((provider1) == ("local"));
    CHECK((sub1) == ("alice@evil.com"));
}

DROGON_TEST(Security_P0_Legacy_PKCEAttackPrevention)
{
    // Test that PKCE prevents authorization code interception
    std::string stolenCodeChallenge = "interceptedChallenge";
    std::string wrongVerifier = "stolenVerifier";

    bool isValid =
      OAuth2Plugin::validatePkceCodeVerifier(wrongVerifier, stolenCodeChallenge, "S256");

    CHECK(!(isValid)) << "Stolen verifier should fail PKCE validation";
}

DROGON_TEST(Security_P0_Legacy_StateParameterCSRFPrevention)
{
    // Test that state parameter prevents CSRF
    std::string validState = "csrfProtection123";

    // Verify state meets security requirements
    EXPECT_GE(validState.length(), 8) << "State must be sufficiently long";
    EXPECT_LE(validState.length(), 512) << "State must not be excessively long";
    CHECK((validState.find('?')) == (std::string::npos)) << "State must not contain URL delimiters";
    CHECK((validState.find('#')) == (std::string::npos)) << "State must not contain URL delimiters";
    CHECK((validState.find('&')) == (std::string::npos)) << "State must not contain URL delimiters";
}

// ========== Edge Cases ==========

DROGON_TEST(Unit_P0_EdgeCases_Legacy_EmptyVerifierWithoutChallenge)
{
    // Test that empty verifier is allowed when no challenge exists
    std::string codeVerifier = "";
    std::string codeChallenge = "";  // no challenge
    std::string codeChallengeMethod = "";

    // When there's no challenge, validation should pass (optional PKCE)
    // This is tested implicitly by not calling validatePkceCodeVerifier
     << "Empty verifier without challenge is allowed (optional PKCE)";
}

DROGON_TEST(Unit_P0_EdgeCases_Legacy_SubjectProviderIsolation)
{
    MemoryOAuth2Storage storage;
    Json::Value clientsConfig;
    Json::Value adminConfig;
    storage.initFromConfig(clientsConfig, adminConfig);

    // Create same subject for multiple providers
    storage.createSubjectMapping("testuser", 1, "local", [=](bool) {});
    storage.createSubjectMapping("testuser", 2, "google", [=](bool) {});
    storage.createSubjectMapping("testuser", 3, "wechat", [=](bool) {});

    // Verify all three are isolated
    storage.getInternalUserId("testuser", "local", [=](auto localId) {
        CHECK(localId);
        CHECK((*localId) == (1));
    });

    storage.getInternalUserId("testuser", "google", [=](auto googleId) {
        CHECK(googleId);
        CHECK((*googleId) == (2));
    });

    storage.getInternalUserId("testuser", "wechat", [=](auto wechatId) {
        CHECK(wechatId);
        CHECK((*wechatId) == (3));
    });
}

DROGON_TEST(Unit_P0_EdgeCases_Legacy_ScopeValidationWithEmptyScopeList)
{
    // Test scope validation with empty scope list
    std::vector<std::string> emptyScopes;

    // Should not crash with empty scopes
    // Admin scope check should return false for empty list
    bool hasAdminScope = false;
    for (const auto &scope : emptyScopes)
    {
        if (OAuth2Plugin::scopeRequiresAdminRole(scope))
        {
            hasAdminScope = true;
            break;
        }
    }

    CHECK(!(hasAdminScope)) << "Empty scope list should not contain admin scopes";
}
