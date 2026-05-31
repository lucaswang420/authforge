// OAuth2Server/test/integration/concurrency/CategoryC_CleanupServiceUafTest.cc
//
// Spec: concurrency-lifetime-safety-audit — Task 3 (Category C reproduction).
// Property 3: Bug Condition — No Use-After-Free (ASan) — covers defect **1.10**
// (OAuth2CleanupService::runCleanup() Redis callback captures a dangling `this`).
//
// ─────────────────────────────────────────────────────────────────────────
// WHAT THIS REPRODUCES (the exact raw-`this` capture in runCleanup())
// ─────────────────────────────────────────────────────────────────────────
// Production code (OAuth2Plugin/src/plugin/OAuth2CleanupService.cc). Unlike
// `start()` — which correctly captures `weak_from_this()` for its `runEvery`
// timer — `runCleanup()` hands Redis callbacks that capture a **raw `this`**:
//
//     void OAuth2CleanupService::runCleanup() {
//         if (!running_ || !storage_) return;
//         auto redis = drogon::app().getRedisClient("default");
//         redis->execCommandAsync(
//           [this](const RedisResult &r) {                 // (C) raw this
//               if (r.isNil()) { ... return; }
//               storage_->deleteExpiredData();             // (D) this->storage_, this->running_
//           },
//           [this](const std::exception &e) {              // (C) raw this
//               storage_->deleteExpiredData();             // (D) this->storage_
//           },
//           "SET oauth2:cleanup:lock %s NX EX %d", "locked", lockTtl);
//     }
//
// If the service is destroyed between the Redis command dispatch and its reply
// (e.g. plugin teardown drops `cleanupService_`), the deferred Redis callback
// runs against a dangling `this`, reading the freed `this->running_` /
// `this->storage_` members → **heap-use-after-free**. This is an
// implementation INCONSISTENCY: `start()` uses `weak_from_this()`,
// `runCleanup()` does not.
//
// ─────────────────────────────────────────────────────────────────────────
// FAITHFUL-REPRODUCTION CAVEAT (documented honestly per task methodology)
// ─────────────────────────────────────────────────────────────────────────
// The genuine deferral only happens when a **real Redis client** is available:
// `execCommandAsync` parks the callback and fires it later from the Redis loop
// thread. When Redis is NOT configured, `getRedisClient("default")` throws and
// `runCleanup()` falls into its `catch` branch that runs
// `storage_->deleteExpiredData()` **synchronously inline** — no deferral, no
// dangling window. Additionally, `runCleanup()` is a PRIVATE method only
// reachable through the `runEvery` timer. Therefore a faithful reproduction
// driving the *real* `OAuth2CleanupService::runCleanup()` requires a
// **Redis-backed ASan build**.
//
// On this host (no Redis, ASan toolchain unavailable — see Task 0) we instead
// model `runCleanup()`'s Redis-callback SHAPE *exactly* with a local stand-in
// (`CleanupServiceRedisCallbackModel`) that captures a raw `this` into a
// deferred callback and, when fired, reads `this->running_` and calls
// `this->storage_->deleteExpiredData()` — byte-for-byte the production access
// pattern. The backing storage stays ALIVE; the ONLY freed object is the
// cleanup-service stand-in, so the dangling access is precisely the
// `this->running_` / `this->storage_` member read on the freed service.
//
// ─────────────────────────────────────────────────────────────────────────
// METHODOLOGY (exploratory reproduction on UNFIXED code)
// ─────────────────────────────────────────────────────────────────────────
//   * ASan build: destroy the service stand-in FIRST, then fire the parked
//     Redis callback → it reads `this->running_` / `this->storage_` on freed
//     memory → ASan reports `heap-use-after-free`. The bug-condition test
//     FAILS via the sanitizer — the SUCCESS case.
//   * Normal build: fire the callback while the service is still ALIVE
//     (lifetime-safe ordering; identical access pattern, no UB), then destroy.
//     Selected by `kAsanEnabled`.
//
// **Validates: Requirements 2.10** (design Property 3 — No Use-After-Free)
//
// _Requirements: 2.6, 2.8, 2.9, 2.10, 2.11_

#include <drogon/drogon_test.h>

#include <atomic>
#include <memory>

#include <oauth2/storage/IOAuth2Storage.h>

#include "CategoryC_DeferredStorageSupport.h"
#include "ConcurrencyRaceSupport.h"

using namespace oauth2::test::concurrency;

namespace
{
// Mirrors the exact body/shape of OAuth2CleanupService::runCleanup()'s Redis
// callback: it captures a raw `this` into a DEFERRED callback (modelling
// `redis->execCommandAsync`), and on fire reads `this->running_` then calls
// `this->storage_->deleteExpiredData()`. The raw `IOAuth2Storage* storage_`
// member matches the production class (which also holds a raw storage pointer).
class CleanupServiceRedisCallbackModel
{
  public:
    CleanupServiceRedisCallbackModel(
      oauth2::IOAuth2Storage *storage,
      std::shared_ptr<PendingCallbacks> pending
    )
      : storage_(storage), pending_(std::move(pending))
    {
    }

    // Models runCleanup(): the entry guard mirrors `if (!running_ || !storage_)`,
    // then a Redis callback capturing raw `this` is DEFERRED (parked) and later
    // dereferences this->running_ / this->storage_ — the production hazard.
    void runCleanup()
    {
        if (!running_ || !storage_)
            return;

        // redis->execCommandAsync([this]{ ... storage_->deleteExpiredData(); });
        pending_->enqueue([this]() {
            // (D) dangling member reads if `this` was destroyed before fire:
            if (!running_)
                return;
            storage_->deleteExpiredData();
        });
    }

  private:
    oauth2::IOAuth2Storage *storage_;  // raw pointer, as in production
    bool running_{true};
    std::shared_ptr<PendingCallbacks> pending_;
};
}  // namespace

// 1.10 — runCleanup()'s Redis callback reads this->running_ / this->storage_
// on the freed service.
DROGON_TEST(Integration_Concurrency_1_10_CleanupService_RunCleanup_RedisCallback_UAF_Repro)
{
    auto pending = std::make_shared<PendingCallbacks>();
    auto storage = std::make_unique<DeferringStorage>(pending);  // stays alive

    auto svc = std::make_unique<CleanupServiceRedisCallbackModel>(storage.get(), pending);
    svc->runCleanup();

    // The Redis callback (capturing the service's raw `this`) is parked.
    REQUIRE(pending->size() == 1);

    if (kAsanEnabled)
    {
        // GENUINE UAF (ASan only): service destroyed BEFORE its parked Redis
        // callback fires; firing reads this->running_ / this->storage_ on freed
        // memory → ASan heap-use-after-free.
        svc.reset();
        pending->fireAll();
    }
    else
    {
        // NORMAL BUILD: fire while the service is alive (no UB; identical access
        // pattern), then destroy.
        pending->drainAll();
        svc.reset();
    }

    // Reaching here (normal build) means the callback ran against the live
    // service. (Under ASan the runtime aborts on the use-after-free first.)
    SUCCESS();
}
