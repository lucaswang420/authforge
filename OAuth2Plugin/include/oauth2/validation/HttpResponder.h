#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <vector>
#include <string>
#include <functional>
#include <json/json.h>

namespace oauth2::validation
{

class HttpResponder
{
  public:
    static drogon::HttpResponsePtr buildErrorResponse(const std::vector<std::string> &errors);
    static void respondWithError(
      const std::string &field, const std::string &reason,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    static void respondWithErrors(
      const std::vector<std::string> &errors,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    static bool respondIfErrors(
      const std::vector<std::string> &errors,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  private:
    static Json::Value buildErrorJson(const std::vector<std::string> &errors);
    static bool detailedErrorsAllowed();
};

}  // namespace oauth2::validation