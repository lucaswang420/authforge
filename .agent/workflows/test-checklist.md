---
description: 测试前置检查清单与测试执行流程
---

# 测试前置检查清单

## 一、基础环境检查

### 1. 配置文件检查

```powershell
if (Test-Path "OAuth2Server/config.json") { Write-Host "✅ 源码配置存在" }
if (Test-Path "build/OAuth2Server/Release/config.json") { Write-Host "✅ 构建目录配置存在" }
```

### 2. 数据库服务检查 (PostgreSQL)

```powershell
# 检查 Docker 容器是否运行
docker ps | Select-String "oauth2-postgres"
# 验证连接
psql -h localhost -U test -d oauth_test -c "SELECT 1;"
```

### 3. Redis 服务检查

```powershell
docker ps | Select-String "oauth2-redis"
# 验证连接
redis-cli -a 123456 ping
```

---

## 二、运行 CTest 自动化测试

项目提供了 `scripts/backend/test.bat` 脚本，它会自动执行标准配置测试以及 CI 配置下的集成测试。

```powershell
# 运行全部 Release 模式测试
.\scripts\backend\test.bat -release
```

**测试结果预期:**
```
[1/2] Running tests with standard config.json...
100% tests passed, 0 tests failed out of XX

[2/2] Running tests with config.ci.json...
100% tests passed, 0 tests failed out of XX
```

---

## 三、故障排查 (Troubleshooting)

- **数据库未初始化**: 运行 `.\scripts\backend\setup_database.bat`。
- **ORM 模型过时**: 运行 `.\scripts\backend\generate_models.bat -y`。
- **构建产物未更新**: 运行 `.\scripts\backend\build.bat -release`。
- **Redis 密码错误**: 默认密码为 `123456`，请检查 `config.json` 中的 `passwd` 字段。
