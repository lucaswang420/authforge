// OAuth2Server/test/integration/concurrency/CategoryC_ServiceChainUafTest.cc
//
// Spec: concurrency-lifetime-safety-audit �?Task 3 (Category C reproduction).
// Property 3: Bug Condition �?No Use-After-Free (ASan) �?covers defect **1.9**
// (TokenService / IdentityService async chains capturing a dangling `this`).
//
// ─────────────────────────────────────────────────────────────────────────
// WHAT THIS REPRODUCES (the exact raw-`this` capture + service teardown timing)
// ─────────────────────────────────────────────────────────────────────────
// Both services hold a RAW `IOAuth2Storage* storage_` and pass async
// continuations capturing a **raw `this`** down multi-level storage chains;
// the continuation then reads `this->storage_` (and TTL/jwk members):
//
//   TokenService::exchangeCodeForToken (TokenService.cc):
//       storage_->validateClient(clientId, clientSecret,
//         [this, code, clientId, redirectUri, codeVerifier, callback](bool isValid){
//             ...
//             storage_->consumeAuthCode(...);   // (D) reads this->storage_
//         });
//
//   TokenService::refreshAccessToken:
//       storage_->atomicRevokeRefreshToken(hashedRt,
//         [this, callback, clientId, hashedRt](optional storedRt){
//             ...
//             storage_->getRefreshToken(...);    // (D) reads this->storage_
//         });
//
//   IdentityService::ensureSubjectMapping (IdentityService.cc):
//       storage_->getInternalUserId(sub, provider,
//         [this, sub, provider, internalUserId, callback](existingUserId){
//             ...
//             storage_->createSubjectMapping(...); // (D) reads this->storage_
//         });
//
//   IdentityService::handleFirstTimeLogin:
//       storage_->createUserForExternalLogin(sub, prov,
//         [this, sub, prov, callback](optional newUserId){
//             ...
//             storage_->createSubjectMapping(...); // (D) reads this->storage_
//         });
//
// Neither class inherits `enable_shared_from_this`. If the owning service is
// released (e.g. plugin teardown drops `tokenService_` / `identityService_`)
// while a storage continuation is still in flight, the continuation reads
// `this->storage_` on the freed service �?**heap-use-after-free**.
//
// We isolate the SERVICE lifetime hazard: the backing `DeferringStorage` stays
// ALIVE (owned by the test) for the whole test, so the ONLY freed object is the
// service. The dangling access is the continuation reading `this->storage_`.
//
// ─────────────────────────────────────────────────────────────────────────
// METHODOLOGY (exploratory reproduction on UNFIXED code)
// ─────────────────────────────────────────────────────────────────────────
//   * ASan build: destroy the service FIRST, then fire the parked
//     continuation �?it reads `this->storage_` on freed memory �?ASan reports
//     `heap-use-after-free`. The bug-condition test FAILS via the sanitizer �?
//     the SUCCESS case for an exploratory reproduction on unfixed code.
//   * Normal build: fire the continuation while the service is still ALIVE
//     (lifetime-safe ordering; identical production path, no UB), then destroy.
//     Selected by `kAsanEnabled`.
//
// **Validates: Requirements 2.9** (design Property 3 �?No Use-After-Free)
//
// _Requirements: 2.6, 2.8, 2.9, 2.10, 2.11_

#include <drogon/drogon_test.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include <json/json.h>

#include <oauth2/services/IdentityService.h>
#include <oauth2/services/TokenService.h>

#include "CategoryC_DeferredStorageSupport.h"
#include "ConcurrencyRaceSupport.h"

using namespace oauth2::test::concurrency;
using oauth2::IdentityService;
using oauth2::TokenService;

namespace
{
// destroy-vs-callback ordering (shared with the other Category C tests):
// ASan �?destroy first (genuine UAF); normal �?fire first (lifetime-safe).
void runServiceTeardownRace(const std::function<void()> &destroyService, PendingCallbacks &pending)
{
    if (kAsanEnabled)
    {
        destroyService();
        pending.fireAll();
    }
    else
    {
        pending.drainAll();
        destroyService();
    }
}
}  // namespace

// 1.9 �?TokenService::exchangeCodeForToken: validateClient continuation reads
// this->storage_ on the freed service.
DROGON_TEST(Integration_Concurrency_1_9_TokenService_ExchangeCode_UAF_Repro)
{
    auto pending = std::make_shared<PendingCallbacks>();
    auto storage = std::make_shared<DeferringStorage>(pending);  // stays alive

    // make_shared (defect 1.9 fix): the production service now captures
    // `shared_from_this()` in its async chains, so it must be owned by a
    // shared_ptr (exactly as OAuth2Plugin::initAndStart owns it). Owned solely
    // by the test here, mirroring the production single-owner teardown.
    auto svc = std::make_shared<TokenService>(storage);

    std::atomic<int> delivered{0};
    svc->exchangeCodeForToken(
      "auth-code", "client-1", "secret", "https://example.test/cb", "verifier",
      [&delivered](const Json::Value &) { delivered.fetch_add(1, std::memory_order_relaxed); }
    );

    // validateClient continuation (capturing the service's raw `this`) parked.
    REQUIRE(pending->size() == 1);

    runServiceTeardownRace([&svc]() { svc.reset(); }, *pending);

    // Normal build: the chain advanced against the live service (consumeAuthCode
    // returned nullopt �?invalid_grant), firing the final callback once. (Under
    // ASan the runtime aborts on the use-after-free before reaching here.)
    CHECK(delivered.load(std::memory_order_relaxed) == 1);
}

// 1.9 �?TokenService::refreshAccessToken: atomicRevokeRefreshToken
// continuation reads this->storage_ on the freed service.
DROGON_TEST(Integration_Concurrency_1_9_TokenService_RefreshToken_UAF_Repro)
{
    auto pending = std::make_shared<PendingCallbacks>();
    auto storage = std::make_shared<DeferringStorage>(pending);

    // make_shared (defect 1.9 fix): shared_from_this() in the production async
    // chain requires shared ownership, mirroring OAuth2Plugin::initAndStart.
    auto svc = std::make_shared<TokenService>(storage);

    std::atomic<int> delivered{0};
    svc->refreshAccessToken("refresh-token", "client-1", [&delivered](const Json::Value &) {
        delivered.fetch_add(1, std::memory_order_relaxed);
    });

    REQUIRE(pending->size() == 1);

    runServiceTeardownRace([&svc]() { svc.reset(); }, *pending);

    CHECK(delivered.load(std::memory_order_relaxed) == 1);
}

// 1.9 �?IdentityService::ensureSubjectMapping: getInternalUserId continuation
// reads this->storage_ on the freed service.
DROGON_TEST(Integration_Concurrency_1_9_IdentityService_EnsureSubjectMapping_UAF_Repro)
{
    auto pending = std::make_shared<PendingCallbacks>();
    auto storage = std::make_shared<DeferringStorage>(pending);

    // make_shared (defect 1.9 fix): shared_from_this() in the production async
    // chain requires shared ownership, mirroring OAuth2Plugin::initAndStart.
    auto svc = std::make_shared<IdentityService>(storage);

    std::atomic<int> delivered{0};
    svc->ensureSubjectMapping("local:alice", "alice", 42, [&delivered]() {
        delivered.fetch_add(1, std::memory_order_relaxed);
    });

    REQUIRE(pending->size() == 1);

    runServiceTeardownRace([&svc]() { svc.reset(); }, *pending);

    CHECK(delivered.load(std::memory_order_relaxed) == 1);
}

// 1.9 �?IdentityService::handleFirstTimeLogin: createUserForExternalLogin
// continuation reads this->storage_ on the freed service.
DROGON_TEST(Integration_Concurrency_1_9_IdentityService_HandleFirstTimeLogin_UAF_Repro)
{
    auto pending = std::make_shared<PendingCallbacks>();
    auto storage = std::make_shared<DeferringStorage>(pending);

    // make_shared (defect 1.9 fix): shared_from_this() in the production async
    // chain requires shared ownership, mirroring OAuth2Plugin::initAndStart.
    auto svc = std::make_shared<IdentityService>(storage);

    std::atomic<int> delivered{0};
    svc->handleFirstTimeLogin("google:sub123", "google", [&delivered](int32_t) {
        delivered.fetch_add(1, std::memory_order_relaxed);
    });

    REQUIRE(pending->size() == 1);

    runServiceTeardownRace([&svc]() { svc.reset(); }, *pending);

    CHECK(delivered.load(std::memory_order_relaxed) == 1);
}
