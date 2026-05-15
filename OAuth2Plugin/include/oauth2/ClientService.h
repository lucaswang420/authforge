#pragma once

#include "IOAuth2Storage.h"
#include <string>
#include <vector>
#include <functional>

namespace oauth2
{

class ClientService
{
  public:
    explicit ClientService(IOAuth2Storage *storage);

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
    IOAuth2Storage *storage_;
};

}  // namespace oauth2
