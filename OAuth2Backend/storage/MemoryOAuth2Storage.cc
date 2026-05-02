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
    return std::chrono::duration_cast<std::chrono::seconds>(
               now.time_since_epoch())
        .count();
}

void MemoryOAuth2Storage::initFromConfig(const Json::Value &clientsConfig)
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
        std::string clientTypeStr =
            clientData.get("type", "CONFIDENTIAL").asString();
        try
        {
            client.clientType = stringToClientType(clientTypeStr);
        }
        catch (const std::exception &e)
        {
            LOG_WARN << "MemoryOAuth2Storage: Invalid client type '"
                     << clientTypeStr << "' for " << clientId
                     << ", defaulting to CONFIDENTIAL";
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
            client.redirectUris.push_back(
                clientData["redirect_uri"].asString());
        }

        clients_[clientId] = client;
    }
}

void MemoryOAuth2Storage::getClient(const std::string &clientId,
                                    ClientCallback &&cb)
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

void MemoryOAuth2Storage::validateClient(const std::string &clientId,
                                         const std::string &clientSecret,
                                         BoolCallback &&cb)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = clients_.find(clientId);
    if (it == clients_.end())
    {
        LOG_DEBUG << "MemoryOAuth2Storage validateClient: Client not found - "
                  << clientId;
        cb(false);
        return;
    }

    const auto &client = it->second;

    // PUBLIC clients skip secret validation
    if (client.clientType == ClientType::PUBLIC)
    {
        LOG_DEBUG << "MemoryOAuth2Storage validateClient: PUBLIC client "
                  << clientId << " accepted without secret";
        cb(true);
        return;
    }

    // CONFIDENTIAL clients MUST validate secret
    if (clientSecret.empty())
    {
        LOG_WARN << "MemoryOAuth2Storage validateClient: CONFIDENTIAL client "
                 << clientId << " missing secret";
        cb(false);
        return;
    }

    // Constant-time comparison to prevent timing attacks
    const std::string &storedHash = client.clientSecretHash;
    size_t cmpLen = (clientSecret.length() < storedHash.length())
                        ? clientSecret.length()
                        : storedHash.length();
    bool valid =
        (constantTimeMemcmp(clientSecret.c_str(), storedHash.c_str(), cmpLen) ==
         0) &&
        clientSecret.length() == storedHash.length();

    LOG_DEBUG << "MemoryOAuth2Storage validateClient: Secret validation "
              << (valid ? "PASSED" : "FAILED");
    cb(valid);
}

void MemoryOAuth2Storage::saveAuthCode(const OAuth2AuthCode &code,
                                       VoidCallback &&cb)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    authCodes_[code.code] = code;
    if (cb)
        cb();
}

void MemoryOAuth2Storage::getAuthCode(const std::string &code,
                                      AuthCodeCallback &&cb)
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

void MemoryOAuth2Storage::markAuthCodeUsed(const std::string &code,
                                           VoidCallback &&cb)
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

void MemoryOAuth2Storage::consumeAuthCode(const std::string &code,
                                          const std::string &redirectUri,
                                          AuthCodeCallback &&cb)
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
                LOG_WARN
                    << "[SECURITY] redirect_uri mismatch in token exchange. "
                    << "Expected: " << it->second.redirectUri
                    << ", Got: " << redirectUri << ", Code: " << code;
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

void MemoryOAuth2Storage::saveAccessToken(const OAuth2AccessToken &token,
                                          VoidCallback &&cb)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    accessTokens_[token.token] = token;
    if (cb)
        cb();
}

void MemoryOAuth2Storage::getAccessToken(const std::string &token,
                                         AccessTokenCallback &&cb)
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

void MemoryOAuth2Storage::saveRefreshToken(const OAuth2RefreshToken &token,
                                           VoidCallback &&cb)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    refreshTokens_[token.token] = token;
    if (cb)
        cb();
}

void MemoryOAuth2Storage::getRefreshToken(const std::string &token,
                                          RefreshTokenCallback &&cb)
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

void MemoryOAuth2Storage::revokeRefreshToken(const std::string &token,
                                             VoidCallback &&cb)
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

}  // namespace oauth2
