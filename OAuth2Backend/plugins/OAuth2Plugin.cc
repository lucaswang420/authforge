#include "OAuth2Plugin.h"
#include "MemoryOAuth2Storage.h"
#include "PostgresOAuth2Storage.h"
#include "RedisOAuth2Storage.h"
#include "CachedOAuth2Storage.h"
#include "../common/utils/SubjectGenerator.h"
#include <drogon/drogon.h>
#include <drogon/utils/Utilities.h>
#include <chrono>

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
        LOG_INFO << "OAuth2 Config Loaded: AuthCode=" << authCodeTtl_
                 << "s, AccessToken=" << accessTokenTtl_ << "s, RefreshToken=" << refreshTokenTtl_
                 << "s";
    }

    LOG_INFO << "OAuth2Plugin initialized with storage type: " << storageType_;

    // Initialize and start cleanup service
    cleanupService_ = std::make_shared<oauth2::OAuth2CleanupService>(storage_.get());

    // Default cleanup 1 hour, or config
    double cleanupInterval = 3600.0;
    if (config.isMember("cleanup_interval_seconds"))
    {
        cleanupInterval = config["cleanup_interval_seconds"].asDouble();
    }
    cleanupService_->start(cleanupInterval);
}

void OAuth2Plugin::initStorage(const Json::Value &config)
{
    storageType_ = config.get("storage_type", "memory").asString();

    if (storageType_ == "postgres")
    {
        auto s = std::make_unique<oauth2::PostgresOAuth2Storage>();
        s->initFromConfig(config["postgres"]);  // Always call to get defaults if missing

        // Try to enable L2 Cache
        try
        {
            auto redis = drogon::app().getRedisClient("default");
            // Keep raw pointer to recover if exception occurs
            oauth2::IOAuth2Storage *rawPtr = s.get();
            std::unique_ptr<oauth2::IOAuth2Storage> baseStorage = std::move(s);
            // Use raw new to avoid make_unique forwarding issues with move-only
            // types in some MSVC versions
            std::unique_ptr<oauth2::IOAuth2Storage> cached(
              new oauth2::CachedOAuth2Storage(std::move(baseStorage), redis)
            );
            storage_ = std::move(cached);
            LOG_INFO << "Using PostgreSQL storage backend with L2 Redis Cache";
        }
        catch (...)
        {
            // Recover the original storage if cache initialization failed
            // The unique_ptr was moved, but we kept the raw pointer
            // Note: baseStorage's destructor will run during stack unwinding,
            // so we need to recreate or use the original pointer
            LOG_ERROR << "Failed to init Cache. Fallback to Postgres without cache.";
            // The original storage was already destroyed when baseStorage went
            // out of scope, so we need to recreate it
            auto s2 = std::make_unique<oauth2::PostgresOAuth2Storage>();
            s2->initFromConfig(config["postgres"]);
            storage_ = std::move(s2);
            LOG_INFO << "Using PostgreSQL storage backend (Cache Disabled)";
        }
    }
    else if (storageType_ == "redis")
    {
        storage_ = oauth2::createRedisStorage(config["redis"]);
        LOG_INFO << "Using Redis storage backend";
    }
    else
    {
        auto s = std::make_unique<oauth2::MemoryOAuth2Storage>();
        if (config.isMember("clients"))
            s->initFromConfig(config["clients"], config["admin_users"]);
        storage_ = std::move(s);
        LOG_INFO << "Using in-memory storage backend";
    }
}

void OAuth2Plugin::shutdown()
{
    LOG_INFO << "OAuth2Plugin shutdown";
    if (cleanupService_)
        cleanupService_->stop();
    storage_.reset();
}

void OAuth2Plugin::validateClient(
  const std::string &clientId,
  const std::string &clientSecret,
  std::function<void(bool)> &&callback
)
{
    if (!storage_)
    {
        callback(false);
        return;
    }
    storage_->validateClient(clientId, clientSecret, std::move(callback));
}

void OAuth2Plugin::validateRedirectUri(
  const std::string &clientId,
  const std::string &redirectUri,
  std::function<void(bool)> &&callback
)
{
    if (!storage_)
    {
        callback(false);
        return;
    }

    // We need to getClient first, then check URIs
    storage_->getClient(
      clientId,
      [callback = std::move(callback), redirectUri](std::optional<oauth2::OAuth2Client> client) {
          if (!client)
          {
              callback(false);
              return;
          }
          for (const auto &uri : client->redirectUris)
          {
              if (uri == redirectUri)
              {
                  callback(true);
                  return;
              }
          }
          callback(false);
      }
    );
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
    using namespace oauth2::utils;

    if (!storage_)
    {
        callback(false, "", "Storage not initialized");
        return;
    }

    auto code = utils::getUuid();
    oauth2::OAuth2AuthCode authCode;
    authCode.code = code;
    authCode.clientId = clientId;
    authCode.userId = subject;  // Store subject instead of userId
    authCode.scope = scope;
    authCode.redirectUri = redirectUri;
    authCode.codeChallenge = codeChallenge;
    authCode.codeChallengeMethod = codeChallengeMethod;

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
                 std::chrono::system_clock::now().time_since_epoch()
    )
                 .count();
    authCode.expiresAt = now + authCodeTtl_;

    LOG_DEBUG << "Generated authorization code for client: " << clientId << ", subject: " << subject
              << ", scope: " << scope
              << ", PKCE: " << (codeChallenge.empty() ? "no" : codeChallengeMethod);

    storage_->saveAuthCode(authCode, [callback = std::move(callback), code]() {
        callback(true, code, "");
    });
}

// Helper to create error JSON
static Json::Value makeError(const std::string &error, const std::string &desc = "")
{
    Json::Value json;
    json["error"] = error;
    if (!desc.empty())
        json["error_description"] = desc;
    return json;
}

void OAuth2Plugin::exchangeCodeForToken(
  const std::string &code,
  const std::string &clientId,
  const std::string &clientSecret,
  const std::string &redirectUri,
  const std::string &codeVerifier,  // P0-3: PKCE code verifier
  std::function<void(const Json::Value &)> &&callback
)
{
    if (!storage_)
    {
        callback(makeError("server_error"));
        return;
    }

    // CRITICAL: Validate client BEFORE consuming auth code
    storage_->validateClient(
      clientId,
      clientSecret,
      [this, code, clientId, redirectUri, codeVerifier, callback = std::move(callback)](
        bool isValid
      ) mutable {
          if (!isValid)
          {
              LOG_WARN << "[AUDIT] Action=ExchangeToken Client=" << clientId
                       << " Success=False Reason=Invalid_client_authentication";
              callback(makeError("invalid_client", "Client authentication failed"));
              return;
          }

          // Client validated, now consume auth code WITH redirect_uri
          // validation
          storage_->consumeAuthCode(
            code,
            redirectUri,
            [this, callback = std::move(callback), clientId, code, codeVerifier](
              std::optional<oauth2::OAuth2AuthCode> authCode
            ) {
                if (!authCode)
                {
                    LOG_WARN << "Invalid code (Not Found, Already Used, or "
                                "redirect_uri mismatch): "
                             << code;
                    callback(makeError("invalid_grant", "Invalid authorization code"));
                    return;
                }
                if (authCode->clientId != clientId)
                {
                    callback(makeError("invalid_client", "Client ID mismatch"));
                    return;
                }

                // P0-3: PKCE Validation - Validate code_verifier if
                // code_challenge was present
                if (!authCode->codeChallenge.empty())
                {
                    if (codeVerifier.empty())
                    {
                        LOG_WARN << "PKCE: code_verifier required but "
                                    "not provided for code: "
                                 << code;
                        callback(makeError(
                          "invalid_request",
                          "code_verifier is required "
                          "when PKCE was used in "
                          "authorization request"
                        ));
                        return;
                    }

                    if (!validatePkceCodeVerifier(
                          codeVerifier, authCode->codeChallenge, authCode->codeChallengeMethod
                        ))
                    {
                        LOG_WARN << "PKCE: code_verifier validation "
                                    "failed for code: "
                                 << code;
                        callback(makeError("invalid_grant", "Invalid code_verifier"));
                        return;
                    }

                    LOG_DEBUG << "PKCE: code_verifier validation "
                                 "successful for code: "
                              << code;
                }

                auto now = std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::system_clock::now().time_since_epoch()
                )
                             .count();

                if (now > authCode->expiresAt)
                {
                    LOG_WARN << "Code expired: " << code;
                    callback(makeError("invalid_grant", "Code expired"));
                    return;
                }

                // Generate Access Token (Code is already marked used by
                // consumeAuthCode) No need to call markAuthCodeUsed
                // again. Generate Access Token (Code is already marked
                // used by consumeAuthCode)

                // Fetch User Roles to return in response (and
                // potentially bake into token if we switched to JWT
                // later)
                storage_->getUserRoles(
                  authCode->userId,
                  [this,
                   callback,
                   authCode,
                   now,
                   accessTokenTtl = accessTokenTtl_,
                   refreshTokenTtl = refreshTokenTtl_](std::vector<std::string> roles) {
                      // Convert roles vector to string for
                      // logs/response
                      Json::Value rolesJson(Json::arrayValue);
                      for (const auto &r : roles)
                          rolesJson.append(r);

                      // Generate Access Token
                      auto tokenStr = utils::getUuid();
                      oauth2::OAuth2AccessToken token;
                      token.token = tokenStr;
                      token.clientId = authCode->clientId;
                      token.userId = authCode->userId;
                      token.scope = authCode->scope;
                      token.expiresAt = now + accessTokenTtl;

                      // Generate Refresh Token
                      auto refreshTokenStr = utils::getUuid();
                      oauth2::OAuth2RefreshToken refreshToken;
                      refreshToken.token = refreshTokenStr;
                      refreshToken.accessToken = tokenStr;
                      refreshToken.clientId = authCode->clientId;
                      refreshToken.userId = authCode->userId;
                      refreshToken.scope = authCode->scope;
                      refreshToken.expiresAt = now + refreshTokenTtl;

                      // Save Access Token
                      storage_->saveAccessToken(
                        token, [this, callback, token, refreshToken, rolesJson]() {
                            // Save
                            // Refresh
                            // Token
                            storage_->saveRefreshToken(
                              refreshToken, [this, callback, token, refreshToken, rolesJson]() {
                                  LOG_INFO << "[AUDIT] Action=IssueToken User=" << token.userId
                                           << " Client=" << token.clientId << " Success=True";

                                  Json::Value json;
                                  json["access_token"] = token.token;
                                  json["token_type"] = "Bearer";
                                  json["expires_in"] =
                                    (Json::Int64)(token.expiresAt -
                                                  std::chrono::duration_cast<std::chrono::seconds>(
                                                    std::chrono::system_clock::now()
                                                      .time_since_epoch()
                                                  )
                                                    .count());
                                  json["refresh_token"] = refreshToken.token;
                                  json["roles"] = rolesJson;  // Extension: Return roles
                                  callback(json);
                              }
                            );
                        }
                      );
                  }
                );
            }
          );
      }
    );
}

void OAuth2Plugin::refreshAccessToken(
  const std::string &refreshTokenStr,
  const std::string &clientId,
  std::function<void(const Json::Value &)> &&callback
)
{
    if (!storage_)
    {
        callback(makeError("server_error"));
        return;
    }

    storage_->getRefreshToken(
      refreshTokenStr,
      [this,
       callback = std::move(callback),
       clientId](std::optional<oauth2::OAuth2RefreshToken> storedRt) {
          if (!storedRt)
          {
              callback(makeError("invalid_grant", "Invalid refresh token"));
              return;
          }
          if (storedRt->clientId != clientId)
          {
              callback(makeError("invalid_client"));
              return;
          }
          if (storedRt->revoked)
          {
              LOG_WARN << "Refresh token revoked: " << storedRt->token;
              callback(makeError("invalid_grant", "Token revoked"));
              return;
          }

          auto now = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch()
          )
                       .count();

          if (now > storedRt->expiresAt)
          {
              callback(makeError("invalid_grant", "Token expired"));
              return;
          }

          // Generate New Tokens (Rolling Refresh Token pattern is safer, but
          // keeping simple for now? Let's implement Rolling: Revoke old RT,
          // Issue new RT)

          // 1. Generate New Access Token
          auto newTokenStr = utils::getUuid();
          oauth2::OAuth2AccessToken token;
          token.token = newTokenStr;
          token.clientId = storedRt->clientId;
          token.userId = storedRt->userId;
          token.scope = storedRt->scope;
          token.expiresAt = now + accessTokenTtl_;

          // 2. Generate New Refresh Token
          auto newRefreshTokenStr = utils::getUuid();
          oauth2::OAuth2RefreshToken newRt;
          newRt.token = newRefreshTokenStr;
          newRt.accessToken = newTokenStr;
          newRt.clientId = storedRt->clientId;
          newRt.userId = storedRt->userId;
          newRt.scope = storedRt->scope;
          newRt.expiresAt = now + refreshTokenTtl_;

          // 3. Save New Access Token
          storage_->saveAccessToken(
            token, [this, callback, token, newRt, oldRefreshToken = storedRt->token]() {
                // 4. Save New Refresh Token
                storage_
                  ->saveRefreshToken(newRt, [this, callback, token, newRt, oldRefreshToken]() {
                      // 5. Revoke old refresh token
                      storage_->revokeRefreshToken(
                        oldRefreshToken, [this, callback, token, newRt, oldRefreshToken](auto...) {
                            LOG_INFO << "[AUDIT] Action=RefreshToken User=" << token.userId
                                     << " OldToken=Revoked NewToken=Issued";

                            Json::Value json;
                            json["access_token"] = token.token;
                            json["token_type"] = "Bearer";
                            json["expires_in"] = (Json::Int64)accessTokenTtl_;
                            json["refresh_token"] = newRt.token;
                            callback(json);
                        }
                      );
                  });
            }
          );
      }
    );
}

void OAuth2Plugin::validateAccessToken(
  const std::string &token,
  std::function<void(std::shared_ptr<AccessToken>)> &&callback
)
{
    if (!storage_)
    {
        callback(nullptr);
        return;
    }

    storage_->getAccessToken(token, [callback](std::optional<oauth2::OAuth2AccessToken> t) {
        if (!t)
        {
            callback(nullptr);
            return;
        }

        auto now = std::chrono::duration_cast<std::chrono::seconds>(
                     std::chrono::system_clock::now().time_since_epoch()
        )
                     .count();

        if (t->revoked)
        {
            LOG_WARN << "Access token revoked: " << t->token;
            callback(nullptr);
            return;
        }
        if (now > t->expiresAt)
        {
            LOG_WARN << "Access token expired: " << t->token;
            callback(nullptr);
            return;
        }

        callback(std::make_shared<oauth2::OAuth2AccessToken>(*t));
    });
}

void OAuth2Plugin::getUserRoles(
  const std::string &userId,
  std::function<void(std::vector<std::string>)> &&callback
)
{
    if (!storage_)
    {
        callback({});
        return;
    }
    storage_->getUserRoles(userId, std::move(callback));
}

// ========== Subject Mapping Methods ==========

void OAuth2Plugin::ensureSubjectMapping(
  const std::string &subject,
  const std::string &username,
  int32_t internalUserId,
  std::function<void()> &&callback
)
{
    using namespace oauth2::utils;

    if (!storage_)
    {
        LOG_ERROR << "Storage not initialized in ensureSubjectMapping";
        callback();
        return;
    }

    // Parse subject to get provider and subject
    auto [provider, sub] = SubjectGenerator::parse(subject);

    // Check if mapping already exists
    storage_->getInternalUserId(
      sub,
      provider,
      [this, sub, provider, internalUserId, username, callback = std::move(callback)](
        auto existingUserId
      ) {
          if (existingUserId)
          {
              // Mapping already exists, verify consistency
              if (*existingUserId == internalUserId)
              {
                  LOG_DEBUG << "Subject mapping verified: " << sub << " (provider: " << provider
                            << ") -> user_id: " << internalUserId;
                  callback();
                  return;
              }
              else
              {
                  LOG_WARN << "Subject mapping conflict: " << sub << " (provider: " << provider
                           << ") -> old:" << *existingUserId << " vs new:" << internalUserId
                           << ". Using existing mapping.";
                  callback();
                  return;
              }
          }

          // Create new mapping
          storage_->createSubjectMapping(
            sub,
            internalUserId,
            provider,
            [this, sub, provider, internalUserId, callback = std::move(callback)](bool success) {
                if (!success)
                {
                    LOG_ERROR << "Failed to create subject mapping for: " << sub
                              << " (provider: " << provider << ") -> user_id: " << internalUserId;
                }
                else
                {
                    LOG_INFO << "Created subject mapping: " << sub << " (provider: " << provider
                             << ") -> user_id: " << internalUserId;
                }
                callback();
            }
          );
      }
    );
}

void OAuth2Plugin::handleFirstTimeLogin(
  const std::string &subject,
  const std::string &provider,
  std::function<void(int32_t)> &&callback
)
{
    using namespace oauth2::utils;

    if (!storage_)
    {
        LOG_ERROR << "Storage not initialized in handleFirstTimeLogin";
        callback(0);
        return;
    }

    // Parse subject to get provider and subject
    auto [prov, sub] = SubjectGenerator::parse(subject);

    // For first-time login, we need to create a new user in the users table
    // This is a simplified implementation - in production, you might want to:
    // 1. Check if user exists in external auth provider
    // 2. Create user record with appropriate defaults
    // 3. Assign default roles
    // 4. Create subject mapping

    // For now, we'll use a simple approach: extract username from subject
    // and use a sequential user ID (in production, use proper user creation)
    static int32_t nextUserId = 1000;  // Start from 1000 to avoid conflicts
    int32_t newUserId = nextUserId++;

    LOG_INFO << "First-time login for subject: " << sub << " (provider: " << prov
             << "), assigned user_id: " << newUserId;

    // Create subject mapping
    storage_->createSubjectMapping(
      sub,
      newUserId,
      prov,
      [this, sub, prov, newUserId, callback = std::move(callback)](bool success) {
          if (!success)
          {
              LOG_ERROR << "Failed to create subject mapping for first-time login: " << sub;
              callback(0);
              return;
          }

          LOG_INFO << "Created subject mapping for first-time login: " << sub
                   << " (provider: " << prov << ") -> user_id: " << newUserId;
          callback(newUserId);
      }
    );
}

// ========== P0-2: Consent Management Method Implementations ==========

void OAuth2Plugin::getInternalUserId(
  const std::string &subject,
  std::function<void(std::optional<int32_t>)> &&callback
)
{
    using namespace oauth2::utils;

    if (!storage_)
    {
        LOG_ERROR << "Storage not initialized in getInternalUserId";
        callback(std::nullopt);
        return;
    }

    // Parse subject to get provider and subject
    auto [provider, sub] = SubjectGenerator::parse(subject);

    // Get internal user ID from storage
    storage_->getInternalUserId(
      sub, provider, [callback = std::move(callback)](std::optional<int32_t> internalUserId) {
          callback(internalUserId);
      }
    );
}

void OAuth2Plugin::hasUserConsent(
  int32_t internalUserId,
  const std::string &clientId,
  const std::string &scope,
  std::function<void(bool)> &&callback
)
{
    if (!storage_)
    {
        LOG_ERROR << "Storage not initialized in hasUserConsent";
        callback(false);
        return;
    }

    storage_->hasUserConsent(internalUserId, clientId, scope, std::move(callback));
}

void OAuth2Plugin::saveUserConsent(
  int32_t internalUserId,
  const std::string &clientId,
  const std::string &scope,
  std::function<void(bool)> &&callback
)
{
    if (!storage_)
    {
        LOG_ERROR << "Storage not initialized in saveUserConsent";
        callback(false);
        return;
    }

    storage_->saveUserConsent(internalUserId, clientId, scope, std::move(callback));
}

// ========== P0-3: PKCE Validation Method Implementations ==========

bool OAuth2Plugin::validatePkceCodeVerifier(
  const std::string &codeVerifier,
  const std::string &codeChallenge,
  const std::string &codeChallengeMethod
)
{
    if (codeVerifier.empty() || codeChallenge.empty())
    {
        LOG_ERROR << "PKCE: Empty code_verifier or code_challenge";
        return false;
    }

    std::string method = codeChallengeMethod;
    if (method.empty())
    {
        // RFC 7636: If code_challenge_method is omitted, default to "plain"
        method = "plain";
    }

    if (method == "plain")
    {
        // Plain method: code_verifier == code_challenge
        bool valid = codeVerifier == codeChallenge;
        LOG_DEBUG << "PKCE: Plain method validation " << (valid ? "succeeded" : "failed");
        return valid;
    }
    else if (method == "S256")
    {
        // S256 method: BASE64URL-encode(SHA256(ASCII(code_verifier))) ==
        // code_challenge
        std::string computedChallenge = generateSha256Hash(codeVerifier);
        bool valid = computedChallenge == codeChallenge;
        LOG_DEBUG << "PKCE: S256 method validation " << (valid ? "succeeded" : "failed")
                  << ", computed: " << computedChallenge << ", expected: " << codeChallenge;
        return valid;
    }
    else
    {
        LOG_ERROR << "PKCE: Unsupported code_challenge_method: " << method;
        return false;
    }
}

std::string OAuth2Plugin::generateSha256Hash(const std::string &input)
{
    // Use Drogon's SHA256 implementation
    std::string hash = drogon::utils::getSha256(input);
    // Convert to base64-url encoding (replace '+' with '-', '/' with '_',
    // remove
    // '=' padding)
    std::string base64Url = drogon::utils::base64Encode(
      reinterpret_cast<const unsigned char *>(hash.c_str()), hash.length()
    );

    // Convert standard base64 to base64-url encoding
    for (char &c : base64Url)
    {
        if (c == '+')
            c = '-';
        else if (c == '/')
            c = '_';
    }

    // Remove padding characters '='
    while (!base64Url.empty() && base64Url.back() == '=')
    {
        base64Url.pop_back();
    }

    return base64Url;
}

// ========== P0-5: Scope Permission Control Method Implementations ==========

void OAuth2Plugin::validateClientScopes(
  const std::string &clientId,
  const std::vector<std::string> &requestedScopes,
  std::function<void(bool, std::string)> &&callback
)
{
    if (!storage_)
    {
        callback(false, "Storage not initialized");
        return;
    }

    // Get client configuration to check allowed scopes
    storage_->getClient(
      clientId,
      [callback = std::move(callback),
       requestedScopes](std::optional<oauth2::OAuth2Client> client) mutable {
          if (!client)
          {
              callback(false, "Client not found");
              return;
          }

          // Tier 1: Check if requested scopes are in client's allowlist
          std::vector<std::string> invalidScopes;
          for (const auto &scope : requestedScopes)
          {
              bool scopeAllowed = false;
              for (const auto &allowedScope : client->allowedScopes)
              {
                  if (scope == allowedScope)
                  {
                      scopeAllowed = true;
                      break;
                  }
              }

              if (!scopeAllowed)
              {
                  invalidScopes.push_back(scope);
              }
          }

          if (!invalidScopes.empty())
          {
              std::string errorMsg = "Scopes not allowed for this client: " + invalidScopes[0];
              for (size_t i = 1; i < invalidScopes.size(); ++i)
              {
                  errorMsg += ", " + invalidScopes[i];
              }
              LOG_WARN << "Client scope validation failed for client: " << client->clientId
                       << ", invalid scopes: " << errorMsg;
              callback(false, errorMsg);
              return;
          }

          LOG_DEBUG << "Client scope validation successful for client: " << client->clientId;
          callback(true, "");
      }
    );
}

void OAuth2Plugin::validateUserRolesForScopes(
  const std::string &userId,
  const std::vector<std::string> &scopes,
  std::function<void(bool, std::string)> &&callback
)
{
    if (!storage_)
    {
        callback(false, "Storage not initialized");
        return;
    }

    // Tier 2: Check if user has required roles for admin scopes
    std::vector<std::string> adminScopes;
    for (const auto &scope : scopes)
    {
        if (scopeRequiresAdminRole(scope))
        {
            adminScopes.push_back(scope);
        }
    }

    // If no admin scopes requested, no role validation needed
    if (adminScopes.empty())
    {
        LOG_DEBUG << "No admin scopes requested, skipping role validation";
        callback(true, "");
        return;
    }

    // Get user roles to validate admin scope access
    getUserRoles(
      userId,
      [callback = std::move(callback), adminScopes](std::vector<std::string> userRoles) mutable {
          // Check if user has admin role
          bool hasAdminRole = false;
          for (const auto &role : userRoles)
          {
              if (role == "admin")
              {
                  hasAdminRole = true;
                  break;
              }
          }

          if (!hasAdminRole)
          {
              std::string errorMsg = "Admin role required for scopes: " + adminScopes[0];
              for (size_t i = 1; i < adminScopes.size(); ++i)
              {
                  errorMsg += ", " + adminScopes[i];
              }
              LOG_WARN << "User role validation failed, admin role required "
                          "for scopes: "
                       << errorMsg;
              callback(false, errorMsg);
              return;
          }

          LOG_DEBUG << "User role validation successful, user has admin role";
          callback(true, "");
      }
    );
}

bool OAuth2Plugin::scopeRequiresAdminRole(const std::string &scope)
{
    // Define which scopes require admin role
    // In production, this should be configurable or loaded from database
    static const std::vector<std::string> adminScopes =
      {"admin", "admin:read", "admin:write", "user:manage", "settings:manage"};

    for (const auto &adminScope : adminScopes)
    {
        if (scope == adminScope || scope.find(adminScope + ":") == 0)
        {
            return true;
        }
    }

    return false;
}
