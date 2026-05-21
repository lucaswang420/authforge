#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <functional>
#include <json/json.h>
#include "OAuth2Types.h"

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
    std::string nonce;                // OIDC nonce (anti-replay)
    int64_t expiresAt;                // Unix timestamp (seconds)
    bool used = false;
};

/**
 * @brief Access Token data structure (extended for P1 features)
 */
struct OAuth2AccessToken
{
    std::string token;
    std::string clientId;
    std::string userId;
    std::string scope;
    int64_t expiresAt;  // Unix timestamp (seconds)
    bool revoked = false;

    // P1: RFC 7662 Token Introspection fields
    int64_t issuedAt = 0;     // Unix timestamp when token was issued (iat)
    std::string issuer;       // Issuer identifier (iss)
    std::string audience;     // Audience identifier (aud)
    int64_t notBefore = 0;    // Token not valid before (nbf)
    int introspectCount = 0;  // Number of introspection requests
    int64_t revokedAt = 0;    // Unix timestamp when token was revoked
    std::string revokedBy;    // Client ID that revoked the token
};

/**
 * @brief Refresh Token data structure (extended for P1 features)
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
    std::string familyId;  // Token family for reuse detection

    // P1: Token Revocation audit fields (RFC 7009)
    int64_t revokedAt = 0;  // Unix timestamp when token was revoked
    std::string revokedBy;  // Client ID that revoked the token
};

/**
 * @brief Token Introspection response (RFC 7662)
 *
 * This structure represents the response format for token introspection
 * as specified in RFC 7662 (OAuth 2.0 Token Introspection).
 */
struct TokenIntrospection
{
    bool active = false;               // Whether the token is currently active
    std::string clientId;              // Client ID that was issued the token
    std::string tokenType = "Bearer";  // Token type (always "Bearer" for OAuth 2.0)
    int64_t exp = 0;                   // Expiration time (exp)
    int64_t iat = 0;                   // Issued at time (iat)
    int64_t nbf = 0;                   // Not before time (nbf)
    std::string sub;                   // Subject (user ID)
    std::string aud;                   // Audience (client ID)
    std::string iss;                   // Issuer (authorization server URL)
    std::string scope;                 // Granted scopes

    /**
     * @brief Convert to JSON for HTTP response
     * @return JSON value object
     */
    Json::Value toJson() const
    {
        Json::Value json;
        json["active"] = active;

        if (active)
        {
            json["client_id"] = clientId;
            json["token_type"] = tokenType;

            if (exp > 0)
                json["exp"] = static_cast<Json::Int64>(exp);
            if (iat > 0)
                json["iat"] = static_cast<Json::Int64>(iat);
            if (nbf > 0)
                json["nbf"] = static_cast<Json::Int64>(nbf);

            if (!sub.empty())
                json["sub"] = sub;
            if (!aud.empty())
                json["aud"] = aud;
            if (!iss.empty())
                json["iss"] = iss;
            if (!scope.empty())
                json["scope"] = scope;
        }

        return json;
    }
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

    // ========== Token Pair Operations (Transactional) ==========

    /**
     * @brief Save access token + refresh token as an atomic pair
     * Default implementation calls saveAccessToken then saveRefreshToken sequentially.
     * PostgreSQL override uses a database transaction for atomicity.
     *
     * @param at Access token to save
     * @param rt Refresh token to save
     * @param cb Callback invoked when both are saved (or on failure)
     */
    virtual void saveTokenPair(
      const OAuth2AccessToken &at,
      const OAuth2RefreshToken &rt,
      VoidCallback &&cb
    )
    {
        // Default: sequential (non-transactional) for Memory/Redis
        saveAccessToken(at, [this, rt, cb = std::move(cb)]() mutable {
            saveRefreshToken(rt, std::move(cb));
        });
    }

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

    /**
     * @brief Atomically revoke a refresh token (CAS operation)
     * Only revokes if the token is currently NOT revoked.
     * Returns the token data if successfully revoked, nullopt if already revoked.
     * Used for refresh token rotation to detect reuse.
     *
     * @param token The hashed refresh token
     * @param cb Callback with token data if CAS succeeded, nullopt if already revoked
     */
    virtual void atomicRevokeRefreshToken(const std::string &token, RefreshTokenCallback &&cb) = 0;

    /**
     * @brief Revoke all tokens in a refresh token family (cascade revocation)
     * Called when refresh token reuse is detected.
     * Revokes all refresh tokens AND their associated access tokens.
     *
     * @param familyId The family ID to cascade-revoke
     * @param cb Callback invoked when revocation completes
     */
    virtual void revokeTokenFamily(const std::string &familyId, VoidCallback &&cb) = 0;

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

    // ========== User Info Operations ==========
    /**
     * @brief Get user information from database
     * @param userId The ID of the user (as string)
     * @param cb Callback with user info JSON or nullopt if not found
     */
    using OptionalJsonCallback = std::function<void(std::optional<Json::Value>)>;
    virtual void getUserInfo(const std::string &userId, OptionalJsonCallback &&cb) = 0;
    virtual void getUserInfo(int32_t internalUserId, OptionalJsonCallback &&cb) = 0;

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

    /**
     * @brief Create a user record for external (third-party) login
     * Inserts into users table with a placeholder password and returns the new user ID.
     * Used by handleFirstTimeLogin for Google/WeChat/etc. providers.
     *
     * @param externalId External user identifier (e.g., Google sub)
     * @param provider Provider name (google, wechat, etc.)
     * @param cb Callback with new internal user ID, or nullopt on failure
     */
    virtual void createUserForExternalLogin(
      const std::string &externalId,
      const std::string &provider,
      OptionalIntCallback &&cb
    )
    {
        // Default implementation: not supported (for Memory/Redis backends)
        cb(std::nullopt);
    }

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

    // ========== P1: Token Introspection (RFC 7662) ==========

    /**
     * @brief Introspect token metadata for RFC 7662 compliance
     * @param token The access token to introspect
     * @param cb Callback with introspection metadata or nullopt if invalid
     */
    using TokenIntrospectionCallback = std::function<void(std::optional<TokenIntrospection>)>;
    virtual void introspectToken(const std::string &token, TokenIntrospectionCallback &&cb) = 0;

    /**
     * @brief Increment introspection count for monitoring
     * @param token The access token
     * @param cb Callback invoked when update completes
     */
    virtual void incrementIntrospectCount(const std::string &token, VoidCallback &&cb) = 0;

    // ========== P1: Token Revocation (RFC 7009) ==========

    /**
     * @brief Revoke an access token with audit trail
     * @param token The access token to revoke
     * @param revokedBy Client ID performing the revocation
     * @param cb Callback invoked when revocation completes
     */
    virtual void revokeAccessToken(
      const std::string &token,
      const std::string &revokedBy,
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
