#pragma once

#include "IOAuth2Storage.h"
#include <string>
#include <vector>
#include <functional>
#include <optional>

namespace oauth2
{

class IdentityService
{
  public:
    explicit IdentityService(IOAuth2Storage *storage);

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
    IOAuth2Storage *storage_;
};

} // namespace oauth2
