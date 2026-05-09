#include "MemoryOAuth2Storage.h"
#include <chrono>
#include <drogon/drogon.h>
#include "../common/types/OAuth2Types.h"

namespace
{
/**
 * @brief Constant-time memory comparison to prevent timing attacks
 * Returns 0 if buffers are equal, non-zero otherwise
 */
inline int constantTimeMemcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *p1 = static_cast<const unsigned char *>(s1);
    const unsigned char *p2 = static_cast<const unsigned char *>(s2);
    int result = 0;
    size_t i;

    for (i = 0; i < n; ++i)
    {
        result |= p1[i] ^ p2[i];
    }

    return result;
}
}  // namespace

namespace oauth2
{

int64_t MemoryOAuth2Storage::getCurrentTimestamp() const
{
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

void MemoryOAuth2Storage::initFromConfig(
  const Json::Value &clientsConfig,
  const Json::Value &adminConfig
)
{
    if (clientsConfig.isNull() || !clientsConfig.isObject())
    {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(mutex_);
    for (const auto &clientId : clientsConfig.getMemberNames())
    {
        const auto &clientData = clientsConfig[clientId];
        OAuth2Client client;
        client.clientId = clientId;

        // Parse client type (default to CONFIDENTIAL for backward
        // compatibility)
        std::string clientTypeStr = clientData.get("type", "CONFIDENTIAL").asString();
        try
        {
            client.clientType = stringToClientType(clientTypeStr);
        }
        catch (const std::exception &e)
        {
            LOG_WARN << "MemoryOAuth2Storage: Invalid client type '" << clientTypeStr << "' for "
                     << clientId << ", defaulting to CONFIDENTIAL";
            client.clientType = ClientType::CONFIDENTIAL;
        }

        // In memory mode, we store plain text or whatever provided as "secret"
        // Ideally we should hash it here too if we want parity, but for memory
        // it's fine.
        client.clientSecretHash = clientData.get("secret", "").asString();

        // Handle redirect_uri (single or array)
        if (clientData["redirect_uri"].isArray())
        {
            for (const auto &uri : clientData["redirect_uri"])
            {
                client.redirectUris.push_back(uri.asString());
            }
        }
        else if (clientData["redirect_uri"].isString())
        {
            client.redirectUris.push_back(clientData["redirect_uri"].asString());
        }

        clients_[clientId] = client;
    }

    // Initialize admin roles from configuration
    if (!adminConfig.isNull() && adminConfig.isObject())
    {
        for (const auto &userId : adminConfig.getMemberNames())
        {
            const auto &rolesData = adminConfig[userId];
            if (rolesData.isArray())
            {
                std::vector<std::string> roles;
                for (const auto &role : rolesData)
                {
                    roles.push_back(role.asString());
                }
                userRoles_[userId] = roles;
                LOG_DEBUG << "MemoryOAuth2Storage: User " << userId
                          << " assigned roles: " << (roles.empty() ? 0 : roles.size());
            }
            else if (rolesData.isString())
            {
                // Single role as string
                userRoles_[userId] = {rolesData.asString()};
                LOG_DEBUG << "MemoryOAuth2Storage: User " << userId
                          << " assigned role: " << rolesData.asString();
            }
        }
    }
    else
    {
        // Default admin configuration for backward compatibility
        LOG_WARN << "MemoryOAuth2Storage: No admin configuration provided, "
                 << "using default admin user 'admin' and '1'";
        userRoles_["admin"] = {"admin", "user"};
        userRoles_["1"] = {"admin", "user"};
    }
}

void MemoryOAuth2Storage::getClient(const std::string &clientId, ClientCallback &&cb)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = clients_.find(clientId);
    if (it != clients_.end())
    {
        cb(it->second);
    }
    else
    {
        cb(std::nullopt);
    }
}

void MemoryOAuth2Storage::validateClient(
  const std::string &clientId,
  const std::string &clientSecret,
  BoolCallback &&cb
)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = clients_.find(clientId);
    if (it == clients_.end())
    {
        LOG_DEBUG << "MemoryOAuth2Storage validateClient: Client not found - " << clientId;
        cb(false);
        return;
    }

    const auto &client = it->second;

    // PUBLIC clients skip secret validation
    if (client.clientType == ClientType::PUBLIC)
    {
        LOG_DEBUG << "MemoryOAuth2Storage validateClient: PUBLIC client " << clientId
                  << " accepted without secret";
        cb(true);
        return;
    }

    // CONFIDENTIAL clients MUST validate secret
    if (clientSecret.empty())
    {
        LOG_WARN << "MemoryOAuth2Storage validateClient: CONFIDENTIAL client " << clientId
                 << " missing secret";
        cb(false);
        return;
    }

    // Constant-time comparison to prevent timing attacks
    const std::string &storedHash = client.clientSecretHash;
    size_t cmpLen =
      (clientSecret.length() < storedHash.length()) ? clientSecret.length() : storedHash.length();
    bool valid = (constantTimeMemcmp(clientSecret.c_str(), storedHash.c_str(), cmpLen) == 0) &&
                 clientSecret.length() == storedHash.length();

    LOG_DEBUG << "MemoryOAuth2Storage validateClient: Secret validation "
              << (valid ? "PASSED" : "FAILED");
    cb(valid);
}

void MemoryOAuth2Storage::saveAuthCode(const OAuth2AuthCode &code, VoidCallback &&cb)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    authCodes_[code.code] = code;
    if (cb)
        cb();
}

void MemoryOAuth2Storage::getAuthCode(const std::string &code, AuthCodeCallback &&cb)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    // Clean up expired codes lazily or just check expiry
    auto it = authCodes_.find(code);
    if (it != authCodes_.end())
    {
        if (it->second.expiresAt > getCurrentTimestamp())
        {
            cb(it->second);
            return;
        }
        else
        {
            authCodes_.erase(it);
        }
    }
    cb(std::nullopt);
}

void MemoryOAuth2Storage::markAuthCodeUsed(const std::string &code, VoidCallback &&cb)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = authCodes_.find(code);
    if (it != authCodes_.end())
    {
        it->second.used = true;
    }
    if (cb)
        cb();
}

void MemoryOAuth2Storage::consumeAuthCode(
  const std::string &code,
  const std::string &redirectUri,
  AuthCodeCallback &&cb
)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = authCodes_.find(code);
    if (it != authCodes_.end())
    {
        if (!it->second.used)
        {
            // CRITICAL: Validate redirect_uri matches authorization
            // Per OAuth2 RFC 6749 Section 4.1.3
            if (!redirectUri.empty() && redirectUri != it->second.redirectUri)
            {
                LOG_WARN << "[SECURITY] redirect_uri mismatch in token exchange. "
                         << "Expected: " << it->second.redirectUri << ", Got: " << redirectUri
                         << ", Code: " << code;
                cb(std::nullopt);
                return;
            }

            it->second.used = true;
            cb(it->second);
            return;
        }
    }
    cb(std::nullopt);
}

void MemoryOAuth2Storage::saveAccessToken(const OAuth2AccessToken &token, VoidCallback &&cb)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    // Ensure P1 fields have default values if not set
    OAuth2AccessToken tokenWithDefaults = token;
    if (tokenWithDefaults.issuedAt == 0)
        tokenWithDefaults.issuedAt = getCurrentTimestamp();
    if (tokenWithDefaults.issuer.empty())
        tokenWithDefaults.issuer = "https://oauth.example.com";
    if (tokenWithDefaults.notBefore == 0)
        tokenWithDefaults.notBefore = getCurrentTimestamp();

    accessTokens_[tokenWithDefaults.token] = tokenWithDefaults;
    if (cb)
        cb();
}

void MemoryOAuth2Storage::getAccessToken(const std::string &token, AccessTokenCallback &&cb)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = accessTokens_.find(token);
    if (it != accessTokens_.end())
    {
        if (it->second.expiresAt > getCurrentTimestamp() && !it->second.revoked)
        {
            cb(it->second);
            return;
        }
    }
    cb(std::nullopt);
}

void MemoryOAuth2Storage::saveRefreshToken(const OAuth2RefreshToken &token, VoidCallback &&cb)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    refreshTokens_[token.token] = token;
    if (cb)
        cb();
}

void MemoryOAuth2Storage::getRefreshToken(const std::string &token, RefreshTokenCallback &&cb)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = refreshTokens_.find(token);
    if (it != refreshTokens_.end())
    {
        if (it->second.expiresAt > getCurrentTimestamp() && !it->second.revoked)
        {
            cb(it->second);
            return;
        }
    }
    cb(std::nullopt);
}

void MemoryOAuth2Storage::revokeRefreshToken(const std::string &token, VoidCallback &&cb)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = refreshTokens_.find(token);
    if (it != refreshTokens_.end())
    {
        it->second.revoked = true;
        LOG_DEBUG << "Refresh token revoked: " << token;
    }
    cb();
}

// Manual cleanup for Memory Storage
void MemoryOAuth2Storage::deleteExpiredData()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    int64_t now = getCurrentTimestamp();
    size_t count = 0;

    // 1. Auth Codes
    for (auto it = authCodes_.begin(); it != authCodes_.end();)
    {
        if (it->second.expiresAt < now)
        {
            it = authCodes_.erase(it);
            count++;
        }
        else
        {
            ++it;
        }
    }

    // 2. Access Tokens
    for (auto it = accessTokens_.begin(); it != accessTokens_.end();)
    {
        if (it->second.expiresAt < now)
        {
            it = accessTokens_.erase(it);
            count++;
        }
        else
        {
            ++it;
        }
    }

    // 3. Refresh Tokens
    for (auto it = refreshTokens_.begin(); it != refreshTokens_.end();)
    {
        if (it->second.expiresAt < now)
        {
            it = refreshTokens_.erase(it);
            count++;
        }
        else
        {
            ++it;
        }
    }
}

void MemoryOAuth2Storage::getUserRoles(const std::string &userId, StringListCallback &&cb)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = userRoles_.find(userId);
    if (it != userRoles_.end())
    {
        cb(it->second);
    }
    else
    {
        // Default to regular user role if no specific configuration
        cb({"user"});
    }
}

// ========== Subject Mapping Operations ==========

void MemoryOAuth2Storage::getInternalUserId(
  const std::string &subject,
  const std::string &provider,
  OptionalIntCallback &&cb
)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::string key = provider + ":" + subject;
    auto it = subjectMappings_.find(key);
    if (it != subjectMappings_.end())
    {
        cb(it->second);
    }
    else
    {
        cb(std::nullopt);
    }
}

void MemoryOAuth2Storage::createSubjectMapping(
  const std::string &subject,
  int32_t internalUserId,
  const std::string &provider,
  BoolCallback &&cb
)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::string key = provider + ":" + subject;
    subjectMappings_[key] = internalUserId;
    LOG_DEBUG << "Created subject mapping: " << key << " -> " << internalUserId;
    cb(true);
}

// ========== Authorization Transaction Operations ==========

void MemoryOAuth2Storage::saveAuthorizationTransaction(
  const AuthorizationTransaction &transaction,
  BoolCallback &&cb
)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    transactions_[transaction.transactionId] = transaction;
    LOG_DEBUG << "Saved authorization transaction: " << transaction.transactionId;
    cb(true);
}

void MemoryOAuth2Storage::getAuthorizationTransaction(
  const std::string &transactionId,
  TransactionCallback &&cb
)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = transactions_.find(transactionId);
    if (it != transactions_.end())
    {
        // Check if expired
        int64_t now = getCurrentTimestamp();
        if (it->second.expiresAt < now)
        {
            LOG_DEBUG << "Transaction expired: " << transactionId;
            transactions_.erase(it);
            cb(std::nullopt);
            return;
        }
        cb(it->second);
    }
    else
    {
        cb(std::nullopt);
    }
}

void MemoryOAuth2Storage::deleteAuthorizationTransaction(
  const std::string &transactionId,
  VoidCallback &&cb
)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    transactions_.erase(transactionId);
    LOG_DEBUG << "Deleted authorization transaction: " << transactionId;
    cb();
}

void MemoryOAuth2Storage::markTransactionConsumed(
  const std::string &transactionId,
  BoolCallback &&cb
)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = transactions_.find(transactionId);
    if (it != transactions_.end() && !it->second.consumed)
    {
        it->second.consumed = true;
        LOG_DEBUG << "Marked transaction as consumed: " << transactionId;
        cb(true);
    }
    else
    {
        LOG_DEBUG << "Transaction already consumed or not found: " << transactionId;
        cb(false);
    }
}

// ========== Scope Management Operations ==========

void MemoryOAuth2Storage::hasUserConsent(
  int32_t internalUserId,
  const std::string &clientId,
  const std::string &scope,
  BoolCallback &&cb
)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::string key = std::to_string(internalUserId) + ":" + clientId + ":" + scope;
    auto it = userConsents_.find(key);
    cb(it != userConsents_.end());
}

void MemoryOAuth2Storage::saveUserConsent(
  int32_t internalUserId,
  const std::string &clientId,
  const std::string &scope,
  BoolCallback &&cb
)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::string key = std::to_string(internalUserId) + ":" + clientId + ":" + scope;
    userConsents_[key] = getCurrentTimestamp();
    LOG_DEBUG << "Saved user consent: " << key;
    cb(true);
}

void MemoryOAuth2Storage::revokeUserConsent(
  int32_t internalUserId,
  const std::string &clientId,
  const std::string &scope,
  VoidCallback &&cb
)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::string key = std::to_string(internalUserId) + ":" + clientId + ":" + scope;
    size_t erased = userConsents_.erase(key);
    LOG_DEBUG << "Revoked user consent: " << key << " (erased: " << erased << ")";
    cb();
}

// ========== Additional getUserRoles overload ==========

void MemoryOAuth2Storage::getUserRoles(int32_t internalUserId, StringListCallback &&cb)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::string userIdStr = std::to_string(internalUserId);
    auto it = userRoles_.find(userIdStr);
    if (it != userRoles_.end())
    {
        cb(it->second);
    }
    else
    {
        // Default to regular user role if no specific configuration
        cb({"user"});
    }
}

// ========== P1: Token Introspection (RFC 7662) ==========

void MemoryOAuth2Storage::introspectToken(
  const std::string &token,
  IOAuth2Storage::TokenIntrospectionCallback &&cb
)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = accessTokens_.find(token);
    if (it != accessTokens_.end())
    {
        const OAuth2AccessToken &accessToken = it->second;
        int64_t now = getCurrentTimestamp();

        // Check if token is revoked or expired
        if (accessToken.revoked || accessToken.expiresAt < now)
        {
            TokenIntrospection introspection;
            introspection.active = false;
            cb(introspection);
            return;
        }

        // Token is active, populate introspection data
        TokenIntrospection introspection;
        introspection.active = true;
        introspection.clientId = accessToken.clientId;
        introspection.tokenType = "Bearer";
        introspection.exp = accessToken.expiresAt;
        introspection.iat = accessToken.issuedAt;
        introspection.iss = accessToken.issuer;
        introspection.aud = accessToken.audience;
        introspection.nbf = accessToken.notBefore;
        introspection.sub = accessToken.userId;
        introspection.scope = accessToken.scope;

        cb(introspection);
        return;
    }

    // Token not found
    TokenIntrospection introspection;
    introspection.active = false;
    cb(introspection);
}

void MemoryOAuth2Storage::incrementIntrospectCount(
  const std::string &token,
  IOAuth2Storage::VoidCallback &&cb
)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = accessTokens_.find(token);
    if (it != accessTokens_.end())
    {
        it->second.introspectCount++;
    }
    if (cb)
        cb();
}

// ========== P1: Token Revocation (RFC 7009) ==========

void MemoryOAuth2Storage::revokeAccessToken(
  const std::string &token,
  const std::string &revokedBy,
  IOAuth2Storage::VoidCallback &&cb
)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = accessTokens_.find(token);
    if (it != accessTokens_.end())
    {
        it->second.revoked = true;
        it->second.revokedAt = getCurrentTimestamp();
        it->second.revokedBy = revokedBy;
        LOG_INFO << "Token revoked successfully in memory storage";
    }
    // Always return success per RFC 7009 (prevent token probing)
    if (cb)
        cb();
}

}  // namespace oauth2
