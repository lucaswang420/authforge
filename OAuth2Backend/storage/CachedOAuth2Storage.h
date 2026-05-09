#pragma once

#include "IOAuth2Storage.h"
#include <drogon/nosql/RedisClient.h>
#include <memory>

namespace oauth2
{

/**
 * @brief Decorator for IOAuth2Storage that adds L2 Redis Caching
 */
class CachedOAuth2Storage : public IOAuth2Storage
{
  public:
    CachedOAuth2Storage(
      std::unique_ptr<IOAuth2Storage> impl,
      drogon::nosql::RedisClientPtr redisClient
    );

    // Client Operations - Pass through or Cache if needed
    void getClient(const std::string &clientId, ClientCallback &&cb) override;
    void validateClient(
      const std::string &clientId,
      const std::string &clientSecret,
      BoolCallback &&cb
    ) override;

    // Authorization Code Operations - Pass through
    void saveAuthCode(const OAuth2AuthCode &code, VoidCallback &&cb) override;
    void getAuthCode(const std::string &code, AuthCodeCallback &&cb) override;
    void markAuthCodeUsed(const std::string &code, VoidCallback &&cb) override;
    void consumeAuthCode(
      const std::string &code,
      const std::string &redirectUri,
      AuthCodeCallback &&cb
    ) override;

    // Access Token Operations - CACHED
    void saveAccessToken(const OAuth2AccessToken &token, VoidCallback &&cb) override;
    void getAccessToken(
      const std::string &token,
      AccessTokenCallback &&cb
    ) override;  // Reads from Cache

    // Refresh Token Operations - Pass through for now
    void saveRefreshToken(const OAuth2RefreshToken &token, VoidCallback &&cb) override;
    void getRefreshToken(const std::string &token, RefreshTokenCallback &&cb) override;
    void revokeRefreshToken(const std::string &token, VoidCallback &&cb) override;

    // Cleanup Operations
    void deleteExpiredData() override;

    // RBAC
    void getUserRoles(const std::string &userId, StringListCallback &&cb) override;
    void getUserRoles(int32_t internalUserId, StringListCallback &&cb) override;

    // Subject Mapping Operations - Pass through
    void getInternalUserId(
      const std::string &subject,
      const std::string &provider,
      OptionalIntCallback &&cb
    ) override;
    void createSubjectMapping(
      const std::string &subject,
      int32_t internalUserId,
      const std::string &provider,
      BoolCallback &&cb
    ) override;

    // Authorization Transaction Operations - Pass through
    void saveAuthorizationTransaction(
      const AuthorizationTransaction &transaction,
      BoolCallback &&cb
    ) override;
    void getAuthorizationTransaction(
      const std::string &transactionId,
      TransactionCallback &&cb
    ) override;
    void deleteAuthorizationTransaction(
      const std::string &transactionId,
      VoidCallback &&cb
    ) override;
    void markTransactionConsumed(const std::string &transactionId, BoolCallback &&cb) override;

    // Scope Management Operations - Pass through
    void hasUserConsent(
      int32_t internalUserId,
      const std::string &clientId,
      const std::string &scope,
      BoolCallback &&cb
    ) override;
    void saveUserConsent(
      int32_t internalUserId,
      const std::string &clientId,
      const std::string &scope,
      BoolCallback &&cb
    ) override;
    void revokeUserConsent(
      int32_t internalUserId,
      const std::string &clientId,
      const std::string &scope,
      VoidCallback &&cb
    ) override;

  private:
    std::unique_ptr<IOAuth2Storage> impl_;
    drogon::nosql::RedisClientPtr redisClient_;
};

}  // namespace oauth2
