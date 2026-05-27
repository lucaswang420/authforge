#include <oauth2/filters/AuthorizationFilter.h>
#include <oauth2/plugin/OAuth2Plugin.h>
#include <drogon/drogon.h>


namespace oauth2::filters {

using namespace drogon;

AuthorizationFilter::AuthorizationFilter()
{
}

void AuthorizationFilter::loadConfig()
{
    if (initialized_)
        return;
    auto config = app().getCustomConfig();
    if (config.isMember("rbac_rules") && config["rbac_rules"].isObject())
    {
        auto rules = config["rbac_rules"];
        for (auto it = rules.begin(); it != rules.end(); ++it)
        {
            std::string pattern = it.name();
            RbacRule rule;
            rule.pathPattern = std::regex(pattern);

            auto rolesJson = *it;
            if (rolesJson.isArray())
            {
                for (const auto &role : rolesJson)
                {
                    rule.allowedRoles.push_back(role.asString());
                }
            }
            rules_.push_back(rule);
            LOG_DEBUG << "RBAC Rule Loaded: " << pattern << " -> " << rule.allowedRoles.size()
                      << " roles";
        }
    }
    // Load public paths (no auth required)
    if (config.isMember("public_paths") && config["public_paths"].isArray())
    {
        for (const auto &path : config["public_paths"])
        {
            publicPaths_.push_back(std::regex(path.asString()));
            LOG_DEBUG << "Public path loaded: " << path.asString();
        }
    }
    initialized_ = true;
}

void AuthorizationFilter::doFilter(
  const HttpRequestPtr &req,
  FilterCallback &&fcb,
  FilterChainCallback &&fccb
)
{
    loadConfig();

    // 1. Extract Token
    std::string token;
    auto authHeader = req->getHeader("Authorization");
    if (!authHeader.empty() && authHeader.find("Bearer ") == 0)
    {
        token = authHeader.substr(7);
    }
    else
    {
        token = req->getParameter("access_token");
    }

    if (token.empty())
    {
        Json::Value error;
        error["error"] = "unauthorized";
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k401Unauthorized);
        fcb(resp);  // Return response -> Use fcb
        return;
    }

    // 2. Validate Token
    auto plugin = app().getPlugin<OAuth2Plugin>();
    if (!plugin)
    {
        LOG_ERROR << "OAuth2Plugin not found!";
        fcb(
          HttpResponse::newHttpResponse(
            k500InternalServerError,
            ContentType::CT_TEXT_PLAIN
          )
        );  // Return response -> Use fcb
        return;
    }

    // Wrap callbacks to avoid move/copy issues in nested lambdas
    // FilterCallback (Arg 2) = Return Response (Stop/Deny)
    // FilterChainCallback (Arg 3) = Continue (Pass)
    auto denyCbPtr = std::make_shared<FilterCallback>(std::move(fcb));
    auto nextCbPtr = std::make_shared<FilterChainCallback>(std::move(fccb));

    plugin->validateAccessToken(
      token,
      [this, req, denyCbPtr, nextCbPtr, plugin](
        std::shared_ptr<OAuth2Plugin::AccessToken> at
      ) mutable {
          if (!at)
          {
              Json::Value error;
              error["error"] = "invalid_token";
              auto resp = HttpResponse::newHttpJsonResponse(error);
              resp->setStatusCode(k401Unauthorized);
              (*denyCbPtr)(resp);
              return;
          }

          // 3. Get User Roles
          plugin->getUserRoles(
            at->userId, [this, req, denyCbPtr, nextCbPtr](std::vector<std::string> roles) mutable {
                // 4. Check Access
                if (checkAccess(roles, req->path()))
                {
                    (*nextCbPtr)();  // ALLOW -> Continue
                }
                else
                {
                    Json::Value error;
                    error["error"] = "forbidden";
                    error["message"] = "Insufficient permissions";
                    auto resp = HttpResponse::newHttpJsonResponse(error);
                    resp->setStatusCode(k403Forbidden);
                    (*denyCbPtr)(resp);  // DENY -> Return 403
                }
            }
          );
      }
    );
}

bool AuthorizationFilter::checkAccess(
  const std::vector<std::string> &userRoles,
  const std::string &path
)
{
    // Check public paths first (no auth required)
    for (const auto &publicPath : publicPaths_)
    {
        if (std::regex_match(path, publicPath))
            return true;
    }

    // Check RBAC rules
    for (const auto &rule : rules_)
    {
        if (std::regex_match(path, rule.pathPattern))
        {
            // Rule matched - check if user has any of the allowed roles
            for (const auto &allowed : rule.allowedRoles)
            {
                for (const auto &userRole : userRoles)
                {
                    if (userRole == allowed)
                        return true;
                }
            }
            // Rule matched but roles didn't -> DENY
            return false;
        }
    }

    // DEFAULT DENY: no rule matched and not in public_paths
    return false;
}

}  // namespace oauth2::filters
