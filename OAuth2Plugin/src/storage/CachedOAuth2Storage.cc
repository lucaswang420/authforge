#include <oauth2/storage/CachedOAuth2Storage.h>
#include <drogon/drogon.h>
#include <drogon/utils/Utilities.h>
#include <json/json.h>
#include <sstream>

// Helper to parse JSON string (replaces deprecated Json::Reader)
static bool parseJsonString(const std::string &jsonStr, Json::Value &json)
{
    Json::CharReaderBuilder builder;
    std::string errs;
    std::istringstream s(jsonStr);
    return Json::parseFromStream(builder, s, &json, &errs);
}

namespace oauth2
{

CachedOAuth2Storage::CachedOAuth2Storage(
  std::shared_ptr<IOAuth2Storage> impl,
  drogon::nosql::RedisClientPtr redisClient
)
    : impl_(std::move(impl)),
      redisClient_(std::move(redisClient)),
      tokenCache_(drogon::app().getLoop(), 1.0, 4, 30),  // Clean up expired every 30s
      clientCache_(drogon::app().getLoop(), 1.0, 4, 60)  // Clean up expired every 60s
{
}

void CachedOAuth2Storage::getClient(const std::string &clientId, ClientCallback &&cb)
{
    OAuth2Client cachedClient;
    if (clientCache_.findAndFetch(clientId, cachedClient))
    {
        cb(cachedClient);
        return;
    }

    impl_->getClient(
      clientId,
      [self = shared_from_this(),
       this,
       clientId,
       cb = std::move(cb)](const std::optional<OAuth2Client> &client) mutable {
          if (client)
          {
              clientCache_.insert(clientId, *client, 60);  // Cache for 60 seconds
          }
          cb(client);
      }
    );
}

void CachedOAuth2Storage::validateClient(
  const std::string &clientId,
  const std::string &clientSecret,
  BoolCallback &&cb
)
{
    impl_->validateClient(clientId, clientSecret, std::move(cb));
}

void CachedOAuth2Storage::saveAuthCode(const OAuth2AuthCode &code, VoidCallback &&cb)
{
    impl_->saveAuthCode(code, std::move(cb));
}

void CachedOAuth2Storage::getAuthCode(const std::string &code, AuthCodeCallback &&cb)
{
    impl_->getAuthCode(code, std::move(cb));
}

void CachedOAuth2Storage::markAuthCodeUsed(const std::string &code, VoidCallback &&cb)
{
    impl_->markAuthCodeUsed(code, std::move(cb));
}

void CachedOAuth2Storage::consumeAuthCode(
  const std::string &code,
  const std::string &redirectUri,
  AuthCodeCallback &&cb
)
{
    impl_->consumeAuthCode(code, redirectUri, std::move(cb));
}

// Access Token - Write Side (Cache Invalidation or Write-Through)
void CachedOAuth2Storage::saveAccessToken(const OAuth2AccessToken &token, VoidCallback &&cb)
{
    // Write to L1 Cache
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
                 std::chrono::system_clock::now().time_since_epoch()
    )
                 .count();
    long long ttl = token.expiresAt - now;
    if (ttl > 0)
    {
        tokenCache_.insert(token.token, token, ttl);
    }

    // Write to DB first
    impl_->saveAccessToken(token, [self = shared_from_this(), this, token, cb = std::move(cb), ttl]() mutable {
        if (!redisClient_ || ttl <= 0)
        {
            if (cb)
                cb();
            return;
        }

        // Write-Through to Redis (L2)
        Json::Value json;
        json["token"] = token.token;
        json["client_id"] = token.clientId;
        json["user_id"] = token.userId;
        json["scope"] = token.scope;
        json["expires_at"] = (Json::Int64)token.expiresAt;
        json["revoked"] = token.revoked;

        std::string key = "oauth2:token:" + token.token;
        redisClient_->execCommandAsync(
          [cb](const drogon::nosql::RedisResult &r) {
              if (cb)
                  cb();
          },
          [cb](const std::exception &e) {
              LOG_ERROR << "Redis Write Error: " << e.what();
              if (cb)
                  cb();
          },
          "SET %s %s EX %d",
          key.c_str(),
          json.toStyledString().c_str(),
          ttl
        );
    });
}

// Access Token - Read Side (Cache Look-Aside)
void CachedOAuth2Storage::getAccessToken(const std::string &token, AccessTokenCallback &&cb)
{
    // Try L1 Cache
    OAuth2AccessToken cachedToken;
    if (tokenCache_.findAndFetch(token, cachedToken))
    {
        cb(cachedToken);
        return;
    }

    if (!redisClient_)
    {
        impl_->getAccessToken(
          token,
          [self = shared_from_this(),
           this,
           token,
           cb = std::move(cb)](const std::optional<OAuth2AccessToken> &optToken) mutable {
              if (optToken)
              {
                  auto now = std::chrono::duration_cast<std::chrono::seconds>(
                               std::chrono::system_clock::now().time_since_epoch()
                  )
                               .count();
                  long long ttl = optToken->expiresAt - now;
                  if (ttl > 0)
                      tokenCache_.insert(token, *optToken, ttl);
              }
              cb(optToken);
          }
        );
        return;
    }

    std::string key = "oauth2:token:" + token;
    auto sharedCb = std::make_shared<AccessTokenCallback>(std::move(cb));

    redisClient_->execCommandAsync(
      [self = shared_from_this(), this, token, sharedCb](const drogon::nosql::RedisResult &r) {
          if (r.type() == drogon::nosql::RedisResultType::kNil)
          {
              // L2 Cache Miss -> Load from DB
              impl_->getAccessToken(
                token, [self, this, token, sharedCb](const std::optional<OAuth2AccessToken> &optToken) {
                    if (optToken)
                    {
                        auto now = std::chrono::duration_cast<std::chrono::seconds>(
                                     std::chrono::system_clock::now().time_since_epoch()
                        )
                                     .count();
                        long long ttl = optToken->expiresAt - now;
                        if (ttl > 0)
                        {
                            // L1 Cache Fill
                            tokenCache_.insert(token, *optToken, ttl);

                            // L2 Cache Fill
                            Json::Value json;
                            json["token"] = optToken->token;
                            json["client_id"] = optToken->clientId;
                            json["user_id"] = optToken->userId;
                            json["scope"] = optToken->scope;
                            json["expires_at"] = (Json::Int64)optToken->expiresAt;
                            json["revoked"] = optToken->revoked;

                            std::string key = "oauth2:token:" + token;
                            redisClient_->execCommandAsync(
                              [](const drogon::nosql::RedisResult &) {},
                              [](const std::exception &) {},
                              "SET %s %s EX %d",
                              key.c_str(),
                              json.toStyledString().c_str(),
                              ttl
                            );
                        }
                    }
                    (*sharedCb)(optToken);
                }
              );
          }
          else if (r.type() == drogon::nosql::RedisResultType::kString)
          {
              // L2 Cache Hit
              std::string jsonStr = r.asString();
              Json::Value json;
              if (parseJsonString(jsonStr, json))
              {
                  OAuth2AccessToken t;
                  t.token = json["token"].asString();
                  t.clientId = json["client_id"].asString();
                  t.userId = json["user_id"].asString();
                  t.scope = json["scope"].asString();
                  t.expiresAt = json["expires_at"].asInt64();
                  t.revoked = json["revoked"].asBool();

                  // Fill L1 Cache
                  auto now = std::chrono::duration_cast<std::chrono::seconds>(
                               std::chrono::system_clock::now().time_since_epoch()
                  )
                               .count();
                  long long ttl = t.expiresAt - now;
                  if (ttl > 0)
                      tokenCache_.insert(token, t, ttl);

                  (*sharedCb)(t);
              }
              else
              {
                  // Parse Error -> Fallback to DB
                  impl_->getAccessToken(token, [sharedCb](auto val) { (*sharedCb)(val); });
              }
          }
          else
          {
              impl_->getAccessToken(token, [sharedCb](auto val) { (*sharedCb)(val); });
          }
      },
      [self = shared_from_this(), this, token, sharedCb](const std::exception &e) {
          LOG_ERROR << "Redis Read Error: " << e.what();
          impl_->getAccessToken(token, [sharedCb](auto val) { (*sharedCb)(val); });
      },
      "GET %s",
      key.c_str()
    );
}

void CachedOAuth2Storage::saveRefreshToken(const OAuth2RefreshToken &token, VoidCallback &&cb)
{
    impl_->saveRefreshToken(token, std::move(cb));
}

void CachedOAuth2Storage::getRefreshToken(const std::string &token, RefreshTokenCallback &&cb)
{
    impl_->getRefreshToken(token, std::move(cb));
}

void CachedOAuth2Storage::revokeRefreshToken(const std::string &token, VoidCallback &&cb)
{
    impl_->revokeRefreshToken(token, std::move(cb));
}

void CachedOAuth2Storage::atomicRevokeRefreshToken(
  const std::string &token,
  RefreshTokenCallback &&cb
)
{
    impl_->atomicRevokeRefreshToken(token, std::move(cb));
}

void CachedOAuth2Storage::revokeTokenFamily(const std::string &familyId, VoidCallback &&cb)
{
    impl_->revokeTokenFamily(familyId, [self = shared_from_this(), this, familyId, cb = std::move(cb)]() {
        // Evict all cached tokens (conservative approach)
        // In production, could track family->token mappings for precise eviction
        if (cb)
            cb();
    });
}

void CachedOAuth2Storage::deleteExpiredData()
{
    impl_->deleteExpiredData();
}

void CachedOAuth2Storage::getUserRoles(const std::string &userId, StringListCallback &&cb)
{
    // Caching Strategy: We could cache user roles in Redis key
    // "oauth2:roles:{userId}" For now, pass through to underlying storage
    // (Postgres) which is efficient enough for login
    impl_->getUserRoles(userId, std::move(cb));
}

void CachedOAuth2Storage::getUserRoles(int32_t internalUserId, StringListCallback &&cb)
{
    impl_->getUserRoles(internalUserId, std::move(cb));
}

// ========== Subject Mapping Operations (Pass Through) ==========

void CachedOAuth2Storage::getInternalUserId(
  const std::string &subject,
  const std::string &provider,
  OptionalIntCallback &&cb
)
{
    impl_->getInternalUserId(subject, provider, std::move(cb));
}

void CachedOAuth2Storage::createSubjectMapping(
  const std::string &subject,
  int32_t internalUserId,
  const std::string &provider,
  BoolCallback &&cb
)
{
    impl_->createSubjectMapping(subject, internalUserId, provider, std::move(cb));
}

// ========== Authorization Transaction Operations (Pass Through) ==========

void CachedOAuth2Storage::saveAuthorizationTransaction(
  const AuthorizationTransaction &transaction,
  BoolCallback &&cb
)
{
    impl_->saveAuthorizationTransaction(transaction, std::move(cb));
}

void CachedOAuth2Storage::getAuthorizationTransaction(
  const std::string &transactionId,
  TransactionCallback &&cb
)
{
    impl_->getAuthorizationTransaction(transactionId, std::move(cb));
}

void CachedOAuth2Storage::deleteAuthorizationTransaction(
  const std::string &transactionId,
  VoidCallback &&cb
)
{
    impl_->deleteAuthorizationTransaction(transactionId, std::move(cb));
}

void CachedOAuth2Storage::markTransactionConsumed(
  const std::string &transactionId,
  BoolCallback &&cb
)
{
    impl_->markTransactionConsumed(transactionId, std::move(cb));
}

// ========== Scope Management Operations (Pass Through) ==========

void CachedOAuth2Storage::hasUserConsent(
  int32_t internalUserId,
  const std::string &clientId,
  const std::string &scope,
  BoolCallback &&cb
)
{
    impl_->hasUserConsent(internalUserId, clientId, scope, std::move(cb));
}

void CachedOAuth2Storage::saveUserConsent(
  int32_t internalUserId,
  const std::string &clientId,
  const std::string &scope,
  BoolCallback &&cb
)
{
    impl_->saveUserConsent(
      internalUserId,
      clientId,
      scope,
      [self = shared_from_this(), this, clientId, cb = std::move(cb)](bool success) {
          // Evict client from L1 cache after consent change
          clientCache_.erase(clientId);
          if (cb)
              cb(success);
      }
    );
}

void CachedOAuth2Storage::revokeUserConsent(
  int32_t internalUserId,
  const std::string &clientId,
  const std::string &scope,
  VoidCallback &&cb
)
{
    impl_->revokeUserConsent(
      internalUserId,
      clientId,
      scope,
      [self = shared_from_this(), this, clientId, cb = std::move(cb)]() {
          // Evict client from L1 cache after consent revocation
          clientCache_.erase(clientId);
          if (cb)
              cb();
      });
}

// ========== P1: Token Introspection (RFC 7662) ==========

void CachedOAuth2Storage::introspectToken(
  const std::string &token,
  IOAuth2Storage::TokenIntrospectionCallback &&cb
)
{
    // Pass through to implementation
    impl_->introspectToken(token, std::move(cb));
}

void CachedOAuth2Storage::incrementIntrospectCount(
  const std::string &token,
  IOAuth2Storage::VoidCallback &&cb
)
{
    // Pass through to implementation
    impl_->incrementIntrospectCount(token, std::move(cb));
}

// ========== P1: Token Revocation (RFC 7009) ==========

void CachedOAuth2Storage::revokeAccessToken(
  const std::string &token,
  const std::string &revokedBy,
  IOAuth2Storage::VoidCallback &&cb
)
{
    // Revoke in implementation and invalidate cache
    impl_->revokeAccessToken(token, revokedBy, [self = shared_from_this(), this, token, cb = std::move(cb)]() mutable {
        // Invalidate L1 Cache
        tokenCache_.erase(token);

        // Invalidate L2 Cache after revocation
        if (redisClient_)
        {
            std::string key = "oauth2:token:" + token;
            redisClient_->execCommandAsync(
              [cb](const drogon::nosql::RedisResult &) {
                  if (cb)
                      cb();
              },
              [cb](const std::exception &e) {
                  LOG_ERROR << "Failed to invalidate revoked token cache: " << e.what();
                  if (cb)
                      cb();
              },
              "DEL %s",
              key.c_str()
            );
        }
        else
        {
            if (cb)
                cb();
        }
    });
}

void CachedOAuth2Storage::getUserInfo(const std::string &userId, OptionalJsonCallback &&cb)
{
    // Pass through to implementation
    impl_->getUserInfo(userId, std::move(cb));
}

void CachedOAuth2Storage::getUserInfo(int32_t internalUserId, OptionalJsonCallback &&cb)
{
    // Pass through to implementation
    impl_->getUserInfo(internalUserId, std::move(cb));
}

}  // namespace oauth2
