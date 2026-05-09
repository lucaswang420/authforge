#pragma once

#include <string>
#include <vector>
#include <drogon/utils/Utilities.h>
#include <openssl/evp.h>

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
 * @param data Input data
 * @return SHA-256 hash (32 bytes)
 */
inline std::vector<unsigned char> sha256(const std::string &data)
{
    const int SHA256_DIGEST_LENGTH = 32;
    std::vector<unsigned char> hash(SHA256_DIGEST_LENGTH);

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, data.c_str(), data.length());
    EVP_DigestFinal_ex(ctx, hash.data(), nullptr);
    EVP_MD_CTX_free(ctx);

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

}  // namespace utils
}  // namespace oauth2
