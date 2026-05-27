// Forwarding shim — ValidationFilter renamed to RequestValidationFilter in P3/P4.
// Removed in P11.
#pragma once
#include <oauth2/filters/RequestValidationFilter.h>

using ValidationFilter [[deprecated("Use oauth2::filters::RequestValidationFilter")]] = oauth2::filters::RequestValidationFilter;