// OAuth2Server/test/integration/concurrency/Property4_CleanupLockBaselineTest.cc
//
// Spec: concurrency-lifetime-safety-audit — Task 4 (Property 4: Preservation).
// Behavioral Equivalence on ¬C(X). Observation object **3.6**: when
// OAuth2CleanupService fires, the "acquired-the-lock-then-clean / no-lock
// single-instance-clean" semantics must be preserved across the category-C fix
// (8.3 aligns runCleanup() to weak_from_this(); the fix must NOT change WHICH
// branch runs cleanup, only make the dangling-`this` access safe).
//
// ─────────────────────────────────────────────────────────────────────────
// WHAT IS PINNED HERE (and the honest CI caveat)
// ─────────────────────────────────────────────────────────────────────────
// Two things make up the 3.6 semantics, captured at two faithfulness levels:
//
//   (1) THE REAL CLEANUP EFFECT (no model): every branch of
//       OAuth2CleanupService::runCleanup() ultimately calls
//       `storage_->deleteExpiredData()`. We pin that genuine effect against the
//       REAL production `MemoryOAuth2Storage::deleteExpiredData()`: expired
//       access/refresh tokens and auth codes are removed, while still-live
//       tokens are preserved. This is the actual, externally observable
//       business outcome of a cleanup run and needs no external service.
//
//   (2) THE LOCK-DECISION BRANCH STRUCTURE (faithful local model): production
//       runCleanup() decides as follows —
//         * Redis available + `SET NX EX` returns a value  → lock ACQUIRED   → clean.
//         * Redis available + `SET NX EX` returns nil      → lock NOT acquired → SKIP.
//         * Redis available but the command errors         → single-instance  → clean.
//         * Redis NOT configured (getRedisClient throws)   → single-instance  → clean.
//       `runCleanup()` is PRIVATE and only reachable via the `runEvery` timer,
//       and the lock branches only execute with a REAL Redis client. On this
//       host (no Redis; see Task 0) driving the genuine private method through
//       all four branches is not possible, so we mirror its decision structure
//       byte-for-byte in `CleanupLockDecisionModel` and assert each branch
//       cleans / skips exactly as production does. A faithful end-to-end
//       capture of the real `runCleanup()` lock branches requires a
//       **Redis-backed CI build** — documented here rather than fabricated.
//
// METHODOLOGY: observation-first, runs on UNFIXED code (F), MUST PASS. No
// production code modified.
//
// **Validates: Requirements 3.6**
//
// _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7_

#include <drogon/drogon_test.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>

#include <oauth2/storage/MemoryOAuth2Storage.h>

#include "Property4_PreservationSupport.h"

using namespace oauth2::test::concurrency;
using oauth2::MemoryOAuth2Storage;

namespace
{
int64_t nowSeconds()
{
    return static_cast<int64_t>(
      std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
      )
        .count()
    );
}

oauth2::OAuth2AccessToken accessToken(const std::string &name, int64_t expiresAt, bool revoked)
{
    oauth2::OAuth2AccessToken t;
    t.token = name;
    t.clientId = "test-client";
    t.userId = "alice";
    t.scope = "openid";
    t.expiresAt = expiresAt;
    t.revoked = revoked;
    return t;
}

// Faithful mirror of OAuth2CleanupService::runCleanup()'s DECISION STRUCTURE.
// `RedisOutcome` models what the production `SET oauth2:cleanup:lock ... NX EX`
// command yields; each value drives the SAME branch the production code takes.
// The model holds a raw storage pointer exactly like the production class.
enum class RedisOutcome
{
    Unavailable,    // getRedisClient threw → catch → single-instance clean
    CommandError,   // execCommandAsync error callback → single-instance clean
    LockAcquired,   // SET NX returned a value → lock acquired → clean
    LockNotAcquired // SET NX returned nil → another instance running → SKIP
};

class CleanupLockDecisionModel
{
  public:
    explicit CleanupLockDecisionModel(oauth2::IOAuth2Storage *storage) : storage_(storage) {}

    // Mirrors runCleanup() branch-for-branch. Returns true iff a cleanup ran.
    bool runCleanup(RedisOutcome outcome)
    {
        if (!running_ || !storage_)  // production entry guard
            return false;

        switch (outcome)
        {
            case RedisOutcome::LockNotAcquired:
                // r.isNil() → "another instance is running" → return (no clean).
                return false;

            case RedisOutcome::LockAcquired:
                // lock acquired → proceed with cleanup.
                storage_->deleteExpiredData();
                return true;

            case RedisOutcome::CommandError:
                // Redis error callback → run cleanup anyway (single instance).
                storage_->deleteExpiredData();
                return true;

            case RedisOutcome::Unavailable:
                // getRedisClient threw → catch → run cleanup without lock.
                storage_->deleteExpiredData();
                return true;
        }
        return false;
    }

  private:
    oauth2::IOAuth2Storage *storage_;  // raw pointer, as in production
    bool running_{true};
};

// A storage that counts deleteExpiredData() invocations on top of the REAL
// MemoryOAuth2Storage cleanup behaviour.
class CountingCleanupStorage : public MemoryOAuth2Storage
{
  public:
    std::atomic<int> cleanups{0};

    void deleteExpiredData() override
    {
        cleanups.fetch_add(1, std::memory_order_relaxed);
        MemoryOAuth2Storage::deleteExpiredData();
    }
};
}  // namespace

// 3.6 PRESERVATION (REAL cleanup effect): deleteExpiredData() removes expired
// tokens and preserves live ones — the genuine outcome every cleanup branch
// produces. Pinned against the real MemoryOAuth2Storage.
DROGON_TEST(Property4_3_6_DeleteExpiredData_RemovesExpired_PreservesLive_Baseline)
{
    CountingCleanupStorage storage;
    const int64_t now = nowSeconds();

    // Seed: one live (far-future) and one expired access token.
    storage.saveAccessToken(accessToken("live-tok", now + 3600, false), []() {});
    storage.saveAccessToken(accessToken("expired-tok", now - 3600, false), []() {});

    // Sanity: live resolves, expired already filtered by getAccessToken.
    bool liveBefore = false;
    storage.getAccessToken("live-tok", [&](auto v) { liveBefore = v.has_value(); });
    CHECK(liveBefore);

    // Run the real cleanup.
    storage.deleteExpiredData();
    CHECK(storage.cleanups.load(std::memory_order_relaxed) == 1);

    // Live token survives; expired token is gone (and stays nullopt).
    bool liveAfter = false;
    storage.getAccessToken("live-tok", [&](auto v) { liveAfter = v.has_value(); });
    CHECK(liveAfter);

    bool expiredAfter = false;
    storage.getAccessToken("expired-tok", [&](auto v) { expiredAfter = v.has_value(); });
    CHECK(!expiredAfter);
}

// 3.6 PRESERVATION (lock AVAILABLE → ACQUIRED → clean): when the distributed
// lock is acquired, cleanup runs exactly once.
DROGON_TEST(Property4_3_6_LockAcquired_RunsCleanup_Baseline)
{
    CountingCleanupStorage storage;
    CleanupLockDecisionModel svc(&storage);

    const bool cleaned = svc.runCleanup(RedisOutcome::LockAcquired);
    CHECK(cleaned);
    CHECK(storage.cleanups.load(std::memory_order_relaxed) == 1);
}

// 3.6 PRESERVATION (lock AVAILABLE → NOT acquired → SKIP): when another
// instance holds the lock, this instance does NOT clean.
DROGON_TEST(Property4_3_6_LockNotAcquired_SkipsCleanup_Baseline)
{
    CountingCleanupStorage storage;
    CleanupLockDecisionModel svc(&storage);

    const bool cleaned = svc.runCleanup(RedisOutcome::LockNotAcquired);
    CHECK(!cleaned);
    CHECK(storage.cleanups.load(std::memory_order_relaxed) == 0);  // skipped
}

// 3.6 PRESERVATION (lock UNAVAILABLE → single-instance clean): when Redis is not
// configured (getRedisClient throws), cleanup runs anyway (single-instance mode).
DROGON_TEST(Property4_3_6_RedisUnavailable_SingleInstanceCleanup_Baseline)
{
    CountingCleanupStorage storage;
    CleanupLockDecisionModel svc(&storage);

    const bool cleaned = svc.runCleanup(RedisOutcome::Unavailable);
    CHECK(cleaned);
    CHECK(storage.cleanups.load(std::memory_order_relaxed) == 1);
}

// 3.6 PRESERVATION (Redis command ERROR → single-instance clean): when the lock
// command itself errors, production falls back to single-instance cleanup.
DROGON_TEST(Property4_3_6_RedisCommandError_SingleInstanceCleanup_Baseline)
{
    CountingCleanupStorage storage;
    CleanupLockDecisionModel svc(&storage);

    const bool cleaned = svc.runCleanup(RedisOutcome::CommandError);
    CHECK(cleaned);
    CHECK(storage.cleanups.load(std::memory_order_relaxed) == 1);
}

// 3.6 PRESERVATION (PBT over random lock-availability sequences): for a
// reproducible random sequence of lock outcomes, the number of cleanups that
// run equals the number of "clean" outcomes (acquired / unavailable / error),
// and "lock not acquired" outcomes are always skipped — the exact 3.6 semantics.
DROGON_TEST(Property4_3_6_RandomizedLockOutcomes_CleanupCountMatches_Baseline)
{
    PreservationInputGen gen(0x36AC36ACu);

    constexpr int kRounds = 8;
    for (int round = 0; round < kRounds; ++round)
    {
        CountingCleanupStorage storage;
        CleanupLockDecisionModel svc(&storage);

        const int fires = gen.intInRange(4, 16);
        int expectedCleanups = 0;
        for (int i = 0; i < fires; ++i)
        {
            // Random outcome across the four production branches.
            RedisOutcome outcome;
            switch (gen.intInRange(0, 3))
            {
                case 0: outcome = RedisOutcome::LockAcquired; break;
                case 1: outcome = RedisOutcome::LockNotAcquired; break;
                case 2: outcome = RedisOutcome::Unavailable; break;
                default: outcome = RedisOutcome::CommandError; break;
            }
            const bool cleaned = svc.runCleanup(outcome);
            if (outcome == RedisOutcome::LockNotAcquired)
                CHECK(!cleaned);
            else
            {
                CHECK(cleaned);
                ++expectedCleanups;
            }
        }

        CHECK(storage.cleanups.load(std::memory_order_relaxed) == expectedCleanups);
    }
}
