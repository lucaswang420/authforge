// OAuth2Server/test/integration/concurrency/CategoryC_ControllerStoragePtrUafTest.cc
//
// Spec: concurrency-lifetime-safety-audit — Task 3 (Category C reproduction).
// Property 3: Bug Condition — No Use-After-Free (ASan) — covers defect **1.11**
// (OAuth2StandardController holding a RAW storage pointer across async).
//
// ─────────────────────────────────────────────────────────────────────────
// WHAT THIS REPRODUCES (the exact raw-storage-pointer capture + reset timing)
// ─────────────────────────────────────────────────────────────────────────
// Production code (OAuth2Plugin/src/controllers/OAuth2StandardController.cc).
// Several controller chains fetch the storage as a RAW pointer via
// `plugin->getStorage()` and then capture / re-use that raw pointer across an
// asynchronous storage operation. For example the `client_credentials` grant:
//
//     auto storage = plugin->getStorage();                 // raw IOAuth2Storage*
//     storage->getClient(clientId, [..., sharedCb](client){
//         ...
//         auto storage2 = plugin->getStorage();            // raw IOAuth2Storage*
//         storage2->saveAccessToken(token, [sharedCb,...](){ ... });  // (D) deref raw ptr
//     });
//
// and the `userinfo` chain (`auto storage = plugin->getStorage(); storage->...`).
// `getStorage()` returns `storage_.get()` — a raw `IOAuth2Storage*` whose
// lifetime is NOT tied to the controller singleton. If `OAuth2Plugin::shutdown()`
// runs `storage_.reset()` while one of these chains is mid-flight, the captured
// raw storage pointer dangles and the next `storage->...` call is a
// **heap-use-after-free** on the destroyed storage object.
//
// NOTE (per design 1.11): fixing 1.3 alone (storage_ → shared_ptr) does NOT fix
// this — only making `getStorage()` return a `shared_ptr` AND having the chain
// capture that shared_ptr keeps the storage alive across the async hop. This
// test reproduces the *current* raw-pointer hazard.
//
// ─────────────────────────────────────────────────────────────────────────
// HOW THIS MODELS THE CONTROLLER FAITHFULLY (no full HTTP stack needed)
// ─────────────────────────────────────────────────────────────────────────
// The defect is independent of HTTP routing: its essence is "a raw
// `IOAuth2Storage*` (from getStorage()) captured into an async continuation,
// the storage reset before the continuation arrives, then the continuation
// dereferences the raw storage pointer". We reproduce exactly that against a
// REAL storage object:
//   * `storageOwner` (a `std::unique_ptr<DeferringStorage>`) models
//     `OAuth2Plugin::storage_`.
//   * `rawStorage = storageOwner.get()` models `plugin->getStorage()`.
//   * the controller-style continuation captures `rawStorage` and, when fired,
//     calls `rawStorage->saveAccessToken(...)` — re-using the raw pointer
//     across the async hop, exactly like the `client_credentials` chain.
//   * resetting `storageOwner` models `shutdown()`'s `storage_.reset()`.
// `DeferringStorage` parks its continuations on a test-owned queue, so the
// reset can deterministically precede the in-flight callback. No DB/Redis,
// no HTTP server required.
//
// ─────────────────────────────────────────────────────────────────────────
// METHODOLOGY (exploratory reproduction on UNFIXED code)
// ─────────────────────────────────────────────────────────────────────────
//   * ASan build: reset the storage owner FIRST, then fire the parked
//     continuation → it calls a virtual method through the dangling raw storage
//     pointer on the freed object → ASan reports `heap-use-after-free`. The
//     bug-condition test FAILS via the sanitizer — the SUCCESS case.
//   * Normal build: fire the continuation while the storage is still ALIVE
//     (lifetime-safe ordering; identical access pattern, no UB), then reset.
//     Selected by `kAsanEnabled`.
//
// **Validates: Requirements 2.11** (design Property 3 — No Use-After-Free)
//
// _Requirements: 2.6, 2.8, 2.9, 2.10, 2.11_

#include <drogon/drogon_test.h>

#include <atomic>
#include <memory>
#include <optional>
#include <string>

#include <oauth2/storage/IOAuth2Storage.h>

#include "CategoryC_DeferredStorageSupport.h"
#include "ConcurrencyRaceSupport.h"

using namespace oauth2::test::concurrency;

// 1.11 — controller-style chain captures a RAW storage pointer (from
// getStorage()) and re-uses it across an async hop after the storage is reset.
DROGON_TEST(Integration_Concurrency_1_11_Controller_RawStoragePointer_AcrossAsync_UAF_Repro)
{
    auto pending = std::make_shared<PendingCallbacks>();

    // Models OAuth2Plugin::storage_ (owning) and plugin->getStorage() (raw).
    auto storageOwner = std::make_unique<DeferringStorage>(pending);
    oauth2::IOAuth2Storage *rawStorage = storageOwner.get();

    std::atomic<int> innerSaved{0};

    // Controller chain step 1: storage->getClient(...). The continuation
    // captures the RAW storage pointer (exactly like the client_credentials
    // grant capturing `plugin`/`storage2`) and re-uses it for step 2.
    rawStorage->getClient(
      "cc-client",
      [rawStorage, &innerSaved](std::optional<oauth2::OAuth2Client> /*client*/) {
          // Controller chain step 2: re-dereference the raw storage pointer to
          // issue the next async storage op. If storage_ was reset, this is the
          // use-after-free.
          auto token = makeLiveAccessToken("cc-token");
          rawStorage->saveAccessToken(token, [&innerSaved]() {
              innerSaved.fetch_add(1, std::memory_order_relaxed);
          });
      }
    );

    // The getClient continuation (holding the raw storage pointer) is parked.
    REQUIRE(pending->size() == 1);

    if (kAsanEnabled)
    {
        // GENUINE UAF (ASan only): storage destroyed BEFORE the controller
        // continuation fires; firing it calls rawStorage->saveAccessToken(...)
        // — a virtual call on freed memory → ASan heap-use-after-free.
        storageOwner.reset();
        pending->fireAll();
    }
    else
    {
        // NORMAL BUILD: drain while storage is alive (no UB; identical access
        // pattern), then reset. saveAccessToken enqueues an inner thunk, so we
        // drain the whole chain.
        pending->drainAll();
        storageOwner.reset();
    }

    // Normal build: the controller chain advanced against the live storage and
    // the inner saveAccessToken callback fired once. (Under ASan the runtime
    // aborts on the use-after-free before reaching here.)
    CHECK(innerSaved.load(std::memory_order_relaxed) == 1);
}
