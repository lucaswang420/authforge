# OAuth2项目测试全面优化方案 (V3.0)

> **版本**: v3.0  
> **状态**: 已评审 (Engineering Ready)  
> **日期**: 2026-05-13  
> **核心目标**: 建立确定、解耦、自动化的质量保障体系，统一测试框架，消除冗余，并建立严谨的实施路径。

---

## 📋 目录

1. [执行摘要与KPI](#1-执行摘要与kpi)
2. [依赖隔离与Mock策略 (V2.0新增)](#2-依赖隔离与mock策略)
3. [测试分类规范与组织形式](#3-测试分类规范与组织形式)
4. [命名规范与执行矩阵](#4-命名规范与执行矩阵)
5. [重复测试治理方案](#5-重复测试治理方案)
6. [覆盖范围提升与异常测试](#6-覆盖范围提升与异常测试)
7. [测试数据管理与沙盒化](#7-测试数据管理与沙盒化)
8. [CI/CD 自动化闭环](#8-cicd自动化闭环)
9. [详细实施计划](#9-详细实施计划)

---

## 1. 执行摘要与KPI

### 1.1 当前痛点诊断
*   **框架耦合**: 65% DROGON_TEST 与 28% GTEST 混用，导致 EventLoop 在 CI 环境偶发性死锁，执行机制不统一。
*   **命名与组织混乱**: 215个测试用例散落在25个文件中，缺乏统一目录结构，命名随意（如 `ValidateValidClientId`）。
*   **测试冗余**: 约 27 个重复用例（12.6% 重复率），增加了30%的维护成本。
*   **依赖不清**: 单元测试直接连接本地DB，导致测试环境高度敏感且运行缓慢。
*   **信心不足**: 平均覆盖率 50%，缺乏异常路径测试，代码在生产环境的边缘Case表现不可控。

### 1.2 优化 KPI
*   **底座统一**: 100% 迁移至 `Drogon Test`。
*   **架构清晰**: 100% 遵守标准目录结构和命名规范。
*   **代码精简**: 重复率降至 2% 以下。
*   **执行确定性**: 核心路径（P0）测试成功率 100%，消除 Flaky Tests。
*   **覆盖深度**: P0 逻辑分支覆盖率 > 95%，异常路径覆盖率 > 30%。
*   **CI效率**: 全量测试执行耗时控制在 5 分钟以内。

---

## 2. 依赖隔离与Mock策略

为了实现测试的**确定性**与**解耦**，必须严格遵循以下依赖准则：

### 2.1 依赖处理矩阵
| 测试类型 | 外部依赖 (DB/Redis/API) | 处理策略 |
| :--- | :--- | :--- |
| **Unit** | 严禁访问真实外部资源 | 使用 **Stub (桩对象)** 或 **Mock (模拟对象)** |
| **Integration** | 允许访问受控外部资源 | 使用 **Docker-compose** 启用的容器化隔离资源 |
| **E2E** | 真实/预发环境资源 | 完整链路调用，使用 **测试专用账号** |

### 2.2 技术实现：接口注入
所有 Service 和 Plugin 必须支持依赖注入（DI），禁止在内部静态硬编码存储逻辑。

```cpp
// 示例：通过接口注入实现确定性测试
DROGON_TEST(Unit_P0_AuthService_Login_WithMockStorage_Success) {
    // 1. 创建 Mock 对象 (使用 Mocking 框架或自定义 Stub)
    auto mockStorage = std::make_shared<MockOAuth2Storage>();
    
    // 设置 Mock 行为：当调用 getUser("alice") 时返回 testUser
    // EXPECT_CALL(*mockStorage, getUser("alice")).WillOnce(Return(testUser)); 
    
    // 2. 注入 Mock 依赖
    AuthService service(mockStorage);
    
    // 3. 执行并断言：即便 DB 断开，此测试也应能通过
    auto result = service.login("alice", "pass");
    CHECK(result.success == true);
}
```

---

## 3. 测试分类规范与组织形式

### 3.1 目标目录结构
重构现有的扁平结构，建立基于测试分类的深层目录：

```text
test/
├── common/                      # 通用测试基础设施
│   ├── test_categories.h       # 测试分类定义枚举
│   ├── test_helpers.h          # 测试工具类 (HttpClient封装等)
│   ├── fixtures/               # 标准测试数据集 (新增)
│   │   ├── default_clients.sql
│   │   └── test_users.json
│   └── mocks/                  # Mock对象定义 (新增)
│       └── MockOAuth2Storage.h
│
├── unit/                        # 单元测试 (快速、无依赖、Mock驱动)
│   ├── validation/             # 验证逻辑测试
│   ├── subject/                # Subject生成测试
│   ├── token/                  # Token生成逻辑测试
│   └── utils/                  # 工具函数测试
│
├── integration/                 # 集成测试 (模块协作、DB事务)
│   ├── storage/                # 存储层与真实DB集成
│   ├── auth/                   # 认证服务与存储层集成
│   └── plugin/                 # 插件级全链路内部集成
│
├── e2e/                         # E2E测试 (完整网络流程)
│   └── oauth2_flows/           # OAuth2四大授权模式流测试
│
├── performance/                 # 性能与压力测试
│   ├── load/                   # 负载并发测试
│   └── benchmark/              # 基准测试
│
├── security/                    # 安全异常测试
│   └── injection/              # 注入与越权测试
│
└── scripts/                     # 测试工具链
    ├── naming_validator.sh     # CI 命名门禁脚本
    └── generate_report.sh      # 覆盖率报告生成
```

### 3.2 核心分类定义 (test_categories.h)
系统化定义 8 大分类和 4 级优先级，作为所有测试的元数据。

```cpp
namespace oauth2::test {

// ========== 测试分类枚举 ==========
enum class TestCategory {
    UNIT,           // 单元测试：独立函数/类测试，无外部依赖 (Mock)
    INTEGRATION,    // 集成测试：模块间集成，有容器化外部依赖
    E2E,           // 端到端测试：完整业务流程，真实网络请求
    PERFORMANCE,   // 性能测试：基准和负载测试
    SECURITY,      // 安全测试：漏洞防护和异常输入测试
    API,          // API测试：接口契约合规性
    DATABASE,     // 数据库测试：Schema与迁移测试
    ACCEPTANCE    // 验收测试：业务场景验收
};

enum class TestPriority {
    P0,  // 核心功能：阻塞发布，核心链路
    P1,  // 重要功能：主要分支，边界条件
    P2,  // 一般功能：次要特性验证
    P3   // 可选功能：锦上添花
};

} // namespace oauth2::test
```

---

## 4. 命名规范与执行矩阵

### 4.1 强制命名格式
**格式**: `[Category]_[Priority]_[Module]_[Feature]_[Scenario]`

*   **Category**: 上述 `TestCategory` 之一 (如 Unit, Integration)
*   **Priority**: P0, P1, P2, P3
*   **Module**: 被测模块的 PascalCase (如 `SubjectGenerator`, `AuthService`)
*   **Feature**: 功能点动词短语 (如 `GenerateForLocalUser`, `ValidateClientId`)
*   **Scenario**: 预期场景或结果 (如 `ReturnsCorrectFormat`, `PreventsAttack`)

**✅ 正确示例**:
*   `DROGON_TEST(Unit_P0_SubjectGenerator_GenerateForLocalUser_ReturnsCorrectFormat)`
*   `DROGON_TEST(Security_P0_Validation_SqlInjection_PreventMaliciousInput)`

**❌ 错误示例**:
*   `DROGON_TEST(ValidateValidClientId)` (无分类/优先级/模块)
*   `TEST(P0_1_SubjectMapping, ...)` (框架混用，格式错误)

### 4.2 命名检查脚本 (naming_validator.sh 节选)
将集成到 Git Pre-commit Hook 中：
```bash
#!/bin/bash
# 验证当前目录下所有 .cc 文件中的 DROGON_TEST 命名
invalid_names=$(grep -rh "DROGON_TEST(" . --include="*.cc" | \
    sed 's/.*DROGON_TEST(\([^)]*\)).*/\1/' | \
    grep -vE '^(Unit|Integration|E2E|Performance|Security|API|Database|Acceptance)_P[0-3]_')

if [ -n "$invalid_names" ]; then
    echo "❌ 命名规范检查失败! 发现以下不合规测试:"
    echo "$invalid_names"
    echo "期望格式: [Category]_[Priority]_[Module]_[Feature]_[Scenario]"
    exit 1
fi
echo "✅ 命名规范检查通过"
```

---

## 5. 重复测试治理方案

### 5.1 识别出的主要重复模式
分析显示共有 27 个重复用例，主要集中在以下区域：
1.  **验证类测试重复 (15个)**: 分散在 `ValidatorTest.cc`, `ValidationHelperTest.cc`, `SecurityValidationTest.cc` 中的 `ClientId` 和 `ClientSecret` 验证。
2.  **Subject 生成重复 (8个)**: 分散在 `SubjectMappingTest.cc` 和 `P0FunctionalityTest.cc`。

### 5.2 合并策略与实施示例
**策略**: 使用结构体数组（Table-Driven Tests）合并功能相同但输入数据不同的测试。

**合并前 (ClientId验证)**:
```cpp
// 散落的 6+ 个测试
DROGON_TEST(ValidateValidClientId) { ... }
DROGON_TEST(ValidateInvalidClientId) { ... }
DROGON_TEST(ValidatorHelper_ValidateClientId_Empty) { ... }
// ...
```

**合并后 (统一的 Data-Driven 测试)**:
```cpp
// 放置于 test/unit/validation/ValidationTest.cc
DROGON_TEST(Unit_P0_Validation_ClientId_AllScenarios_ComprehensiveValidation) {
    struct ClientIdTestCase {
        std::string clientId;
        bool shouldBeValid;
        std::string description;
    };
    
    std::vector<ClientIdTestCase> testCases = {
        {"my-client", true, "Simple valid ID"},
        {"client_123", true, "With numbers"},
        {"invalid@client!", false, "Special characters rejected"},
        {"", false, "Empty ID rejected"},
        {std::string(100, 'a'), false, "Too long ID rejected"}
    };
    
    for (const auto& tc : testCases) {
        auto result = Validator::validateClientId(tc.clientId);
        DROGON_CHECK_EQ(result.isValid, tc.shouldBeValid) 
            << "Failed: " << tc.description << " [" << tc.clientId << "]";
    }
}
```

### 5.3 清理计划优先级
*   **W3**: 清理 P0 级严重的验证类重复 (ValidatorTest 家族)。
*   **W4**: 清理 P1 级 Subject 生成与存储接口的冗余。

---

## 6. 覆盖范围提升与异常测试

### 6.1 分层覆盖率目标
*   **P0 核心流程**: 95%+ 覆盖率 (如 `OAuth2Controller` Token 生成逻辑)。
*   **P1 重要功能**: 80%+ 覆盖率。
*   **异常路径 (Chaos/Negative)**: 新增专门的安全与异常测试，覆盖率要求 > 30%。

### 6.2 必须补充的关键测试 (含异常场景)

**Controllers 层缺少的核心与异常测试**:
*   `Unit_P0_OAuth2Controller_GenerateCode_StateParameter`: 验证 State 的防跨站回传。
*   `Integration_P0_OAuth2Controller_TokenEndpoint_RateLimitExceeded`: **(异常测试)** 模拟超载请求，验证 429 返回。
*   `Integration_P0_OAuth2Controller_TokenExchange_RedirectUriMismatch_ReturnsError`: **(RFC 6749 安全要求)** 验证 Token 请求中的 Redirect URI 必须与授权请求一致。
*   `Unit_P0_OAuth2Controller_Authorize_ScopeValidation_Tier1_ClientAllowlist`: 验证客户端请求的作用域是否在允许列表中。

**Plugins & Services 核心业务逻辑补充**:
*   `Unit_P0_OAuth2Plugin_ValidateClientScopes_RestrictsToAllowlist`: 客户端作用域权限校验。
*   `Unit_P0_OAuth2Plugin_ValidateUserRolesForScopes_AdminScopeProtection`: 验证管理员作用域仅允许特定角色的用户使用。
*   `Integration_P0_OAuth2Plugin_UserConsent_SaveAndRetrieve`: 验证用户授权（Consent）的保存与读取逻辑。
*   `Integration_P1_OAuth2Plugin_TokenIntrospection_RFC7662_Success`: **(令牌内省)** 验证 RFC 7662 令牌信息查询接口。
*   `Integration_P1_OAuth2Plugin_TokenRevocation_RFC7009_AuditTrail`: **(令牌撤销)** 验证令牌主动撤销及审计记录。

**Security 分类的异常注入测试**:
```cpp
DROGON_TEST(Security_P0_Validation_SqlInjection_PreventMaliciousInput) {
    std::vector<std::string> maliciousInputs = {
        "admin' OR '1'='1",
        "admin'; DROP TABLE users; --"
    };
    
    auto client = HttpClient::newHttpClient("http://localhost:5555");
    for (const auto& payload : maliciousInputs) {
        auto req = HttpRequest::newHttpRequest();
        req->setPath("/oauth2/login");
        req->setParameter("username", payload); // 注入点
        
        auto resp = client->sendRequest(req);
        // 必须被安全拦截，绝不能返回 200 或造成服务 Crash
        DROGON_CHECK_TRUE(resp->statusCode() == k400BadRequest || resp->statusCode() == k401Unauthorized);
    }
}
```

**并发竞争测试 (Flaky 预防)**:
*   `Performance_P1_TokenGeneration_ConcurrentRequests_NoDeadlock`: 验证 100 线程并发请求同一个授权码，数据库事务隔离级别是否正确处理。

---

## 7. 测试数据管理与沙盒化

### 7.1 数据库沙盒化 (Sandboxing)
解决“测试相互干扰”与“每次必须清空数据库”的问题：

1.  **单元测试 (Unit)**: 允许使用 DB。优先使用 `SQLite :memory:` 或 **事务回滚机制**。
2.  **集成测试 (Integration)**: 必须包裹在事务中，并在结束时强制 `ROLLBACK`。

```cpp
// 示例：测试专用的事务包装器
DROGON_TEST(Integration_P0_Storage_SaveToken_DataPersisted) {
    auto dbClient = drogon::app().getDbClient("postgres");
    
    // 开启测试事务
    dbClient->execSqlSync("BEGIN"); 
    
    // ... 执行保存逻辑 ...
    
    // 验证逻辑
    
    // 强制回滚，保持 DB 干净
    dbClient->execSqlSync("ROLLBACK"); 
}
```

### 7.2 标准化 Fixtures
禁止在代码中到处散落硬编码的测试账号。统一在 `test/common/fixtures/test_users.json` 中维护：
```json
{
  "users": [
    {"username": "admin_test", "role": "admin", "expected_subject": "local:admin_test"},
    {"username": "guest_test", "role": "guest", "expected_subject": "local:guest_test"}
  ]
}
```
测试启动时自动加载该配置库。

---

## 8. CI/CD 自动化闭环

### 8.1 CI 门禁检查点 (Gatekeepers)
将测试工具链固化为 CI Pipeline (如 GitHub Actions) 必须通过的节点：

1.  **Code Style & Naming Gate**:
    运行 `naming_validator.sh` 确保命名合规。
2.  **Build & Unit Tests (Fast Path)**:
    运行无依赖的 Unit 目录测试，耗时要求 < 1 分钟。
3.  **Integration & DB Tests (Dockerized)**:
    拉起 PostgreSQL 和 Redis 容器，执行集成测试。
4.  **Coverage Gate**:
    使用 `LCOV` 提取覆盖率。若 PR 导致核心模块覆盖率下降，Pipeline 标记为 **Failed**。

### 8.2 质量看板 (Dashboard)
维护 `docs/test/dashboard.md`：
*   **Flaky Tests**: 自动统计在 CI 中时过时不过的测试，挂 P0 级 Issue 修复。
*   **慢测试黑名单**: 耗时 > 500ms 的 Unit 测试必须被重构为 Mock 测试。

---

## 9. 详细实施计划 (9周)

| 阶段 | 周次 | 核心任务与产出物 | 负责人视角 |
| :--- | :--- | :--- | :--- |
| **Phase 1: 基础设施建设** | W1 | 建立新的深层目录结构 (`test/unit`, `test/integration` 等)。 | 架构调整 |
| | W2 | 引入 Mocking 框架，建立 `test/common/fixtures` 数据集，配置好 SQLite/PG 沙盒化环境。 | 底座夯实 |
| **Phase 2: 存量治理与迁移** | W3 | 彻底移除 GTEST，将现存混用测试全部改写为 `DROGON_TEST`。执行旧测试的批量重命名脚本。 | 统一规范 |
| | W4 | 按“表驱动”模式合并 27 个冗余的验证和 Subject 测试。启用 `naming_validator.sh` 本地钩子。 | 消除技术债 |
| **Phase 3: 核心分支冲刺** | W5 | 补充 Controller 层 P0 级测试 (涵盖授权流的核心分支)。 | 业务覆盖 |
| | W6 | 补充 Plugin 和 Service 层的接口层集成测试。核心路径覆盖率达成 95%。 | 链路覆盖 |
| **Phase 4: 健壮性与异常测试** | W7 | 编写 Security 分类的 SQL注入、XSS 预防用例。 | 边界防御 |
| | W8 | 编写 Performance 与并发竞争用例（验证死锁处理和降级逻辑）。异常路径覆盖率 > 30%。 | 稳定性防御 |
| **Phase 5: 自动化与度量** | W9 | 完善 GitHub Actions CI 配置（包含容器起停、门禁脚本）。生成第一份可交互 HTML 覆盖率报告与质量看板。 | 研发效能闭环 |

---

## 附录：现有测试用例迁移映射表 (Migration Map)

为确保 215 个存量测试平稳迁移，以下是核心模块的“旧 -> 新”映射指南：

### A. 验证与逻辑类 (Unit Test 优先)

| 旧文件 / 测试集 | 存量数 | 新目录 | 新命名建议 (示例) | 备注 |
| :--- | :--- | :--- | :--- | :--- |
| `ValidatorTest.cc` | 3 | `test/unit/validation/` | `Unit_P0_Validation_ClientId_...` | 合并冗余 |
| `ValidationHelperTest.cc` | 6 | `test/unit/validation/` | `Unit_P0_Validation_ClientId_...` | 与 ValidatorTest 合并 |
| `SubjectMappingTest.cc` | 6 | `test/unit/subject/` | `Unit_P0_Subject_Generator_...` | 统一为数据驱动模式 |
| `StringUtilsTest.cc` | 4 | `test/unit/utils/` | `Unit_P2_Utils_String_...` | 保持纯单元测试 |
| `CryptoUtilsTest.cc` | 5 | `test/unit/utils/` | `Unit_P0_Utils_Crypto_...` | 核心安全算法 |

### B. 存储与数据类 (Integration Test 优先)

| 旧文件 / 测试集 | 存量数 | 新目录 | 新命名建议 (示例) | 备注 |
| :--- | :--- | :--- | :--- | :--- |
| `MemoryStorageTest.cc` | 6 | `test/integration/storage/` | `Integration_P1_Storage_Memory_...` | 使用沙盒回滚 |
| `PostgresStorageTest.cc`| 8 | `test/integration/storage/` | `Integration_P0_Storage_Postgres_...`| 核心存储验证 |
| `RedisStorageTest.cc` | 4 | `test/integration/storage/` | `Integration_P1_Storage_Redis_...` | 验证缓存一致性 |
| `AdvancedStorageTest.cc`| 2 | `test/integration/storage/` | `Integration_P1_Storage_Mixed_...` | 跨层存储协作 |

### C. 控制器与插件类 (E2E / Integration 混合)

| 旧文件 / 测试集 | 存量数 | 新目录 | 新命名建议 (示例) | 备注 |
| :--- | :--- | :--- | :--- | :--- |
| `OAuth2ControllerTest.cc`| 12 | `test/integration/controller/` | `Integration_P0_OAuth2_Token_...` | 涉及 DB 协作 |
| `AdminControllerTest.cc` | 8 | `test/integration/controller/` | `Integration_P1_Admin_Role_...` | 权限管理验证 |
| `OAuth2PluginTest.cc` | 10 | `test/integration/plugin/` | `Integration_P0_Plugin_AuthFlow_...` | 核心插件逻辑 |
| `MetricsPluginTest.cc` | 5 | `test/integration/plugin/` | `Integration_P2_Plugin_Metrics_...` | 性能监控数据 |
| `LoginFlowTest.cc` | 15 | `test/e2e/oauth2_flows/` | `E2E_P0_Auth_Login_...` | 完整业务流 |

### D. 混合与特殊类 (待拆解)

| 旧文件 / 测试集 | 存量数 | 处理策略 | 目标位置 |
| :--- | :--- | :--- | :--- |
| `P0FunctionalityTest.cc` | 2 | **拆解**: 将 Subject 逻辑移至 Unit，流程逻辑移至 E2E | `unit/subject/` & `e2e/flows/` |
| `SecurityValidationTest.cc`| 2 | **重命名并合并**: 归类到 Security 分类 | `test/security/injection/` |
| `ConfigManagerTest.cc` | 4 | **保持 Unit**: 仅测试解析逻辑 | `test/unit/config/` |

---
*本文档为 OAuth2 Backend 测试体系的最终执行纲领。自发布之日起，所有新增 PR 必须遵守本文档的命名与 Mock 规范。*