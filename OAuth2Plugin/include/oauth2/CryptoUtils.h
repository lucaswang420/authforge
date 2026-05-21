#pragma once

#include <string>
#include <vector>
#include <drogon/utils/Utilities.h>

// Forward declare OpenSSL types only when needed in implementation
// For header-only functions, we use Drogon's wrappers which handle OpenSSL internally

namespace oauth2
{
namespace utils
{

/**
 * @brief Base64URL encode data (RFC 4648 Section 5)
 *
 * Uses Drogon's base64Encode with urlSafe=true parameter.
 * This replaces '+' with '-' and '/' with '_' for URL-safe encoding.
 *
 * @param data Data to encode
 * @return Base64URL encoded string (without padding)
 */
inline std::string base64UrlEncode(const std::string &data)
{
    // Use Drogon's base64Encode with urlSafe=true and unpadded
    return drogon::utils::base64EncodeUnpadded(data, true);
}

/**
 * @brief Base64URL encode binary data
 *
 * @param bytes Binary data to encode
 * @param length Length of binary data
 * @return Base64URL encoded string (without padding)
 */
inline std::string base64UrlEncode(const unsigned char *bytes, size_t length)
{
    return drogon::utils::base64EncodeUnpadded(bytes, length, true);
}

/**
 * @brief Compute SHA-256 hash (RFC 7636 for PKCE)
 *
 * Uses Drogon's getSha256 which returns a hex string, then converts to binary.
 *
 * @param data Input data
 * @return SHA-256 hash as vector of unsigned chars (32 bytes)
 */
inline std::vector<unsigned char> sha256(const std::string &data)
{
    // Drogon getSha256 returns lowercase hex string (64 chars)
    std::string hexStr = drogon::utils::getSha256(data.data(), data.length());
    std::vector<unsigned char> hash;
    hash.reserve(32);
    for (size_t i = 0; i + 1 < hexStr.size(); i += 2)
    {
        unsigned char byte = 0;
        char hi = hexStr[i];
        char lo = hexStr[i + 1];
        byte = static_cast<unsigned char>(
          ((hi >= 'a' ? hi - 'a' + 10 : hi - '0') << 4) | (lo >= 'a' ? lo - 'a' + 10 : lo - '0')
        );
        hash.push_back(byte);
    }
    return hash;
}

/**
 * @brief Compute PKCE code challenge from verifier (RFC 7636)
 *
 * For method "S256": code_challenge = BASE64URL(SHA256(ASCII(code_verifier)))
 * For method "plain": code_challenge = code_verifier
 *
 * @param codeVerifier The code verifier (43-128 characters)
 * @param method Challenge method ("plain" or "S256")
 * @return The code challenge
 */
inline std::string computeCodeChallenge(const std::string &codeVerifier, const std::string &method)
{
    if (method == "S256")
    {
        // SHA-256(code_verifier) -> base64url
        auto hash = sha256(codeVerifier);
        return base64UrlEncode(hash.data(), hash.size());
    }
    else
    {
        // plain method
        return codeVerifier;
    }
}

/**
 * @brief Validate code verifier format (RFC 7636)
 *
 * Code verifier must be 43-128 characters of [A-Za-z0-9-._~]
 *
 * @param codeVerifier The code verifier to validate
 * @return true if valid, false otherwise
 */
inline bool isValidCodeVerifier(const std::string &codeVerifier)
{
    if (codeVerifier.length() < 43 || codeVerifier.length() > 128)
    {
        return false;
    }

    // Check character set: [A-Za-z0-9-._~]
    for (char c : codeVerifier)
    {
        if (
          !std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '.' && c != '_' &&
          c != '~'
        )
        {
            return false;
        }
    }

    return true;
}

/**
 * @brief Validate code challenge format (RFC 7636)
 *
 * Code challenge must be 43-128 characters of [A-Za-z0-9-._~]
 *
 * @param codeChallenge The code challenge to validate
 * @return true if valid, false otherwise
 */
inline bool isValidCodeChallenge(const std::string &codeChallenge)
{
    if (codeChallenge.length() < 43 || codeChallenge.length() > 128)
    {
        return false;
    }

    // Check character set: [A-Za-z0-9-._~]
    for (char c : codeChallenge)
    {
        if (
          !std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '.' && c != '_' &&
          c != '~'
        )
        {
            return false;
        }
    }

    return true;
}

/**
 * @brief Generate a cryptographically secure random token
 *
 * Uses Drogon's secureRandomBytes to generate high-entropy random data,
 * then encodes it as base64url (no padding).
 * Default 32 bytes = 256 bits of entropy, producing a 43-character string.
 *
 * @param bytes Number of random bytes (default 32 = 256 bits)
 * @return Base64URL encoded random token string
 */
inline std::string generateSecureToken(size_t bytes = 32)
{
    std::vector<unsigned char> buffer(bytes);
    if (!drogon::utils::secureRandomBytes(buffer.data(), bytes))
    {
        // Fallback to Drogon UUID if secure random fails (should never happen)
        return drogon::utils::getUuid() + drogon::utils::getUuid();
    }
    return base64UrlEncode(buffer.data(), buffer.size());
}

/**
 * @brief Hash a token for secure storage
 *
 * Computes SHA-256 of the raw token and returns lowercase hex string (64 chars).
 * Used to store tokens in the database without exposing the raw value.
 * On lookup, the raw token from the client is hashed before querying.
 *
 * @param rawToken The raw token string (as returned to the client)
 * @return Lowercase hex SHA-256 hash (64 characters)
 */
inline std::string hashToken(const std::string &rawToken)
{
    return drogon::utils::getSha256(rawToken.data(), rawToken.length());
}

}  // namespace utils
}  // namespace oauth2
