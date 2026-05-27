// oauth2/validation/legacy.h 鈥?Backward-compatibility shim for P3..P10.
// Maps old common::validation::* names to the new oauth2::validation::* names.
// This file is deleted in P11 (see design 搂6.1.6, tasks.md 12.2).
#pragma once

#include <oauth2/validation/Rules.h>
#include <oauth2/validation/RuleEngine.h>
#include <oauth2/validation/RuleSet.h>
#include <oauth2/validation/HttpResponder.h>

namespace common::validation {
    using Validator = ::oauth2::validation::RuleEngine;
    using ValidatorHelper = ::oauth2::validation::RuleSet;
    using ValidationHelper = ::oauth2::validation::HttpResponder;
    using ValidationRuleConfig = ::oauth2::validation::Rule;
    using ValidationResult = ::oauth2::validation::Result;

    // Enum compat (old SCREAMING_CASE -> new PascalCase)
    using RuleType = ::oauth2::validation::RuleType;
    // Legacy enum values still accessible via common::validation::ValidationRuleType
    using ValidationRuleType = ::oauth2::validation::RuleType;

    // Constants remain accessible (they are in oauth2::validation namespace
    // now; re-export them here for legacy callers).
    using ::oauth2::validation::CLIENT_ID_PATTERN;
    using ::oauth2::validation::CLIENT_ID_MIN_LEN;
    using ::oauth2::validation::CLIENT_ID_MAX_LEN;
    using ::oauth2::validation::REDIRECT_URI_PATTERN;
    using ::oauth2::validation::REDIRECT_URI_MIN_LEN;
    using ::oauth2::validation::REDIRECT_URI_MAX_LEN;
    using ::oauth2::validation::SCOPE_PATTERN;
    using ::oauth2::validation::SCOPE_MIN_LEN;
    using ::oauth2::validation::SCOPE_MAX_LEN;
    using ::oauth2::validation::TOKEN_PATTERN;
    using ::oauth2::validation::TOKEN_MIN_LEN;
    using ::oauth2::validation::RESPONSE_TYPE_PATTERN;
    using ::oauth2::validation::GRANT_TYPE_PATTERN;
    using ::oauth2::validation::USERNAME_PATTERN;
    using ::oauth2::validation::PASSWORD_PATTERN;
}