#pragma once
#include <drogon/HttpController.h>
using namespace drogon;

class GitHubController : public drogon::HttpController<GitHubController>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(GitHubController::login, "/api/github/login", Post, Options);
    METHOD_LIST_END

    void login(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
};
