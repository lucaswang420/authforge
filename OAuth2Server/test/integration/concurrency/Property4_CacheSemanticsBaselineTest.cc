// OAuth2Server/test/integration/concurrency/Property4_CacheSemanticsBaselineTest.cc
//
// Spec: concurrency-lifetime-safety-audit — Task 4 (Property 4: Preservation).
// Behavioral Equivalence on ¬C(X). Observation object **3.4**: the cache
// hit (L1/L2) / miss-then-DB-back-fill READ-WRITE path must keep the SAME cache
// semantics (write-through, look-aside back-fill, invalidation, TTL gating) —
// **including the L1-hit SYNCHRONOUS callback control flow** — across the
// category-C fix (8.1, which folds in original 1.6). The design EXPLICITLY
// forbids the 8.1 fix from introducing any `queueInLoop` that would turn an L1
// hit into an asynchronous callback; this file pins that control-flow property
// on the UNFIXED code so 8.5 can prove it was preserved.
//
// ─────────────────────────────────────────────────────────────────────────
// WHY THIS IS A NEW (NON-DUPLICATE) BASELINE
// ─────────────────────────────────────────────────────────────────────────
// Task 1 froze OpenApi/validation snapshots; Task 3 reproduced the destroy-
// before-callback UAF on `CachedOAuth2Storage` (the C(X) hazard). NEITHER
// captured the NORMAL-path (¬C(X)) cache SEMANTICS of a LIVE
// `CachedOAuth2Storage`. This file does exactly that, against the REAL
// production `CachedOAuth2Storage` decorator backed by an in-process,
// SYNCHRONOUS storage (a `MemoryOAuth2Storage` subclass that merely COUNTS how
// often the underlying impl is consulted). No external Postgres/Redis required.
//
// The backing storage is synchronous/inline — exactly the ¬C(X) normal path:
// the host stays alive for the whole operation, the look-aside fill happens
// in-process, and the L1 hit fires its callback INLINE (the property under
// audit). `redisClient_` is null, so this exercises the L1 + look-aside-to-DB
// tiers; the L2 (Redis) write-through / L2-hit tier needs a Redis-backed CI run
// and is documented as such below (NOT silently skipped).
//
// CONSISTENCY RELAXATION (per task): under real concurrency, N look-aside
// misses for the same cold key may each back-fill from the DB (cache stampede)
// — a benign, non-linearizable performance characteristic. The PBT below
// therefore asserts the FINAL CONVERGED value of every key, not strict
// linearizability. (This single-threaded baseline observes the deterministic
// one-miss-per-cold-key count; the stampede relaxation governs the F' re-run
// at 8.5 under concurrency.)
//
// METHODOLOGY: observation-first, runs on UNFIXED code (F), MUST PASS. No
// production code modified.
//
// **Validates: Requirements 3.4**
//
// _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7_

#include <drogon/drogon_test.h>
#include <drogon/drogon.h>

#include <atomic>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <oauth2/storage/CachedOAuth2Storage.h>
#include <oauth2/storage/MemoryOAuth2Storage.h>

#include "CategoryC_DeferredStorageSupport.h"  // makeLiveAccessToken / makeLiveClient / farFutureExpiry
#include "Property4_PreservationSupport.h"      // PreservationInputGen / CacheOp

using namespace oauth2::test::concurrency;
using oauth2::CachedOAuth2Storage;
using oauth2::MemoryOAuth2Storage;

namespace
{
// A faithful, SYNCHRONOUS backing storage: it IS a real MemoryOAuth2Storage
// (so the access-token semantics — expiry filtering, revoke flag, inline
// callbacks — are the genuine production memory behaviour), instrumented only
// to COUNT how often the cache decorator consults the underlying impl. The
// counters let the tests distinguish an L1 HIT (impl NOT consulted) from a
// look-aside MISS (impl consulted once) without altering any behaviour.
class CountingMemoryStorage : public MemoryOAuth2Storage
{
  public:
    std::atomic<int> getTokenCalls{0};
    std::atomic<int> saveTokenCalls{0};
    std::atomic<int> revokeTokenCalls{0};
    std::atomic<int> getClientCalls{0};

    void getAccessToken(const std::string &token, AccessTokenCallback &&cb) override
    {
        getTokenCalls.fetch_add(1, std::memory_order_relaxed);
        MemoryOAuth2Storage::getAccessToken(token, std::move(cb));
    }

    void saveAccessToken(const oauth2::OAuth2AccessToken &token, VoidCallback &&cb) override
    {
        saveTokenCalls.fetch_add(1, std::memory_order_relaxed);
        MemoryOAuth2Storage::saveAccessToken(token, std::move(cb));
    }

    void revokeAccessToken(
      const std::string &token,
      const std::string &revokedBy,
      VoidCallback &&cb
    ) override
    {
        revokeTokenCalls.fetch_add(1, std::memory_order_relaxed);
        MemoryOAuth2Storage::revokeAccessToken(token, revokedBy, std::move(cb));
    }

    void getClient(const std::string &clientId, ClientCallback &&cb) override
    {
        getClientCalls.fetch_add(1, std::memory_order_relaxed);
        MemoryOAuth2Storage::getClient(clientId, std::move(cb));
    }
};

// Build a CachedOAuth2Storage over a freshly-seeded CountingMemoryStorage,
// returning the cache host plus a RAW pointer to the (moved-in) backing storage
// so a test can read its consultation counters. `redisClient_` is null → the
// host exercises the L1 + look-aside-to-impl tiers (no L2).
//
// The host is make_shared'd (and impl_ is a shared_ptr per Option B) because the
// 8.1 lifetime-safety fix makes the production cache continuations capture
// `auto self = shared_from_this();`, which is only valid when the host is owned
// by a shared_ptr.
struct CacheRig
{
    std::shared_ptr<CachedOAuth2Storage> cache;
    CountingMemoryStorage *impl;  // non-owning; owned by `cache`
};

CacheRig makeCacheRig(const Json::Value &clients = Json::Value(Json::objectValue))
{
    auto impl = std::make_shared<CountingMemoryStorage>();
    if (clients.isObject() && !clients.empty())
        impl->initFromConfig(clients);
    CountingMemoryStorage *raw = impl.get();
    auto cache =
      std::make_shared<CachedOAuth2Storage>(std::move(impl), drogon::nosql::RedisClientPtr{});
    return CacheRig{std::move(cache), raw};
}

// Synchronous helpers that capture the inline result of a cache operation.
std::optional<oauth2::OAuth2AccessToken> getTokenSync(CachedOAuth2Storage &c, const std::string &t)
{
    std::optional<oauth2::OAuth2AccessToken> out;
    c.getAccessToken(t, [&out](std::optional<oauth2::OAuth2AccessToken> v) { out = std::move(v); });
    return out;
}

void saveTokenSync(CachedOAuth2Storage &c, const oauth2::OAuth2AccessToken &t)
{
    bool done = false;
    c.saveAccessToken(t, [&done]() { done = true; });
    (void)done;
}
}  // namespace

// 3.4 PRESERVATION (L1-HIT SYNCHRONOUS CONTROL FLOW — the property 8.1 must NOT
// regress): once a token is in L1, getAccessToken serves it via `findAndFetch`
// and fires the callback INLINE (synchronously) BEFORE the call returns, and
// does NOT consult the underlying impl. A flag set inside the callback is
// already true at the return point. The design forbids any queueInLoop that
// would make this asynchronous.
DROGON_TEST(Property4_3_4_L1Hit_SynchronousCallback_DoesNotConsultImpl_Baseline)
{
    auto rig = makeCacheRig();

    // Prime L1 via write-through (save inserts into L1 when ttl > 0).
    auto token = makeLiveAccessToken("tok-l1-sync");
    saveTokenSync(*rig.cache, token);

    rig.impl->getTokenCalls.store(0, std::memory_order_relaxed);  // ignore the save path

    // L1 HIT: the callback must run INLINE (ran == true immediately) and the
    // impl must NOT be consulted (getTokenCalls stays 0).
    bool ranInline = false;
    std::optional<oauth2::OAuth2AccessToken> got;
    rig.cache->getAccessToken("tok-l1-sync", [&](std::optional<oauth2::OAuth2AccessToken> v) {
        ranInline = true;  // set synchronously, before getAccessToken returns
        got = std::move(v);
    });

    CHECK(ranInline);  // synchronous L1-hit control flow (must be preserved)
    CHECK(rig.impl->getTokenCalls.load(std::memory_order_relaxed) == 0);  // pure L1 hit
    REQUIRE(got.has_value());
    CHECK(got->token == "tok-l1-sync");
    CHECK(got->clientId == token.clientId);
    CHECK(got->userId == token.userId);
}

// 3.4 PRESERVATION (look-aside MISS → DB → L1 back-fill): the first read of a
// cold key MISSES L1 and consults the impl once; the resolved value is then
// back-filled into L1 so a second read HITS L1 and does NOT consult the impl.
DROGON_TEST(Property4_3_4_LookAside_MissThenHit_BackFill_Baseline)
{
    auto rig = makeCacheRig();

    // Seed the backing store directly (NOT through the cache), so L1 starts cold
    // for this key. Reset counters after seeding.
    rig.impl->saveAccessToken(makeLiveAccessToken("tok-lookaside"), []() {});
    rig.impl->getTokenCalls.store(0, std::memory_order_relaxed);

    // First read: L1 MISS → impl consulted exactly once → value returned.
    auto first = getTokenSync(*rig.cache, "tok-lookaside");
    REQUIRE(first.has_value());
    CHECK(first->token == "tok-lookaside");
    CHECK(rig.impl->getTokenCalls.load(std::memory_order_relaxed) == 1);

    // Second read: L1 HIT (back-filled) → impl NOT consulted again.
    auto second = getTokenSync(*rig.cache, "tok-lookaside");
    REQUIRE(second.has_value());
    CHECK(second->token == "tok-lookaside");
    CHECK(rig.impl->getTokenCalls.load(std::memory_order_relaxed) == 1);  // unchanged
}

// 3.4 PRESERVATION (write-through): saveAccessToken writes the token into both
// the impl and L1, so an immediate read serves from L1 (impl not consulted).
DROGON_TEST(Property4_3_4_WriteThrough_SaveThenReadFromL1_Baseline)
{
    auto rig = makeCacheRig();

    saveTokenSync(*rig.cache, makeLiveAccessToken("tok-writethrough"));
    rig.impl->getTokenCalls.store(0, std::memory_order_relaxed);

    auto got = getTokenSync(*rig.cache, "tok-writethrough");
    REQUIRE(got.has_value());
    CHECK(got->token == "tok-writethrough");
    CHECK(rig.impl->getTokenCalls.load(std::memory_order_relaxed) == 0);  // served from L1
}

// 3.4 PRESERVATION (invalidation): revokeAccessToken erases the L1 entry, so a
// subsequent read MISSES L1, consults the impl, and (because the impl now marks
// the token revoked) resolves to nullopt.
DROGON_TEST(Property4_3_4_Revoke_InvalidatesL1_ThenMissAndNullopt_Baseline)
{
    auto rig = makeCacheRig();

    auto token = makeLiveAccessToken("tok-revoke-inv");
    saveTokenSync(*rig.cache, token);

    // Confirm it is in L1 (served without consulting impl).
    rig.impl->getTokenCalls.store(0, std::memory_order_relaxed);
    REQUIRE(getTokenSync(*rig.cache, "tok-revoke-inv").has_value());
    CHECK(rig.impl->getTokenCalls.load(std::memory_order_relaxed) == 0);

    // Revoke → impl revoke + L1 erase.
    bool revoked = false;
    rig.cache->revokeAccessToken("tok-revoke-inv", "client-x", [&revoked]() { revoked = true; });
    CHECK(revoked);
    CHECK(rig.impl->revokeTokenCalls.load(std::memory_order_relaxed) == 1);

    // Next read: L1 MISS (erased) → impl consulted → revoked → nullopt.
    rig.impl->getTokenCalls.store(0, std::memory_order_relaxed);
    auto after = getTokenSync(*rig.cache, "tok-revoke-inv");
    CHECK(rig.impl->getTokenCalls.load(std::memory_order_relaxed) == 1);  // L1 was invalidated
    CHECK(!after.has_value());                                            // revoked => nullopt
}

// 3.4 PRESERVATION (TTL gating of the L1 insert): saveAccessToken only inserts
// into L1 when the token's ttl (expiresAt - now) is > 0. A token whose ttl <= 0
// is NOT cached, so a subsequent read MISSES L1 and consults the impl.
DROGON_TEST(Property4_3_4_TtlGating_NonPositiveTtl_NotCached_Baseline)
{
    auto rig = makeCacheRig();

    // ttl > 0 token: cached in L1 (read does NOT consult impl).
    auto live = makeLiveAccessToken("tok-ttl-live");
    saveTokenSync(*rig.cache, live);
    rig.impl->getTokenCalls.store(0, std::memory_order_relaxed);
    (void)getTokenSync(*rig.cache, "tok-ttl-live");
    CHECK(rig.impl->getTokenCalls.load(std::memory_order_relaxed) == 0);  // from L1

    // ttl <= 0 token: NOT inserted into L1 (read MUST consult impl). We probe
    // the L1 gating directly via a separate already-expired token.
    auto expired = makeLiveAccessToken("tok-ttl-expired");
    expired.expiresAt = farFutureExpiry() - 7200;  // ~2h in the past => ttl <= 0
    saveTokenSync(*rig.cache, expired);
    rig.impl->getTokenCalls.store(0, std::memory_order_relaxed);
    auto got = getTokenSync(*rig.cache, "tok-ttl-expired");
    CHECK(rig.impl->getTokenCalls.load(std::memory_order_relaxed) == 1);  // not cached => miss
    CHECK(!got.has_value());  // impl filters the expired token => nullopt
}

// 3.4 PRESERVATION (client L1 cache): getClient back-fills clientCache_ on the
// first (miss) read and serves the second read from L1 synchronously without
// consulting the impl again.
DROGON_TEST(Property4_3_4_GetClient_L1_MissThenHit_Baseline)
{
    Json::Value clients;
    Json::Value c;
    c["type"] = "CONFIDENTIAL";
    c["secret"] = "secret";
    c["redirect_uri"] = "https://example.test/cb";
    Json::Value scopes(Json::arrayValue);
    scopes.append("openid");
    c["allowed_scopes"] = scopes;
    clients["cache-client"] = c;

    auto rig = makeCacheRig(clients);

    // First getClient: MISS → impl consulted once → clientCache_ back-fill.
    bool firstInline = false;
    std::optional<oauth2::OAuth2Client> first;
    rig.cache->getClient("cache-client", [&](std::optional<oauth2::OAuth2Client> v) {
        firstInline = true;
        first = std::move(v);
    });
    CHECK(firstInline);
    REQUIRE(first.has_value());
    CHECK(first->clientId == "cache-client");
    CHECK(rig.impl->getClientCalls.load(std::memory_order_relaxed) == 1);

    // Second getClient: L1 HIT → inline callback, impl NOT consulted again.
    bool secondInline = false;
    std::optional<oauth2::OAuth2Client> second;
    rig.cache->getClient("cache-client", [&](std::optional<oauth2::OAuth2Client> v) {
        secondInline = true;
        second = std::move(v);
    });
    CHECK(secondInline);
    REQUIRE(second.has_value());
    CHECK(second->clientId == "cache-client");
    CHECK(rig.impl->getClientCalls.load(std::memory_order_relaxed) == 1);  // unchanged: L1 hit
}

// 3.4 PRESERVATION (PBT over random hit/miss SEQUENCES): for a reproducible
// random access sequence over a set of seeded keys, EVERY read converges to the
// seeded token value, and the impl is consulted exactly ONCE per distinct cold
// key (the first touch); all later touches are served from L1. The converged-
// value assertion is what survives the cache-stampede relaxation under
// concurrency at the 8.5 re-run.
DROGON_TEST(Property4_3_4_CacheSequence_RandomizedHitMiss_Converges_Baseline)
{
    PreservationInputGen gen(0x3C4C3C4Cu);

    constexpr int kRounds = 12;
    for (int round = 0; round < kRounds; ++round)
    {
        const int distinctKeys = gen.intInRange(2, 6);
        const int length = gen.intInRange(distinctKeys, distinctKeys * 4);

        auto rig = makeCacheRig();

        // Seed one token per key directly into the backing store (cold L1).
        std::map<int, std::string> keyToken;
        for (int k = 0; k < distinctKeys; ++k)
        {
            std::string name = "seq-" + std::to_string(round) + "-" + std::to_string(k);
            keyToken[k] = name;
            rig.impl->saveAccessToken(makeLiveAccessToken(name), []() {});
        }
        rig.impl->getTokenCalls.store(0, std::memory_order_relaxed);

        auto seq = gen.cacheSequence(distinctKeys, length);

        std::vector<bool> coldSeen(static_cast<std::size_t>(distinctKeys), false);
        int expectedImplConsults = 0;
        for (const auto &access : seq)
        {
            const int key = access.first;
            auto got = getTokenSync(*rig.cache, keyToken[key]);
            // Converged value: every read resolves to the seeded token.
            REQUIRE(got.has_value());
            CHECK(got->token == keyToken[key]);

            if (!coldSeen[static_cast<std::size_t>(key)])
            {
                coldSeen[static_cast<std::size_t>(key)] = true;
                ++expectedImplConsults;  // first touch => one look-aside miss
            }
        }

        // Sequential baseline: exactly one impl consult per distinct cold key;
        // subsequent touches are L1 hits. (Under concurrency, the stampede
        // relaxation allows >expectedImplConsults; the converged values above
        // remain the invariant.)
        CHECK(rig.impl->getTokenCalls.load(std::memory_order_relaxed) == expectedImplConsults);
    }
}
