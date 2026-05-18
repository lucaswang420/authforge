---
description: 执行单元测试和集成测试 (CTest)
---

# 测试执行流程

## 1. 快速执行 (推荐)

使用 `scripts/backend/test.bat` 脚本。该脚本会自动检测环境并在标准配置及 CI 配置下运行两轮测试，确保代码在不同环境下的兼容性。

```powershell
# 运行全部 Release 模式测试
.\scripts\backend\test.bat -release
```

## 2. 手动执行

如果需要更细粒度的控制，可以直接在 `build` 目录下运行 `ctest` 或直接运行测试可执行文件。

### A. 确认构建产物存在
```powershell
Test-Path "build\OAuth2Server\test\Release\OAuth2Test_test.exe"
```

### B. 运行所有测试 (CTest)
```powershell
cd build
ctest -C Release --output-on-failure
```

### C. 直接运行测试程序 (查看详细输出)
```powershell
cd build\OAuth2Server\test\Release
.\OAuth2Test_test.exe
```

## 3. 运行特定模块测试

你可以使用 `-r` (run) 参数来指定运行特定的测试用例：

```powershell
# 仅运行数据库存储测试
.\OAuth2Test_test.exe -r PostgresStorageTest

# 仅运行缓存存储测试
.\OAuth2Test_test.exe -r RedisStorageTest

# 仅运行 OAuth2 核心逻辑测试
.\OAuth2Test_test.exe -r PluginTest
```

## 测试结果分类

| 测试用例名称 | 类型 | 依赖说明 |
|---------|------|------|
| ConfigTest | 单元测试 | 无 |
| MemoryStorageTest | 单元测试 | 无 |
| HodorTest | 单元测试 | 无 |
| RedisStorageTest | 集成测试 | 需要运行中的 Redis |
| PostgresStorageTest | 集成测试 | 需要运行中的 PostgreSQL |
| PluginTest | 集成测试 | 需要运行中的 PostgreSQL/Redis |
| IntegrationE2E | E2E测试 | 完整集成环境 |
