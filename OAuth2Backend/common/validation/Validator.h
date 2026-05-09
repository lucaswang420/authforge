#pragma once

#include "ValidationRules.h"
#include <vector>
#include <utility>

namespace common::validation
{

class Validator
{
  public:
    // Basic validation methods
    static ValidationResult notEmpty(const std::string &value, const std::string &fieldName);
    static ValidationResult length(
      const std::string &value,
      const std::string &fieldName,
      size_t minLen,
      size_t maxLen
    );
    static ValidationResult regex(
      const std::string &value,
      const std::string &fieldName,
      const std::string &pattern
    );
    static ValidationResult numericRange(
      int value,
      const std::string &fieldName,
      int minVal,
      int maxVal
    );

    // OAuth2-specific validation
    static ValidationResult validateClientId(const std::string &clientId);
    static ValidationResult validateClientSecret(const std::string &secret);
    static ValidationResult validateRedirectUri(const std::string &uri);
    static ValidationResult validateScope(const std::string &scope);
    static ValidationResult validateResponseType(const std::string &type);
    static ValidationResult validateGrantType(const std::string &type);
    static ValidationResult validateToken(const std::string &token);

    // Batch validation
    static std::vector<ValidationResult> validateAll(
      const std::vector<std::pair<std::string, std::string>> &fieldsAndValues,
      const std::vector<ValidationRuleType> &rules
    );
};

}  // namespace common::validation
