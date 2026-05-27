#pragma once

#include <drogon/HttpFilter.h>
#include <drogon/drogon.h>
#include <string>
#include <vector>
#include <regex>

using namespace drogon;

namespace oauth2::filters {

class AuthorizationFilter : public HttpFilter<AuthorizationFilter>
{
  public:
    AuthorizationFilter();
    void doFilter(
      const HttpRequestPtr &req,
      FilterCallback &&fcb,
      FilterChainCallback &&fccb
    ) override;

  private:
    // Path regex -> Allowed Roles
    struct RbacRule
    {
        std::regex pathPattern;
        std::vector<std::string> allowedRoles;
    };

    std::vector<RbacRule> rules_;
    std::vector<std::regex> publicPaths_;
    bool initialized_ = false;

    void loadConfig();
    bool checkAccess(const std::vector<std::string> &userRoles, const std::string &path);
};

}  // namespace oauth2::filters
