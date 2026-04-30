#pragma once

#include "ErrorTypes.h"
#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <functional>

namespace common::error
{
using drogon::orm::DrogonDbException;

class ErrorHandler
{
  public:
    // Handle specific exception types
    static Error handleDbException(const DrogonDbException &e);
    static Error handleValidationError(const std::string &field,
                                       const std::string &reason);

    // Utility functions
    static std::string generateRequestId();
    static void logError(const Error &error, const std::string &context = "");
};

}  // namespace common::error
