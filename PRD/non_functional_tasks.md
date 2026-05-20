# 非功能性完善任务清单 (Post P0+P1)

> 创建日期: 2026-05-20  
> 目标: P0+P1 功能实施后的质量保障、文档、CI/CD 和测试基础设施完善

---

## 一、优先级总览

| 优先级 | 方向 | 任务数 | 预计工期 |
|--------|------|--------|----------|
| P0-Critical | 基础设施修复 (CI/Docker 脚本适配新 schema) | 2 | 1h |
| P1-High | E2E 测试覆盖核心接口 | 3 | 3-4h |
| P1-High | OpenAPI 规范更新 | 1 | 2h |
| P2-Medium | 单元/集成测试补充 | 4 | 4-6h |
| P2-Medium | CI/CD 流水线完善 | 2 | 2h |
| P3-Low | 文档更新 | 3 | 2-3h |

---

## 二、P0-Critical: 基础设施修复

### NF-1: full_test_docker.bat 适配新 migration 结构

**问题**: `full_test_docker.bat` 仍引用旧的 `sql/001_oauth2_core.sql` 等文件，不会创建 P0+P1 新增的表（password_reset_tokens, email_verification_tokens, audit_logs, mfa 列等）。

**改动**:
- 将 Step 2 的 4 条 `docker exec ... < sql/00x.sql` 替换为循环执行 `sql/migrations/V*.sql`
- 之后执行 `sql/seed/*.sql`

**文件**: `scripts/backend/full_test_docker.bat`

---

### NF-2: CI Linux workflow 适配新 migration 结构

**问题**: `.github/workflows/ci-linux.yml` 使用旧 SQL 文件初始化 DB。

**改动**:
- 将 DB 初始化步骤改为按顺序执行 `OAuth2Server/sql/migrations/V*.sql`
- 之后执行 `OAuth2Server/sql/seed/*.sql`

**文件**: `.github/workflows/ci-linux.yml`

---

## 三、P1-High: E2E 测试覆盖

### NF-3: test-oauth2-endpoints.bat 新增核心接口测试

**当前覆盖**: Health, Login, Token Exchange, UserInfo, Admin Dashboard (5 个)

**需新增**:

| # | 测试 | 端点 | 验证点 |
|---|------|------|--------|
| 6 | Client Credentials | POST /oauth2/token | grant_type=client_credentials → 返回 access_token, 无 refresh_token |
| 7 | Token Refresh | POST /oauth2/token | grant_type=refresh_token → 新 access_token + 新 refresh_token |
| 8 | OIDC Discovery | GET /.well-known/openid-configuration | 返回 issuer, jwks_uri, 支持的 scopes |
| 9 | JWKS | GET /.well-known/jwks.json | 返回 keys 数组, kty=RSA, alg=RS256 |
| 10 | Token Introspection | POST /oauth2/introspect | active=true, 包含 sub/scope/exp |
| 11 | Token Revocation | POST /oauth2/revoke | 200 OK, 之后 introspect 返回 active=false |
| 12 | User Registration | POST /api/register | 200, 新用户可登录 |
| 13 | User Profile | GET /api/me | 返回 username, email, mfa_enabled |

**文件**: `scripts/backend/test-oauth2-endpoints.bat`

---

### NF-4: 新增 test-p1-features.bat (P1 功能专项测试)

**独立脚本**, 测试 P1 新增的复杂流程:

| # | 测试 | 流程 |
|---|------|------|
| 1 | 密码重置 | request → 从日志获取 token → confirm → 旧 token 失效 |
| 2 | MFA 设置 | setup → 获取 secret → 生成 TOTP → verify → 启用成功 |
| 3 | 用户自服务 | GET /api/me → PUT /api/me/password → 旧 token 失效 |
| 4 | Admin API | GET /api/admin/clients → POST /api/admin/clients → 验证创建 |
| 5 | id_token 验证 | login(scope=openid) → token → 验证 id_token 有 3 段 JWT |

**文件**: `scripts/backend/test-p1-features.bat` (新建)

---

### NF-5: 创建 CONFIDENTIAL 测试客户端 seed

**问题**: E2E 测试 client_credentials 需要 CONFIDENTIAL 客户端，当前 seed 只有 PUBLIC 的 vue-client。

**改动**: 在 `sql/seed/` 新增 `dev_backend_client.sql`

```sql
INSERT INTO oauth2_clients (client_id, client_type, client_secret, salt, name, redirect_uris, allowed_grant_types)
VALUES ('backend-svc', 'CONFIDENTIAL', '<sha256(test-secret+test-salt)>', 'test-salt', 'Backend Service', '', 'client_credentials')
ON CONFLICT (client_id) DO NOTHING;
```

**文件**: `OAuth2Server/sql/seed/dev_backend_client.sql` (新建)

---

## 四、P1-High: OpenAPI 规范更新

### NF-6: openapi.yaml 补充 P0+P1 新端点

**缺失端点** (共 18 个):

**身份生命周期**:
- `POST /api/password-reset/request`
- `POST /api/password-reset/confirm`
- `GET /api/verify-email`
- `POST /api/verify-email/resend`

**MFA**:
- `POST /api/me/mfa/setup`
- `POST /api/me/mfa/verify`
- `POST /api/me/mfa/disable`
- `POST /oauth2/mfa/verify`

**Admin API**:
- `GET /api/admin/clients`
- `POST /api/admin/clients`
- `GET /api/admin/users`
- `GET /api/admin/scopes`

**用户自服务**:
- `GET /api/me`
- `PUT /api/me/password`
- `GET /api/me/authorized-apps`
- `DELETE /api/me/authorized-apps/{clientId}`

**OIDC**:
- `GET /.well-known/openid-configuration`
- `GET /.well-known/jwks.json`

**还需更新**:
- `/oauth2/token`: 添加 `client_credentials` grant_type 和 `id_token` 响应字段
- `/oauth2/userinfo`: `sub` 示例改为 UUID 格式
- `/health`: 添加 `/health/live` 和 `/health/ready`

**文件**: `OAuth2Server/openapi.yaml`

---

## 五、P2-Medium: 单元/集成测试补充

### NF-7: PasswordHasher 单元测试

**测试点**:
- `hash()` 输出格式: `$pbkdf2-sha256$310000$<hex>$<hex>`
- `verify()` 正确密码 → true
- `verify()` 错误密码 → false
- `verify()` legacy SHA-256 格式 → true (向后兼容)
- `needsRehash()` PBKDF2 → false, SHA-256 → true

**文件**: `OAuth2Server/test/unit/utils/PasswordHasherTest.cc` (新建)

---

### NF-8: TotpUtils 单元测试

**测试点**:
- `generateSecret()` 返回 32 字符 base32
- `generateCode()` 返回 6 位数字
- `verifyCode()` 当前 code → true
- `verifyCode()` 错误 code → false
- `generateBackupCodes()` 返回 10 个 8 字符码

**文件**: `OAuth2Server/test/unit/utils/TotpUtilsTest.cc` (新建)

---

### NF-9: TokenService 集成测试 (RT Reuse Detection)

**测试点**:
- 正常刷新: RT_A → RT_B (A 被撤销, B 有效)
- 重用检测: 再次使用 RT_A → invalid_grant + family 级联撤销
- 并发刷新: 两个并发请求用同一 RT → 只有一个成功

**文件**: `OAuth2Server/test/integration/token/RefreshTokenReuseTest.cc` (新建)

---

### NF-10: CryptoUtils 单元测试

**测试点**:
- `generateSecureToken()` 返回 43 字符 base64url
- `generateSecureToken()` 两次调用结果不同
- `hashToken()` 返回 64 字符 hex
- `hashToken()` 相同输入 → 相同输出
- `hashToken()` 不同输入 → 不同输出

**文件**: `OAuth2Server/test/unit/utils/CryptoUtilsTest.cc` (新建)

---

## 六、P2-Medium: CI/CD 完善

### NF-11: CI 添加 OpenAPI 验证步骤

**改动**: 在 Linux CI 中添加 `validate-openapi.sh` 执行步骤

```yaml
- name: Validate OpenAPI Spec
  run: |
    npm install -g @apidevtools/swagger-cli
    swagger-cli validate OAuth2Server/openapi.yaml
```

**文件**: `.github/workflows/ci-linux.yml`

---

### NF-12: CI Windows workflow 更新

**问题**: Windows CI 可能未适配新的 Conan 依赖和 OpenSSL 链接。

**改动**:
- 确认 `find_package(OpenSSL)` 在 CI 环境中能找到
- 确认 `common/utils/*.cc` 被正确编译

**文件**: `.github/workflows/ci-windows.yml`

---

## 七、P3-Low: 文档更新

### NF-13: 更新 api_reference.md

**改动**: 添加所有 P1 新端点的 API 参考文档（请求/响应格式、错误码、示例）

**文件**: `docs/backend/api_reference.md`

---

### NF-14: 更新 security_architecture.md

**改动**:
- Token 安全: 描述 hash 存储策略
- 密码哈希: 描述 PBKDF2-SHA256 (310K iterations) + 渐进迁移
- RT Reuse Detection: 描述 family 追踪 + 级联撤销
- MFA: 描述 TOTP 实现和备份码策略

**文件**: `docs/backend/security_architecture.md`

---

### NF-15: 新增 OIDC 集成指南

**内容**:
- Discovery 端点说明
- JWKS 端点说明
- id_token 格式和验证方法
- 使用标准 OIDC 客户端库接入示例

**文件**: `docs/backend/oidc_guide.md` (新建)

---

## 八、实施顺序建议

```
Phase 1 (立即): NF-1, NF-2 — 修复 CI/Docker 脚本 (1h)
Phase 2 (当天): NF-3, NF-5 — E2E 测试核心接口 (2h)
Phase 3 (次日): NF-6 — OpenAPI 更新 (2h)
Phase 4 (次日): NF-7, NF-8, NF-10 — 单元测试 (3h)
Phase 5 (后续): NF-9, NF-11, NF-12 — 集成测试 + CI (2h)
Phase 6 (后续): NF-4, NF-13, NF-14, NF-15 — 文档 + P1 专项测试 (3h)
```

---

## 九、PROGRESS.md 需更新

当前 PROGRESS.md 仍显示 P1 为 5/11，实际已全部完成。需要更新为最终状态。
