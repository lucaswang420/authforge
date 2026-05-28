#include <drogon/drogon_test.h>
#include <drogon/drogon.h>
#include <oauth2/plugin/OAuth2Plugin.h>
#include <oauth2/utils/CryptoUtils.h>
#include <future>
#include <chrono>
#include <limits>

#ifdef max
#undef max
#endif

using namespace oauth2;

DROGON_TEST(Integration_P0_Storage_Advanced_Works)
{
    // 1. Setup Plugin
    auto plugin = std::make_shared<OAuth2Plugin>();
    Json::Value config;
    config["storage_type"] = "memory";
    plugin->initAndStart(config);

    auto storage = plugin->getStorage();

    // 2. Test Revocation
    {
        std::string rawToken = "revoked_token_123";
        std::string hashedToken = oauth2::utils::hashToken(rawToken);

        OAuth2AccessToken revokedToken;
        revokedToken.token = hashedToken;
        revokedToken.clientId = "client1";
        revokedToken.userId = "user1";
        revokedToken.expiresAt = std::numeric_limits<int64_t>::max();
        revokedToken.revoked = true;  // Key Flag

        std::promise<void> pSave;
        storage->saveAccessToken(revokedToken, [&]() { pSave.set_value(); });
        pSave.get_future().get();

        // Validate via Plugin (passes raw token, plugin hashes internally)
        std::promise<std::shared_ptr<OAuth2AccessToken>> pVal;
        plugin->validateAccessToken(rawToken, [&](std::shared_ptr<OAuth2AccessToken> t) {
            pVal.set_value(t);
        });
        auto t = pVal.get_future().get();
        CHECK(t == nullptr);  // Should detect revocation
    }

    // 3. Test Expiration
    {
        std::string rawToken = "expired_token_123";
        std::string hashedToken = oauth2::utils::hashToken(rawToken);

        OAuth2AccessToken expiredToken;
        expiredToken.token = hashedToken;
        expiredToken.clientId = "client1";
        expiredToken.userId = "user1";
        // Set specific past time
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
                     std::chrono::system_clock::now().time_since_epoch()
        )
                     .count();
        expiredToken.expiresAt = now - 100;  // Expired 100s ago
        expiredToken.revoked = false;

        std::promise<void> pSave;
        storage->saveAccessToken(expiredToken, [&]() { pSave.set_value(); });
        pSave.get_future().get();

        // Validate via Plugin (passes raw token)
        std::promise<std::shared_ptr<OAuth2AccessToken>> pVal;
        plugin->validateAccessToken(rawToken, [&](std::shared_ptr<OAuth2AccessToken> t) {
            pVal.set_value(t);
        });
        auto t = pVal.get_future().get();
        CHECK(t == nullptr);  // Should detect expiration
    }

    // 4. Test Valid Token (Control)
    {
        std::string rawToken = "valid_token_123";
        std::string hashedToken = oauth2::utils::hashToken(rawToken);

        OAuth2AccessToken validToken;
        validToken.token = hashedToken;
        validToken.clientId = "client1";
        validToken.userId = "user1";
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
                     std::chrono::system_clock::now().time_since_epoch()
        )
                     .count();
        validToken.expiresAt = now + 100;
        validToken.revoked = false;

        std::promise<void> pSave;
        storage->saveAccessToken(validToken, [&]() { pSave.set_value(); });
        pSave.get_future().get();

        std::promise<std::shared_ptr<OAuth2AccessToken>> pVal;
        plugin->validateAccessToken(rawToken, [&](std::shared_ptr<OAuth2AccessToken> t) {
            pVal.set_value(t);
        });
        auto t = pVal.get_future().get();
        CHECK(t != nullptr);
        CHECK(t->token == hashedToken);
    }

    // 5. Test Cleanup (Phase 24)
    {
        std::string rawToken = "expired_token_456";
        std::string hashedToken = oauth2::utils::hashToken(rawToken);

        // Add one more expired token
        OAuth2AccessToken expiredToken2;
        expiredToken2.token = hashedToken;
        expiredToken2.clientId = "client1";
        expiredToken2.userId = "user1";
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
                     std::chrono::system_clock::now().time_since_epoch()
        )
                     .count();
        expiredToken2.expiresAt = now - 1000;
        expiredToken2.revoked = false;

        std::promise<void> pSave;
        storage->saveAccessToken(expiredToken2, [&]() { pSave.set_value(); });
        pSave.get_future().get();

        // Manual cleanup call
        storage->deleteExpiredData();

        // Verify via validation (Should definitely be gone)
        std::promise<std::shared_ptr<OAuth2AccessToken>> pVal;
        plugin->validateAccessToken(rawToken, [&](std::shared_ptr<OAuth2AccessToken> t) {
            pVal.set_value(t);
        });
        auto t = pVal.get_future().get();
        CHECK(t == nullptr);
    }
}
