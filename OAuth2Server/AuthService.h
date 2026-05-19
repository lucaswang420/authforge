#pragma once

#include <drogon/drogon.h>
#include <functional>
#include <string>
#include <optional>

namespace services
{

/**
 * @brief Result of a successful user authentication
 */
struct AuthResult
{
    int internalId;        // Internal auto-increment ID (for DB operations)
    std::string publicSub; // Public UUID subject (for OAuth2 tokens, never expose internalId)
};

class AuthService
{
  public:
    /**
     * @brief Async validate user credentials
     * @param callback Returns AuthResult on success, nullopt on failure
     */
    static void validateUser(
      const std::string &username,
      const std::string &password,
      std::function<void(std::optional<AuthResult>)> &&callback
    );

    /**
     * @brief Async register a new user
     * @param callback Returns empty string on success, error message on failure
     */
    static void registerUser(
      const std::string &username,
      const std::string &password,
      const std::string &email,
      std::function<void(const std::string &error)> &&callback
    );

    /**
     * @brief Fetch user info and roles from database
     * @param userId Internal user ID
     * @param callback Returns user info JSON or nullopt if not found
     */
    static void getUserInfo(
      int userId,
      std::function<void(std::optional<Json::Value> userInfo)> &&callback
    );
};

}  // namespace services
