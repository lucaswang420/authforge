#pragma once

#include <oauth2/storage/IOAuth2Storage.h>
#include <drogon/nosql/RedisClient.h>
#include <drogon/CacheMap.h>
#include <memory>

namespace oauth2
{

/**
 * @brief Decorator for IOAuth2Storage that adds L1 Memory and L2 Redis Caching
 *
 * Defect 1.8 (incl. original 1.6) lifetime safety: this decorator inherits
 * std::enable_shared_from_this so its async continuations can capture an
 * `auto self = shared_from_this();` strong reference. That keeps the host
 * object (and its tokenCache_/clientCache_/redisClient_ members) alive until
 * the in-flight callback finishes, even if OAuth2Plugin::shutdown() calls
 * storage_.reset() in the meantime.
 *
 * Option B (nested ownership): impl_ is a std::shared_ptr so the inner storage
 * (created via make_shared from its CONCRETE type) keeps its own armed control
 * block and shared_from_this() is valid for the inner storage in BOTH the
 * wrapped (impl_) and direct roles.
 */
class CachedOAuth2Storage : public IOAuth2Storage,
                            public std::enable_shared_from_this<CachedOAuth2Storage>
{
  public:
    CachedOAuth2Storage(
      std::shared_ptr<IOAuth2Storage> impl,
      drogon::nosql::RedisClientPtr redisClient
    );

    // Client Operations - CACHED (L1)
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

    // Access Token Operations - CACHED (L1 + L2)
    void saveAccessToken(const OAuth2AccessToken &token, VoidCallback &&cb) override;
    void getAccessToken(
      const std::string &token,
      AccessTokenCallback &&cb
    ) override;  // Reads from Cache

    // Refresh Token Operations - Pass through for now
    void saveRefreshToken(const OAuth2RefreshToken &token, VoidCallback &&cb) override;
    void getRefreshToken(const std::string &token, RefreshTokenCallback &&cb) override;
    void revokeRefreshToken(const std::string &token, VoidCallback &&cb) override;
    void atomicRevokeRefreshToken(const std::string &token, RefreshTokenCallback &&cb) override;
    void revokeTokenFamily(const std::string &familyId, VoidCallback &&cb) override;

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

    // ========== P1: Token Introspection (RFC 7662) ==========
    void introspectToken(const std::string &token, TokenIntrospectionCallback &&cb) override;
    void incrementIntrospectCount(const std::string &token, VoidCallback &&cb) override;

    // ========== P1: Token Revocation (RFC 7009) ==========
    void revokeAccessToken(
      const std::string &token,
      const std::string &revokedBy,
      VoidCallback &&cb
    ) override;

    // ========== User Info Operations ==========
    void getUserInfo(const std::string &userId, OptionalJsonCallback &&cb) override;
    void getUserInfo(int32_t internalUserId, OptionalJsonCallback &&cb) override;

  private:
    std::shared_ptr<IOAuth2Storage> impl_;
    drogon::nosql::RedisClientPtr redisClient_;
    drogon::CacheMap<std::string, OAuth2AccessToken> tokenCache_;
    drogon::CacheMap<std::string, OAuth2Client> clientCache_;
};

}  // namespace oauth2
