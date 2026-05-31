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
 *
 * ── Concurrency contract: init-once, then read-only (defect 1.5) ───────────
 * This class follows an "initialize exactly once during startup, read-only at
 * runtime" contract:
 *   - init() MUST be called exactly once, during OAuth2Plugin::initAndStart(),
 *     BEFORE the server begins accepting requests / posting tasks to the event
 *     loop. init() is the ONLY mutating member; it is guarded so that a second
 *     call logs an error and is a no-op (it will NOT re-allocate rsaKey_ or
 *     overwrite kid_), removing the run-time mutation entry point.
 *   - signJwt(), getJwks(), getKeyId(), isInitialized() are all const and only
 *     read the key state; they are safe to call concurrently from many request
 *     threads with no per-call locking.
 *
 * Happens-before reasoning: because init() completes before the first request
 * is published to the event loop, the queueInLoop / event-loop task posting
 * that delivers each request forms a release->acquire pair that makes the fully
 * initialized key state visible to every worker thread. After that point the
 * state is immutable, so concurrent reads cannot race with any write — no
 * mutex is required on the read path. To enforce immutability at the type
 * level, OAuth2Plugin publishes this object as a std::shared_ptr<const
 * JwkManager> (see OAuth2Plugin / TokenService): once published, the const
 * pointer makes any run-time mutation a compile error.
 *
 * OpenSSL concurrency assumption: see the comment on signJwt() — concurrent
 * signing is safe only under OpenSSL >= 1.1.0 with threads enabled.
 */
class JwkManager
{
  public:
    JwkManager() = default;
    ~JwkManager();

    /**
     * @brief Initialize from configuration (call EXACTLY ONCE, at startup)
     *
     * Loads the RSA private key from an env var / file path, or generates an
     * ephemeral dev key as a fallback. This is the only mutating method.
     *
     * init-once guard (defect 1.5): once a previous call has succeeded
     * (isInitialized() == true), any further call is rejected — it logs an
     * error and returns without re-allocating rsaKey_ or overwriting kid_, so
     * the key state can never change underneath concurrent readers. Call this
     * during OAuth2Plugin::initAndStart(), before request handling begins.
     *
     * @param config JSON config with "signing_key_path" / "kid" (or env vars)
     * @return true if a key is loaded and the manager is initialized (including
     *         the no-op case where it was already initialized); false on
     *         first-time initialization failure.
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
