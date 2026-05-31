// OAuth2Server/test/integration/concurrency/Property4_TokenFlowBaselineTest.cc
//
// Spec: concurrency-lifetime-safety-audit �?Task 4 (Property 4: Preservation).
// Behavioral Equivalence on ¬C(X). Observation objects **3.2** and **3.5**:
//   3.2 �?authorization-code exchange, token refresh, validate, introspect and
//         revoke in the NORMAL (post-init, non-teardown) timing must return the
//         SAME success responses, error codes and JSON structure across the
//         category-C fixes (8.x, re-verified at 8.5).
//   3.5 �?within the object lifetime (no destruction) the async callbacks run
//         normally and produce the SAME business side effects (token persisted,
//         then queryable; revoked token no longer validates).
//
// ─────────────────────────────────────────────────────────────────────────
// FAITHFULNESS / WHY MemoryOAuth2Storage
// ─────────────────────────────────────────────────────────────────────────
// These baselines exercise the REAL production services (TokenService /
// IdentityService) over the REAL in-process `MemoryOAuth2Storage`, which invokes
// its callbacks SYNCHRONOUSLY / inline. That is exactly the ¬C(X) normal path:
// the issuing object stays alive for the whole async chain, so the business
// callbacks complete and their side effects are observable in-process �?no
// external Postgres/Redis required. (The destroy-before-callback race is the
// C(X) hazard covered by Task 3; here we pin the LIVE-object behavior.)
//
// Token VALUES are random by design (generateSecureToken), so the baseline pins
// the response SHAPE / error codes / JSON structure and the persisted-then-
// queryable invariant �?NOT the random token bytes. This is the stable,
// F-vs-F'-comparable contract.
//
// PBT: a fixed-seed generator (PreservationInputGen) drives random-but-
// reproducible client / scope / token / grant combinations through the service
// operations; every generated normal input must yield the frozen shape.
//
// METHODOLOGY: observation-first, runs on UNFIXED code (F), MUST PASS. No
// production code modified.
//
// **Validates: Requirements 3.2, 3.5**
//
// _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7_

#include <drogon/drogon_test.h>
#include <json/json.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>

#include <oauth2/services/TokenService.h>
#include <oauth2/services/IdentityService.h>
#include <oauth2/storage/MemoryOAuth2Storage.h>
#include <oauth2/utils/JwkManager.h>

#include "Property4_PreservationSupport.h"

using namespace oauth2::test::concurrency;
using oauth2::IdentityService;
using oauth2::MemoryOAuth2Storage;
using oauth2::TokenService;

namespace
{
// A storage seeded with one CONFIDENTIAL client ("test-client"/"secret") and an
// admin role mapping �?enough to drive the full authorization-code �?token flow.
// Returns a shared_ptr because the production services now take shared ownership
// of the storage (defect 1.3 fix: TokenService/IdentityService hold
// std::shared_ptr<IOAuth2Storage>).
std::shared_ptr<MemoryOAuth2Storage> makeSeededStorage()
{
    auto storage = std::make_shared<MemoryOAuth2Storage>();

    Json::Value clients;
    Json::Value c;
    c["type"] = "CONFIDENTIAL";
    c["secret"] = "secret";
    c["redirect_uri"] = "https://example.test/cb";
    Json::Value scopes(Json::arrayValue);
    scopes.append("openid");
    scopes.append("profile");
    scopes.append("email");
    c["allowed_scopes"] = scopes;
    clients["test-client"] = c;

    Json::Value admins;  // userId "alice" -> ["user"]
    Json::Value roles(Json::arrayValue);
    roles.append("user");
    admins["alice"] = roles;

    storage->initFromConfig(clients, admins);
    return storage;
}

// Drive generateAuthorizationCode synchronously and return the raw code.
std::string issueAuthCode(
  TokenService &svc,
  const std::string &clientId,
  const std::string &subject,
  const std::string &scope,
  const std::string &redirectUri,
  const std::string &nonce = "")
{
    std::string rawCode;
    bool ok = false;
    svc.generateAuthorizationCode(
      clientId, subject, scope, redirectUri, "", "", nonce,
      [&](bool success, std::string code, std::string /*err*/) {
          ok = success;
          rawCode = std::move(code);
      });
    (void)ok;
    return rawCode;
}
}  // namespace

// 3.2 / 3.5 PRESERVATION: authorization_code exchange happy path returns the
// frozen JSON shape and persists the token (queryable via validateAccessToken).
DROGON_TEST(Property4_3_2_ExchangeCode_HappyPath_Shape_Baseline)
{
    auto storage = makeSeededStorage();
    auto svc = std::make_shared<TokenService>(storage);

    const std::string redirectUri = "https://example.test/cb";
    std::string rawCode =
      issueAuthCode(*svc, "test-client", "alice", "openid profile", redirectUri);
    REQUIRE(!rawCode.empty());

    Json::Value result;
    bool called = false;
    svc->exchangeCodeForToken(
      rawCode, "test-client", "secret", redirectUri, "",
      [&](const Json::Value &json) {
          result = json;
          called = true;
      });

    REQUIRE(called);
    // Frozen success shape (3.2):
    REQUIRE(result.isMember("access_token"));
    CHECK(!result["access_token"].asString().empty());
    CHECK(result["token_type"].asString() == "Bearer");
    CHECK(result["expires_in"].asInt64() == 3600);
    REQUIRE(result.isMember("refresh_token"));
    CHECK(!result["refresh_token"].asString().empty());
    REQUIRE(result.isMember("roles"));
    CHECK(result["roles"].isArray());
    // No id_token without a JwkManager configured.
    CHECK(!result.isMember("id_token"));
    // No error fields on success.
    CHECK(!result.isMember("error"));

    // 3.5 side effect: the issued access token is persisted and now validates.
    std::shared_ptr<oauth2::OAuth2AccessToken> validated;
    bool vcalled = false;
    svc->validateAccessToken(result["access_token"].asString(), [&](auto at) {
        validated = at;
        vcalled = true;
    });
    REQUIRE(vcalled);
    REQUIRE(validated != nullptr);
    CHECK(validated->clientId == "test-client");
    CHECK(validated->userId == "alice");
    CHECK(validated->scope == "openid profile");
}

// 3.3-adjacent / 3.2: with a JwkManager configured and an "openid" scope, the
// exchange additionally returns a non-empty id_token. (The JWKS/id_token
// signing invariants themselves are pinned in Property4_JwkBaselineTest.cc.)
DROGON_TEST(Property4_3_2_ExchangeCode_WithOpenId_IssuesIdToken_Baseline)
{
    auto storage = makeSeededStorage();
    auto svc = std::make_shared<TokenService>(storage);

    auto jwk = std::make_shared<oauth2::JwkManager>();
    REQUIRE(jwk->init(Json::Value(Json::objectValue)) == true);  // ephemeral dev key
    svc->setJwkManager(jwk);

    const std::string redirectUri = "https://example.test/cb";
    std::string rawCode =
      issueAuthCode(*svc, "test-client", "alice", "openid email", redirectUri, "nonce-xyz");
    REQUIRE(!rawCode.empty());

    Json::Value result;
    bool called = false;
    svc->exchangeCodeForToken(
      rawCode, "test-client", "secret", redirectUri, "",
      [&](const Json::Value &json) {
          result = json;
          called = true;
      });

    REQUIRE(called);
    REQUIRE(result.isMember("id_token"));
    // A JWT has the header.payload.signature shape (two '.').
    const std::string idt = result["id_token"].asString();
    CHECK(!idt.empty());
    CHECK(std::count(idt.begin(), idt.end(), '.') == 2);
}

// 3.2 PRESERVATION (error codes): the frozen error envelope { error,
// error_description } for the standard failure timings.
DROGON_TEST(Property4_3_2_ExchangeCode_ErrorCodes_Baseline)
{
    auto storage = makeSeededStorage();
    auto svc = std::make_shared<TokenService>(storage);
    const std::string redirectUri = "https://example.test/cb";

    // (a) invalid_client: wrong secret.
    {
        std::string rawCode = issueAuthCode(*svc, "test-client", "alice", "openid", redirectUri);
        Json::Value r;
        svc->exchangeCodeForToken(rawCode, "test-client", "WRONG", redirectUri, "",
                                  [&](const Json::Value &j) { r = j; });
        CHECK(r["error"].asString() == "invalid_client");
        CHECK(r["error_description"].asString() == "Client authentication failed");
    }
    // (b) invalid_grant: unknown / already-consumed code.
    {
        Json::Value r;
        svc->exchangeCodeForToken("does-not-exist", "test-client", "secret", redirectUri, "",
                                  [&](const Json::Value &j) { r = j; });
        CHECK(r["error"].asString() == "invalid_grant");
        CHECK(r["error_description"].asString() == "Invalid authorization code");
    }
    // (c) invalid_client: client id mismatch (code issued to a different client).
    //     The code is valid but exchanged under the wrong (also valid) client.
    {
        // Seed a second confidential client so validateClient passes for it.
        // (Re-seed storage to include "other-client".)
        auto storage2 = std::make_shared<MemoryOAuth2Storage>();
        Json::Value clients;
        Json::Value a;
        a["type"] = "CONFIDENTIAL";
        a["secret"] = "secret";
        a["redirect_uri"] = redirectUri;
        clients["test-client"] = a;
        Json::Value b;
        b["type"] = "CONFIDENTIAL";
        b["secret"] = "secret2";
        b["redirect_uri"] = redirectUri;
        clients["other-client"] = b;
        storage2->initFromConfig(clients, Json::Value::nullSingleton());

        auto svc2 = std::make_shared<TokenService>(storage2);
        std::string rawCode = issueAuthCode(*svc2, "test-client", "alice", "openid", redirectUri);
        Json::Value r;
        svc2->exchangeCodeForToken(rawCode, "other-client", "secret2", redirectUri, "",
                                   [&](const Json::Value &j) { r = j; });
        CHECK(r["error"].asString() == "invalid_client");
        CHECK(r["error_description"].asString() == "Client ID mismatch");
    }
}

// 3.2 / 3.5 PRESERVATION: refresh happy path returns the frozen shape; reusing
// the rotated (now-revoked) refresh token yields the frozen invalid_grant error
// (F's actual behavior: memory getRefreshToken filters revoked tokens, so the
// second attempt resolves to "Invalid or revoked refresh token").
DROGON_TEST(Property4_3_2_RefreshToken_HappyPathAndReuse_Baseline)
{
    auto storage = makeSeededStorage();
    auto svc = std::make_shared<TokenService>(storage);
    const std::string redirectUri = "https://example.test/cb";

    // Mint an initial token pair via the code exchange.
    std::string rawCode = issueAuthCode(*svc, "test-client", "alice", "openid", redirectUri);
    Json::Value exchanged;
    svc->exchangeCodeForToken(rawCode, "test-client", "secret", redirectUri, "",
                              [&](const Json::Value &j) { exchanged = j; });
    REQUIRE(exchanged.isMember("refresh_token"));
    const std::string rt = exchanged["refresh_token"].asString();

    // First refresh: frozen success shape.
    Json::Value refreshed;
    bool called = false;
    svc->refreshAccessToken(rt, "test-client", [&](const Json::Value &j) {
        refreshed = j;
        called = true;
    });
    REQUIRE(called);
    REQUIRE(refreshed.isMember("access_token"));
    CHECK(!refreshed["access_token"].asString().empty());
    CHECK(refreshed["token_type"].asString() == "Bearer");
    CHECK(refreshed["expires_in"].asInt64() == 3600);
    REQUIRE(refreshed.isMember("refresh_token"));
    CHECK(!refreshed["refresh_token"].asString().empty());
    CHECK(!refreshed.isMember("error"));

    // Reusing the ORIGINAL (now rotated/revoked) refresh token -> invalid_grant.
    Json::Value reuse;
    bool rcalled = false;
    svc->refreshAccessToken(rt, "test-client", [&](const Json::Value &j) {
        reuse = j;
        rcalled = true;
    });
    REQUIRE(rcalled);
    CHECK(reuse["error"].asString() == "invalid_grant");
    CHECK(reuse["error_description"].asString() == "Invalid or revoked refresh token");
}

// 3.2 PRESERVATION: introspection returns active=true with the frozen field set
// for a live token, and active=false for an unknown token.
DROGON_TEST(Property4_3_2_Introspect_ActiveAndInactive_Baseline)
{
    auto storage = makeSeededStorage();
    auto svc = std::make_shared<TokenService>(storage);
    const std::string redirectUri = "https://example.test/cb";

    std::string rawCode =
      issueAuthCode(*svc, "test-client", "alice", "openid profile", redirectUri);
    Json::Value exchanged;
    svc->exchangeCodeForToken(rawCode, "test-client", "secret", redirectUri, "",
                              [&](const Json::Value &j) { exchanged = j; });
    const std::string accessToken = exchanged["access_token"].asString();

    // Active token.
    std::optional<oauth2::TokenIntrospection> active;
    svc->introspectToken(accessToken, [&](auto v) { active = v; });
    REQUIRE(active.has_value());
    CHECK(active->active == true);
    CHECK(active->clientId == "test-client");
    CHECK(active->tokenType == "Bearer");
    CHECK(active->sub == "alice");
    CHECK(active->scope == "openid profile");
    CHECK(active->exp > 0);

    // Verify the introspection JSON shape too (RFC 7662 toJson()).
    Json::Value j = active->toJson();
    CHECK(j["active"].asBool() == true);
    CHECK(j["client_id"].asString() == "test-client");
    CHECK(j["token_type"].asString() == "Bearer");

    // Unknown token -> active=false.
    std::optional<oauth2::TokenIntrospection> inactive;
    svc->introspectToken("totally-unknown-token", [&](auto v) { inactive = v; });
    REQUIRE(inactive.has_value());
    CHECK(inactive->active == false);
    CHECK(inactive->toJson()["active"].asBool() == false);
}

// 3.2 / 3.5 PRESERVATION: revoke succeeds (RFC 7009 always-success), and the
// revoked token subsequently fails validation (side effect persisted).
DROGON_TEST(Property4_3_2_RevokeAccessToken_ThenInvalid_Baseline)
{
    auto storage = makeSeededStorage();
    auto svc = std::make_shared<TokenService>(storage);
    const std::string redirectUri = "https://example.test/cb";

    std::string rawCode = issueAuthCode(*svc, "test-client", "alice", "openid", redirectUri);
    Json::Value exchanged;
    svc->exchangeCodeForToken(rawCode, "test-client", "secret", redirectUri, "",
                              [&](const Json::Value &j) { exchanged = j; });
    const std::string accessToken = exchanged["access_token"].asString();

    // Token validates before revocation.
    std::shared_ptr<oauth2::OAuth2AccessToken> before;
    svc->validateAccessToken(accessToken, [&](auto at) { before = at; });
    REQUIRE(before != nullptr);

    // Revoke (always invokes the completion callback per RFC 7009).
    bool revoked = false;
    svc->revokeAccessToken(accessToken, "test-client", [&]() { revoked = true; });
    CHECK(revoked);

    // After revocation the token no longer validates.
    std::shared_ptr<oauth2::OAuth2AccessToken> after;
    bool acalled = false;
    svc->validateAccessToken(accessToken, [&](auto at) {
        after = at;
        acalled = true;
    });
    REQUIRE(acalled);
    CHECK(after == nullptr);
}

// 3.2 PRESERVATION (PBT over random client/scope/grant combos): every randomly
// generated NORMAL authorization-code exchange yields the frozen success shape.
DROGON_TEST(Property4_3_2_ExchangeCode_RandomizedCombos_ShapeStable_Baseline)
{
    PreservationInputGen gen(0x70CE0001u);
    const std::string redirectUri = "https://example.test/cb";

    constexpr int kRounds = 24;
    for (int round = 0; round < kRounds; ++round)
    {
        auto storage = makeSeededStorage();
        auto svc = std::make_shared<TokenService>(storage);

        const std::string subject = "user-" + std::to_string(gen.intInRange(1, 9999));
        const std::string scope = gen.scope();

        std::string rawCode = issueAuthCode(*svc, "test-client", subject, scope, redirectUri);
        REQUIRE(!rawCode.empty());

        Json::Value result;
        bool called = false;
        svc->exchangeCodeForToken(rawCode, "test-client", "secret", redirectUri, "",
                                  [&](const Json::Value &j) {
                                      result = j;
                                      called = true;
                                  });
        REQUIRE(called);
        // Shape invariant across all normal combos.
        CHECK(result.isMember("access_token"));
        CHECK(result["token_type"].asString() == "Bearer");
        CHECK(result["expires_in"].asInt64() == 3600);
        CHECK(result.isMember("refresh_token"));
        CHECK(result.isMember("roles"));
        CHECK(!result.isMember("error"));
    }
}

// 3.5 PRESERVATION: IdentityService subject mapping side effects complete in
// lifetime �?ensureSubjectMapping creates the mapping, and a subsequent lookup
// resolves the internal user id.
DROGON_TEST(Property4_3_5_IdentityService_SubjectMapping_SideEffect_Baseline)
{
    auto storage = makeSeededStorage();
    auto svc = std::make_shared<IdentityService>(storage);

    // Initially unmapped.
    std::optional<int32_t> before;
    bool bcalled = false;
    svc->getInternalUserId("local:bob", [&](std::optional<int32_t> v) {
        before = v;
        bcalled = true;
    });
    REQUIRE(bcalled);
    CHECK(!before.has_value());

    // ensureSubjectMapping persists the mapping (business side effect).
    bool mapped = false;
    svc->ensureSubjectMapping("local:bob", "bob", 777, [&]() { mapped = true; });
    REQUIRE(mapped);

    // Subsequent lookup resolves the mapped id.
    std::optional<int32_t> after;
    bool acalled = false;
    svc->getInternalUserId("local:bob", [&](std::optional<int32_t> v) {
        after = v;
        acalled = true;
    });
    REQUIRE(acalled);
    REQUIRE(after.has_value());
    CHECK(*after == 777);
}
