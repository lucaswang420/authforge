#pragma once

#include <drogon/HttpFilter.h>
#include <oauth2/plugin/OAuth2Plugin.h>

using namespace drogon;

namespace oauth2::filters
{

class OAuth2AuthFilter : public drogon::HttpFilter<OAuth2AuthFilter>
{
  public:
    OAuth2AuthFilter()
    {
    }

    void doFilter(
      const HttpRequestPtr &req,
      FilterCallback &&fcb,
      FilterChainCallback &&fccb
    ) override;
};

}  // namespace oauth2::filters
