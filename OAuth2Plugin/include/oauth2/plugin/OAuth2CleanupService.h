#pragma once

#include <drogon/drogon.h>
#include <memory>
#include <oauth2/storage/IOAuth2Storage.h>

namespace oauth2
{

class OAuth2CleanupService : public std::enable_shared_from_this<OAuth2CleanupService>
{
  public:
    // Shared ownership of the storage (defect 1.3 fix): holds a
    // std::shared_ptr<IOAuth2Storage> instead of a raw pointer so the storage
    // lifetime covers the cleanup service (and any in-flight cleanup callback).
    OAuth2CleanupService(std::shared_ptr<IOAuth2Storage> storage);
    ~OAuth2CleanupService();

    void start(double intervalSeconds);
    void stop();

  private:
    std::shared_ptr<IOAuth2Storage> storage_;
    uint64_t timerId_ = 0;
    bool running_ = false;
    bool stopped_ = false;  // Track if stop() has been called
    double interval_ = 3600;

    void runCleanup();
};

}  // namespace oauth2
