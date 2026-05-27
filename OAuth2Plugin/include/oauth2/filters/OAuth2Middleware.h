// Forwarding shim — OAuth2Middleware renamed to OAuth2AuthFilter in P4.
// See design §6.2.6. Removed in P11.
#pragma once
#include <oauth2/filters/OAuth2AuthFilter.h>

// Deprecated typedef for source-level compat (P4..P10).
namespace oauth2::filters {
    using OAuth2Middleware [[deprecated("Use OAuth2AuthFilter")]] = OAuth2AuthFilter;
}