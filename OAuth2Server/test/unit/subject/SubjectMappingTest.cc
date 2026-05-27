#include <chrono>
#include <drogon/drogon_test.h>
#include <oauth2/SubjectGenerator.h>
#include <oauth2/storage/MemoryOAuth2Storage.h>
#include <json/json.h>

using namespace oauth2::utils;

// ========== SubjectGenerator Tests ==========

DROGON_TEST(Unit_P0_SubjectGenerator_ForLocalUser_Works)
{
    std::string subject = SubjectGenerator::forLocalUser("alice");
    CHECK((subject) == ("local:alice"));
}

DROGON_TEST(Unit_P0_SubjectGenerator_ForGoogleUser_Works)
{
    std::string subject = SubjectGenerator::forGoogleUser("google123");
    CHECK((subject) == ("google:google123"));
}

DROGON_TEST(Unit_P0_SubjectGenerator_ForWeChatUser_Works)
{
    std::string subject = SubjectGenerator::forWeChatUser("wechat_openid");
    CHECK((subject) == ("wechat:wechat_openid"));
}

DROGON_TEST(Unit_P0_SubjectGenerator_ParseWithProvider_Works)
{
    auto [provider, sub] = SubjectGenerator::parse("google:abc123");
    CHECK((provider) == ("google"));
    CHECK((sub) == ("abc123"));
}

DROGON_TEST(Unit_P0_SubjectGenerator_ParseWithoutProvider_Works)
{
    auto [provider, sub] = SubjectGenerator::parse("alice");
    CHECK((provider) == ("local"));
    CHECK((sub) == ("alice"));
}

DROGON_TEST(Unit_P0_SubjectGenerator_ParseLocalWithProvider_Works)
{
    auto [provider, sub] = SubjectGenerator::parse("local:alice");
    CHECK((provider) == ("local"));
    CHECK((sub) == ("alice"));
}

DROGON_TEST(Unit_P0_SubjectGenerator_ParseWeChatProvider_Works)
{
    auto [provider, sub] = SubjectGenerator::parse("wechat:openid123");
    CHECK((provider) == ("wechat"));
    CHECK((sub) == ("openid123"));
}

DROGON_TEST(Unit_P0_SubjectGenerator_ParseWithColonInSubject_Works)
{
    // Test that unknown providers are treated as local
    auto [provider, sub] = SubjectGenerator::parse("unknown:provider:value");
    CHECK((provider) == ("local"));
    CHECK((sub) == ("unknown:provider:value"));
}

DROGON_TEST(Unit_P0_SubjectGenerator_ForCustomProvider_Works)
{
    std::string subject = SubjectGenerator::forProvider("custom", "user123");
    CHECK((subject) == ("custom:user123"));
}

DROGON_TEST(Unit_P0_SubjectGenerator_IsValidValidSubject_Works)
{
    CHECK(SubjectGenerator::isValid("local:alice"));
    CHECK(SubjectGenerator::isValid("google:sub123"));
}

DROGON_TEST(Unit_P0_SubjectGenerator_IsValidInvalidSubject_Works)
{
    CHECK(!(SubjectGenerator::isValid("")));
    CHECK(!(SubjectGenerator::isValid("local")));
    CHECK(!(SubjectGenerator::isValid(":alice")));
}

// ========== Memory Storage Subject Mapping Tests ==========

DROGON_TEST(Unit_P0_SubjectMapping_CreateAndGetMapping_Works)
{
    oauth2::MemoryOAuth2Storage storage;
    Json::Value clientsConfig;
    Json::Value adminConfig;
    storage.initFromConfig(clientsConfig, adminConfig);

    bool createCalled = false;
    bool getCalled = false;

    // Create mapping
    storage.createSubjectMapping("alice", 1, "local", [&](bool success) {
        createCalled = true;
        CHECK(success);
    });

    // Get mapping
    storage.getInternalUserId("alice", "local", [&](auto userIdOpt) {
        getCalled = true;
        CHECK(userIdOpt);
        CHECK((*userIdOpt) == (1));
    });

    CHECK(createCalled);
    CHECK(getCalled);
}

DROGON_TEST(Unit_P0_SubjectMapping_ProviderIsolation_Works)
{
    oauth2::MemoryOAuth2Storage storage;
    Json::Value clientsConfig;
    Json::Value adminConfig;
    storage.initFromConfig(clientsConfig, adminConfig);

    // Create same subject for different providers
    storage.createSubjectMapping("alice", 1, "local", [&](bool) {});
    storage.createSubjectMapping("alice", 2, "google", [&](bool) {});

    // Verify they are isolated
    storage.getInternalUserId("alice", "local", [&](auto localUserId) {
        CHECK(localUserId);
        CHECK((*localUserId) == (1));
    });

    storage.getInternalUserId("alice", "google", [&](auto googleUserId) {
        CHECK(googleUserId);
        CHECK((*googleUserId) == (2));
    });
}

DROGON_TEST(Unit_P0_SubjectMapping_GetNonExistentMapping_Works)
{
    oauth2::MemoryOAuth2Storage storage;
    Json::Value clientsConfig;
    Json::Value adminConfig;
    storage.initFromConfig(clientsConfig, adminConfig);

    storage.getInternalUserId("nonexistent", "local", [&](auto userIdOpt) { CHECK(!(userIdOpt)); });
}

DROGON_TEST(Unit_P0_SubjectMapping_UpdateExistingMapping_Works)
{
    oauth2::MemoryOAuth2Storage storage;
    Json::Value clientsConfig;
    Json::Value adminConfig;
    storage.initFromConfig(clientsConfig, adminConfig);

    // Create initial mapping
    storage.createSubjectMapping("alice", 1, "local", [&](bool) {});

    // Try to create same mapping again (should be idempotent in real
    // implementation)
    storage.createSubjectMapping("alice", 1, "local", [&](bool success) {
        CHECK(success);  // Should succeed even if already exists
    });

    // Verify the mapping still points to the correct user
    storage.getInternalUserId("alice", "local", [&](auto userIdOpt) {
        CHECK(userIdOpt);
        CHECK((*userIdOpt) == (1));
    });
}

// ========== User Consent Tests ==========

DROGON_TEST(Unit_P0_UserConsent_SaveAndCheckConsent_Works)
{
    oauth2::MemoryOAuth2Storage storage;
    Json::Value clientsConfig;
    Json::Value adminConfig;
    storage.initFromConfig(clientsConfig, adminConfig);

    // Save consent
    storage.saveUserConsent(1, "vue-client", "openid", [&](bool success) { CHECK(success); });

    // Check consent exists
    storage.hasUserConsent(1, "vue-client", "openid", [&](bool hasConsent) { CHECK(hasConsent); });

    // Check non-existent consent
    storage.hasUserConsent(1, "vue-client", "admin", [&](bool hasConsent) {
        CHECK(!(hasConsent));
    });
}

DROGON_TEST(Unit_P0_UserConsent_RevokeConsent_Works)
{
    oauth2::MemoryOAuth2Storage storage;
    Json::Value clientsConfig;
    Json::Value adminConfig;
    storage.initFromConfig(clientsConfig, adminConfig);

    // Save consent
    storage.saveUserConsent(1, "vue-client", "profile", [&](bool) {});

    // Verify it exists
    storage.hasUserConsent(1, "vue-client", "profile", [&](bool hasConsent) { CHECK(hasConsent); });

    // Revoke consent
    storage.revokeUserConsent(1, "vue-client", "profile", [&]() {});

    // Verify it's removed
    storage.hasUserConsent(1, "vue-client", "profile", [&](bool hasConsent) {
        CHECK(!(hasConsent));
    });
}

// ========== Authorization Transaction Tests ==========

DROGON_TEST(Unit_P0_AuthorizationTransaction_SaveAndGetTransaction_Works)
{
    oauth2::MemoryOAuth2Storage storage;
    Json::Value clientsConfig;
    Json::Value adminConfig;
    storage.initFromConfig(clientsConfig, adminConfig);

    oauth2::IOAuth2Storage::AuthorizationTransaction transaction;
    transaction.transactionId = "txn123";
    transaction.clientId = "vue-client";
    transaction.subject = "local:alice";
    transaction.redirectUri = "http://localhost:5173/callback";
    transaction.state = "state123";
    transaction.expiresAt = std::chrono::duration_cast<std::chrono::seconds>(
                              std::chrono::system_clock::now().time_since_epoch()
                            )
                              .count() +
                            600;  // 10 minutes from now

    // Save transaction
    storage.saveAuthorizationTransaction(transaction, [&](bool success) { CHECK(success); });

    // Get transaction
    storage.getAuthorizationTransaction("txn123", [&](auto txnOpt) {
        CHECK(txnOpt);
        CHECK((txnOpt->transactionId) == ("txn123"));
        CHECK((txnOpt->clientId) == ("vue-client"));
        CHECK((txnOpt->subject) == ("local:alice"));
        CHECK(!(txnOpt->consumed));
    });
}

DROGON_TEST(Unit_P0_AuthorizationTransaction_MarkConsumed_Works)
{
    oauth2::MemoryOAuth2Storage storage;
    Json::Value clientsConfig;
    Json::Value adminConfig;
    storage.initFromConfig(clientsConfig, adminConfig);

    oauth2::IOAuth2Storage::AuthorizationTransaction transaction;
    transaction.transactionId = "txn456";
    transaction.clientId = "vue-client";
    transaction.subject = "local:bob";
    transaction.redirectUri = "http://localhost:5173/callback";
    transaction.state = "state456";
    transaction.expiresAt = std::chrono::duration_cast<std::chrono::seconds>(
                              std::chrono::system_clock::now().time_since_epoch()
                            )
                              .count() +
                            600;

    // Save transaction
    storage.saveAuthorizationTransaction(transaction, [&](bool) {});

    // Mark as consumed
    storage.markTransactionConsumed("txn456", [&](bool success) { CHECK(success); });

    // Try to mark again (should fail)
    storage.markTransactionConsumed("txn456", [&](bool success) {
        CHECK(!(success));  // Already consumed
    });

    // Verify consumed status
    storage.getAuthorizationTransaction("txn456", [&](auto txnOpt) {
        CHECK(txnOpt);
        CHECK(txnOpt->consumed);
    });
}

DROGON_TEST(Unit_P0_AuthorizationTransaction_DeleteTransaction_Works)
{
    oauth2::MemoryOAuth2Storage storage;
    Json::Value clientsConfig;
    Json::Value adminConfig;
    storage.initFromConfig(clientsConfig, adminConfig);

    oauth2::IOAuth2Storage::AuthorizationTransaction transaction;
    transaction.transactionId = "txn789";
    transaction.clientId = "vue-client";
    transaction.subject = "local:charlie";
    transaction.redirectUri = "http://localhost:5173/callback";
    transaction.state = "state789";
    transaction.expiresAt = std::chrono::duration_cast<std::chrono::seconds>(
                              std::chrono::system_clock::now().time_since_epoch()
                            )
                              .count() +
                            600;

    // Save transaction
    storage.saveAuthorizationTransaction(transaction, [&](bool) {});

    // Delete transaction
    storage.deleteAuthorizationTransaction("txn789", [&]() {});

    // Try to get deleted transaction
    storage.getAuthorizationTransaction("txn789", [&](auto txnOpt) { CHECK(!(txnOpt)); });
}
