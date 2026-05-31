#pragma once

#include <oauth2/storage/IOAuth2Storage.h>
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace oauth2
{

// Defect 1.9 fix (async-chain dangling `this`, same-origin as TokenService /
// IdentityService): ClientService inherits std::enable_shared_from_this so any
// asynchronous storage chain can capture `auto self = shared_from_this();` and
// thread it through nested continuations, keeping the service alive until the
// in-flight callback completes. The service is always created via
// std::make_shared (OAuth2Plugin::initAndStart), so shared_from_this() is valid
// at runtime.
class ClientService : public std::enable_shared_from_this<ClientService>
{
  public:
    // Shared ownership of the storage (defect 1.3 fix): holds a
    // std::shared_ptr<IOAuth2Storage> instead of a raw pointer so the storage
    // lifetime covers every user. A null shared_ptr is accepted.
    explicit ClientService(std::shared_ptr<IOAuth2Storage> storage);

    void validateClient(
      const std::string &clientId,
      const std::string &clientSecret,
      std::function<void(bool)> &&callback
    );

    void validateRedirectUri(
      const std::string &clientId,
      const std::string &redirectUri,
      std::function<void(bool)> &&callback
    );

    void validateClientScopes(
      const std::string &clientId,
      const std::vector<std::string> &requestedScopes,
      std::function<void(bool, std::string)> &&callback
    );

  private:
    std::shared_ptr<IOAuth2Storage> storage_;
};

}  // namespace oauth2
