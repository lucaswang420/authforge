#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

/**
 * @brief Device Authorization Grant Controller (RFC 8628)
 *
 * Implements the Device Authorization Grant flow for devices with limited input
 * capabilities (TVs, CLI tools, IoT devices). The device displays a user code
 * that the user enters on another device to authorize access.
 *
 * Endpoints:
 * - POST /oauth2/device_authorization — Device requests authorization
 * - POST /oauth2/device/approve — User approves the device (admin-only)
 */
class DeviceAuthController : public drogon::HttpController<DeviceAuthController>
{
  public:
    METHOD_LIST_BEGIN
    // Device Authorization Request (no auth required)
    ADD_METHOD_TO(DeviceAuthController::deviceAuthorization, "/oauth2/device_authorization", Post);

    // User Approval (admin-only)
    ADD_METHOD_TO(
      DeviceAuthController::approveDevice,
      "/oauth2/device/approve",
      Post,
      "oauth2::filters::AuthorizationFilter"
    );
    METHOD_LIST_END

    void deviceAuthorization(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

    void approveDevice(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback
    );

  private:
    /**
     * @brief Generate a user-friendly 8-character uppercase alphanumeric code
     */
    static std::string generateUserCode();
};
