// Forwarding shim header for OAuth2 plugin public API.
//
// This header was relocated to <oauth2/config/ConfigManager.h> in spec
// repo-structure-refactor phase P1 (see design.md ?4.1, ?6.6.4). It
// remains here only so existing #include <oauth2/ConfigManager.h> callers
// keep compiling during P1..P10. P11 removes the shim entirely.
//
// _Design: ?2.8 P1, ?6.6.4_
// _Requirements: 2.5, 14.3_

#pragma once

#include <oauth2/config/ConfigManager.h>