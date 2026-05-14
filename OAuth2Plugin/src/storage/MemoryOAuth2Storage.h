#pragma once

#include <oauth2/IOAuth2Storage.h>
#include <mutex>
#include <unordered_map>
#include <json/json.h>

namespace oauth2
{

/**
 * @brief In-memory implementation of OAuth2 storage
 *
 * Suitable for development and testing environments.
 * All data is lost on server restart.
 */
class MemoryOAuth2Storage : public IOAuth2Storage
{
  public:
    /**
     * @brief Initialize with client configuration from JSON
     * @param clientsConfig JSON object with client definitions
     * @param adminConfig JSON object with admin user definitions (optional)
     */
    void initFromConfig(
      const Json::Value &clientsConfig,
      const Json::Value &adminConfig = Json::Value::nullSingleton()
    );

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

    // Refresh Token Operations
    void saveRefreshToken(const OAuth2RefreshToken &token, VoidCallback &&cb) override;
    void getRefreshToken(const std::string &token, RefreshTokenCallback &&cb) override;
    void revokeRefreshToken(const std::string &token, VoidCallback &&cb) override;

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

  private:
    std::recursive_mutex mutex_;
    std::unordered_map<std::string, OAuth2Client> clients_;
    std::unordered_map<std::string, OAuth2AuthCode> authCodes_;
    std::unordered_map<std::string, OAuth2AccessToken> accessTokens_;
    std::unordered_map<std::string, OAuth2RefreshToken> refreshTokens_;
    std::unordered_map<std::string, std::vector<std::string>> userRoles_;

    // Subject mapping: "provider:subject" -> internal_user_id
    std::unordered_map<std::string, int32_t> subjectMappings_;

    // Authorization transactions
    std::unordered_map<std::string, AuthorizationTransaction> transactions_;

    // User consents: "user_id:client_id:scope" -> timestamp
    std::unordered_map<std::string, int64_t> userConsents_;

    int64_t getCurrentTimestamp() const;
};

}  // namespace oauth2
