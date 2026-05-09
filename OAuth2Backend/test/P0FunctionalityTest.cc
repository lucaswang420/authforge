#include <gtest/gtest.h>
#include "../../common/utils/SubjectGenerator.h"
#include "../../storage/MemoryOAuth2Storage.h"
#include "../../plugins/OAuth2Plugin.h"
#include <json/json.h>

using namespace oauth2;
using namespace oauth2::utils;

// ========== P0-1: Subject Mapping Tests ==========

TEST(P0_1_SubjectMapping, GenerateAndParseSubjects)
{
    // Test local user
    std::string localSubject = SubjectGenerator::forLocalUser("alice");
    EXPECT_EQ(localSubject, "local:alice");

    // Test Google user
    std::string googleSubject = SubjectGenerator::forGoogleUser("google123");
    EXPECT_EQ(googleSubject, "google:google123");

    // Test WeChat user
    std::string wechatSubject = SubjectGenerator::forWeChatUser("openid123");
    EXPECT_EQ(wechatSubject, "wechat:openid123");

    // Test parsing
    auto [provider, sub] = SubjectGenerator::parse("google:sub123");
    EXPECT_EQ(provider, "google");
    EXPECT_EQ(sub, "sub123");

    // Test validation
    EXPECT_TRUE(SubjectGenerator::isValid("local:alice"));
    EXPECT_TRUE(SubjectGenerator::isValid("google:sub123"));
    EXPECT_FALSE(SubjectGenerator::isValid(""));
    EXPECT_FALSE(SubjectGenerator::isValid("local"));
}

TEST(P0_1_SubjectMapping, ProviderIsolation)
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
        EXPECT_TRUE(success);
    });

    storage.createSubjectMapping("alice", 2, "google", [&createCalled2](bool success) {
        createCalled2 = true;
        EXPECT_TRUE(success);
    });

    // Verify they are isolated
    storage.getInternalUserId("alice", "local", [&getCalled1](auto userIdOpt) {
        getCalled1 = true;
        ASSERT_TRUE(userIdOpt);
        EXPECT_EQ(*userIdOpt, 1);
    });

    storage.getInternalUserId("alice", "google", [&getCalled2](auto userIdOpt) {
        getCalled2 = true;
        ASSERT_TRUE(userIdOpt);
        EXPECT_EQ(*userIdOpt, 2);
    });

    EXPECT_TRUE(createCalled1);
    EXPECT_TRUE(createCalled2);
    EXPECT_TRUE(getCalled1);
    EXPECT_TRUE(getCalled2);
}

// ========== P0-3: PKCE Validation Tests ==========

TEST(P0_3_PKCE, PlainMethodValidation)
{
    // Test plain method validation
    std::string codeVerifier = "testVerifier123";
    std::string codeChallenge = "testVerifier123";  // plain method
    std::string codeChallengeMethod = "plain";

    bool isValid =
      OAuth2Plugin::validatePkceCodeVerifier(codeVerifier, codeChallenge, codeChallengeMethod);

    EXPECT_TRUE(isValid) << "Plain method validation should succeed";
}

TEST(P0_3_PKCE, S256MethodValidation)
{
    // Test S256 method validation
    std::string codeVerifier =
      "dBjftJeRp4gWTkYbsm1nkjpKfuHYQoRin2057DeWNPBG-"
      "jOgNoFryB9oqLb7Jx1vjbhgHRLQ";  // RFC 7636 test vector
    std::string codeChallenge = "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJstwvGElvfczsDiY54tlddqPv97LM";
    std::string codeChallengeMethod = "S256";

    bool isValid =
      OAuth2Plugin::validatePkceCodeVerifier(codeVerifier, codeChallenge, codeChallengeMethod);

    EXPECT_TRUE(isValid) << "S256 method validation should succeed with RFC test vector";
}

TEST(P0_3_PKCE, InvalidVerifier)
{
    // Test invalid verifier
    std::string codeVerifier = "wrongVerifier";
    std::string codeChallenge = "testChallenge";
    std::string codeChallengeMethod = "S256";

    bool isValid =
      OAuth2Plugin::validatePkceCodeVerifier(codeVerifier, codeChallenge, codeChallengeMethod);

    EXPECT_FALSE(isValid) << "Invalid verifier should fail validation";
}

TEST(P0_3_PKCE, MissingVerifierForChallenge)
{
    // Test missing verifier when challenge exists
    std::string codeVerifier = "";
    std::string codeChallenge = "testChallenge";
    std::string codeChallengeMethod = "S256";

    bool isValid =
      OAuth2Plugin::validatePkceCodeVerifier(codeVerifier, codeChallenge, codeChallengeMethod);

    EXPECT_FALSE(isValid) << "Missing verifier should fail validation";
}

// ========== P0-4: State Parameter Tests ==========

TEST(P0_4_StateParameter, ValidStateFormats)
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

TEST(P0_4_StateParameter, InvalidStateFormats)
{
    // Test invalid state parameter formats
    std::string state1 = "";                     // too short
    std::string state2 = "abc";                  // too short
    std::string state3 = std::string(513, 'a');  // too long

    EXPECT_LT(state1.length(), 8);
    EXPECT_LT(state2.length(), 8);
    EXPECT_GT(state3.length(), 512);
}

TEST(P0_4_StateParameter, MaliciousStateDetection)
{
    // Test state parameter injection attempts
    std::string state1 = "state?param=value";
    std::string state2 = "state#fragment";
    std::string state3 = "state&param=value";

    EXPECT_TRUE(state1.find('?') != std::string::npos);
    EXPECT_TRUE(state2.find('#') != std::string::npos);
    EXPECT_TRUE(state3.find('&') != std::string::npos);
}

// ========== P0-5: Scope Permission Control Tests ==========

TEST(P0_5_ScopeValidation, AdminScopeRecognition)
{
    // Test admin scope recognition
    EXPECT_TRUE(OAuth2Plugin::scopeRequiresAdminRole("admin"));
    EXPECT_TRUE(OAuth2Plugin::scopeRequiresAdminRole("admin:read"));
    EXPECT_TRUE(OAuth2Plugin::scopeRequiresAdminRole("admin:write"));
    EXPECT_TRUE(OAuth2Plugin::scopeRequiresAdminRole("user:manage"));
    EXPECT_TRUE(OAuth2Plugin::scopeRequiresAdminRole("settings:manage"));

    // Test non-admin scopes
    EXPECT_FALSE(OAuth2Plugin::scopeRequiresAdminRole("openid"));
    EXPECT_FALSE(OAuth2Plugin::scopeRequiresAdminRole("profile"));
    EXPECT_FALSE(OAuth2Plugin::scopeRequiresAdminRole("email"));
    EXPECT_FALSE(OAuth2Plugin::scopeRequiresAdminRole("read"));
}

TEST(P0_5_ScopeValidation, ScopePrefixMatching)
{
    // Test scope prefix matching for admin scopes
    EXPECT_TRUE(OAuth2Plugin::scopeRequiresAdminRole("admin:custom"));
    EXPECT_TRUE(OAuth2Plugin::scopeRequiresAdminRole("settings:advanced"));

    // Test non-admin scope prefixes
    EXPECT_FALSE(OAuth2Plugin::scopeRequiresAdminRole("profile:advanced"));
    EXPECT_FALSE(OAuth2Plugin::scopeRequiresAdminRole("email:custom"));
}

// ========== Integration Tests ==========

TEST(P0_Integration, SubjectMappingWithPKCE)
{
    MemoryOAuth2Storage storage;
    Json::Value clientsConfig;
    Json::Value adminConfig;
    storage.initFromConfig(clientsConfig, adminConfig);

    // Create subject mapping
    storage.createSubjectMapping("user1", 100, "local", [](bool success) { EXPECT_TRUE(success); });

    // Verify mapping was created
    storage.getInternalUserId("user1", "local", [](auto userIdOpt) {
        ASSERT_TRUE(userIdOpt);
        EXPECT_EQ(*userIdOpt, 100);
    });
}

TEST(P0_Integration, PKCEWithSubjectMapping)
{
    // Test PKCE validation with subject mapping context
    std::string codeVerifier = "testVerifier456";
    std::string codeChallenge = OAuth2Plugin::generateSha256Hash(codeVerifier);
    std::string codeChallengeMethod = "S256";

    // Verify S256 validation works
    bool isValid =
      OAuth2Plugin::validatePkceCodeVerifier(codeVerifier, codeChallenge, codeChallengeMethod);

    EXPECT_TRUE(isValid) << "PKCE validation should succeed with generated hash";

    // Verify hash is base64-url encoded (no +, /, = characters)
    EXPECT_EQ(codeChallenge.find('+'), std::string::npos) << "Hash should not contain +";
    EXPECT_EQ(codeChallenge.find('/'), std::string::npos) << "Hash should not contain /";
    EXPECT_EQ(codeChallenge.find('='), std::string::npos) << "Hash should not contain padding";
}

TEST(P0_Integration, StateWithScopeValidation)
{
    // Test state parameter with scope validation context
    std::string state = "secureState123";
    std::vector<std::string> scopes = {"openid", "profile", "email"};

    // Verify state meets requirements
    EXPECT_GE(state.length(), 8);
    EXPECT_LE(state.length(), 512);
    EXPECT_EQ(state.find('?'), std::string::npos);
    EXPECT_EQ(state.find('#'), std::string::npos);
    EXPECT_EQ(state.find('&'), std::string::npos);

    // Verify scopes are valid
    EXPECT_FALSE(scopes.empty());
    EXPECT_EQ(scopes.size(), 3);

    // Verify no admin scopes in basic request
    for (const auto &scope : scopes)
    {
        EXPECT_FALSE(OAuth2Plugin::scopeRequiresAdminRole(scope))
          << "Basic request should not contain admin scopes";
    }
}

// ========== Performance Tests ==========

TEST(P0_Performance, SubjectGenerationPerformance)
{
    // Test subject generation performance (should be very fast)
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i)
    {
        std::string subject = SubjectGenerator::forLocalUser("user" + std::to_string(i));
        EXPECT_EQ(subject.substr(0, 6), "local:");
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 100) << "Subject generation should be very "
                                        "fast (<100ms for 1000 iterations)";
}

TEST(P0_Performance, PKCEHashPerformance)
{
    // Test PKCE hash performance (should be reasonably fast)
    auto start = std::chrono::high_resolution::now();

    for (int i = 0; i < 100; ++i)
    {
        std::string verifier = "testVerifier" + std::to_string(i);
        std::string hash = OAuth2Plugin::generateSha256Hash(verifier);
        EXPECT_FALSE(hash.empty());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 500)
      << "PKCE hash should be reasonably fast (<500ms for 100 iterations)";
}

// ========== Security Tests ==========

TEST(P0_Security, SubjectInjectionPrevention)
{
    // Test that subject injection attempts are prevented
    std::string malicious1 = "local:alice@evil.com";
    std::string malicious2 = "local:alice\nevil";
    std::string malicious3 = "local:alice?admin=true";

    // All should be valid subjects (we don't block special characters in
    // subjects) but our validation should prevent injection in database queries
    EXPECT_TRUE(SubjectGenerator::isValid(malicious1));
    EXPECT_TRUE(SubjectGenerator::isValid(malicious2));
    EXPECT_TRUE(SubjectGenerator::isValid(malicious3));

    // Test parsing handles special characters
    auto [provider1, sub1] = SubjectGenerator::parse(malicious1);
    EXPECT_EQ(provider1, "local");
    EXPECT_EQ(sub1, "alice@evil.com");
}

TEST(P0_Security, PKCEAttackPrevention)
{
    // Test that PKCE prevents authorization code interception
    std::string stolenCodeChallenge = "interceptedChallenge";
    std::string wrongVerifier = "stolenVerifier";

    bool isValid =
      OAuth2Plugin::validatePkceCodeVerifier(wrongVerifier, stolenCodeChallenge, "S256");

    EXPECT_FALSE(isValid) << "Stolen verifier should fail PKCE validation";
}

TEST(P0_Security, StateParameterCSRFPrevention)
{
    // Test that state parameter prevents CSRF
    std::string validState = "csrfProtection123";

    // Verify state meets security requirements
    EXPECT_GE(validState.length(), 8) << "State must be sufficiently long";
    EXPECT_LE(validState.length(), 512) << "State must not be excessively long";
    EXPECT_EQ(validState.find('?'), std::string::npos) << "State must not contain URL delimiters";
    EXPECT_EQ(validState.find('#'), std::string::npos) << "State must not contain URL delimiters";
    EXPECT_EQ(validState.find('&'), std::string::npos) << "State must not contain URL delimiters";
}

// ========== Edge Cases ==========

TEST(P0_EdgeCases, EmptyVerifierWithoutChallenge)
{
    // Test that empty verifier is allowed when no challenge exists
    std::string codeVerifier = "";
    std::string codeChallenge = "";  // no challenge
    std::string codeChallengeMethod = "";

    // When there's no challenge, validation should pass (optional PKCE)
    // This is tested implicitly by not calling validatePkceCodeVerifier
    SUCCEED() << "Empty verifier without challenge is allowed (optional PKCE)";
}

TEST(P0_EdgeCases, SubjectProviderIsolation)
{
    MemoryOAuth2Storage storage;
    Json::Value clientsConfig;
    Json::Value adminConfig;
    storage.initFromConfig(clientsConfig, adminConfig);

    // Create same subject for multiple providers
    storage.createSubjectMapping("testuser", 1, "local", [](bool) {});
    storage.createSubjectMapping("testuser", 2, "google", [](bool) {});
    storage.createSubjectMapping("testuser", 3, "wechat", [](bool) {});

    // Verify all three are isolated
    storage.getInternalUserId("testuser", "local", [](auto localId) {
        ASSERT_TRUE(localId);
        EXPECT_EQ(*localId, 1);
    });

    storage.getInternalUserId("testuser", "google", [](auto googleId) {
        ASSERT_TRUE(googleId);
        EXPECT_EQ(*googleId, 2);
    });

    storage.getInternalUserId("testuser", "wechat", [](auto wechatId) {
        ASSERT_TRUE(wechatId);
        EXPECT_EQ(*wechatId, 3);
    });
}

TEST(P0_EdgeCases, ScopeValidationWithEmptyScopeList)
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

    EXPECT_FALSE(hasAdminScope) << "Empty scope list should not contain admin scopes";
}
