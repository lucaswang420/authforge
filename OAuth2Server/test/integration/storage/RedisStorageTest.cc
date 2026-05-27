
#include <drogon/drogon_test.h>
#include <drogon/drogon.h>
#include <oauth2/storage/RedisOAuth2Storage.h>
#include <oauth2/OAuth2Plugin.h>
#include <future>

using namespace oauth2;

DROGON_TEST(Integration_P1_Storage_Redis_Works)
{
    // Skip if storage type is memory
    auto plugin = drogon::app().getPlugin<OAuth2Plugin>();
    if (plugin && plugin->getStorageType() == "memory")
    {
        return;
    }

    drogon::nosql::RedisClientPtr client;
    try
    {
        client = drogon::app().getRedisClient("default");
    }
    catch (...)
    {
        LOG_WARN << "Redis client not available (Exception). Skipping Redis "
                    "integration tests.";
        return;
    }

    if (!client)
    {
        LOG_WARN << "Redis client not available (Null). Skipping Redis "
                    "integration tests.";
        return;
    }

    // 1. Setup
    auto storage = std::make_shared<RedisOAuth2Storage>();
    // RedisStorage doesn't necessarily need initFromConfig if using global
    // drogon redis client, but looking at valid implementation, it takes config
    // only for creation. The constructor uses "default" client name.

    // 2. Auth Code Flow Integration
    OAuth2AuthCode code;
    code.code = "test_redis_code_123";
    code.clientId = "vue-client";  // Must exist or we mimic it
    code.userId = "user_redis";
    code.scope = "read";
    code.redirectUri = "http://localhost/cb";
    code.expiresAt = std::time(nullptr) + 60;
    code.used = false;

    // Save
    {
        std::promise<void> p;
        auto f = p.get_future();
        storage->saveAuthCode(code, [&]() { p.set_value(); });
        if (f.wait_for(std::chrono::seconds(30)) == std::future_status::timeout)
        {
            throw std::runtime_error("TIMEOUT");
        }
        f.get();
        LOG_INFO << "Redis: Saved Auth Code";
    }

    // Get
    {
        std::promise<std::optional<OAuth2AuthCode>> p;
        auto f = p.get_future();
        storage->getAuthCode("test_redis_code_123", [&](auto c) { p.set_value(c); });
        if (f.wait_for(std::chrono::seconds(30)) == std::future_status::timeout)
        {
            throw std::runtime_error("TIMEOUT");
        }
        auto c = f.get();
        if (!c.has_value())
        {
            LOG_ERROR << "Redis: Failed to retrieve Auth Code";
        }
        CHECK(c.has_value());
        if (c)
        {
            CHECK(c->code == "test_redis_code_123");
            CHECK(c->clientId == "vue-client");
        }
        LOG_INFO << "Redis: Retrieved Auth Code";
    }

    // Cleanup
    {
        std::promise<void> p;
        auto f = p.get_future();
        client->execCommandAsync(
          [=](const drogon::nosql::RedisResult &r) {},
          [&](const std::exception &e) {},
          "DEL oauth2:code:test_redis_code_123"
        );
        // We don't strictly wait here because Redis is fast, but better to
        // wait? Actually execCommandAsync doesn't block. Let's rely on fast
        // execution or just fire and forget if acceptable, BUT strict cleanup
        // requires waiting. Since we don't have a callback to set promise in
        // the simple signature above (wait, I need callback signature)

        client->execCommandAsync(
          [&](const drogon::nosql::RedisResult &r) { p.set_value(); },
          [&](const std::exception &e) { p.set_value(); },
          "DEL oauth2:code:test_redis_code_123"
        );
        if (f.wait_for(std::chrono::seconds(30)) == std::future_status::timeout)
        {
            throw std::runtime_error("TIMEOUT");
        }
        f.get();
        LOG_INFO << "Redis: Cleaned up test keys";
    }
}
