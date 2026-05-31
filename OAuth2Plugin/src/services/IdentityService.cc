#include <oauth2/services/IdentityService.h>
#include <oauth2/utils/SubjectGenerator.h>
#include <drogon/drogon.h>

namespace oauth2
{

IdentityService::IdentityService(std::shared_ptr<IOAuth2Storage> storage)
    : storage_(std::move(storage))
{
}

void IdentityService::getUserRoles(
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

void IdentityService::ensureSubjectMapping(
  const std::string &subject,
  const std::string &username,
  int32_t internalUserId,
  std::function<void()> &&callback
)
{
    if (!storage_)
    {
        callback();
        return;
    }

    auto [provider, sub] = utils::SubjectGenerator::parse(subject);

    // Defect 1.9 fix: capture `self` (shared owner) at the OUTERMOST async call
    // and thread the SAME `self` through the nested continuation, so the
    // service stays alive until the in-flight callback completes. `this` is
    // kept for unchanged member access (`storage_`); `self` guarantees `this`
    // never dangles.
    auto self = shared_from_this();
    storage_->getInternalUserId(
      sub,
      provider,
      [self, this, sub, provider, internalUserId, callback = std::move(callback)](
        auto existingUserId
      ) {
          if (existingUserId)
          {
              callback();
              return;
          }

          storage_->createSubjectMapping(
            sub, internalUserId, provider, [callback = std::move(callback)](bool success) {
                callback();
            }
          );
      }
    );
}

void IdentityService::handleFirstTimeLogin(
  const std::string &subject,
  const std::string &provider,
  std::function<void(int32_t)> &&callback
)
{
    if (!storage_)
    {
        callback(0);
        return;
    }

    auto [prov, sub] = utils::SubjectGenerator::parse(subject);

    // Create a real user in the database via storage interface
    //
    // Defect 1.9 fix: capture `self` (shared owner) at the OUTERMOST async call
    // and thread the SAME `self` through the nested continuation, so the
    // service stays alive until the in-flight callback completes. `this` is
    // kept for unchanged member access (`storage_`); `self` guarantees `this`
    // never dangles.
    auto self = shared_from_this();
    storage_->createUserForExternalLogin(
      sub,
      prov,
      [self, this, sub, prov, callback = std::move(callback)](std::optional<int32_t> newUserId) {
          if (!newUserId || *newUserId == 0)
          {
              LOG_ERROR << "Failed to create user for external login: " << prov << ":" << sub;
              callback(0);
              return;
          }
          // Create subject mapping
          storage_->createSubjectMapping(
            sub,
            *newUserId,
            prov,
            [newUserId = *newUserId, callback = std::move(callback)](bool success) {
                callback(success ? newUserId : 0);
            }
          );
      }
    );
}

void IdentityService::getInternalUserId(
  const std::string &subject,
  std::function<void(std::optional<int32_t>)> &&callback
)
{
    if (!storage_)
    {
        callback(std::nullopt);
        return;
    }

    auto [provider, sub] = utils::SubjectGenerator::parse(subject);

    storage_->getInternalUserId(sub, provider, std::move(callback));
}

void IdentityService::hasUserConsent(
  int32_t internalUserId,
  const std::string &clientId,
  const std::string &scope,
  std::function<void(bool)> &&callback
)
{
    if (!storage_)
    {
        callback(false);
        return;
    }
    storage_->hasUserConsent(internalUserId, clientId, scope, std::move(callback));
}

void IdentityService::saveUserConsent(
  int32_t internalUserId,
  const std::string &clientId,
  const std::string &scope,
  std::function<void(bool)> &&callback
)
{
    if (!storage_)
    {
        callback(false);
        return;
    }
    storage_->saveUserConsent(internalUserId, clientId, scope, std::move(callback));
}

void IdentityService::validateUserRolesForScopes(
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

    std::vector<std::string> adminScopes;
    for (const auto &scope : scopes)
    {
        if (scopeRequiresAdminRole(scope))
        {
            adminScopes.push_back(scope);
        }
    }

    if (adminScopes.empty())
    {
        callback(true, "");
        return;
    }

    getUserRoles(
      userId,
      [callback = std::move(callback), adminScopes](std::vector<std::string> userRoles) mutable {
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
              callback(false, "Admin role required for requested scopes");
              return;
          }

          callback(true, "");
      }
    );
}

bool IdentityService::scopeRequiresAdminRole(const std::string &scope)
{
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

}  // namespace oauth2
