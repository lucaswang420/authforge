#include "WeChatController.h"
#include <drogon/HttpClient.h>
#include "../common/documentation/OpenApiGenerator.h"

// TODO: REPLACE WITH YOUR REAL CREDENTIALS
const std::string WECHAT_APPID_KEY = "appid";
const std::string WECHAT_SECRET_KEY = "secret";

std::string getWeChatConfig(const std::string &key)
{
    auto config = drogon::app().getCustomConfig();
    if (config.isMember("external_auth") &&
        config["external_auth"].isMember("wechat"))
    {
        return config["external_auth"]["wechat"].get(key, "").asString();
    }
    return "";
}

// Register OpenAPI documentation (executed once at startup)
namespace
{
struct WeChatControllerDocs
{
    WeChatControllerDocs()
    {
        using namespace common::documentation;

        Json::Value successExample;
        successExample["openid"] = "oXXXXXXXXXXXXXXXXXXXXXXXXXX";
        successExample["nickname"] = "WeChat User";
        successExample["headimgurl"] = "https://thirdwx.qlogo.cn/...";

        Json::Value errorExample;
        errorExample["error"] = "Missing code parameter";

        OpenApiGenerator::addEndpoint(
            {.path = "/api/wechat/login",
             .method = "POST",
             .summary = "WeChat OAuth2 Login",
             .description =
                 "Exchange WeChat authorization code for user information. "
                 "This endpoint handles the server-side OAuth2 flow with "
                 "WeChat Open Platform.",
             .tags = {"External Auth", "WeChat"},
             .parameters =
                 {{"code",
                   "Authorization code from WeChat OAuth2 callback (required)",
                   ParameterType::STRING,
                   ParameterLocation::QUERY,
                   true}},
             .responses = {{200, "WeChat user info retrieved successfully"},
                           {400, "Invalid request (missing or invalid code)"},
                           {502, "Failed to contact WeChat API"}},
             .responseExamples = {{200, successExample}, {400, errorExample}},
             .requiresAuth = false});
    }
};

WeChatControllerDocs docs_;
}  // namespace

void WeChatController::login(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    // Handle OPTIONS for CORS
    if (req->method() == Options)
    {
        auto resp = HttpResponse::newHttpResponse();
        callback(resp);
        return;
    }

    // Try to get code from POST body first, then fallback to query parameter
    std::string code;

    // Check Content-Type and parse accordingly
    auto contentType = req->getHeader("Content-Type");
    if (contentType.find("application/x-www-form-urlencoded") !=
        std::string::npos)
    {
        // Parse from POST body
        auto body = req->getBody();
        // Simple parsing for "code=xxx" format
        size_t codePos = body.find("code=");
        if (codePos != std::string::npos)
        {
            size_t valueStart = codePos + 5;  // "code=" length
            size_t valueEnd = body.find("&", valueStart);
            if (valueEnd == std::string::npos)
            {
                valueEnd = body.length();
            }
            code = body.substr(valueStart, valueEnd - valueStart);
        }
    }

    // Fallback to query parameter
    if (code.empty())
    {
        code = req->getParameter("code");
    }

    if (code.empty())
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Missing code parameter");
        callback(resp);
        return;
    }

    // 1. Exchange Code for Access Token
    // API:
    // https://api.weixin.qq.com/sns/oauth2/access_token?appid=APPID&secret=SECRET&code=CODE&grant_type=authorization_code
    auto client = HttpClient::newHttpClient("https://api.weixin.qq.com");
    auto request = HttpRequest::newHttpRequest();
    std::string path =
        "/sns/oauth2/access_token?appid=" + getWeChatConfig(WECHAT_APPID_KEY) +
        "&secret=" + getWeChatConfig(WECHAT_SECRET_KEY) + "&code=" + code +
        "&grant_type=authorization_code";
    request->setPath(path);

    // Keep the main callback alive
    auto callbackPtr =
        std::make_shared<std::function<void(const HttpResponsePtr &)>>(
            std::move(callback));

    client->sendRequest(
        request,
        [callbackPtr, client](ReqResult result,
                              const HttpResponsePtr &response) {
            if (result != ReqResult::Ok || !response ||
                response->getStatusCode() != k200OK)
            {
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k502BadGateway);
                resp->setBody("Failed to contact WeChat API");
                (*callbackPtr)(resp);
                return;
            }

            auto json = *response->getJsonObject();
            if (json.isMember("errcode") && json["errcode"].asInt() != 0)
            {
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k400BadRequest);
                resp->setBody("WeChat Error: " + json["errmsg"].asString());
                (*callbackPtr)(resp);
                return;
            }

            std::string accessToken = json["access_token"].asString();
            std::string openid = json["openid"].asString();

            // 2. Fetch User Info
            // API:
            // https://api.weixin.qq.com/sns/userinfo?access_token=ACCESS_TOKEN&openid=OPENID
            auto client2 =
                HttpClient::newHttpClient("https://api.weixin.qq.com");
            auto req2 = HttpRequest::newHttpRequest();
            req2->setPath("/sns/userinfo?access_token=" + accessToken +
                          "&openid=" + openid);

            client2->sendRequest(
                req2,
                [callbackPtr](ReqResult res2, const HttpResponsePtr &resp2) {
                    if (res2 != ReqResult::Ok || !resp2)
                    {
                        auto errResp = HttpResponse::newHttpResponse();
                        errResp->setStatusCode(k502BadGateway);
                        errResp->setBody("Failed to fetch WeChat UserInfo");
                        (*callbackPtr)(errResp);
                        return;
                    }

                    // Filter response to only include necessary fields
                    // (security best practice)
                    auto wechatData = resp2->getJsonObject();
                    Json::Value filteredJson;
                    filteredJson["openid"] =
                        (*wechatData).get("openid", "").asString();
                    filteredJson["nickname"] =
                        (*wechatData).get("nickname", "").asString();
                    filteredJson["headimgurl"] =
                        (*wechatData).get("headimgurl", "").asString();
                    filteredJson["sex"] = (*wechatData).get("sex", 0).asInt();
                    filteredJson["city"] =
                        (*wechatData).get("city", "").asString();
                    filteredJson["province"] =
                        (*wechatData).get("province", "").asString();
                    filteredJson["country"] =
                        (*wechatData).get("country", "").asString();

                    auto finalResp =
                        HttpResponse::newHttpJsonResponse(filteredJson);
                    (*callbackPtr)(finalResp);
                });
        });
}
