// OAuth2Server/test/integration/concurrency/CategoryC_DeferredStorageSupport.h
//
// Spec: concurrency-lifetime-safety-audit — Task 3 (Category C reproduction).
// Property 3: Bug Condition — No Use-After-Free (ASan). Covers defects
// 1.8 (incl. original 1.6), 1.9, 1.10, 1.11.
//
// ─────────────────────────────────────────────────────────────────────────
// WHY A "DEFERRING" STORAGE DOUBLE
// ─────────────────────────────────────────────────────────────────────────
// Every Category C defect is the SAME timing hazard: an object issues an
// asynchronous storage operation whose continuation captures a raw `this`
// (or a raw `IOAuth2Storage*`), the object is destroyed (e.g.
// `OAuth2Plugin::shutdown()` → `storage_.reset()`, or a service released), and
// THEN the in-flight continuation arrives and dereferences the now-dangling
// pointer to touch an already-freed member.
//
// In production that deferral is done by Drogon's `DbClient` / `RedisClient`:
// they STORE the captured callback and invoke it later, from the client's loop
// thread, possibly after the issuer is gone. `MemoryOAuth2Storage` cannot
// reproduce this because it invokes its callbacks SYNCHRONOUSLY (inline), so
// the continuation always runs while the issuer is still alive.
//
// `DeferringStorage` is a real `IOAuth2Storage` implementation that models the
// DbClient/RedisClient behaviour precisely: instead of invoking the callback
// inline, it ENQUEUES the continuation onto an external `PendingCallbacks`
// queue owned by the test. The test then chooses the ordering:
//
//   * ASan build  (kAsanEnabled): destroy the issuer FIRST, then
//     `pending.fireAll()` → the production continuation runs against freed
//     memory → ASan reports `heap-use-after-free`. The bug-condition test
//     FAILS via the sanitizer — the SUCCESS case for an exploratory
//     reproduction on UNFIXED code.
//   * Normal build: `pending.fireAll()` FIRST (issuer still alive), then
//     destroy → the SAME production code path runs against a LIVE object
//     (full compile + run coverage, no undefined behaviour) and passes
//     deterministically WITHOUT claiming any UAF was found.
//
// The queue holds only `std::function<void()>` thunks; it never touches the
// object under test on its own. The ONLY dangling access is the captured
// `this` / raw pointer INSIDE a production continuation — exactly the defect.
//
// _Requirements: 2.6, 2.8, 2.9, 2.10, 2.11_

#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <oauth2/storage/IOAuth2Storage.h>

#include "ConcurrencyRaceSupport.h"

namespace oauth2::test::concurrency
{
// A token / client whose expiry is far in the future, so the production
// look-aside cache code takes its "ttl > 0 → cache fill" branch (which is the
// branch that touches the CacheMap member through the captured `this`).
inline int64_t farFutureExpiry()
{
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
                 std::chrono::system_clock::now().time_since_epoch()
    )
                 .count();
    return static_cast<int64_t>(now) + 3600;
}

inline oauth2::OAuth2AccessToken makeLiveAccessToken(const std::string &token)
{
    oauth2::OAuth2AccessToken t;
    t.token = token;
    t.clientId = "test-client";
    t.userId = "test-user";
    t.scope = "openid";
    t.expiresAt = farFutureExpiry();
    t.revoked = false;
    return t;
}

inline oauth2::OAuth2Client makeLiveClient(const std::string &clientId)
{
    oauth2::OAuth2Client c;
    c.clientId = clientId;
    c.clientType = oauth2::ClientType::CONFIDENTIAL;
    c.clientSecretHash = "hash";
    c.salt = "salt";
    c.redirectUris = {"https://example.test/cb"};
    c.allowedScopes = {"openid"};
    return c;
}

// ─────────────────────────────────────────────────────────────────────────
// DeferringStorage — IOAuth2Storage whose callbacks are deferred (queued),
// modelling Drogon's DbClient/RedisClient async dispatch.
// ─────────────────────────────────────────────────────────────────────────
// Every operation that takes a callback ENQUEUES a thunk delivering a value
// chosen to drive the PRODUCTION caller into its member-touching branch:
//   * getAccessToken  → an optional token (drives CachedOAuth2Storage into
//                       `tokenCache_.insert`).
//   * getClient       → an optional client (drives `clientCache_.insert`).
//   * revokeAccessToken → void (drives `tokenCache_.erase`).
//   * validateClient  → true (drives TokenService::exchangeCodeForToken's
//                       outer continuation, which reads `this->storage_`).
//   * atomicRevokeRefreshToken → nullopt (drives
//                       TokenService::refreshAccessToken's continuation, which
//                       reads `this->storage_`).
//   * getInternalUserId → nullopt (drives IdentityService::ensureSubjectMapping
//                       into `storage_->createSubjectMapping`).
//   * createUserForExternalLogin → an int (drives
//                       IdentityService::handleFirstTimeLogin similarly).
// The thunk captures only the production callback + a value; it does NOT keep
// the DeferringStorage alive, so the storage may itself be torn down when the
// owning object is destroyed.
class DeferringStorage : public oauth2::IOAuth2Storage
{
  public:
    explicit DeferringStorage(std::shared_ptr<PendingCallbacks> pending)
      : pending_(std::move(pending))
    {
    }

    // ----- Client Operations -----
    void getClient(const std::string &clientId, ClientCallback &&cb) override
    {
        auto client = makeLiveClient(clientId);
        pending_->enqueue([cb = std::move(cb), client]() {
            cb(std::optional<oauth2::OAuth2Client>(client));
        });
    }

    void validateClient(
      const std::string & /*clientId*/,
      const std::string & /*clientSecret*/,
      BoolCallback &&cb
    ) override
    {
        pending_->enqueue([cb = std::move(cb)]() { cb(true); });
    }

    // ----- Authorization Code Operations -----
    void saveAuthCode(const oauth2::OAuth2AuthCode & /*code*/, VoidCallback &&cb) override
    {
        enqueueVoid(std::move(cb));
    }

    void getAuthCode(const std::string & /*code*/, AuthCodeCallback &&cb) override
    {
        pending_->enqueue([cb = std::move(cb)]() { cb(std::nullopt); });
    }

    void markAuthCodeUsed(const std::string & /*code*/, VoidCallback &&cb) override
    {
        enqueueVoid(std::move(cb));
    }

    void consumeAuthCode(
      const std::string & /*code*/,
      const std::string & /*redirectUri*/,
      AuthCodeCallback &&cb
    ) override
    {
        pending_->enqueue([cb = std::move(cb)]() { cb(std::nullopt); });
    }

    // ----- Access Token Operations -----
    void saveAccessToken(const oauth2::OAuth2AccessToken & /*token*/, VoidCallback &&cb) override
    {
        enqueueVoid(std::move(cb));
    }

    void getAccessToken(const std::string &token, AccessTokenCallback &&cb) override
    {
        auto t = makeLiveAccessToken(token);
        pending_->enqueue([cb = std::move(cb), t]() {
            cb(std::optional<oauth2::OAuth2AccessToken>(t));
        });
    }

    void saveTokenPair(
      const oauth2::OAuth2AccessToken & /*at*/,
      const oauth2::OAuth2RefreshToken & /*rt*/,
      VoidCallback &&cb
    ) override
    {
        enqueueVoid(std::move(cb));
    }

    // ----- Refresh Token Operations -----
    void saveRefreshToken(const oauth2::OAuth2RefreshToken & /*token*/, VoidCallback &&cb) override
    {
        enqueueVoid(std::move(cb));
    }

    void getRefreshToken(const std::string & /*token*/, RefreshTokenCallback &&cb) override
    {
        pending_->enqueue([cb = std::move(cb)]() { cb(std::nullopt); });
    }

    void revokeRefreshToken(const std::string & /*token*/, VoidCallback &&cb) override
    {
        enqueueVoid(std::move(cb));
    }

    void atomicRevokeRefreshToken(const std::string & /*token*/, RefreshTokenCallback &&cb) override
    {
        // nullopt → drives TokenService::refreshAccessToken into its
        // "token not found / reuse" branch, which reads `this->storage_`.
        pending_->enqueue([cb = std::move(cb)]() { cb(std::nullopt); });
    }

    void revokeTokenFamily(const std::string & /*familyId*/, VoidCallback &&cb) override
    {
        enqueueVoid(std::move(cb));
    }

    // ----- Cleanup -----
    void deleteExpiredData() override
    {
        // Synchronous in production; nothing to defer.
    }

    // ----- RBAC -----
    void getUserRoles(const std::string & /*userId*/, StringListCallback &&cb) override
    {
        pending_->enqueue([cb = std::move(cb)]() { cb(std::vector<std::string>{}); });
    }

    void getUserRoles(int32_t /*internalUserId*/, StringListCallback &&cb) override
    {
        pending_->enqueue([cb = std::move(cb)]() { cb(std::vector<std::string>{}); });
    }

    // ----- User Info -----
    void getUserInfo(const std::string & /*userId*/, OptionalJsonCallback &&cb) override
    {
        pending_->enqueue([cb = std::move(cb)]() { cb(std::nullopt); });
    }

    void getUserInfo(int32_t /*internalUserId*/, OptionalJsonCallback &&cb) override
    {
        pending_->enqueue([cb = std::move(cb)]() { cb(std::nullopt); });
    }

    // ----- Subject Mapping -----
    void getInternalUserId(
      const std::string & /*subject*/,
      const std::string & /*provider*/,
      OptionalIntCallback &&cb
    ) override
    {
        // nullopt → drives IdentityService::ensureSubjectMapping into the
        // `storage_->createSubjectMapping` branch (reads `this->storage_`).
        pending_->enqueue([cb = std::move(cb)]() { cb(std::nullopt); });
    }

    void createSubjectMapping(
      const std::string & /*subject*/,
      int32_t /*internalUserId*/,
      const std::string & /*provider*/,
      BoolCallback &&cb
    ) override
    {
        pending_->enqueue([cb = std::move(cb)]() { cb(true); });
    }

    void createUserForExternalLogin(
      const std::string & /*externalId*/,
      const std::string & /*provider*/,
      OptionalIntCallback &&cb
    ) override
    {
        // A non-zero id → drives IdentityService::handleFirstTimeLogin into the
        // `storage_->createSubjectMapping` branch (reads `this->storage_`).
        pending_->enqueue([cb = std::move(cb)]() { cb(std::optional<int32_t>(123)); });
    }

    // ----- Authorization Transactions -----
    void saveAuthorizationTransaction(
      const AuthorizationTransaction & /*transaction*/,
      BoolCallback &&cb
    ) override
    {
        pending_->enqueue([cb = std::move(cb)]() { cb(true); });
    }

    void getAuthorizationTransaction(
      const std::string & /*transactionId*/,
      TransactionCallback &&cb
    ) override
    {
        pending_->enqueue([cb = std::move(cb)]() { cb(std::nullopt); });
    }

    void deleteAuthorizationTransaction(
      const std::string & /*transactionId*/,
      VoidCallback &&cb
    ) override
    {
        enqueueVoid(std::move(cb));
    }

    void markTransactionConsumed(const std::string & /*transactionId*/, BoolCallback &&cb) override
    {
        pending_->enqueue([cb = std::move(cb)]() { cb(true); });
    }

    // ----- Scope Management -----
    void hasUserConsent(
      int32_t /*internalUserId*/,
      const std::string & /*clientId*/,
      const std::string & /*scope*/,
      BoolCallback &&cb
    ) override
    {
        pending_->enqueue([cb = std::move(cb)]() { cb(false); });
    }

    void saveUserConsent(
      int32_t /*internalUserId*/,
      const std::string & /*clientId*/,
      const std::string & /*scope*/,
      BoolCallback &&cb
    ) override
    {
        pending_->enqueue([cb = std::move(cb)]() { cb(true); });
    }

    void revokeUserConsent(
      int32_t /*internalUserId*/,
      const std::string & /*clientId*/,
      const std::string & /*scope*/,
      VoidCallback &&cb
    ) override
    {
        enqueueVoid(std::move(cb));
    }

    // ----- Introspection -----
    void introspectToken(const std::string & /*token*/, TokenIntrospectionCallback &&cb) override
    {
        pending_->enqueue([cb = std::move(cb)]() { cb(std::nullopt); });
    }

    void incrementIntrospectCount(const std::string & /*token*/, VoidCallback &&cb) override
    {
        enqueueVoid(std::move(cb));
    }

    // ----- Revocation -----
    void revokeAccessToken(
      const std::string & /*token*/,
      const std::string & /*revokedBy*/,
      VoidCallback &&cb
    ) override
    {
        enqueueVoid(std::move(cb));
    }

  private:
    void enqueueVoid(VoidCallback &&cb)
    {
        pending_->enqueue([cb = std::move(cb)]() {
            if (cb)
                cb();
        });
    }

    std::shared_ptr<PendingCallbacks> pending_;
};
}  // namespace oauth2::test::concurrency
