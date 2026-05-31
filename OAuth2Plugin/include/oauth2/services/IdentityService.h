#pragma once

#include <oauth2/storage/IOAuth2Storage.h>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <optional>

namespace oauth2
{

// Defect 1.9 fix (async-chain dangling `this`): IdentityService inherits
// std::enable_shared_from_this so its asynchronous storage chains
// (ensureSubjectMapping / handleFirstTimeLogin / validateUserRolesForScopes)
// can capture `auto self = shared_from_this();` at the outermost async call and
// thread that same `self` through every nested continuation, keeping the
// service alive until the in-flight callback completes (no use-after-free on
// teardown). The service is always created via std::make_shared
// (OAuth2Plugin::initAndStart), so shared_from_this() is valid at runtime. The
// synchronous pure-function call site (scopeRequiresAdminRole via a
// stack-constructed IdentityService(nullptr) temporary) never calls
// shared_from_this(), so it keeps working without shared ownership.
class IdentityService : public std::enable_shared_from_this<IdentityService>
{
  public:
    // Shared ownership of the storage (defect 1.3 fix): holds a
    // std::shared_ptr<IOAuth2Storage> instead of a raw pointer so the storage
    // lifetime covers every user. A null shared_ptr is accepted for the
    // pure-function call sites (e.g. OAuth2Plugin::scopeRequiresAdminRole via
    // IdentityService(nullptr)).
    explicit IdentityService(std::shared_ptr<IOAuth2Storage> storage);

    void getUserRoles(
      const std::string &userId,
      std::function<void(std::vector<std::string>)> &&callback
    );

    void ensureSubjectMapping(
      const std::string &subject,
      const std::string &username,
      int32_t internalUserId,
      std::function<void()> &&callback
    );

    void handleFirstTimeLogin(
      const std::string &subject,
      const std::string &provider,
      std::function<void(int32_t)> &&callback
    );

    void getInternalUserId(
      const std::string &subject,
      std::function<void(std::optional<int32_t>)> &&callback
    );

    void hasUserConsent(
      int32_t internalUserId,
      const std::string &clientId,
      const std::string &scope,
      std::function<void(bool)> &&callback
    );

    void saveUserConsent(
      int32_t internalUserId,
      const std::string &clientId,
      const std::string &scope,
      std::function<void(bool)> &&callback
    );

    void validateUserRolesForScopes(
      const std::string &userId,
      const std::vector<std::string> &scopes,
      std::function<void(bool, std::string)> &&callback
    );

    bool scopeRequiresAdminRole(const std::string &scope);

  private:
    std::shared_ptr<IOAuth2Storage> storage_;
};

}  // namespace oauth2
