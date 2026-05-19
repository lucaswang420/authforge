#include <oauth2/TokenService.h>
#include <oauth2/SubjectGenerator.h>
#include <oauth2/CryptoUtils.h>
#include <oauth2/JwkManager.h>
#include <drogon/utils/Utilities.h>
#include <chrono>

namespace oauth2
{

TokenService::TokenService(
  IOAuth2Storage *storage,
  int64_t authCodeTtl,
  int64_t accessTokenTtl,
  int64_t refreshTokenTtl
)
    : storage_(storage),
      authCodeTtl_(authCodeTtl),
      accessTokenTtl_(accessTokenTtl),
      refreshTokenTtl_(refreshTokenTtl)
{
}

void TokenService::generateAuthorizationCode(
  const std::string &clientId,
  const std::string &subject,
  const std::string &scope,
  const std::string &redirectUri,
  const std::string &codeChallenge,
  const std::string &codeChallengeMethod,
  std::function<void(bool, std::string, std::string)> &&callback
)
{
    if (!storage_)
    {
        callback(false, "", "Storage not initialized");
        return;
    }

    auto code = utils::generateSecureToken();
    OAuth2AuthCode authCode;
    authCode.code = utils::hashToken(code);
    authCode.clientId = clientId;
    authCode.userId = subject;
    authCode.scope = scope;
    authCode.redirectUri = redirectUri;
    authCode.codeChallenge = codeChallenge;
    authCode.codeChallengeMethod = codeChallengeMethod;

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
                 std::chrono::system_clock::now().time_since_epoch()
    )
                 .count();
    authCode.expiresAt = now + authCodeTtl_;

    storage_->saveAuthCode(authCode, [callback = std::move(callback), code]() {
        callback(true, code, "");
    });
}

static Json::Value makeError(const std::string &error, const std::string &desc = "")
{
    Json::Value json;
    json["error"] = error;
    if (!desc.empty())
        json["error_description"] = desc;
    return json;
}

void TokenService::exchangeCodeForToken(
  const std::string &code,
  const std::string &clientId,
  const std::string &clientSecret,
  const std::string &redirectUri,
  const std::string &codeVerifier,
  std::function<void(const Json::Value &)> &&callback
)
{
    if (!storage_)
    {
        callback(makeError("server_error"));
        return;
    }

    storage_->validateClient(
      clientId,
      clientSecret,
      [this, code, clientId, redirectUri, codeVerifier, callback = std::move(callback)](
        bool isValid
      ) mutable {
          if (!isValid)
          {
              callback(makeError("invalid_client", "Client authentication failed"));
              return;
          }

          storage_->consumeAuthCode(
            utils::hashToken(code),
            redirectUri,
            [this, callback = std::move(callback), clientId, code, codeVerifier](
              std::optional<OAuth2AuthCode> authCode
            ) {
                if (!authCode)
                {
                    callback(makeError("invalid_grant", "Invalid authorization code"));
                    return;
                }
                if (authCode->clientId != clientId)
                {
                    callback(makeError("invalid_client", "Client ID mismatch"));
                    return;
                }

                if (!authCode->codeChallenge.empty())
                {
                    if (
                      codeVerifier.empty() ||
                      !validatePkceCodeVerifier(
                        codeVerifier, authCode->codeChallenge, authCode->codeChallengeMethod
                      )
                    )
                    {
                        callback(makeError("invalid_grant", "PKCE validation failed"));
                        return;
                    }
                }

                auto now = std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::system_clock::now().time_since_epoch()
                )
                             .count();

                if (now > authCode->expiresAt)
                {
                    callback(makeError("invalid_grant", "Code expired"));
                    return;
                }

                storage_->getUserRoles(
                  authCode->userId,
                  [this, callback, authCode, now](std::vector<std::string> roles) {
                      Json::Value rolesJson(Json::arrayValue);
                      for (const auto &r : roles)
                          rolesJson.append(r);

                      auto tokenStr = utils::generateSecureToken();
                      OAuth2AccessToken token;
                      token.token = utils::hashToken(tokenStr);
                      token.clientId = authCode->clientId;
                      token.userId = authCode->userId;
                      token.scope = authCode->scope;
                      token.expiresAt = now + accessTokenTtl_;

                      auto refreshTokenStr = utils::generateSecureToken();
                      auto familyId = utils::generateSecureToken(16);  // New family
                      OAuth2RefreshToken refreshToken;
                      refreshToken.token = utils::hashToken(refreshTokenStr);
                      refreshToken.accessToken = token.token;
                      refreshToken.clientId = authCode->clientId;
                      refreshToken.userId = authCode->userId;
                      refreshToken.scope = authCode->scope;
                      refreshToken.expiresAt = now + refreshTokenTtl_;
                      refreshToken.familyId = familyId;

                      storage_->saveTokenPair(
                        token, refreshToken,
                        [this, callback, tokenStr, refreshTokenStr, rolesJson, authCode, now]() {
                            Json::Value json;
                            json["access_token"] = tokenStr;
                            json["token_type"] = "Bearer";
                            json["expires_in"] = (Json::Int64)(3600);
                            json["refresh_token"] = refreshTokenStr;
                            json["roles"] = rolesJson;

                            // Issue id_token if scope includes "openid"
                            if (jwkManager_ && jwkManager_->isInitialized() &&
                                authCode->scope.find("openid") != std::string::npos)
                            {
                                auto customConfig = drogon::app().getCustomConfig();
                                std::string issuer = "http://localhost:5555";
                                if (customConfig.isMember("metadata") &&
                                    customConfig["metadata"].isMember("issuer"))
                                {
                                    issuer = customConfig["metadata"]["issuer"].asString();
                                }

                                Json::Value idTokenClaims;
                                idTokenClaims["iss"] = issuer;
                                idTokenClaims["sub"] = authCode->userId;
                                idTokenClaims["aud"] = authCode->clientId;
                                idTokenClaims["iat"] = (Json::Int64)now;
                                idTokenClaims["exp"] = (Json::Int64)(now + 3600);

                                std::string idToken = jwkManager_->signJwt(idTokenClaims);
                                if (!idToken.empty())
                                {
                                    json["id_token"] = idToken;
                                }
                            }

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

void TokenService::refreshAccessToken(
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

    auto hashedRt = utils::hashToken(refreshTokenStr);

    // Atomic CAS: revoke the old RT and get its data
    // If it's already revoked, this means reuse -> cascade revoke family
    storage_->atomicRevokeRefreshToken(
      hashedRt,
      [this, callback = std::move(callback), clientId, hashedRt](
        std::optional<OAuth2RefreshToken> storedRt
      ) mutable {
          if (!storedRt)
          {
              // Token not found OR already revoked -> possible reuse attack
              // Try to get the token to check if it exists but is revoked
              storage_->getRefreshToken(
                hashedRt,
                [this, callback = std::move(callback)](
                  std::optional<OAuth2RefreshToken> maybeRevoked
                ) {
                    if (maybeRevoked && maybeRevoked->revoked && !maybeRevoked->familyId.empty())
                    {
                        // REUSE DETECTED! Cascade revoke the entire family
                        LOG_WARN << "[SECURITY] Refresh token reuse detected! "
                                 << "Revoking token family: " << maybeRevoked->familyId;
                        storage_->revokeTokenFamily(
                          maybeRevoked->familyId,
                          [callback]() {
                              callback(makeError("invalid_grant", "Token reuse detected"));
                          }
                        );
                    }
                    else
                    {
                        callback(makeError("invalid_grant", "Invalid or revoked refresh token"));
                    }
                }
              );
              return;
          }

          // Normal path: token was valid and is now revoked
          if (storedRt->clientId != clientId)
          {
              callback(makeError("invalid_grant", "Client mismatch"));
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

          // Issue new token pair, inheriting the family
          auto newTokenStr = utils::generateSecureToken();
          OAuth2AccessToken token;
          token.token = utils::hashToken(newTokenStr);
          token.clientId = storedRt->clientId;
          token.userId = storedRt->userId;
          token.scope = storedRt->scope;
          token.expiresAt = now + accessTokenTtl_;

          auto newRefreshTokenStr = utils::generateSecureToken();
          OAuth2RefreshToken newRt;
          newRt.token = utils::hashToken(newRefreshTokenStr);
          newRt.accessToken = token.token;
          newRt.clientId = storedRt->clientId;
          newRt.userId = storedRt->userId;
          newRt.scope = storedRt->scope;
          newRt.expiresAt = now + refreshTokenTtl_;
          newRt.familyId = storedRt->familyId;  // Inherit family

          storage_->saveTokenPair(
            token, newRt,
            [callback, newTokenStr, newRefreshTokenStr]() {
                Json::Value json;
                json["access_token"] = newTokenStr;
                json["token_type"] = "Bearer";
                json["expires_in"] = (Json::Int64)3600;
                json["refresh_token"] = newRefreshTokenStr;
                callback(json);
            }
          );
      }
    );
}

void TokenService::validateAccessToken(
  const std::string &token,
  std::function<void(std::shared_ptr<OAuth2AccessToken>)> &&callback
)
{
    if (!storage_)
    {
        callback(nullptr);
        return;
    }

    auto hashedToken = utils::hashToken(token);
    storage_->getAccessToken(hashedToken, [callback](std::optional<OAuth2AccessToken> t) {
        if (!t || t->revoked)
        {
            callback(nullptr);
            return;
        }

        auto now = std::chrono::duration_cast<std::chrono::seconds>(
                     std::chrono::system_clock::now().time_since_epoch()
        )
                     .count();

        if (now > t->expiresAt)
        {
            callback(nullptr);
            return;
        }

        callback(std::make_shared<OAuth2AccessToken>(*t));
    });
}

void TokenService::introspectToken(
  const std::string &token,
  std::function<void(std::optional<TokenIntrospection>)> &&callback
)
{
    if (!storage_)
    {
        callback(std::nullopt);
        return;
    }
    auto hashedToken = utils::hashToken(token);
    storage_->introspectToken(hashedToken, std::move(callback));
}

void TokenService::revokeAccessToken(
  const std::string &token,
  const std::string &revokedBy,
  std::function<void()> &&callback
)
{
    if (!storage_)
    {
        if (callback)
            callback();
        return;
    }
    auto hashedToken = utils::hashToken(token);
    storage_->revokeAccessToken(hashedToken, revokedBy, std::move(callback));
}

bool TokenService::validatePkceCodeVerifier(
  const std::string &codeVerifier,
  const std::string &codeChallenge,
  const std::string &codeChallengeMethod
)
{
    std::string method = codeChallengeMethod.empty() ? "plain" : codeChallengeMethod;

    if (method == "plain")
    {
        return codeVerifier == codeChallenge;
    }
    else if (method == "S256")
    {
        return generateSha256Hash(codeVerifier) == codeChallenge;
    }
    return false;
}

std::string TokenService::generateSha256Hash(const std::string &input)
{
    std::string hash = drogon::utils::getSha256(input);
    std::string base64Url = drogon::utils::base64Encode(
      reinterpret_cast<const unsigned char *>(hash.c_str()), hash.length()
    );

    for (char &c : base64Url)
    {
        if (c == '+')
            c = '-';
        else if (c == '/')
            c = '_';
    }
    while (!base64Url.empty() && base64Url.back() == '=')
    {
        base64Url.pop_back();
    }
    return base64Url;
}

}  // namespace oauth2
