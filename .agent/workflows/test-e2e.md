---
description: 执行完整的端到端测试流程 (DB -> Build -> Test -> Endpoints)
---

# E2E 端到端测试

项目提供了一键式的 E2E 测试脚本，用于验证从数据库持久化到 API 端点的完整链路。

## 1. 运行一键测试 (本地)

使用 `full_test.bat` 脚本，它会依次重置数据库、生成模型、编译项目并运行所有单元及端点测试。

```powershell
.\scripts\backend\full_test.bat
```

## 2. 运行一键测试 (Docker)

如果你偏好在 Docker 环境下进行测试（验证 C++ 库在容器内的表现），请使用：

```powershell
.\scripts\backend\full_test_docker.bat
```

## 3. 手动执行 E2E 步骤

如果你需要精确控制测试流程，可以手动按顺序执行：

### Step A: 启动并初始化环境
```powershell
.\scripts\backend\docker_postgres_start.bat
.\scripts\backend\setup_database.bat
.\scripts\backend\generate_models.bat -y
```

### Step B: 编译并启动服务器
```powershell
.\scripts\backend\build.bat -release
# 在新窗口运行
.\scripts\backend\run_server.bat -release
```

### Step C: 运行端点测试脚本
```powershell
.\scripts\backend\test-oauth2-endpoints.bat
```

## 4. 验证检查点

- **DB 初始化**: 检查 `oauth2_clients` 记录。
- **Token 交换**: 确保 `/oauth2/token` 返回有效的 JWT 或随机字符串 token。
- **RBAC 拦截**: 验证 `/api/admin/dashboard` 在无 token 或角色不匹配时返回 403。
- **清理**: 服务器在测试结束后应正常停止，无 SegFault 报错。
