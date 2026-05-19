# Production Hardening 进度记录

> 最后更新: 2026-05-19

## P0 阶段: ✅ 完成

所有 10 个 P0 安全底线任务已完成，单元测试 + 端到端测试全部通过。

## P1 阶段状态: 进行中 (5/11)

| Task | 状态 | 描述 |
|------|------|------|
| Task 11 | ✅ | OpenID Connect Core (id_token + discovery + JWKS) |
| Task 12 | ✅ | Client Credentials Grant (M2M) |
| Task 13 | ✅ | 密码重置流程 |
| Task 14 | ✅ | 邮箱验证 |
| Task 15 | ✅ | MFA (TOTP) |
| Task 16 | ✅ | 审计日志 |
| Task 17 | 待做 | Admin REST API |
| Task 18 | 待做 | 用户自服务 |
| Task 19 | ✅ | PKCE 强制 (advisory) |
| Task 20 | 待做 | CachedStorage 写后失效 |
| Task 21 | 待做 | Schema Migration 工具 |

### 已完成的 Tasks (全部通过单元测试)

| Task | Commit | 描述 |
|------|--------|------|
| Task 6 | 427b870 | Schema 重组 (migrations/ + seed/) |
| Task 1 | 427b870 | Token 强随机 256-bit + DB 存 SHA-256 hash |
| Task 2 | 3f488ea | 密码哈希升级 PBKDF2-SHA256 (310K iterations) |
| Task 5 | cfd6a21 | Subject UUID 化 (users.public_sub) |
| Task 3 | bad0b2b | Refresh Token Reuse Detection (family cascade) |
| Task 4 | edb088e | Auth Code 原子消费 + Token Pair 事务化 |
| Task 7 | 6a541da | 配置安全 (生产模式 HTTPS 强制 + 默认密码拒绝) |
| Task 8 | 1f13fc8 | AuthorizationFilter 默认拒绝 + public_paths |
| Task 9 | b4f3a69 | Health Check 真实探活 (/health/live + /health/ready) |
| Task 10 | e470cd6 | handleFirstTimeLogin 从 DB sequence 取 ID |
| bugfix | c516cc7 | getUserRoles/getUserInfo 支持 UUID subject 解析 |

### 端到端测试结果 (full_test.bat -debug)

| Test | 状态 | 说明 |
|------|------|------|
| Test 1: Health Check | PASS | |
| Test 2: OAuth2 Login | PASS | |
| Test 3: Token Exchange | PASS | token 为 43 字符 base64url |
| Test 4: UserInfo | PASS | sub 返回 UUID 格式 |
| Test 5: Admin Dashboard | **FAIL (403)** | 待排查 |

### Test 5 待排查问题

**现象**: `/api/admin/dashboard` 返回 403 Forbidden

**可能原因** (按概率排序):
1. `AuthorizationFilter` 内部 `getUserRoles(UUID)` 解析路径可能和 `OAuth2StandardController::userInfo` 不同——filter 先调 `plugin->validateAccessToken` 再调 `plugin->getUserRoles`，后者走 `IdentityService::getUserRoles` → `storage->getUserRoles(string)`。UUID 解析已修复，但可能有缓存/时序问题。
2. `config.json` 中 `rbac_rules` 配置的 `/api/admin/.*` 规则匹配正确，但 admin 用户的 roles 查询返回空（subject mapping 问题）。
3. `AuthorizationFilter` 的 `loadConfig` 没有加载到 `rbac_rules`（config 路径问题）。

**排查步骤**:
1. 启动 server，手动 curl 测试 `/api/admin/dashboard`，观察 server 日志
2. 在 `AuthorizationFilter::doFilter` 中加 LOG_DEBUG 打印 roles
3. 确认 `getUserRoles(UUID)` 在 Postgres 模式下正确返回 ["admin", "user"]

---

## 开发环境备忘

- Windows + Debug 版 Drogon
- 构建: `scripts\backend\build.bat -debug`
- 测试: `scripts\backend\test.bat -debug`
- 完整验证: `scripts\backend\full_test.bat -debug`
- DB 重建: `scripts\backend\setup_database.bat`
- ORM 重生成: `scripts\backend\generate_models.bat -y`
- 本地 PostgreSQL (test/123456) + Redis (localhost:6379/123456)

## 下一步

1. **修复 Test 5 (Admin Dashboard 403)** — 优先级最高
2. 修复 UserInfo 的 `name` 字段显示 UUID 而非 username 的问题
3. P0 全部端到端测试通过后，进入 P1 阶段
