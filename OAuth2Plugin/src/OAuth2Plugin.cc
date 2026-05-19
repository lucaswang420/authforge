#include <oauth2/OAuth2Plugin.h>
#include <oauth2/controllers/OAuth2StandardController.h>
#include <oauth2/filters/OAuth2Middleware.h>
#include <oauth2/JwkManager.h>
#include "storage/MemoryOAuth2Storage.h"
#include "storage/PostgresOAuth2Storage.h"
#include "storage/RedisOAuth2Storage.h"
#include "storage/CachedOAuth2Storage.h"
#include <drogon/drogon.h>

using namespace drogon;

void OAuth2Plugin::initAndStart(const Json::Value &config)
{
    LOG_INFO << "OAuth2Plugin loading...";
    initStorage(config);

    // Load TTL Config
    if (config.isMember("tokens"))
    {
        auto tokens = config["tokens"];
        authCodeTtl_ = tokens.get("auth_code_ttl", 600).asInt64();
        accessTokenTtl_ = tokens.get("access_token_ttl", 3600).asInt64();
        refreshTokenTtl_ = tokens.get("refresh_token_ttl", 2592000).asInt64();
    }

    // Initialize JWK Manager for OIDC id_token signing
    jwkManager_ = std::make_shared<oauth2::JwkManager>();
    if (config.isMember("oidc"))
    {
        jwkManager_->init(config["oidc"]);
    }
    else
    {
        // Initialize with empty config (will generate ephemeral key)
        Json::Value emptyConfig;
        jwkManager_->init(emptyConfig);
    }

    // Initialize Services
    tokenService_ = std::make_shared<oauth2::TokenService>(
      storage_.get(), authCodeTtl_, accessTokenTtl_, refreshTokenTtl_
    );
    tokenService_->setJwkManager(jwkManager_);
    clientService_ = std::make_shared<oauth2::ClientService>(storage_.get());
    identityService_ = std::make_shared<oauth2::IdentityService>(storage_.get());

    // Initialize Cleanup Service
    cleanupService_ = std::make_shared<oauth2::OAuth2CleanupService>(storage_.get());
    double cleanupInterval = config.get("cleanup_interval_seconds", 3600.0).asDouble();
    cleanupService_->start(cleanupInterval);

    LOG_INFO << "OAuth2Plugin initialized with storage type: " << storageType_;
}

void OAuth2Plugin::initStorage(const Json::Value &config)
{
    storageType_ = config.get("storage_type", "memory").asString();

    if (storageType_ == "postgres")
    {
        auto s = std::make_unique<oauth2::PostgresOAuth2Storage>();
        s->initFromConfig(config["postgres"]);
        try
        {
            auto redis = drogon::app().getRedisClient("default");
            std::unique_ptr<oauth2::IOAuth2Storage> baseStorage = std::move(s);
            storage_ = std::unique_ptr<oauth2::IOAuth2Storage>(
              new oauth2::CachedOAuth2Storage(std::move(baseStorage), redis)
            );
            LOG_INFO << "Using PostgreSQL storage backend with L2 Redis Cache";
        }
        catch (...)
        {
            LOG_ERROR << "Failed to init Cache. Fallback to Postgres without cache.";
            auto s2 = std::make_unique<oauth2::PostgresOAuth2Storage>();
            s2->initFromConfig(config["postgres"]);
            storage_ = std::move(s2);
        }
    }
    else if (storageType_ == "redis")
    {
        storage_ = oauth2::createRedisStorage(config["redis"]);
    }
    else
    {
        auto s = std::make_unique<oauth2::MemoryOAuth2Storage>();
        if (config.isMember("clients"))
            s->initFromConfig(config["clients"], config["admin_users"]);
        storage_ = std::move(s);
    }
}

void OAuth2Plugin::shutdown()
{
    if (cleanupService_)
        cleanupService_->stop();
    storage_.reset();
}

// ========== Delegate to Services ==========

void OAuth2Plugin::validateClient(
  const std::string &clientId,
  const std::string &clientSecret,
  std::function<void(bool)> &&callback
)
{
    clientService_->validateClient(clientId, clientSecret, std::move(callback));
}

void OAuth2Plugin::validateRedirectUri(
  const std::string &clientId,
  const std::string &redirectUri,
  std::function<void(bool)> &&callback
)
{
    clientService_->validateRedirectUri(clientId, redirectUri, std::move(callback));
}

void OAuth2Plugin::generateAuthorizationCode(
  const std::string &clientId,
  const std::string &subject,
  const std::string &scope,
  const std::string &redirectUri,
  const std::string &codeChallenge,
  const std::string &codeChallengeMethod,
  std::function<void(bool, std::string, std::string)> &&callback
)
{
    tokenService_->generateAuthorizationCode(
      clientId, subject, scope, redirectUri, codeChallenge, codeChallengeMethod, std::move(callback)
    );
}

void OAuth2Plugin::exchangeCodeForToken(
  const std::string &code,
  const std::string &clientId,
  const std::string &clientSecret,
  const std::string &redirectUri,
  const std::string &codeVerifier,
  std::function<void(const Json::Value &)> &&callback
)
{
    tokenService_->exchangeCodeForToken(
      code, clientId, clientSecret, redirectUri, codeVerifier, std::move(callback)
    );
}

void OAuth2Plugin::refreshAccessToken(
  const std::string &refreshToken,
  const std::string &clientId,
  std::function<void(const Json::Value &)> &&callback
)
{
    tokenService_->refreshAccessToken(refreshToken, clientId, std::move(callback));
}

void OAuth2Plugin::validateAccessToken(
  const std::string &token,
  std::function<void(std::shared_ptr<AccessToken>)> &&callback
)
{
    tokenService_->validateAccessToken(token, std::move(callback));
}

void OAuth2Plugin::getUserRoles(
  const std::string &userId,
  std::function<void(std::vector<std::string>)> &&callback
)
{
    identityService_->getUserRoles(userId, std::move(callback));
}

void OAuth2Plugin::getInternalUserId(
  const std::string &subject,
  std::function<void(std::optional<int32_t>)> &&callback
)
{
    identityService_->getInternalUserId(subject, std::move(callback));
}

void OAuth2Plugin::hasUserConsent(
  int32_t internalUserId,
  const std::string &clientId,
  const std::string &scope,
  std::function<void(bool)> &&callback
)
{
    identityService_->hasUserConsent(internalUserId, clientId, scope, std::move(callback));
}

void OAuth2Plugin::saveUserConsent(
  int32_t internalUserId,
  const std::string &clientId,
  const std::string &scope,
  std::function<void(bool)> &&callback
)
{
    identityService_->saveUserConsent(internalUserId, clientId, scope, std::move(callback));
}

void OAuth2Plugin::validateClientScopes(
  const std::string &clientId,
  const std::vector<std::string> &requestedScopes,
  std::function<void(bool, std::string)> &&callback
)
{
    clientService_->validateClientScopes(clientId, requestedScopes, std::move(callback));
}

void OAuth2Plugin::validateUserRolesForScopes(
  const std::string &userId,
  const std::vector<std::string> &scopes,
  std::function<void(bool, std::string)> &&callback
)
{
    identityService_->validateUserRolesForScopes(userId, scopes, std::move(callback));
}

void OAuth2Plugin::introspectToken(
  const std::string &token,
  std::function<void(std::optional<oauth2::TokenIntrospection>)> &&callback
)
{
    tokenService_->introspectToken(token, std::move(callback));
}

void OAuth2Plugin::incrementIntrospectCount(
  const std::string &token,
  std::function<void()> &&callback
)
{
    if (storage_)
        storage_->incrementIntrospectCount(token, std::move(callback));
}

void OAuth2Plugin::revokeAccessToken(
  const std::string &token,
  const std::string &revokedBy,
  std::function<void()> &&callback
)
{
    tokenService_->revokeAccessToken(token, revokedBy, std::move(callback));
}

bool OAuth2Plugin::validatePkceCodeVerifier(
  const std::string &codeVerifier,
  const std::string &codeChallenge,
  const std::string &codeChallengeMethod
)
{
    // Keeping this static for convenience but could delegate to TokenService if needed
    // For now, it's just a pure function. Let's redirect to a helper if needed or keep static
    // logic. Actually, TokenService has a private version. Let's make it a public static utility in
    // common if needed. For now, I'll just re-implement or delegate to a private instance of
    // TokenService if possible. But since it's static in Plugin, I'll just re-implement briefly or
    // move to common/utils.
    return oauth2::TokenService(nullptr)
      .validatePkceCodeVerifier(codeVerifier, codeChallenge, codeChallengeMethod);
}

std::string OAuth2Plugin::generateSha256Hash(const std::string &input)
{
    return oauth2::TokenService(nullptr).generateSha256Hash(input);
}

bool OAuth2Plugin::scopeRequiresAdminRole(const std::string &scope)
{
    return oauth2::IdentityService(nullptr).scopeRequiresAdminRole(scope);
}

void OAuth2Plugin::ensureSubjectMapping(
  const std::string &subject,
  const std::string &username,
  int32_t internalUserId,
  std::function<void()> &&callback
)
{
    identityService_->ensureSubjectMapping(subject, username, internalUserId, std::move(callback));
}

void OAuth2Plugin::handleFirstTimeLogin(
  const std::string &subject,
  const std::string &provider,
  std::function<void(int32_t)> &&callback
)
{
    identityService_->handleFirstTimeLogin(subject, provider, std::move(callback));
}
