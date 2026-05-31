// OAuth2Server/test/integration/concurrency/CategoryC_CachedStorageUafTest.cc
//
// Spec: concurrency-lifetime-safety-audit — Task 3 (Category C reproduction).
// Property 3: Bug Condition — No Use-After-Free (ASan) — covers defect
// **1.8 (including the original 1.6 `tokenCache_` / `clientCache_` members)**.
//
// ─────────────────────────────────────────────────────────────────────────
// WHAT THIS REPRODUCES (the exact raw-`this` capture + reset timing)
// ─────────────────────────────────────────────────────────────────────────
// Production code (OAuth2Plugin/src/storage/CachedOAuth2Storage.cc). Each
// cached operation hands an async continuation that captures a **raw `this`**
// to the underlying storage (`impl_`) or to `redisClient_`, and the
// continuation then touches a MEMBER of the CachedOAuth2Storage:
//
//   getAccessToken():   impl_->getAccessToken(token,
//                         [this, token, cb](optToken){
//                             ... tokenCache_.insert(token, *optToken, ttl);  // (M) member
//                             cb(optToken); });
//   saveAccessToken():  impl_->saveAccessToken(token,
//                         [this, token, cb, ttl](){
//                             if (!redisClient_ || ttl<=0) { cb(); return; }  // (M) member read
//                             ... });
//   revokeAccessToken():impl_->revokeAccessToken(token, revokedBy,
//                         [this, token, cb](){
//                             tokenCache_.erase(token);                       // (M) member
//                             if (redisClient_) { ... } else { cb(); } });
//   getClient():        impl_->getClient(clientId,
//                         [this, clientId, cb](client){
//                             if (client) clientCache_.insert(clientId,*client,60); // (M) member
//                             cb(client); });
//
// `CachedOAuth2Storage` does NOT inherit `enable_shared_from_this` and, in
// production, `OAuth2Plugin::storage_` owns it via `std::unique_ptr`. At
// `shutdown()` the plugin calls `storage_.reset()`. If an async storage
// continuation is still in flight (Postgres `DbClient` / Redis `RedisClient`
// dispatch the callback later, from their own loop thread), the reset destroys
// `tokenCache_` / `clientCache_` / `redisClient_` while the continuation still
// holds a dangling `this` → **heap-use-after-free** when it touches (M).
//
// ─────────────────────────────────────────────────────────────────────────
// WHY `DeferringStorage` (and NOT `MemoryOAuth2Storage`) backs `impl_`
// ─────────────────────────────────────────────────────────────────────────
// The hazard only exists because the underlying storage invokes the
// continuation *later* (asynchronously). `MemoryOAuth2Storage` invokes its
// callbacks **synchronously / inline**, so its continuations always run while
// the host is still alive — it CANNOT reproduce the deferral. `DeferringStorage`
// (see CategoryC_DeferredStorageSupport.h) is a real `IOAuth2Storage` that
// faithfully models the Postgres/Redis behaviour: it parks the continuation on
// an external queue the TEST owns, so the test can destroy the host
// CachedOAuth2Storage FIRST and fire the continuation AFTERWARDS — exactly the
// "object destroyed before the in-flight callback arrives" timing. This needs
// NO external DB/Redis service.
//
// ─────────────────────────────────────────────────────────────────────────
// METHODOLOGY (exploratory reproduction on UNFIXED code)
// ─────────────────────────────────────────────────────────────────────────
//   * Built with AddressSanitizer (-fsanitize=address) the genuine ordering
//     (reset host, THEN fire continuation) makes the continuation touch freed
//     members → ASan reports `heap-use-after-free` and aborts. The
//     bug-condition test FAILS via the sanitizer — the SUCCESS case for an
//     exploratory reproduction on unfixed code.
//   * On a NORMAL (non-instrumented) build the genuine UAF is undefined
//     behaviour that can crash even without a sanitizer, so we DO NOT run it:
//     we fire the continuation while the host is still ALIVE (lifetime-safe
//     ordering), exercising the identical production path for compile+run
//     coverage, and the test passes deterministically without claiming a UAF
//     was detected. The ordering is selected by `kAsanEnabled`
//     (see ConcurrencyRaceSupport.h).
//
// **Validates: Requirements 2.6, 2.8** (design Property 3 — No Use-After-Free)
//
// _Requirements: 2.6, 2.8, 2.9, 2.10, 2.11_

#include <drogon/drogon_test.h>
#include <drogon/drogon.h>

#include <atomic>
#include <memory>
#include <string>

#include <oauth2/storage/CachedOAuth2Storage.h>

#include "CategoryC_DeferredStorageSupport.h"
#include "ConcurrencyRaceSupport.h"

using namespace oauth2::test::concurrency;
using oauth2::CachedOAuth2Storage;

namespace
{
// Selects the destroy-vs-callback ordering. On an ASan build we destroy the
// host FIRST so the in-flight continuation runs against freed memory (the
// genuine UAF). On a normal build we fire the continuation while the host is
// still alive (lifetime-safe; full path coverage, no UB).
//
// `destroyHost` releases the CachedOAuth2Storage owner; `pending` holds the
// parked production continuation(s).
void runDestroyVsCallbackRace(const std::function<void()> &destroyHost, PendingCallbacks &pending)
{
    if (kAsanEnabled)
    {
        // GENUINE UAF (ASan only): host (and its tokenCache_/clientCache_/
        // redisClient_ members) destroyed BEFORE the dangling-`this`
        // continuation fires. A single fireAll() is enough — the first
        // continuation touches freed members and ASan aborts.
        destroyHost();
        pending.fireAll();
    }
    else
    {
        // NORMAL BUILD: continuation fires while the host is still alive (no UB,
        // identical production path), then the host is destroyed normally.
        pending.drainAll();
        destroyHost();
    }
}
}  // namespace

// 1.8 / 1.6 — getAccessToken(): L1 cache-fill continuation touches tokenCache_
// through a dangling `this` after the host is reset.
DROGON_TEST(Integration_Concurrency_1_8_CachedStorage_GetAccessToken_UAF_Repro)
{
    auto pending = std::make_shared<PendingCallbacks>();

    // impl_ = DeferringStorage (parks continuations); redisClient_ = nullptr so
    // getAccessToken takes the impl_->getAccessToken path whose continuation
    // captures `this` and fills tokenCache_.
    //
    // make_shared (host + impl): after the 8.1 fix the production continuation
    // captures `auto self = shared_from_this();`, which requires the host to be
    // owned by a shared_ptr (otherwise shared_from_this() throws bad_weak_ptr).
    // Option B also makes impl_ a shared_ptr.
    auto host = std::make_shared<CachedOAuth2Storage>(
      std::make_shared<DeferringStorage>(pending), drogon::nosql::RedisClientPtr{}
    );

    std::atomic<int> delivered{0};
    host->getAccessToken("tok-getat", [&delivered](std::optional<oauth2::OAuth2AccessToken>) {
        delivered.fetch_add(1, std::memory_order_relaxed);
    });

    // The impl_ continuation (capturing host's raw `this`) is now parked.
    REQUIRE(pending->size() == 1);

    runDestroyVsCallbackRace([&host]() { host.reset(); }, *pending);

    // On a normal build the continuation ran against the live host and the
    // final callback fired exactly once. (Under ASan we never reach here — the
    // runtime aborts on the heap-use-after-free, which is the intended result.)
    CHECK(delivered.load(std::memory_order_relaxed) == 1);
}

// 1.8 — saveAccessToken(): write-through continuation reads redisClient_
// through a dangling `this` after the host is reset.
DROGON_TEST(Integration_Concurrency_1_8_CachedStorage_SaveAccessToken_UAF_Repro)
{
    auto pending = std::make_shared<PendingCallbacks>();
    auto host = std::make_shared<CachedOAuth2Storage>(
      std::make_shared<DeferringStorage>(pending), drogon::nosql::RedisClientPtr{}
    );

    std::atomic<int> delivered{0};
    auto token = makeLiveAccessToken("tok-saveat");
    host->saveAccessToken(token, [&delivered]() {
        delivered.fetch_add(1, std::memory_order_relaxed);
    });

    REQUIRE(pending->size() == 1);

    runDestroyVsCallbackRace([&host]() { host.reset(); }, *pending);

    CHECK(delivered.load(std::memory_order_relaxed) == 1);
}

// 1.8 — revokeAccessToken(): invalidation continuation touches tokenCache_ and
// reads redisClient_ through a dangling `this` after the host is reset.
DROGON_TEST(Integration_Concurrency_1_8_CachedStorage_RevokeAccessToken_UAF_Repro)
{
    auto pending = std::make_shared<PendingCallbacks>();
    auto host = std::make_shared<CachedOAuth2Storage>(
      std::make_shared<DeferringStorage>(pending), drogon::nosql::RedisClientPtr{}
    );

    std::atomic<int> delivered{0};
    host->revokeAccessToken("tok-revoke", "client-x", [&delivered]() {
        delivered.fetch_add(1, std::memory_order_relaxed);
    });

    REQUIRE(pending->size() == 1);

    runDestroyVsCallbackRace([&host]() { host.reset(); }, *pending);

    CHECK(delivered.load(std::memory_order_relaxed) == 1);
}

// 1.6 (folded into 1.8) — getClient(): client cache-fill continuation touches
// clientCache_ through a dangling `this` after the host is reset.
DROGON_TEST(Integration_Concurrency_1_8_CachedStorage_GetClient_ClientCache_UAF_Repro)
{
    auto pending = std::make_shared<PendingCallbacks>();
    auto host = std::make_shared<CachedOAuth2Storage>(
      std::make_shared<DeferringStorage>(pending), drogon::nosql::RedisClientPtr{}
    );

    std::atomic<int> delivered{0};
    host->getClient("client-getc", [&delivered](std::optional<oauth2::OAuth2Client>) {
        delivered.fetch_add(1, std::memory_order_relaxed);
    });

    REQUIRE(pending->size() == 1);

    runDestroyVsCallbackRace([&host]() { host.reset(); }, *pending);

    CHECK(delivered.load(std::memory_order_relaxed) == 1);
}
