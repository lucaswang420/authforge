#include <drogon/drogon_test.h>
#include <drogon/drogon.h>
#include <filesystem>
#include <oauth2/config/ConfigManager.h>
#include <oauth2/plugin/OAuth2Plugin.h>

#ifdef _WIN32
#define setenv(name, value, overwrite) _putenv_s(name, value)
#define unsetenv(name) _putenv_s(name, "")
#endif

// ============================================================================
// Database-Agnostic Tests (Run in all storage modes)
// ============================================================================

DROGON_TEST(Unit_P0_ConfigManager_Legacy_LoadValidConfig)
{
    std::string configPath = "./config.json";
    if (!std::filesystem::exists(configPath))
        configPath = "../config.json";
    if (!std::filesystem::exists(configPath))
        configPath = "../../config.json";
    if (!std::filesystem::exists(configPath))
        configPath = "../../../config.json";

    Json::Value config;
    CHECK(common::config::ConfigManager::load(configPath, config) == true);
    CHECK(config.isNull() == false);
    CHECK(config.isMember("db_clients") == true);
}

DROGON_TEST(Unit_P0_ConfigManager_Legacy_TypeSafeAccessWithDefault)
{
    Json::Value config;
    config["port"] = 8080;

    auto port = common::config::ConfigManager::get<int>(config, "port", 0);
    CHECK(port == 8080);

    auto missing = common::config::ConfigManager::get<int>(config, "missing", 123);
    CHECK(missing == 123);
}

DROGON_TEST(Unit_P0_ConfigManager_Legacy_ValidateMissingRequiredField)
{
    Json::Value config;
    // Test completely missing db_clients field (not even empty array)
    // Empty arrays are valid for memory storage mode

    std::string errMsg;
    CHECK(common::config::ConfigManager::validate(config, errMsg) == false);
    CHECK(errMsg.find("db_clients") != std::string::npos);
}

DROGON_TEST(Unit_P0_ConfigManager_Legacy_ValidatePortRange)
{
    Json::Value config;
    config["db_clients"][0]["port"] = 70000;  // Invalid port
    config["redis_clients"][0]["port"] = 65535;

    std::string errMsg;
    CHECK(common::config::ConfigManager::validate(config, errMsg) == false);
    CHECK(errMsg.find("port") != std::string::npos);
}

// ============================================================================
// Database-Dependent Tests (Skipped in memory storage mode)
// ============================================================================

DROGON_TEST(Unit_P0_ConfigManager_Legacy_Database_EnvOverrideDbHost)
{
    // Skip this test in memory storage mode (no db_clients configured)
    auto plugin = drogon::app().getPlugin<OAuth2Plugin>();
    if (plugin && plugin->getStorageType() == "memory")
    {
        return;
    }

    // Set environment variable
    setenv("OAUTH2_DB_HOST", "test-host", 1);

    std::string configPath = "./config.json";
    if (!std::filesystem::exists(configPath))
        configPath = "../config.json";
    if (!std::filesystem::exists(configPath))
        configPath = "../../config.json";
    if (!std::filesystem::exists(configPath))
        configPath = "../../../config.json";

    Json::Value config;
    CHECK(common::config::ConfigManager::load(configPath, config) == true);

    auto dbHost = common::config::ConfigManager::get<std::string>(config, "db_clients.0.host");
    CHECK(dbHost == "test-host");

    unsetenv("OAUTH2_DB_HOST");
}
