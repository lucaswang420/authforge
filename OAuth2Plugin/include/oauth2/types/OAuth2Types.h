#pragma once

#include <string>
#include <stdexcept>

namespace oauth2
{

/**
 * @brief OAuth2 Client Types per RFC 6749
 *
 * CONFIDENTIAL: Clients capable of maintaining the confidentiality of their
 * credentials (e.g., web apps running on a server, backend services)
 *
 * PUBLIC: Clients incapable of maintaining the confidentiality of their
 * credentials (e.g., SPA apps, mobile apps, desktop apps)
 */
enum class ClientType
{
    PUBLIC,
    CONFIDENTIAL
};

/**
 * @brief OAuth2 Grant Types per RFC 6749
 */
enum class GrantType
{
    AUTHORIZATION_CODE,
    REFRESH_TOKEN,
    CLIENT_CREDENTIALS,
    IMPLICIT
};

/**
 * @brief OAuth2 Error Codes per RFC 6749 Section 5.2
 */
enum class OAuth2Error
{
    INVALID_CLIENT,          // Client authentication failed
    INVALID_GRANT,           // The authorization code or refresh token is invalid
    UNAUTHORIZED_CLIENT,     // Client is not authorized to use this grant type
    UNSUPPORTED_GRANT_TYPE,  // Grant type not supported
    INVALID_REQUEST,         // Missing or invalid parameter
    ACCESS_DENIED,           // Resource owner denied access
    UNSUPPORTED_RESPONSE_TYPE,
    INVALID_SCOPE,
    SERVER_ERROR,
    TEMPORARILY_UNAVAILABLE
};

/**
 * @brief Convert ClientType enum to string
 */
inline std::string clientTypeToString(ClientType type)
{
    switch (type)
    {
        case ClientType::PUBLIC:
            return "PUBLIC";
        case ClientType::CONFIDENTIAL:
            return "CONFIDENTIAL";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief Convert string to ClientType enum
 * @throws std::invalid_argument if string is invalid
 */
inline ClientType stringToClientType(const std::string &str)
{
    if (str == "PUBLIC")
    {
        return ClientType::PUBLIC;
    }
    else if (str == "CONFIDENTIAL")
    {
        return ClientType::CONFIDENTIAL;
    }
    throw std::invalid_argument("Invalid ClientType string: " + str);
}

/**
 * @brief Convert OAuth2Error enum to string (error code)
 */
inline std::string oauth2ErrorToString(OAuth2Error error)
{
    switch (error)
    {
        case OAuth2Error::INVALID_CLIENT:
            return "invalid_client";
        case OAuth2Error::INVALID_GRANT:
            return "invalid_grant";
        case OAuth2Error::UNAUTHORIZED_CLIENT:
            return "unauthorized_client";
        case OAuth2Error::UNSUPPORTED_GRANT_TYPE:
            return "unsupported_grant_type";
        case OAuth2Error::INVALID_REQUEST:
            return "invalid_request";
        case OAuth2Error::ACCESS_DENIED:
            return "access_denied";
        case OAuth2Error::UNSUPPORTED_RESPONSE_TYPE:
            return "unsupported_response_type";
        case OAuth2Error::INVALID_SCOPE:
            return "invalid_scope";
        case OAuth2Error::SERVER_ERROR:
            return "server_error";
        case OAuth2Error::TEMPORARILY_UNAVAILABLE:
            return "temporarily_unavailable";
        default:
            return "unknown_error";
    }
}

/**
 * @brief Get HTTP status code for OAuth2 error per RFC 6749
 *
 * 401 Unauthorized: Authentication errors (invalid_client, unauthorized_client)
 * 400 Bad Request: All other OAuth2 errors
 *
 * @param error OAuth2 error type
 * @return HTTP status code
 */
inline int getHttpStatusCode(OAuth2Error error)
{
    switch (error)
    {
        case OAuth2Error::INVALID_CLIENT:
        case OAuth2Error::UNAUTHORIZED_CLIENT:
            return 401;  // Unauthorized
        default:
            return 400;  // Bad Request
    }
}

/**
 * @brief Get HTTP status code from OAuth2 error string
 * @param errorCode Error code string (e.g., "invalid_client")
 * @return HTTP status code
 */
inline int getHttpStatusCode(const std::string &errorCode)
{
    if (errorCode == "invalid_client" || errorCode == "unauthorized_client")
    {
        return 401;  // Unauthorized
    }
    return 400;  // Bad Request
}

}  // namespace oauth2
