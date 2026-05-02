#pragma once

#include <drogon/plugins/Plugin.h>
#include "IOAuth2Storage.h"
#include "OAuth2CleanupService.h"
#include <string>
#include <memory>
#include <functional>

class OAuth2Plugin : public drogon::Plugin<OAuth2Plugin>
{
  public:
    using AccessToken = oauth2::OAuth2AccessToken;
    using Client = oauth2::OAuth2Client;

    OAuth2Plugin() = default;
    void initAndStart(const Json::Value &config) override;
    void shutdown() override;

    // ========== Async API with Callbacks ==========

    /**
     * @brief Validate if client exists and secret matches (Async)
     */
    void validateClient(const std::string &clientId,
                        const std::string &clientSecret,
                        std::function<void(bool)> &&callback);

    /**
     * @brief Validate redirect URI (Async)
     */
    void validateRedirectUri(const std::string &clientId,
                             const std::string &redirectUri,
                             std::function<void(bool)> &&callback);

    /**
     * @brief Generate Authorization Code (Async)
     */
    void generateAuthorizationCode(const std::string &clientId,
                                   const std::string &userId,
                                   const std::string &scope,
                                   const std::string &redirectUri,
                                   std::function<void(std::string)> &&callback);

    /**
     * @brief Exchange Code for Access Token (Async)
     * Returns JSON with {access_token, refresh_token, expires_in} or {error}
     *
     * @param code Authorization code from authorize endpoint
     * @param clientId Client identifier
     * @param clientSecret Client secret (required for CONFIDENTIAL clients,
     * empty for PUBLIC)
     * @param redirectUri Redirect URI from token request (must match
     * authorization request per OAuth2 RFC 6749 Section 4.1.3)
     * @param callback Callback with token response or error
     */
    void exchangeCodeForToken(
        const std::string &code,
        const std::string &clientId,
        const std::string &clientSecret,
        const std::string &redirectUri,
        std::function<void(const Json::Value &)> &&callback);

    /**
     * @brief Refresh Access Token (Async)
     * Returns JSON with {access_token, refresh_token, expires_in} or {error}
     */
    void refreshAccessToken(
        const std::string &refreshToken,
        const std::string &clientId,
        std::function<void(const Json::Value &)> &&callback);

    /**
     * @brief Validate Access Token (Async)
     */
    void validateAccessToken(
        const std::string &token,
        std::function<void(std::shared_ptr<AccessToken>)> &&callback);

    /**
     * @brief Get User Roles (Async)
     */
    void getUserRoles(const std::string &userId,
                      std::function<void(std::vector<std::string>)> &&callback);

    // ========== Storage Access ==========
    oauth2::IOAuth2Storage *getStorage()
    {
        return storage_.get();
    }

    const std::string &getStorageType() const
    {
        return storageType_;
    }

  private:
    std::unique_ptr<oauth2::IOAuth2Storage> storage_;
    std::shared_ptr<oauth2::OAuth2CleanupService> cleanupService_;
    std::string storageType_;

    // TTL Configuration (Seconds)
    // Note: These are set once during initAndStart() and only read afterwards
    // Thread-safe due to happens-before guarantee (init before requests)
    long long authCodeTtl_{600};
    long long accessTokenTtl_{3600};
    long long refreshTokenTtl_{3600 * 24 * 30};

    void initStorage(const Json::Value &config);
};
