#include <oauth2/ConfigManager.h>
#include <drogon/drogon.h>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <algorithm>

namespace common::config
{

bool ConfigManager::load(const std::string &configPath, Json::Value &config)
{
    std::ifstream configFile(configPath);
    if (!configFile.is_open())
    {
        LOG_ERROR << "Config file not found: " << configPath;
        return false;
    }

    Json::CharReaderBuilder builder;
    std::string errs;
    if (!Json::parseFromStream(builder, configFile, &config, &errs))
    {
        LOG_ERROR << "Failed to parse config file: " << errs;
        return false;
    }

    // Apply environment variable overrides
    applyEnvOverrides(config, OAUTH2_ENV_OVERRIDES);

    return true;
}

void ConfigManager::applyEnvOverrides(Json::Value &config, const std::vector<EnvOverride> &rules)
{
    for (const auto &rule : rules)
    {
        if (const char *envValue = std::getenv(rule.envVar))
        {
            Json::Value *ptr = getJsonPointer(config, rule.configPath);
            if (ptr)
            {
                if (rule.isNumeric)
                {
                    *ptr = parseInt(envValue);
                }
                else
                {
                    *ptr = envValue;
                }
            }
        }
    }
}

Json::Value *ConfigManager::getJsonPointer(Json::Value &root, const std::string &path)
{
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string part;

    while (std::getline(ss, part, '.'))
    {
        parts.push_back(part);
    }

    Json::Value *current = &root;
    for (const auto &p : parts)
    {
        if (current->isNull())
        {
            return nullptr;
        }

        // Check if it's an array index
        if (!p.empty() && std::all_of(p.begin(), p.end(), ::isdigit))
        {
            if (!current->isArray())
            {
                return nullptr;
            }
            size_t index = std::stoul(p);
            if (index >= current->size())
            {
                return nullptr;
            }
            current = &((*current)[static_cast<int>(index)]);
        }
        else
        {
            if (!current->isObject() || !current->isMember(p))
            {
                return nullptr;
            }
            current = &((*current)[p]);
        }
    }

    return current;
}

int ConfigManager::parseInt(const std::string &str)
{
    try
    {
        return std::stoi(str);
    }
    catch (...)
    {
        return 0;
    }
}

bool ConfigManager::validate(const Json::Value &config, std::string &errorMessage)
{
    // Check db_clients section exists and is an array (can be empty for memory
    // storage)
    if (!config.isMember("db_clients") || !config["db_clients"].isArray())
    {
        errorMessage = "Missing or invalid 'db_clients' configuration";
        return false;
    }

    // Check redis_clients section exists and is an array (can be empty for
    // memory storage)
    if (!config.isMember("redis_clients") || !config["redis_clients"].isArray())
    {
        errorMessage = "Missing or invalid 'redis_clients' configuration";
        return false;
    }

    // Validate port ranges if db_clients is not empty
    if (config["db_clients"].size() > 0 && config["db_clients"][0].isMember("port"))
    {
        int port = config["db_clients"][0]["port"].asInt();
        if (port < 1 || port > 65535)
        {
            errorMessage = "Database port out of range (1-65535)";
            return false;
        }
    }

    // Validate port ranges if redis_clients is not empty
    if (config["redis_clients"].size() > 0 && config["redis_clients"][0].isMember("port"))
    {
        int port = config["redis_clients"][0]["port"].asInt();
        if (port < 1 || port > 65535)
        {
            errorMessage = "Redis port out of range (1-65535)";
            return false;
        }
    }

    return true;
}

}  // namespace common::config
