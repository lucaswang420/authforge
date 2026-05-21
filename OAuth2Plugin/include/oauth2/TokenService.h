#pragma once

#include "IOAuth2Storage.h"
#include <drogon/drogon.h>
#include <memory>
#include <string>
#include <functional>

namespace oauth2
{

class JwkManager;  // Forward declaration

class TokenService
{
  public:
    explicit TokenService(
      IOAuth2Storage *storage,
      int64_t authCodeTtl = 600,
      int64_t accessTokenTtl = 3600,
      int64_t refreshTokenTtl = 2592000
    );

    void setJwkManager(std::shared_ptr<JwkManager> jwkManager)
    {
        jwkManager_ = jwkManager;
    }

    /**
     * @brief Generate Authorization Code
     */
    void generateAuthorizationCode(
      const std::string &clientId,
      const std::string &subject,
      const std::string &scope,
      const std::string &redirectUri,
      const std::string &codeChallenge,
      const std::string &codeChallengeMethod,
      const std::string &nonce,
      std::function<void(bool, std::string, std::string)> &&callback
    );

    /**
     * @brief Exchange Code for Access Token
     */
    void exchangeCodeForToken(
      const std::string &code,
      const std::string &clientId,
      const std::string &clientSecret,
      const std::string &redirectUri,
      const std::string &codeVerifier,
      std::function<void(const Json::Value &)> &&callback
    );

    /**
     * @brief Refresh Access Token
     */
    void refreshAccessToken(
      const std::string &refreshToken,
      const std::string &clientId,
      std::function<void(const Json::Value &)> &&callback
    );

    /**
     * @brief Validate Access Token
     */
    void validateAccessToken(
      const std::string &token,
      std::function<void(std::shared_ptr<OAuth2AccessToken>)> &&callback
    );

    /**
     * @brief Introspect Token (RFC 7662)
     */
    void introspectToken(
      const std::string &token,
      std::function<void(std::optional<TokenIntrospection>)> &&callback
    );

    /**
     * @brief Revoke Access Token (RFC 7009)
     */
    void revokeAccessToken(
      const std::string &token,
      const std::string &revokedBy,
      std::function<void()> &&callback
    );

    bool validatePkceCodeVerifier(
      const std::string &codeVerifier,
      const std::string &codeChallenge,
      const std::string &codeChallengeMethod
    );

    std::string generateSha256Hash(const std::string &input);

  private:
    IOAuth2Storage *storage_;
    int64_t authCodeTtl_;
    int64_t accessTokenTtl_;
    int64_t refreshTokenTtl_;
    std::shared_ptr<JwkManager> jwkManager_;
};

}  // namespace oauth2
