
#include <drogon/drogon_test.h>
#include <drogon/drogon.h>
#include <oauth2/storage/PostgresOAuth2Storage.h>
#include <oauth2/plugin/OAuth2Plugin.h>
#include <future>

using namespace oauth2;

DROGON_TEST(Integration_P0_Storage_Postgres_Works)
{
    // Skip if storage type is memory
    auto plugin = drogon::app().getPlugin<OAuth2Plugin>();
    if (plugin && plugin->getStorageType() == "memory")
    {
        return;
    }

    // Check DB availability
    drogon::orm::DbClientPtr client;
    try
    {
        client = drogon::app().getDbClient();
    }
    catch (...)
    {
        LOG_WARN << "DB client not available (Exception). Skipping Postgres "
                    "integration tests.";
        return;
    }

    if (!client)
    {
        LOG_WARN << "DB client not available (Null). Skipping Postgres "
                    "integration tests.";
        return;
    }

    // 1. Setup
    auto storage = std::make_shared<PostgresOAuth2Storage>();
    storage->initFromConfig(Json::Value());  // Init with default client

    // 2. Auth Code Flow Integration
    OAuth2AuthCode code;
    code.code = "test_pg_code_123";
    code.clientId = "vue-client";  // Use seeded client
    code.userId = "user_pg";
    code.scope = "write";
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
        LOG_INFO << "Postgres: Saved Auth Code";
    }

    // Delay to ensure db consistency (optional/hacky but good for diagnosis)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Get
    {
        std::promise<std::optional<OAuth2AuthCode>> p;
        auto f = p.get_future();
        storage->getAuthCode("test_pg_code_123", [&](auto c) { p.set_value(c); });
        if (f.wait_for(std::chrono::seconds(30)) == std::future_status::timeout)
        {
            throw std::runtime_error("TIMEOUT");
        }
        auto c = f.get();
        if (!c.has_value())
        {
            LOG_ERROR << "Postgres: Failed to retrieve Auth Code";
        }
        CHECK(c.has_value());
        if (c)
        {
            CHECK(c->code == "test_pg_code_123");
            CHECK(c->clientId == "vue-client");
        }
        LOG_INFO << "Postgres: Retrieved Auth Code";
    }

    // Cleanup (Optional but good practice)
    // Cleanup (Synchronous wait)
    {
        std::promise<void> p;
        auto f = p.get_future();
        client->execSqlAsync(
          "DELETE FROM oauth2_codes WHERE code = $1",
          [&](const drogon::orm::Result &) { p.set_value(); },
          [&](const drogon::orm::DrogonDbException &e) {
              LOG_ERROR << "Postgres Cleanup Failed: " << e.base().what();
              p.set_value();
          },
          "test_pg_code_123"
        );
        if (f.wait_for(std::chrono::seconds(30)) == std::future_status::timeout)
        {
            throw std::runtime_error("TIMEOUT");
        }
        f.get();
        LOG_INFO << "Postgres: Cleaned up test data";
    }
}
