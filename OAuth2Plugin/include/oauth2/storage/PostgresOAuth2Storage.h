#pragma once

#include <oauth2/storage/IOAuth2Storage.h>
#include <drogon/orm/DbClient.h>
#include <memory>

namespace oauth2
{

/**
 * @brief Direct PostgreSQL IOAuth2Storage implementation.
 *
 * Defect 1.8 lifetime safety: inherits std::enable_shared_from_this so async
 * continuations (revokeAccessToken) can capture `auto self = shared_from_this();`
 * and keep this object alive until the in-flight DB callback completes. Valid in
 * both roles under Option B: created via make_shared from the concrete type both
 * as OAuth2Plugin::storage_ (postgres no-cache fallback) and as
 * CachedOAuth2Storage::impl_ (the shared_ptr inner storage).
 */
class PostgresOAuth2Storage : public IOAuth2Storage,
                              public std::enable_shared_from_this<PostgresOAuth2Storage>
{
  public:
    PostgresOAuth2Storage() = default;

    /**
     * @brief Initialize from config
     */
    void initFromConfig(const Json::Value &config);

    // Client Operations
    void getClient(const std::string &clientId, ClientCallback &&cb) override;
    void validateClient(
      const std::string &clientId,
      const std::string &clientSecret,
      BoolCallback &&cb
    ) override;

    // Authorization Code Operations
    void saveAuthCode(const OAuth2AuthCode &code, VoidCallback &&cb) override;
    void getAuthCode(const std::string &code, AuthCodeCallback &&cb) override;
    void markAuthCodeUsed(const std::string &code, VoidCallback &&cb) override;
    void consumeAuthCode(
      const std::string &code,
      const std::string &redirectUri,
      AuthCodeCallback &&cb
    ) override;

    // Access Token Operations
    void saveAccessToken(const OAuth2AccessToken &token, VoidCallback &&cb) override;
    void getAccessToken(const std::string &token, AccessTokenCallback &&cb) override;

    // Transactional token pair save
    void saveTokenPair(
      const OAuth2AccessToken &at,
      const OAuth2RefreshToken &rt,
      VoidCallback &&cb
    ) override;

    // Refresh Token Operations
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

    // Subject Mapping Operations
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

    void createUserForExternalLogin(
      const std::string &externalId,
      const std::string &provider,
      OptionalIntCallback &&cb
    ) override;

    // Authorization Transaction Operations
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

    // Scope Management Operations
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
    drogon::orm::DbClientPtr dbClientMaster_;
    drogon::orm::DbClientPtr dbClientReader_;
    std::string dbClientName_ = "default";
    std::string dbClientReaderName_ = "default";
};

}  // namespace oauth2
