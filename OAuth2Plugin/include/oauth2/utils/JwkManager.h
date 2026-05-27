#pragma once

#include <string>
#include <json/json.h>
#include <memory>

namespace oauth2
{

/**
 * @brief Manages RSA signing keys for OpenID Connect id_token
 *
 * Loads RSA private key from PEM file or environment variable.
 * Signs JWT tokens using RS256 algorithm.
 * Exposes public key in JWK format for /.well-known/jwks.json
 */
class JwkManager
{
  public:
    JwkManager() = default;
    ~JwkManager();

    /**
     * @brief Initialize from configuration
     * Loads RSA private key from file path or generates ephemeral key for dev.
     *
     * @param config JSON config with "signing_key_path" or "signing_key_env"
     * @return true if key loaded successfully
     */
    bool init(const Json::Value &config);

    /**
     * @brief Sign a JWT payload with RS256
     * @param header JSON header (typ, alg, kid)
     * @param payload JSON payload (claims)
     * @return Signed JWT string (header.payload.signature)
     */
    std::string signJwt(const Json::Value &payload) const;

    /**
     * @brief Get the JWKS (JSON Web Key Set) containing public key(s)
     * @return JSON object with "keys" array
     */
    Json::Value getJwks() const;

    /**
     * @brief Get the current key ID
     */
    const std::string &getKeyId() const
    {
        return kid_;
    }

    /**
     * @brief Check if manager is initialized with a valid key
     */
    bool isInitialized() const
    {
        return initialized_;
    }

  private:
    void *rsaKey_ = nullptr;  // EVP_PKEY* (opaque to avoid OpenSSL header in public API)
    std::string kid_;
    bool initialized_ = false;

    // Generate an ephemeral RSA key for development/testing
    bool generateEphemeralKey();

    // Load RSA private key from PEM string
    bool loadFromPem(const std::string &pemData);

    // Base64URL encode binary data
    static std::string base64UrlEncode(const unsigned char *data, size_t len);
    static std::string base64UrlEncode(const std::string &data);

    // Extract RSA public key components (n, e) for JWK
    bool getPublicKeyComponents(std::string &n, std::string &e) const;
};

}  // namespace oauth2
