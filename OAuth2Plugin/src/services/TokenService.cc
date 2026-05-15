#include <oauth2/TokenService.h>
#include <oauth2/SubjectGenerator.h>
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

    auto code = drogon::utils::getUuid();
    OAuth2AuthCode authCode;
    authCode.code = code;
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
            code,
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

                      auto tokenStr = drogon::utils::getUuid();
                      OAuth2AccessToken token;
                      token.token = tokenStr;
                      token.clientId = authCode->clientId;
                      token.userId = authCode->userId;
                      token.scope = authCode->scope;
                      token.expiresAt = now + accessTokenTtl_;

                      auto refreshTokenStr = drogon::utils::getUuid();
                      OAuth2RefreshToken refreshToken;
                      refreshToken.token = refreshTokenStr;
                      refreshToken.accessToken = tokenStr;
                      refreshToken.clientId = authCode->clientId;
                      refreshToken.userId = authCode->userId;
                      refreshToken.scope = authCode->scope;
                      refreshToken.expiresAt = now + refreshTokenTtl_;

                      storage_->saveAccessToken(
                        token, [this, callback, token, refreshToken, rolesJson]() {
                            storage_->saveRefreshToken(
                              refreshToken, [this, callback, token, refreshToken, rolesJson]() {
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
                                  json["roles"] = rolesJson;
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

    storage_->getRefreshToken(
      refreshTokenStr,
      [this, callback = std::move(callback), clientId](std::optional<OAuth2RefreshToken> storedRt) {
          if (!storedRt || storedRt->clientId != clientId || storedRt->revoked)
          {
              callback(makeError("invalid_grant", "Invalid or revoked refresh token"));
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

          auto newTokenStr = drogon::utils::getUuid();
          OAuth2AccessToken token;
          token.token = newTokenStr;
          token.clientId = storedRt->clientId;
          token.userId = storedRt->userId;
          token.scope = storedRt->scope;
          token.expiresAt = now + accessTokenTtl_;

          auto newRefreshTokenStr = drogon::utils::getUuid();
          OAuth2RefreshToken newRt;
          newRt.token = newRefreshTokenStr;
          newRt.accessToken = newTokenStr;
          newRt.clientId = storedRt->clientId;
          newRt.userId = storedRt->userId;
          newRt.scope = storedRt->scope;
          newRt.expiresAt = now + refreshTokenTtl_;

          storage_->saveAccessToken(
            token, [this, callback, token, newRt, oldRefreshToken = storedRt->token]() {
                storage_
                  ->saveRefreshToken(newRt, [this, callback, token, newRt, oldRefreshToken]() {
                      storage_->revokeRefreshToken(
                        oldRefreshToken, [this, callback, token, newRt](auto...) {
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

    storage_->getAccessToken(token, [callback](std::optional<OAuth2AccessToken> t) {
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
    storage_->introspectToken(token, std::move(callback));
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
    storage_->revokeAccessToken(token, revokedBy, std::move(callback));
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
