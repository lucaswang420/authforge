---
description: 在 Docker 环境下验证系统功能
---

# Docker 环境验证流程

## 1. 运行一键验证脚本 (推荐)

项目提供了 `scripts/backend/full_test_docker.bat`，它会自动执行完整的 Docker 基础设施启动、数据库初始化及端点验证。

```powershell
# 执行一键 Docker 验证
.\scripts\backend\full_test_docker.bat
```

**该流程验证：**
- Docker 容器能否正常启动并健康运行。
- `docker exec` 能否正确执行 `psql` 初始化。
- C++ 编译产物能否在映射的 Docker 环境下正常工作。
- 所有的 OAuth2 端点能否正常响应。

---

## 2. 手动分步验证

如果你需要手动启动 Docker 服务并验证：

### Step A: 启动数据库
```powershell
# 启动 PostgreSQL 和 Redis
docker-compose up -d oauth2-postgres oauth2-redis
```

### Step B: 验证容器健康
```powershell
# 检查容器状态
docker ps --filter "name=oauth2"
# 预期：oauth2-postgres 状态为 healthy，oauth2-redis 状态为 Up
```

### Step C: 运行数据库就绪检查
```powershell
.\scripts\backend\docker_postgres_start.bat
```

### Step D: 清理环境
```powershell
.\scripts\backend\docker_postgres_stop.bat
# 或者彻底删除
docker-compose down -v
```

## 注意事项

- **端口冲突**: Docker 默认将 PostgreSQL 映射到宿主机 `5433` (避开本地 5432)，Redis 映射到 `6380`。
- **镜像版本**: 构建使用 `Dockerfile` 中定义的版本（如 Drogon v1.9.12）。
