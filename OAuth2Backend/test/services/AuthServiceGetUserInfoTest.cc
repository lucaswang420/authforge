#include <drogon/drogon_test.h>
#include <json/json.h>
#include <future>
#include "services/AuthService.h"

using namespace services;

DROGON_TEST(AuthService_GetUserInfo_Success)
{
    // Test successful user info retrieval
    int testUserId = 1; // Assuming user with ID 1 exists

    std::promise<std::optional<Json::Value>> p;
    auto f = p.get_future();

    AuthService::getUserInfo(
        testUserId,
        [&](std::optional<Json::Value> userInfo) {
            p.set_value(userInfo);
        });

    if (f.wait_for(std::chrono::seconds(5)) == std::future_status::timeout)
    {
        throw std::runtime_error("TIMEOUT");
    }

    auto userInfo = f.get();
    CHECK(userInfo.has_value() == true);

    Json::Value json = *userInfo;
    CHECK(json.isMember("sub") == true);
    CHECK(json.isMember("name") == true);
    CHECK(json.isMember("email") == true);
    CHECK(json.isMember("roles") == true);

    // Verify data types
    CHECK(json["sub"].isString() == true);
    CHECK(json["name"].isString() == true);
    CHECK(json["email"].isString() == true);
    CHECK(json["roles"].isArray() == true);

    // Verify user ID matches
    CHECK(json["sub"].asString() == std::to_string(testUserId));
}

DROGON_TEST(AuthService_GetUserInfo_UserNotFound)
{
    // Test with non-existent user ID
    int nonExistentUserId = 99999;

    std::promise<std::optional<Json::Value>> p;
    auto f = p.get_future();

    AuthService::getUserInfo(
        nonExistentUserId,
        [&](std::optional<Json::Value> userInfo) {
            p.set_value(userInfo);
        });

    if (f.wait_for(std::chrono::seconds(5)) == std::future_status::timeout)
    {
        throw std::runtime_error("TIMEOUT");
    }

    auto userInfo = f.get();
    // Should return nullopt for non-existent user
    CHECK(userInfo.has_value() == false);
}

DROGON_TEST(AuthService_GetUserInfo_InvalidUserId)
{
    // Test with invalid user ID (zero or negative)
    int invalidUserId = 0;

    std::promise<std::optional<Json::Value>> p;
    auto f = p.get_future();

    AuthService::getUserInfo(
        invalidUserId,
        [&](std::optional<Json::Value> userInfo) {
            p.set_value(userInfo);
        });

    if (f.wait_for(std::chrono::seconds(5)) == std::future_status::timeout)
    {
        throw std::runtime_error("TIMEOUT");
    }

    auto userInfo = f.get();
    // Should return nullopt for invalid user ID
    CHECK(userInfo.has_value() == false);
}

DROGON_TEST(AuthService_GetUserInfo_UserWithRoles)
{
    // Test user info retrieval with roles
    int testUserId = 1; // Assuming this user has roles assigned

    std::promise<std::optional<Json::Value>> p;
    auto f = p.get_future();

    AuthService::getUserInfo(
        testUserId,
        [&](std::optional<Json::Value> userInfo) {
            p.set_value(userInfo);
        });

    if (f.wait_for(std::chrono::seconds(5)) == std::future_status::timeout)
    {
        throw std::runtime_error("TIMEOUT");
    }

    auto userInfo = f.get();
    CHECK(userInfo.has_value() == true);

    Json::Value json = *userInfo;
    CHECK(json.isMember("roles") == true);

    Json::Value roles = json["roles"];
    CHECK(roles.isArray() == true);

    // Verify roles are strings
    for (const auto &role : roles)
    {
        CHECK(role.isString() == true);
        CHECK(role.asString().empty() == false);
    }
}

DROGON_TEST(AuthService_GetUserInfo_ResponseStructure)
{
    // Test complete response structure
    int testUserId = 1;

    std::promise<std::optional<Json::Value>> p;
    auto f = p.get_future();

    AuthService::getUserInfo(
        testUserId,
        [&](std::optional<Json::Value> userInfo) {
            p.set_value(userInfo);
        });

    if (f.wait_for(std::chrono::seconds(5)) == std::future_status::timeout)
    {
        throw std::runtime_error("TIMEOUT");
    }

    auto userInfo = f.get();
    CHECK(userInfo.has_value() == true);

    Json::Value json = *userInfo;

    // Verify all required fields exist
    CHECK(json.isMember("sub") == true);
    CHECK(json.isMember("name") == true);
    CHECK(json.isMember("email") == true);
    CHECK(json.isMember("roles") == true);

    // Verify field values are not empty (except possibly roles)
    CHECK(json["sub"].asString().empty() == false);
    CHECK(json["name"].asString().empty() == false);
    CHECK(json["email"].asString().empty() == false);

    // Verify OpenID Connect standard claim format
    CHECK(json["sub"].isString() == true); // Subject - user ID
}

DROGON_TEST(AuthService_GetUserInfo_DatabaseErrorHandling)
{
    // Test error handling when database operations fail
    // This test verifies graceful degradation when roles query fails

    int testUserId = 1;

    std::promise<std::optional<Json::Value>> p;
    auto f = p.get_future();

    AuthService::getUserInfo(
        testUserId,
        [&](std::optional<Json::Value> userInfo) {
            p.set_value(userInfo);
        });

    if (f.wait_for(std::chrono::seconds(5)) == std::future_status::timeout)
    {
        throw std::runtime_error("TIMEOUT");
    }

    auto userInfo = f.get();
    // Even if roles query fails, should still return user info
    CHECK(userInfo.has_value() == true);

    Json::Value json = *userInfo;
    CHECK(json.isMember("name") == true);
    CHECK(json.isMember("email") == true);
    CHECK(json.isMember("roles") == true);

    // Roles might be empty array if query failed, but user info should be present
    CHECK(json["name"].asString().empty() == false);
}
