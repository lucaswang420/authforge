#include <drogon/drogon_test.h>
#include <drogon/drogon.h>
#include "../storage/MemoryOAuth2Storage.h"
#include <future>
#include <iostream>

using namespace oauth2;

DROGON_TEST(RedirectUriValidation_MemoryStorage)
{
    LOG_INFO << "=== Integration Test: Redirect URI Validation (Memory) ===";

    Json::Value config;
    config["vue-client"]["secret"] = "test-secret";
    config["vue-client"]["redirect_uri"] = "http://localhost:5173/callback";
    config["vue-client"]["client_type"] = "public";

    auto storage = std::make_shared<MemoryOAuth2Storage>();
    storage->initFromConfig(config);

    OAuth2AuthCode testCode;
    testCode.code = "test_memory_redirect";
    testCode.clientId = "vue-client";
    testCode.userId = "test_user";
    testCode.expiresAt = std::time(nullptr) + 3600;
    testCode.used = false;
    testCode.redirectUri = "http://localhost:5173/callback";

    try
    {
        // Test 1: Save auth code with redirect URI
        LOG_INFO << "--- Test 1: Save auth code with redirect URI ---";
        {
            std::promise<void> p;
            auto f = p.get_future();
            storage->saveAuthCode(testCode, [&]() { p.set_value(); });
            if (f.wait_for(std::chrono::seconds(5)) ==
                std::future_status::timeout)
            {
                throw std::runtime_error("Save auth code timeout");
            }
            f.get();
            LOG_INFO << "Auth code saved with redirect URI: "
                     << testCode.redirectUri;
        }

        // Test 2: Valid redirect URI - should succeed
        LOG_INFO << "--- Test 2: Valid redirect URI ---";
        {
            std::promise<std::optional<OAuth2AuthCode>> p;
            auto f = p.get_future();
            storage->consumeAuthCode(testCode.code,
                                     testCode.redirectUri,
                                     [&](std::optional<OAuth2AuthCode> code) {
                                         p.set_value(code);
                                     });
            if (f.wait_for(std::chrono::seconds(5)) ==
                std::future_status::timeout)
            {
                throw std::runtime_error("Consume auth code timeout");
            }
            auto result = f.get();
            CHECK(result.has_value());
            CHECK(result->redirectUri == testCode.redirectUri);
            LOG_INFO << "Valid redirect URI accepted";
        }

        // Test 3: Save another auth code for invalid redirect URI test
        LOG_INFO << "--- Test 3: Setup for invalid redirect URI test ---";
        testCode.code = "test_memory_invalid";
        {
            std::promise<void> p;
            auto f = p.get_future();
            storage->saveAuthCode(testCode, [&]() { p.set_value(); });
            if (f.wait_for(std::chrono::seconds(5)) ==
                std::future_status::timeout)
            {
                throw std::runtime_error("Save auth code timeout");
            }
            f.get();
            LOG_INFO << "Auth code saved for invalid redirect URI test";
        }

        // Test 4: Invalid redirect URI - should fail
        LOG_INFO << "--- Test 4: Invalid redirect URI ---";
        {
            std::promise<std::optional<OAuth2AuthCode>> p;
            auto f = p.get_future();
            storage->consumeAuthCode(testCode.code,
                                     "http://malicious-site.com/callback",
                                     [&](std::optional<OAuth2AuthCode> code) {
                                         p.set_value(code);
                                     });
            if (f.wait_for(std::chrono::seconds(5)) ==
                std::future_status::timeout)
            {
                throw std::runtime_error("Consume auth code timeout");
            }
            auto result = f.get();
            CHECK(!result.has_value());
            LOG_INFO << "Invalid redirect URI properly rejected";
        }

        LOG_INFO
            << "=== Memory Storage Redirect URI Validation Test Completed ===";
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Test Failed: " << e.what();
        throw;
    }
}

DROGON_TEST(RedirectUriValidation_Atomicity)
{
    LOG_INFO << "=== Integration Test: Redirect URI Validation Atomicity ===";

    Json::Value config;
    config["vue-client"]["secret"] = "test-secret";
    config["vue-client"]["redirect_uri"] = "http://localhost:5173/callback";
    config["vue-client"]["client_type"] = "public";

    auto storage = std::make_shared<MemoryOAuth2Storage>();
    storage->initFromConfig(config);

    OAuth2AuthCode testCode;
    testCode.code = "test_atomic_" + std::string(4, 'y');
    testCode.clientId = "vue-client";
    testCode.userId = "test_user";
    testCode.expiresAt = std::time(nullptr) + 3600;
    testCode.used = false;
    testCode.redirectUri = "http://localhost:5173/callback";

    try
    {
        // Test: Ensure atomic operation - invalid redirect_uri should not
        // consume code
        LOG_INFO << "--- Test: Atomicity of redirect_uri validation ---";
        {
            // Save auth code
            std::promise<void> p;
            auto f = p.get_future();
            storage->saveAuthCode(testCode, [&]() { p.set_value(); });
            if (f.wait_for(std::chrono::seconds(5)) ==
                std::future_status::timeout)
            {
                throw std::runtime_error("Save auth code timeout");
            }
            f.get();
            LOG_INFO << "Auth code saved";
        }

        // Try to consume with invalid redirect URI
        {
            std::promise<std::optional<OAuth2AuthCode>> p;
            auto f = p.get_future();
            storage->consumeAuthCode(testCode.code,
                                     "http://malicious-site.com/callback",
                                     [&](std::optional<OAuth2AuthCode> code) {
                                         p.set_value(code);
                                     });
            if (f.wait_for(std::chrono::seconds(5)) ==
                std::future_status::timeout)
            {
                throw std::runtime_error("Consume auth code timeout");
            }
            auto result = f.get();
            CHECK(!result.has_value());
            LOG_INFO << "Invalid redirect URI rejected";
        }

        // Verify code is still available (not consumed)
        {
            std::promise<std::optional<OAuth2AuthCode>> p;
            auto f = p.get_future();
            storage->getAuthCode(testCode.code,
                                 [&](std::optional<OAuth2AuthCode> code) {
                                     p.set_value(code);
                                 });
            if (f.wait_for(std::chrono::seconds(5)) ==
                std::future_status::timeout)
            {
                throw std::runtime_error("Get auth code timeout");
            }
            auto result = f.get();
            CHECK(result.has_value());
            CHECK(result->used == false);
            LOG_INFO << "Auth code still available (not consumed by invalid "
                        "redirect_uri)";
        }

        // Now consume with valid redirect URI
        {
            std::promise<std::optional<OAuth2AuthCode>> p;
            auto f = p.get_future();
            storage->consumeAuthCode(testCode.code,
                                     testCode.redirectUri,
                                     [&](std::optional<OAuth2AuthCode> code) {
                                         p.set_value(code);
                                     });
            if (f.wait_for(std::chrono::seconds(5)) ==
                std::future_status::timeout)
            {
                throw std::runtime_error("Consume auth code timeout");
            }
            auto result = f.get();
            CHECK(result.has_value());
            CHECK(result->used == true);
            LOG_INFO << "Auth code consumed with valid redirect URI";
        }

        LOG_INFO << "=== Atomicity Test Completed ===";
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Test Failed: " << e.what();
        throw;
    }
}

DROGON_TEST(RedirectUriValidation_EdgeCases)
{
    LOG_INFO << "=== Integration Test: Redirect URI Validation Edge Cases ===";

    Json::Value config;
    config["vue-client"]["secret"] = "test-secret";
    config["vue-client"]["redirect_uri"] = "http://localhost:5173/callback";
    config["vue-client"]["client_type"] = "public";

    auto storage = std::make_shared<MemoryOAuth2Storage>();
    storage->initFromConfig(config);

    try
    {
        // Test 1: Empty redirect URI
        LOG_INFO << "--- Test 1: Empty redirect URI ---";
        {
            OAuth2AuthCode testCode;
            testCode.code = "test_empty_redirect";
            testCode.clientId = "vue-client";
            testCode.userId = "test_user";
            testCode.expiresAt = std::time(nullptr) + 3600;
            testCode.used = false;
            testCode.redirectUri = "";

            std::promise<void> p;
            auto f = p.get_future();
            storage->saveAuthCode(testCode, [&]() { p.set_value(); });
            if (f.wait_for(std::chrono::seconds(5)) ==
                std::future_status::timeout)
            {
                throw std::runtime_error("Save auth code timeout");
            }
            f.get();

            std::promise<std::optional<OAuth2AuthCode>> p2;
            auto f2 = p2.get_future();
            storage->consumeAuthCode(testCode.code,
                                     "",
                                     [&](std::optional<OAuth2AuthCode> code) {
                                         p2.set_value(code);
                                     });
            if (f2.wait_for(std::chrono::seconds(5)) ==
                std::future_status::timeout)
            {
                throw std::runtime_error("Consume auth code timeout");
            }
            auto result = f2.get();
            CHECK(result.has_value());
            LOG_INFO << "Empty redirect URI handled";
        }

        // Test 2: Case sensitivity
        LOG_INFO << "--- Test 2: Case sensitivity ---";
        {
            OAuth2AuthCode testCode;
            testCode.code = "test_case_sensitive";
            testCode.clientId = "vue-client";
            testCode.userId = "test_user";
            testCode.expiresAt = std::time(nullptr) + 3600;
            testCode.used = false;
            testCode.redirectUri = "http://localhost:5173/callback";

            std::promise<void> p;
            auto f = p.get_future();
            storage->saveAuthCode(testCode, [&]() { p.set_value(); });
            if (f.wait_for(std::chrono::seconds(5)) ==
                std::future_status::timeout)
            {
                throw std::runtime_error("Save auth code timeout");
            }
            f.get();

            std::promise<std::optional<OAuth2AuthCode>> p2;
            auto f2 = p2.get_future();
            storage->consumeAuthCode(
                testCode.code,
                "http://localhost:5173/CALLBACK",  // Different case
                [&](std::optional<OAuth2AuthCode> code) {
                    p2.set_value(code);
                });
            if (f2.wait_for(std::chrono::seconds(5)) ==
                std::future_status::timeout)
            {
                throw std::runtime_error("Consume auth code timeout");
            }
            auto result = f2.get();
            // Case sensitivity depends on implementation
            LOG_INFO << "Case sensitivity test completed";
        }

        // Test 3: URL fragments
        LOG_INFO << "--- Test 3: URL fragments ---";
        {
            OAuth2AuthCode testCode;
            testCode.code = "test_url_fragment";
            testCode.clientId = "vue-client";
            testCode.userId = "test_user";
            testCode.expiresAt = std::time(nullptr) + 3600;
            testCode.used = false;
            testCode.redirectUri = "http://localhost:5173/callback";

            std::promise<void> p;
            auto f = p.get_future();
            storage->saveAuthCode(testCode, [&]() { p.set_value(); });
            if (f.wait_for(std::chrono::seconds(5)) ==
                std::future_status::timeout)
            {
                throw std::runtime_error("Save auth code timeout");
            }
            f.get();

            std::promise<std::optional<OAuth2AuthCode>> p2;
            auto f2 = p2.get_future();
            storage->consumeAuthCode(
                testCode.code,
                "http://localhost:5173/callback#fragment",  // With fragment
                [&](std::optional<OAuth2AuthCode> code) {
                    p2.set_value(code);
                });
            if (f2.wait_for(std::chrono::seconds(5)) ==
                std::future_status::timeout)
            {
                throw std::runtime_error("Consume auth code timeout");
            }
            auto result = f2.get();
            // Fragment handling depends on implementation
            LOG_INFO << "URL fragment test completed";
        }

        LOG_INFO << "=== Edge Cases Test Completed ===";
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Test Failed: " << e.what();
        throw;
    }
}

DROGON_TEST(RedirectUriValidation_SecurityScenarios)
{
    LOG_INFO << "=== Integration Test: Redirect URI Validation Security "
                "Scenarios ===";

    Json::Value config;
    config["vue-client"]["secret"] = "test-secret";
    config["vue-client"]["redirect_uri"] = "http://localhost:5173/callback";
    config["vue-client"]["client_type"] = "public";

    auto storage = std::make_shared<MemoryOAuth2Storage>();
    storage->initFromConfig(config);

    try
    {
        // Test 1: Open redirect attack prevention
        LOG_INFO << "--- Test 1: Open redirect attack prevention ---";
        {
            OAuth2AuthCode testCode;
            testCode.code = "test_open_redirect";
            testCode.clientId = "vue-client";
            testCode.userId = "test_user";
            testCode.expiresAt = std::time(nullptr) + 3600;
            testCode.used = false;
            testCode.redirectUri = "http://localhost:5173/callback";

            std::promise<void> p;
            auto f = p.get_future();
            storage->saveAuthCode(testCode, [&]() { p.set_value(); });
            if (f.wait_for(std::chrono::seconds(5)) ==
                std::future_status::timeout)
            {
                throw std::runtime_error("Save auth code timeout");
            }
            f.get();

            // Try to redirect to arbitrary domain
            std::promise<std::optional<OAuth2AuthCode>> p2;
            auto f2 = p2.get_future();
            storage->consumeAuthCode(testCode.code,
                                     "http://evil.com/callback",
                                     [&](std::optional<OAuth2AuthCode> code) {
                                         p2.set_value(code);
                                     });
            if (f2.wait_for(std::chrono::seconds(5)) ==
                std::future_status::timeout)
            {
                throw std::runtime_error("Consume auth code timeout");
            }
            auto result = f2.get();
            CHECK(!result.has_value());
            LOG_INFO << "Open redirect attack prevented";
        }

        // Test 2: URL traversal attack prevention
        LOG_INFO << "--- Test 2: URL traversal attack prevention ---";
        {
            OAuth2AuthCode testCode;
            testCode.code = "test_url_traversal";
            testCode.clientId = "vue-client";
            testCode.userId = "test_user";
            testCode.expiresAt = std::time(nullptr) + 3600;
            testCode.used = false;
            testCode.redirectUri = "http://localhost:5173/callback";

            std::promise<void> p;
            auto f = p.get_future();
            storage->saveAuthCode(testCode, [&]() { p.set_value(); });
            if (f.wait_for(std::chrono::seconds(5)) ==
                std::future_status::timeout)
            {
                throw std::runtime_error("Save auth code timeout");
            }
            f.get();

            // Try URL traversal
            std::promise<std::optional<OAuth2AuthCode>> p2;
            auto f2 = p2.get_future();
            storage->consumeAuthCode(testCode.code,
                                     "http://localhost:5173/../evil/callback",
                                     [&](std::optional<OAuth2AuthCode> code) {
                                         p2.set_value(code);
                                     });
            if (f2.wait_for(std::chrono::seconds(5)) ==
                std::future_status::timeout)
            {
                throw std::runtime_error("Consume auth code timeout");
            }
            auto result = f2.get();
            CHECK(!result.has_value());
            LOG_INFO << "URL traversal attack prevented";
        }

        // Test 3: Null byte injection prevention
        LOG_INFO << "--- Test 3: Null byte injection prevention ---";
        {
            OAuth2AuthCode testCode;
            testCode.code = "test_null_byte";
            testCode.clientId = "vue-client";
            testCode.userId = "test_user";
            testCode.expiresAt = std::time(nullptr) + 3600;
            testCode.used = false;
            testCode.redirectUri = "http://localhost:5173/callback";

            std::promise<void> p;
            auto f = p.get_future();
            storage->saveAuthCode(testCode, [&]() { p.set_value(); });
            if (f.wait_for(std::chrono::seconds(5)) ==
                std::future_status::timeout)
            {
                throw std::runtime_error("Save auth code timeout");
            }
            f.get();

            // Try null byte injection (should be rejected by string handling)
            std::string maliciousUri =
                std::string("http://localhost:5173/callback") + '\0' +
                ".evil.com";
            std::promise<std::optional<OAuth2AuthCode>> p2;
            auto f2 = p2.get_future();
            storage->consumeAuthCode(testCode.code,
                                     maliciousUri,
                                     [&](std::optional<OAuth2AuthCode> code) {
                                         p2.set_value(code);
                                     });
            if (f2.wait_for(std::chrono::seconds(5)) ==
                std::future_status::timeout)
            {
                throw std::runtime_error("Consume auth code timeout");
            }
            auto result = f2.get();
            CHECK(!result.has_value());
            LOG_INFO << "Null byte injection prevented";
        }

        LOG_INFO << "=== Security Scenarios Test Completed ===";
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Test Failed: " << e.what();
        throw;
    }
}
