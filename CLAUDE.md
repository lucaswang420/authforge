# OAuth2 Plugin Example - Claude Code 项目规范

> 本文档基于 backend-rule.md 和 Drogon OAuth2 项目特性，为 Claude Code 提供项目级开发指导。

## 项目概述

**Drogon OAuth2.0 Provider & Vue Client Demo** - 一个功能完整的 OAuth2.0 授权服务器实现，支持本地认证和外部提供商（微信）集成。

### 技术栈
- **后端**: Drogon C++ Web Framework (C++17)
- **数据库**: PostgreSQL 14+ (主存储), Redis 7+ (缓存)
- **前端**: Vue.js 3
- **构建**: CMake 3.20+, Conan (Windows), vcpkg (可选)

### 项目结构
```
OAuth2Backend/
├── controllers/      # HTTP 请求处理层 (薄层设计)
├── filters/          # 中间件 (Token 验证, RBAC 权限检查)
├── plugins/          # 核心业务逻辑 (OAuth2Plugin, MetricsPlugin)
├── services/         # 业务服务层 (AuthService, CleanupService)
├── storage/          # 数据访问层 (Postgres, Redis, Memory, Cached)
├── models/           # ORM 生成的数据模型 (禁止修改)
├── test/             # 测试套件 (单元/集成/E2E)
└── config.json       # 主配置文件
```

---

## 一、架构规范

### [MUST] Drogon 框架优先原则
- 优先使用 Drogon 内置功能，避免引入三方库
- 引入新库必须在 PR 中说明必要性
- Drogon 库本地路径: `D:\work\development\Repos\backend\drogon`

### [MUST] 分层架构

**Controller 层** (薄层设计)
- 仅处理 HTTP 请求/响应，不含业务逻辑
- 验证请求格式，调用 Plugin/Service 层
- 返回标准 HTTP 状态码和 JSON 响应
- 文件位置: `OAuth2Backend/controllers/`

**Plugin/Service 层** (核心业务逻辑)
- Plugin 模式实现依赖注入和单例管理
- 封装可复用的业务逻辑
- 文件位置: `OAuth2Backend/plugins/`, `OAuth2Backend/services/`

**Storage 层** (数据访问)
- 接口定义: `IOAuth2Storage.h`
- 实现类: `PostgresOAuth2Storage`, `RedisOAuth2Storage`, `MemoryOAuth2Storage`, `CachedOAuth2Storage`
- 支持 Strategy 模式切换存储实现

**Model 层** (ORM 映射)
- ORM 生成的类位于 `OAuth2Backend/models/`
- **禁止修改** ORM 生成的类，变更需用 `drogon_ctl` 重新生成
- **禁止使用 raw SQL**，必须使用 ORM Mapper (特殊情况需说明必要性)

### [MUST] 异步编程规范

**回调接口优先**
1. 优先使用异步回调接口 (`Mapper::findOne`, `execSqlAsync`)
2. 其次使用同步接口 (`Mapper::findBy` with future)
3. **禁止使用协程接口** (`CoroMapper`)

**Lambda 捕获规范**
- [+] 捕获 `sharedCb`: `[sharedCb]`
- [-] 捕获裸指针: `[this]`, `[&var]`
- 如需使用裸指针，必须在 PR 中说明生命周期保障方案 (`shared_from_this`, `weak_ptr`)

---

## 二、数据访问规范

### [MUST] ORM 使用规范

**禁止使用 raw SQL 的情况**

- [-] `SELECT * FROM table WHERE condition` → 使用 `Mapper::findBy`
- [-] `INSERT INTO table VALUES (...)` → 使用 `Mapper::insert`
- [-] `UPDATE table SET ...` → 使用 `Mapper::update`
- [-] JOIN 查询 → 拆分为多个 ORM 查询或使用 `Criteria::In`

**允许使用 raw SQL 的特殊情况**

- [+] PostgreSQL `UPDATE ... RETURNING` (原子操作)
- [+] DDL 操作 (表结构变更，需用 SchemaSetup.cc)
- [+] 批量操作优化 (需说明必要性)
- [+] 测试代码清理

**Drogon ORM Mapper 完整实现示例**

以下示例展示了如何正确使用 ORM 替代 raw SQL，同时遵循异步回调规范：

```cpp
```cpp
// [+] 使用 ORM 替代 JOIN
void getUserRoles(const std::string &userId, StringListCallback &&cb) {
    auto sharedCb = std::make_shared<StringListCallback>(std::move(cb));

    // Step 1: 查询 UserRoles
    Mapper<UserRoles> urMapper(dbClient);
    urMapper.findBy(
        Criteria(UserRoles::Cols::_user_id, CompareOperator::EQ, uid),
        [sharedCb](const std::vector<UserRoles> &userRoles) {
            // Step 2: 提取 roleIds
            std::vector<int32_t> roleIds;
            for (const auto &ur : userRoles) {
                roleIds.push_back(ur.getValueOfRoleId());
            }

            // Step 3: 批量查询 Roles
            Mapper<Roles> roleMapper(dbClient);
            roleMapper.findBy(
                Criteria(Roles::Cols::_id, CompareOperator::In, roleIds),
                [sharedCb](const std::vector<Roles> &roles) {
                    std::vector<std::string> names;
                    for (const auto &role : roles) {
                        names.push_back(role.getValueOfName());
                    }
                    (*sharedCb)(names);
                },
                [sharedCb](const DrogonDbException &e) {
                    (*sharedCb)({});
                });
        },
        [sharedCb](const DrogonDbException &e) {
            (*sharedCb)({});
        });
}
```

**示例说明**：

1. [+] **Callback 生命周期管理**: 使用 `std::make_shared<StringListCallback>(std::move(cb))` 确保 callback 对象在异步操作完成前不被销毁
2. [+] **替代 JOIN 查询**: 将 `SELECT r.name FROM roles r JOIN user_roles ur` 拆分为两个 ORM 查询
3. [+] **错误处理**: 所有异步回调都有错误处理分支，确保 `(*sharedCb)(...)` 总是被调用
4. [+] **Lambda 捕获**: 所有回调都捕获 `[sharedCb]` 而非裸指针

### [MUST] 数据库连接管理
- 读写分离: `dbClientMaster_` (写), `dbClientReader_` (读)
- 连接池配置在 `config.json` 中
- 异步操作使用共享的 DbClientPtr

---

## 三、配置管理规范

### [MUST] 配置文件
- 主配置文件: `config.json`
- 环境特定: `config.dev.json`, `config.ci.json`, `config.prod.json`
- 启动时自动检查配置文件存在性

### [MUST] 敏感信息保护

- [-] 禁止明文存储密码/密钥在配置文件中
- [+] 使用环境变量覆盖: `OAUTH2_DB_PASSWORD`, `OAUTH2_REDIS_PASSWORD`
- 示例:
```json
{
  "db_client_name": "default",
  "redis_host": "127.0.0.1",
  "redis_password": "${OAUTH2_REDIS_PASSWORD}"  // 从环境变量读取
}
```

---

## 四、测试规范

### [MUST] 测试分级执行

**1. 单元测试** (每次代码变更)
```bash
cd build
ctest
```
- 快速验证核心逻辑
- 测试覆盖率目标: 80%+
- 执行时间: < 30 秒

**2. 集成测试** (API 接口级)
```bash
# Linux 完整测试
ctest -R Integration

# Windows 仅内存存储测试
ctest -R Memory
```
- 验证端到端功能
- 测试 PostgreSQL/Redis 集成 (Linux)
- 测试内存存储 (Windows)

**3. 浏览器集成测试** (重大变更后)
- OAuth2 完整流程验证
- 授权码流程
- Token 刷新流程
- RBAC 权限控制

### [MUST] 测试失败处理
- 同一错误重复 3 次后停止测试
- 分析根本原因，不要盲目重试
- 参考不同平台方案解决问题

### [MUST] 测试文件组织
- `PluginTest.cc` - Plugin 核心逻辑测试
- `FunctionalTest.cc` - OAuth2 功能测试
- `PostgresStorageTest.cc` - PostgreSQL 集成测试
- `IntegrationE2ETest.cc` - 端到端流程测试
- `RateLimiterTest.cc` - 安全功能测试

---

## 五、构建部署规范

### [MUST] Windows 构建流程

**推荐方式：使用项目脚本**
```cmd
cd OAuth2Backend\scripts
build.bat
# 或构建 Debug 版本
build.bat -debug
```

**手动构建步骤**
1. **安装依赖**: Visual Studio 2019/2022, CMake 3.20+, Conan 1.50+, Git for Windows
2. **初始化 Conan profile** (必需，仅首次):
   ```cmd
   conan profile detect --force
   ```
3. **安装 Conan 依赖**:
   ```cmd
   cd OAuth2Backend
   mkdir build && cd build
   conan install .. -s compiler="msvc" -s compiler.version=194 -s compiler.cppstd=20 -s build_type=Release --output-folder . --build=missing
   ```
4. **配置 CMake**:
   ```cmd
   cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20 -DCMAKE_TOOLCHAIN_FILE="conan_toolchain.cmake" -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
   ```
5. **编译项目**:
   ```cmd
   cmake --build . --parallel --config Release
   ```

**注意事项**:
- Conan 依赖安装必须在 CMake 配置**之前**完成
- `compiler.version=194` 对应 MSVC 2022 (MSVC v19.4)
- 如需使用 MSVC 2019，修改为 `compiler.version=192`
- C++20 标准是项目要求，不可降级到 C++17

### [MUST] Linux/macOS 构建流程
1. 安装系统依赖: PostgreSQL, Redis (可选)
2. 配置 CMake: `cmake -B build`
3. 编译: `cmake --build build`
4. 测试: `cd build && ctest`

### [MUST] 跨平台 CI/CD
- **Linux (Ubuntu 22.04)**: GCC + PostgreSQL + Redis 完整测试
- **Windows (Server 2022)**: MSVC 2022 + 内存存储测试
- **macOS (14)**: Clang + ARM64 构建验证 (测试禁用)

---

## 六、代码质量规范

### [MUST] 代码风格
- 遵循 C++17 标准
- 使用 Google C++ Style Guide (Drogon 默认)
- 行长度限制: 100 字符
- 使用 clang-format 自动格式化
- **禁止使用 emoji 字符** - 在代码、注释、文档、脚本和日志中使用 ASCII 符号替代
  - 禁止使用: checkmarks, crosses, magnifying glasses, light bulbs, targets 等 emoji 符号
  - 推荐使用: `[+]`, `[-]`, `[*]`, `[!]`, `>>>`, `===` 等 ASCII 符号
  - 原因: Windows 终端兼容性问题，跨平台显示不一致
  - 适用范围: C++ 代码、注释、PowerShell 脚本、Shell 脚本、文档、日志输出

### [MUST] 错误处理
- 所有 Drogon 异常必须捕获: `catch (const DrogonDbException &e)`
- 异步回调错误处理: 必须在失败时调用 `(*sharedCb)(errorResult)`
- 日志级别: `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN`, `LOG_ERROR`

### [MUST] 性能优化
- 优先使用异步接口，避免阻塞
- 合理使用缓存 (CachedOAuth2Storage)
- 数据库查询优化: 使用索引，避免 N+1 查询
- 连接池配置: 根据并发需求调整

---

## 七、安全规范

### [MUST] 输入验证
- 所有用户输入必须验证
- SQL 参数化查询: 使用 ORM Criteria，禁止字符串拼接
- XSS 防护: 使用 Drogon 内置 CSP 和模板转义

### [MUST] 认证授权
- OAuth2 流程严格遵守 RFC 6749
- Token 有效期管理: Access Token (1h), Refresh Token (30d)
- RBAC 权限检查: 所有受保护端点必须验证权限

### [MUST] 敏感数据保护
- 密码使用 SHA-256 + salt 哈希
- Client Secret 使用 SHA-256 + salt 存储
- 禁止日志中输出敏感信息 (密码, token)

---

## 八、开发流程规范

### [MUST] Git 提交规范

- 每个迭代完成后先更新文档
- 然后执行 `git commit`
- [+] 允许 `git commit`
- [-] **禁止 `git push`** (需人工审核后推送)

### [MUST] 调试规范
- 调试代码在问题解决后必须移除
- 使用条件日志: `LOG_DEBUG << "Debug info: " << variable;`
- 生产构建禁用 DEBUG 日志

### [MUST] 任务完成标准

- [+] 所有测试通过 (`ctest`)
- [+] 代码通过静态分析 (clang-tidy)
- [+] 跨平台 CI 构建成功
- [+] 文档更新完整

---

## 九、常见问题排查

### Issue: Mapper 异步回调崩溃
**原因**: callback 对象生命周期管理不当
**解决**: 使用 `std::make_shared<CallbackType>(std::move(cb))`

### Issue: PostgreSQL 连接超时
**原因**: 连接池配置不当或数据库未启动
**解决**: 检查 `config.json` 中的连接池配置，确保 PostgreSQL 服务运行

### Issue: macOS 测试崩溃
**原因**: Drogon 框架与 C++17/20 兼容性问题
**解决**: macOS CI 仅验证构建，测试在 Linux/Windows 执行

---

## 十、项目特定注意事项

### OAuth2 核心流程
- **授权码流程**: `/oauth/authorize` → `/oauth/callback` → `/oauth/token`
- **Token 刷新**: `/oauth/token` (grant_type=refresh_token)
- **用户认证**: `/auth/login` → 返回 JWT token

### RBAC 权限系统
- **用户-角色-权限**: users ↔ user_roles ↔ roles ↔ role_permissions ↔ permissions
- **权限检查**: `RolePermissionFilter` 中间件自动验证
- **权限缓存**: Redis 缓存用户角色，减少数据库查询

### 多存储策略
- **生产环境**: `CachedOAuth2Storage(Postgres + Redis)`
- **测试环境**: `MemoryOAuth2Storage` (快速测试)
- **开发环境**: `PostgresOAuth2Storage` 或 `RedisOAuth2Storage`

### 外部提供商集成
- **微信登录**: `WeChatController` 处理 OAuth2 流程
- **Google 登录**: `GoogleController` (示例实现)
- **扩展指南**: 参考 `OAuth2Plugin` 的 `validateClient` 方法

---

## 十一、Claude Code 使用建议

### 推荐工作流

1. **功能开发**
   ```
   1. 阅读 CLAUDE.md 和 backend-rule.md
   2. 使用 /plan 规划实现方案
   3. 编写代码遵循架构规范
   4. 运行单元测试验证
   5. 提交 PR 说明变更
   ```

2. **Bug 修复**
   ```
   1. 使用 /debug 模式分析问题
   2. 定位根本原因
   3. 修复并添加测试用例
   4. 验证所有平台构建
   ```

3. **性能优化**
   ```
   1. 使用性能分析工具 (perf, VTune)
   2. 识别瓶颈 (数据库查询, 序列化)
   3. 优化热点代码
   4. 回归测试确保功能不变
   ```

### 常用命令

```bash
# 构建项目
cmake -B build && cmake --build build

# 运行测试
cd build && ctest --output-on-failure

# 代码格式化
clang-format -i OAuth2Backend/**/*.cc OAuth2Backend/**/*.h

# 静态分析
clang-tidy OAuth2Backend/**/*.cc -- -I OAuth2Backend/

# 启动开发服务器
cd build && ./OAuth2Backend -c config.json
```

---

**文档版本**: v1.0
**最后更新**: 2026-04-24
**维护者**: OAuth2 Plugin 开发团队
