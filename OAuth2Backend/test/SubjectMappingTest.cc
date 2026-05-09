#include <gtest/gtest.h>
#include "../../common/utils/SubjectGenerator.h"
#include "../../storage/MemoryOAuth2Storage.h"
#include <json/json.h>

using namespace oauth2::utils;

// ========== SubjectGenerator Tests ==========

TEST(SubjectGenerator, ForLocalUser)
{
    std::string subject = SubjectGenerator::forLocalUser("alice");
    EXPECT_EQ(subject, "local:alice");
}

TEST(SubjectGenerator, ForGoogleUser)
{
    std::string subject = SubjectGenerator::forGoogleUser("google123");
    EXPECT_EQ(subject, "google:google123");
}

TEST(SubjectGenerator, ForWeChatUser)
{
    std::string subject = SubjectGenerator::forWeChatUser("wechat_openid");
    EXPECT_EQ(subject, "wechat:wechat_openid");
}

TEST(SubjectGenerator, ParseWithProvider)
{
    auto [provider, sub] = SubjectGenerator::parse("google:abc123");
    EXPECT_EQ(provider, "google");
    EXPECT_EQ(sub, "abc123");
}

TEST(SubjectGenerator, ParseWithoutProvider)
{
    auto [provider, sub] = SubjectGenerator::parse("alice");
    EXPECT_EQ(provider, "local");
    EXPECT_EQ(sub, "alice");
}

TEST(SubjectGenerator, ParseLocalWithProvider)
{
    auto [provider, sub] = SubjectGenerator::parse("local:alice");
    EXPECT_EQ(provider, "local");
    EXPECT_EQ(sub, "alice");
}

TEST(SubjectGenerator, ParseWeChatProvider)
{
    auto [provider, sub] = SubjectGenerator::parse("wechat:openid123");
    EXPECT_EQ(provider, "wechat");
    EXPECT_EQ(sub, "openid123");
}

TEST(SubjectGenerator, ParseWithColonInSubject)
{
    // Test that unknown providers are treated as local
    auto [provider, sub] = SubjectGenerator::parse("unknown:provider:value");
    EXPECT_EQ(provider, "local");
    EXPECT_EQ(sub, "unknown:provider:value");
}

TEST(SubjectGenerator, ForCustomProvider)
{
    std::string subject = SubjectGenerator::forProvider("custom", "user123");
    EXPECT_EQ(subject, "custom:user123");
}

TEST(SubjectGenerator, IsValidValidSubject)
{
    EXPECT_TRUE(SubjectGenerator::isValid("local:alice"));
    EXPECT_TRUE(SubjectGenerator::isValid("google:sub123"));
}

TEST(SubjectGenerator, IsValidInvalidSubject)
{
    EXPECT_FALSE(SubjectGenerator::isValid(""));
    EXPECT_FALSE(SubjectGenerator::isValid("local"));
    EXPECT_FALSE(SubjectGenerator::isValid(":alice"));
}

// ========== Memory Storage Subject Mapping Tests ==========

TEST(SubjectMapping, CreateAndGetMapping)
{
    MemoryOAuth2Storage storage;
    Json::Value clientsConfig;
    Json::Value adminConfig;
    storage.initFromConfig(clientsConfig, adminConfig);

    bool createCalled = false;
    bool getCalled = false;

    // Create mapping
    storage.createSubjectMapping("alice",
                                 1,
                                 "local",
                                 [&createCalled](bool success) {
                                     createCalled = true;
                                     EXPECT_TRUE(success);
                                 });

    // Get mapping
    storage.getInternalUserId("alice", "local", [&getCalled](auto userIdOpt) {
        getCalled = true;
        ASSERT_TRUE(userIdOpt);
        EXPECT_EQ(*userIdOpt, 1);
    });

    EXPECT_TRUE(createCalled);
    EXPECT_TRUE(getCalled);
}

TEST(SubjectMapping, ProviderIsolation)
{
    MemoryOAuth2Storage storage;
    Json::Value clientsConfig;
    Json::Value adminConfig;
    storage.initFromConfig(clientsConfig, adminConfig);

    // Create same subject for different providers
    storage.createSubjectMapping("alice", 1, "local", [](bool) {});
    storage.createSubjectMapping("alice", 2, "google", [](bool) {});

    // Verify they are isolated
    storage.getInternalUserId("alice", "local", [](auto localUserId) {
        ASSERT_TRUE(localUserId);
        EXPECT_EQ(*localUserId, 1);
    });

    storage.getInternalUserId("alice", "google", [](auto googleUserId) {
        ASSERT_TRUE(googleUserId);
        EXPECT_EQ(*googleUserId, 2);
    });
}

TEST(SubjectMapping, GetNonExistentMapping)
{
    MemoryOAuth2Storage storage;
    Json::Value clientsConfig;
    Json::Value adminConfig;
    storage.initFromConfig(clientsConfig, adminConfig);

    storage.getInternalUserId("nonexistent", "local", [](auto userIdOpt) {
        EXPECT_FALSE(userIdOpt);
    });
}

TEST(SubjectMapping, UpdateExistingMapping)
{
    MemoryOAuth2Storage storage;
    Json::Value clientsConfig;
    Json::Value adminConfig;
    storage.initFromConfig(clientsConfig, adminConfig);

    // Create initial mapping
    storage.createSubjectMapping("alice", 1, "local", [](bool) {});

    // Try to create same mapping again (should be idempotent in real
    // implementation)
    storage.createSubjectMapping("alice", 1, "local", [](bool success) {
        EXPECT_TRUE(success);  // Should succeed even if already exists
    });

    // Verify the mapping still points to the correct user
    storage.getInternalUserId("alice", "local", [](auto userIdOpt) {
        ASSERT_TRUE(userIdOpt);
        EXPECT_EQ(*userIdOpt, 1);
    });
}

// ========== User Consent Tests ==========

TEST(UserConsent, SaveAndCheckConsent)
{
    MemoryOAuth2Storage storage;
    Json::Value clientsConfig;
    Json::Value adminConfig;
    storage.initFromConfig(clientsConfig, adminConfig);

    // Save consent
    storage.saveUserConsent(1, "vue-client", "openid", [](bool success) {
        EXPECT_TRUE(success);
    });

    // Check consent exists
    storage.hasUserConsent(1, "vue-client", "openid", [](bool hasConsent) {
        EXPECT_TRUE(hasConsent);
    });

    // Check non-existent consent
    storage.hasUserConsent(1, "vue-client", "admin", [](bool hasConsent) {
        EXPECT_FALSE(hasConsent);
    });
}

TEST(UserConsent, RevokeConsent)
{
    MemoryOAuth2Storage storage;
    Json::Value clientsConfig;
    Json::Value adminConfig;
    storage.initFromConfig(clientsConfig, adminConfig);

    // Save consent
    storage.saveUserConsent(1, "vue-client", "profile", [](bool) {});

    // Verify it exists
    storage.hasUserConsent(1, "vue-client", "profile", [](bool hasConsent) {
        EXPECT_TRUE(hasConsent);
    });

    // Revoke consent
    storage.revokeUserConsent(1, "vue-client", "profile", []() {});

    // Verify it's removed
    storage.hasUserConsent(1, "vue-client", "profile", [](bool hasConsent) {
        EXPECT_FALSE(hasConsent);
    });
}

// ========== Authorization Transaction Tests ==========

TEST(AuthorizationTransaction, SaveAndGetTransaction)
{
    MemoryOAuth2Storage storage;
    Json::Value clientsConfig;
    Json::Value adminConfig;
    storage.initFromConfig(clientsConfig, adminConfig);

    oauth2::IOAuth2Storage::AuthorizationTransaction transaction;
    transaction.transactionId = "txn123";
    transaction.clientId = "vue-client";
    transaction.subject = "local:alice";
    transaction.redirectUri = "http://localhost:5173/callback";
    transaction.state = "state123";
    transaction.expiresAt =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count() +
        600;  // 10 minutes from now

    // Save transaction
    storage.saveAuthorizationTransaction(transaction, [](bool success) {
        EXPECT_TRUE(success);
    });

    // Get transaction
    storage.getAuthorizationTransaction("txn123", [](auto txnOpt) {
        ASSERT_TRUE(txnOpt);
        EXPECT_EQ(txnOpt->transactionId, "txn123");
        EXPECT_EQ(txnOpt->clientId, "vue-client");
        EXPECT_EQ(txnOpt->subject, "local:alice");
        EXPECT_FALSE(txnOpt->consumed);
    });
}

TEST(AuthorizationTransaction, MarkConsumed)
{
    MemoryOAuth2Storage storage;
    Json::Value clientsConfig;
    Json::Value adminConfig;
    storage.initFromConfig(clientsConfig, adminConfig);

    oauth2::IOAuth2Storage::AuthorizationTransaction transaction;
    transaction.transactionId = "txn456";
    transaction.clientId = "vue-client";
    transaction.subject = "local:bob";
    transaction.redirectUri = "http://localhost:5173/callback";
    transaction.state = "state456";
    transaction.expiresAt =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count() +
        600;

    // Save transaction
    storage.saveAuthorizationTransaction(transaction, [](bool) {});

    // Mark as consumed
    storage.markTransactionConsumed("txn456",
                                    [](bool success) { EXPECT_TRUE(success); });

    // Try to mark again (should fail)
    storage.markTransactionConsumed("txn456", [](bool success) {
        EXPECT_FALSE(success);  // Already consumed
    });

    // Verify consumed status
    storage.getAuthorizationTransaction("txn456", [](auto txnOpt) {
        ASSERT_TRUE(txnOpt);
        EXPECT_TRUE(txnOpt->consumed);
    });
}

TEST(AuthorizationTransaction, DeleteTransaction)
{
    MemoryOAuth2Storage storage;
    Json::Value clientsConfig;
    Json::Value adminConfig;
    storage.initFromConfig(clientsConfig, adminConfig);

    oauth2::IOAuth2Storage::AuthorizationTransaction transaction;
    transaction.transactionId = "txn789";
    transaction.clientId = "vue-client";
    transaction.subject = "local:charlie";
    transaction.redirectUri = "http://localhost:5173/callback";
    transaction.state = "state789";
    transaction.expiresAt =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count() +
        600;

    // Save transaction
    storage.saveAuthorizationTransaction(transaction, [](bool) {});

    // Delete transaction
    storage.deleteAuthorizationTransaction("txn789", []() {});

    // Try to get deleted transaction
    storage.getAuthorizationTransaction("txn789", [](auto txnOpt) {
        EXPECT_FALSE(txnOpt);
    });
}
