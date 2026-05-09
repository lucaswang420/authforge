#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <functional>
#include "../common/types/OAuth2Types.h"

namespace oauth2
{

/**
 * @brief OAuth2 Client data structure
 */
struct OAuth2Client
{
    std::string clientId;
    ClientType clientType;
    std::string clientSecretHash;
    std::string salt;
    std::vector<std::string> redirectUris;
    std::vector<std::string> allowedScopes;
};

/**
 * @brief Authorization Code data structure
 */
struct OAuth2AuthCode
{
    std::string code;
    std::string clientId;
    std::string userId;
    std::string scope;
    std::string redirectUri;
    std::string codeChallenge;        // PKCE support
    std::string codeChallengeMethod;  // "plain" or "S256"
    int64_t expiresAt;                // Unix timestamp (seconds)
    bool used = false;
};

/**
 * @brief Access Token data structure
 */
struct OAuth2AccessToken
{
    std::string token;
    std::string clientId;
    std::string userId;
    std::string scope;
    int64_t expiresAt;  // Unix timestamp (seconds)
    bool revoked = false;
};

/**
 * @brief Refresh Token data structure
 */
struct OAuth2RefreshToken
{
    std::string token;
    std::string accessToken;
    std::string clientId;
    std::string userId;
    std::string scope;
    int64_t expiresAt;
    bool revoked = false;
};

/**
 * @brief Abstract storage interface for OAuth2 data
 *
 * Implementations use ASYNCHRONOUS CALLBACKS.
 * Callbacks are invoked when the operation completes.
 */
class IOAuth2Storage
{
  public:
    virtual ~IOAuth2Storage() = default;

    // Callback types
    using ClientCallback = std::function<void(std::optional<OAuth2Client>)>;
    using AuthCodeCallback = std::function<void(std::optional<OAuth2AuthCode>)>;
    using AccessTokenCallback = std::function<void(std::optional<OAuth2AccessToken>)>;
    using RefreshTokenCallback = std::function<void(std::optional<OAuth2RefreshToken>)>;
    using VoidCallback = std::function<void()>;
    using BoolCallback = std::function<void(bool)>;

    // ========== Client Operations ==========

    /**
     * @brief Get client by ID
     */
    virtual void getClient(const std::string &clientId, ClientCallback &&cb) = 0;

    /**
     * @brief Validate client credentials
     */
    virtual void validateClient(
      const std::string &clientId,
      const std::string &clientSecret,
      BoolCallback &&cb
    ) = 0;

    // ========== Authorization Code Operations ==========

    /**
     * @brief Save a new authorization code
     */
    virtual void saveAuthCode(const OAuth2AuthCode &code, VoidCallback &&cb) = 0;

    /**
     * @brief Get authorization code by code value
     */
    virtual void getAuthCode(const std::string &code, AuthCodeCallback &&cb) = 0;

    /**
     * @brief Mark an authorization code as used (single-use enforcement)
     */
    virtual void markAuthCodeUsed(const std::string &code, VoidCallback &&cb) = 0;

    /**
     * @brief Atomic Consume: Get Code, Check if Used, Mark Used, Return Code.
     * If code not found OR already used, callback with std::nullopt.
     *
     * CRITICAL: Per OAuth2 RFC 6749 Section 4.1.3, MUST validate redirect_uri
     * matches the value used in authorization request. Returns nullopt if
     * mismatch.
     *
     * @param code Authorization code
     * @param redirectUri Redirect URI from token request (must match
     * authorization)
     * @param cb Callback with auth code data or nullopt if
     * invalid/used/mismatch
     */
    virtual void consumeAuthCode(
      const std::string &code,
      const std::string &redirectUri,
      AuthCodeCallback &&cb
    ) = 0;

    // ========== Access Token Operations ==========

    /**
     * @brief Save a new access token
     */
    virtual void saveAccessToken(const OAuth2AccessToken &token, VoidCallback &&cb) = 0;

    /**
     * @brief Get access token by token value
     */
    virtual void getAccessToken(const std::string &token, AccessTokenCallback &&cb) = 0;

    // ========== Refresh Token Operations ==========

    /**
     * @brief Save a new refresh token
     */
    virtual void saveRefreshToken(const OAuth2RefreshToken &token, VoidCallback &&cb) = 0;

    /**
     * @brief Get refresh token by token value
     */
    virtual void getRefreshToken(const std::string &token, RefreshTokenCallback &&cb) = 0;

    /**
     * @brief Revoke a refresh token
     * @param token The refresh token to revoke
     * @param cb Callback invoked when revocation completes
     */
    virtual void revokeRefreshToken(const std::string &token, VoidCallback &&cb) = 0;

    using StringListCallback = std::function<void(std::vector<std::string>)>;

    // ========== User/Role Operations ==========

    /**
     * @brief Get roles assigned to a user
     * @param userId The ID of the user (as string)
     * @param cb Callback with list of role names
     */
    virtual void getUserRoles(const std::string &userId, StringListCallback &&cb) = 0;

    /**
     * @brief Get roles assigned to a user by internal user ID
     * @param internalUserId The internal user ID (integer)
     * @param cb Callback with list of role names
     */
    virtual void getUserRoles(int32_t internalUserId, StringListCallback &&cb) = 0;

    // ========== Subject Mapping Operations ==========

    /**
     * @brief Get internal user ID by OAuth2 subject and provider
     * @param subject OAuth2/OpenID Connect subject (within provider scope)
     * @param provider Provider name ('local', 'google', 'wechat', etc.)
     * @param cb Callback with internal user ID or std::nullopt if not found
     */
    using OptionalIntCallback = std::function<void(std::optional<int32_t>)>;
    virtual void getInternalUserId(
      const std::string &subject,
      const std::string &provider,
      OptionalIntCallback &&cb
    ) = 0;

    /**
     * @brief Create a new subject mapping
     * @param subject OAuth2/OpenID Connect subject
     * @param internalUserId Internal user ID from users table
     * @param provider Provider name
     * @param cb Callback invoked with true on success, false on failure
     */
    virtual void createSubjectMapping(
      const std::string &subject,
      int32_t internalUserId,
      const std::string &provider,
      BoolCallback &&cb
    ) = 0;

    // ========== Authorization Transaction Operations ==========

    /**
     * @brief Authorization Transaction for consent flow
     * Preserves complete OAuth2 authorization context across user consent
     * interaction
     */
    struct AuthorizationTransaction
    {
        std::string transactionId;
        std::string clientId;
        std::string subject;
        std::string redirectUri;
        std::string state;
        std::string codeChallenge;
        std::string codeChallengeMethod;
        std::vector<std::string> requestedScopes;
        std::vector<std::string> validScopes;
        std::vector<std::string> consentRequiredScopes;
        bool consumed = false;
        int64_t expiresAt;
    };

    using TransactionCallback = std::function<void(std::optional<AuthorizationTransaction>)>;

    /**
     * @brief Save authorization transaction to storage
     * @param transaction Authorization transaction data
     * @param cb Callback invoked with true on success, false on failure
     */
    virtual void saveAuthorizationTransaction(
      const AuthorizationTransaction &transaction,
      BoolCallback &&cb
    ) = 0;

    /**
     * @brief Get authorization transaction by ID
     * @param transactionId Transaction ID
     * @param cb Callback with transaction data or std::nullopt if not
     * found/expired
     */
    virtual void getAuthorizationTransaction(
      const std::string &transactionId,
      TransactionCallback &&cb
    ) = 0;

    /**
     * @brief Delete authorization transaction
     * @param transactionId Transaction ID
     * @param cb Callback invoked when deletion completes
     */
    virtual void deleteAuthorizationTransaction(
      const std::string &transactionId,
      VoidCallback &&cb
    ) = 0;

    /**
     * @brief Mark authorization transaction as consumed (prevent duplicate
     * submissions)
     * @param transactionId Transaction ID
     * @param cb Callback invoked with true if successfully marked, false if
     * already consumed
     */
    virtual void markTransactionConsumed(const std::string &transactionId, BoolCallback &&cb) = 0;

    // ========== Scope Management Operations ==========

    /**
     * @brief Check if user has granted consent for a specific scope
     * @param internalUserId Internal user ID
     * @param clientId Client ID
     * @param scope Scope name
     * @param cb Callback with true if consent exists, false otherwise
     */
    virtual void hasUserConsent(
      int32_t internalUserId,
      const std::string &clientId,
      const std::string &scope,
      BoolCallback &&cb
    ) = 0;

    /**
     * @brief Save user consent for a specific scope
     * @param internalUserId Internal user ID
     * @param clientId Client ID
     * @param scope Scope name
     * @param cb Callback invoked with true on success, false on failure
     */
    virtual void saveUserConsent(
      int32_t internalUserId,
      const std::string &clientId,
      const std::string &scope,
      BoolCallback &&cb
    ) = 0;

    /**
     * @brief Revoke user consent for a specific scope
     * @param internalUserId Internal user ID
     * @param clientId Client ID
     * @param scope Scope name
     * @param cb Callback invoked when revocation completes
     */
    virtual void revokeUserConsent(
      int32_t internalUserId,
      const std::string &clientId,
      const std::string &scope,
      VoidCallback &&cb
    ) = 0;

    // ========== Cleanup Operations ==========

    /**
     * @brief Delete expired data (codes, tokens)
     * Implementations should remove all expired entries.
     */
    virtual void deleteExpiredData() = 0;
};

}  // namespace oauth2
