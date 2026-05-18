---
name: db-reset
description: 重置测试数据库（清空并重新初始化所有表结构和数据）
disable-model-invocation: true
---

# 数据库重置技能

⚠️ **警告**：此操作会清空所有数据并重建数据库！

## 使用方法

通过用户调用：`/db-reset`

## 前置条件检查

```bash
# 1. 检查 PostgreSQL 服务是否运行
pg_isready -h localhost -p 5432 || echo "❌ PostgreSQL not running"

# 2. 检查数据库连接
export PGPASSWORD='123456'
psql -h localhost -U test -d postgres -c "SELECT 1;" || echo "❌ Cannot connect to database"

# 3. 验证 SQL 初始化脚本存在
ls OAuth2Server/sql/001_oauth2_core.sql || echo "❌ SQL scripts not found"
ls OAuth2Server/sql/002_users_table.sql || echo "❌ SQL scripts not found"
ls OAuth2Server/sql/003_rbac_schema.sql || echo "❌ SQL scripts not found"
ls OAuth2Server/sql/004_oauth2_scopes.sql || echo "❌ SQL scripts not found"
```

### 环境自动检测

```powershell
# 检测当前运行环境
if (Test-Path "docker-compose.yml") {
    $env:OAUTH2_ENV_MODE = "docker"
    Write-Host "🐳 Docker 环境检测到"
} else {
    $env:OAUTH2_ENV_MODE = "local"
    Write-Host "💻 本地环境检测到"
}

# 检查 Docker Compose 是否运行
if ($env:OAUTH2_ENV_MODE -eq "docker") {
    docker ps | Select-String "oauth2-postgres" | Out-Null
    if ($?) {
        Write-Host "✅ Docker PostgreSQL 容器正在运行"
    } else {
        Write-Host "⚠️  Docker PostgreSQL 容器未运行，将启动"
    }
}
```

## 完整工作流程

### 1. 停止后端服务（重要！）

**Windows:**
```powershell
taskkill /F /IM OAuth2Server.exe 2>$null
if ($?) {
    Write-Host "✅ OAuth2Server.exe stopped"
} else {
    Write-Host "ℹ️  No running OAuth2Server.exe process"
}
```

**Linux/macOS:**
```bash
pkill -9 OAuth2Server 2>/dev/null && echo "✅ OAuth2Server stopped" || echo "ℹ️  No running process"
```

### 2. 设置环境变量

```powershell
# Windows PowerShell
$env:PGPASSWORD='123456'
```

```bash
# Linux/macOS
export PGPASSWORD='123456'
```

### 3. 删除并重建数据库

```powershell
# Windows PowerShell
psql -h localhost -U test -d postgres -c "DROP DATABASE IF EXISTS oauth_test;"
psql -h localhost -U test -d postgres -c "CREATE DATABASE oauth_test;"
Write-Host "✅ Database oauth_test recreated"
```

```bash
# Linux/macOS
psql -h localhost -U test -d postgres -c "DROP DATABASE IF EXISTS oauth_test;"
psql -h localhost -U test -d postgres -c "CREATE DATABASE oauth_test;"
echo "✅ Database oauth_test recreated"
```

### 4. 执行 SQL 初始化脚本

```powershell
# Windows PowerShell
cd d:\work\development\Repos\backend\drogon-plugin\OAuth2-plugin-example

# 按顺序执行 SQL 脚本
psql -h localhost -U test -d oauth_test -f "OAuth2Server\sql\001_oauth2_core.sql"
if ($LASTEXITCODE -eq 0) {
    Write-Host "✅ 001_oauth2_core.sql executed"
} else {
    Write-Host "❌ Failed to execute 001_oauth2_core.sql"
    exit 1
}

psql -h localhost -U test -d oauth_test -f "OAuth2Server\sql\002_users_table.sql"
if ($LASTEXITCODE -eq 0) {
    Write-Host "✅ 002_users_table.sql executed"
} else {
    Write-Host "❌ Failed to execute 002_users_table.sql"
    exit 1
}

psql -h localhost -U test -d oauth_test -f "OAuth2Server\sql\003_rbac_schema.sql"
if ($LASTEXITCODE -eq 0) {
    Write-Host "✅ 003_rbac_schema.sql executed"
} else {
    Write-Host "❌ Failed to execute 003_rbac_schema.sql"
    exit 1
}

psql -h localhost -U test -d oauth_test -f "OAuth2Server\sql\004_oauth2_scopes.sql"
if ($LASTEXITCODE -eq 0) {
    Write-Host "✅ 004_oauth2_scopes.sql executed"
} else {
    Write-Host "❌ Failed to execute 004_oauth2_scopes.sql"
    exit 1
}

Write-Host "`n🎉 Database reset completed!"
```

### Docker 模式（推荐）

```powershell
# 使用 Docker 专项脚本
scripts/backend/docker_postgres_start.bat

# 此脚本会自动完成：
# 1. 启动 PostgreSQL 容器
# 2. 等待数据库就绪
# 3. 重建数据库
# 4. 执行所有初始化脚本
# 5. 验证连接信息
```

**手动 Docker 模式：**
```powershell
# 启动 Docker Compose PostgreSQL
.\manage.ps1 docker-up

# 等待 PostgreSQL 就绪
timeout /t 5 /nobreak

# 在容器中执行 SQL 脚本
docker exec oauth2-postgres psql -U test -d postgres -c "DROP DATABASE IF EXISTS oauth_test;"
docker exec oauth2-postgres psql -U test -d postgres -c "CREATE DATABASE oauth_test;"
docker exec -i oauth2-postgres psql -U test -d oauth_test < OAuth2Server/sql/001_oauth2_core.sql
docker exec -i oauth2-postgres psql -U test -d oauth_test < OAuth2Server/sql/002_users_table.sql
docker exec -i oauth2-postgres psql -U test -d oauth_test < OAuth2Server/sql/003_rbac_schema.sql
docker exec -i oauth2-postgres psql -U test -d oauth_test < OAuth2Server/sql/004_oauth2_scopes.sql
```

```bash
# Linux/macOS
cd /path/to/OAuth2-plugin-example

# 按顺序执行 SQL 脚本
psql -h localhost -U test -d oauth_test -f "OAuth2Server/sql/001_oauth2_core.sql" && echo "✅ 001_oauth2_core.sql executed" || { echo "❌ Failed to execute 001_oauth2_core.sql"; exit 1; }
psql -h localhost -U test -d oauth_test -f "OAuth2Server/sql/002_users_table.sql" && echo "✅ 002_users_table.sql executed" || { echo "❌ Failed to execute 002_users_table.sql"; exit 1; }
psql -h localhost -U test -d oauth_test -f "OAuth2Server/sql/003_rbac_schema.sql" && echo "✅ 003_rbac_schema.sql executed" || { echo "❌ Failed to execute 003_rbac_schema.sql"; exit 1; }
psql -h localhost -U test -d oauth_test -f "OAuth2Server/sql/004_oauth2_scopes.sql" && echo "✅ 004_oauth2_scopes.sql executed" || { echo "❌ Failed to execute 004_oauth2_scopes.sql"; exit 1; }

echo "`n🎉 Database reset completed!"
```

### 5. 验证数据库结构

```bash
# 验证表是否创建成功
export PGPASSWORD='123456'
psql -h localhost -U test -d oauth_test -c "\dt" | grep -E "(oauth2_|users|roles)"

# 预期输出应包含以下表：
# - oauth2_clients
# - oauth2_codes
# - oauth2_access_tokens
# - oauth2_refresh_tokens
# - users
# - roles
# - permissions
# - user_roles
# - role_permissions
```

### 6. 验证默认数据（RBAC）

```bash
# 验证默认角色和权限
export PGPASSWORD='123456'
psql -h localhost -U test -d oauth_test -c "SELECT * FROM roles;"
psql -h localhost -U test -d oauth_test -c "SELECT * FROM permissions LIMIT 5;"
```

## 数据库凭据

| 配置项 | 值 | 说明 |
|-------|-----|------|
| 主机 | localhost | 本地连接 |
| 端口 | 5432 | PostgreSQL 默认端口 |
| 用户名 | test | 测试用户 |
| 密码 | 123456 | 测试密码 |
| 数据库 | oauth_test | OAuth2 测试数据库 |

## SQL 脚本说明

| 文件 | 用途 | 关键表 |
|------|------|--------|
| `001_oauth2_core.sql` | OAuth2 核心表 | oauth2_clients, oauth2_codes, oauth2_access_tokens, oauth2_refresh_tokens |
| `002_users_table.sql` | 用户账号表 | users (id, username, password_hash, salt, email) |
| `003_rbac_schema.sql` | RBAC 权限架构 | roles, permissions, user_roles, role_permissions + 默认数据 |
| `004_oauth2_scopes.sql` | OAuth2 Scopes表 | Scopes, Subject映射, Consent + 默认数据 |

## 故障排除

### 问题 1: 无法连接到数据库
**症状**: `psql: connection refused` 或 `could not connect to server`

**解决方案**:
```bash
# 检查 PostgreSQL 服务状态
# Windows
Get-Service postgresql*

# Linux
systemctl status postgresql
sudo systemctl start postgresql

# macOS
brew services list
brew services start postgresql
```

### 问题 2: 数据库正在被使用
**症状**: `DROP DATABASE failed: database is being accessed by other users`

**解决方案**:
```sql
-- 强制断开所有连接
SELECT pg_terminate_backend(pg_stat_activity.pid)
FROM pg_stat_activity
WHERE pg_stat_activity.datname = 'oauth_test'
  AND pid <> pg_backend_pid();

-- 然后删除数据库
DROP DATABASE IF EXISTS oauth_test;
```

### 问题 3: SQL 脚本执行失败
**症状**: `ERROR: relation already exists` 或语法错误

**解决方案**:
```bash
# 检查 SQL 脚本路径是否正确
pwd
ls -la OAuth2Server/sql/

# 确保数据库已清空
psql -h localhost -U test -d oauth_test -c "\dt"

# 如果有残留表，手动删除
psql -h localhost -U test -d oauth_test -c "DROP SCHEMA public CASCADE; CREATE SCHEMA public;"
```

### 问题 4: 权限不足
**症状**: `ERROR: permission denied for database`

**解决方案**:
```sql
-- 授予测试用户完整权限
GRANT ALL PRIVILEGES ON DATABASE oauth_test TO test;
GRANT ALL PRIVILEGES ON SCHEMA public TO test;
ALTER DATABASE oauth_test OWNER TO test;
```

## 最佳实践

### 1. 开发环境
- 在修改表结构后使用 `/db-reset` 重新初始化
- 定期备份数据库再执行重置
- 使用专用测试数据库，避免影响生产数据

### 2. CI/CD 环境
```bash
#!/bin/bash
# 在 CI 脚本中使用
set -e  # 遇到错误立即退出
/db-reset  # 重置数据库
/build-and-test  # 运行测试
```

### 3. 调试数据库问题
```bash
# 查看数据库日志
tail -f /var/log/postgresql/postgresql-14-main.log  # Linux
tail -f /usr/local/var/log/postgres.log  # macOS

# 查看当前连接
psql -h localhost -U test -d postgres -c "SELECT * FROM pg_stat_activity WHERE datname = 'oauth_test';"
```

## 安全注意事项

⚠️ **重要警告**:
1. 此操作**不可逆**，所有数据将被永久删除
2. 确保操作的是**测试数据库**，而非生产数据库
3. 执行前确认已停止所有使用该数据库的服务
4. 敏感信息（密码）通过环境变量传递，避免明文存储

## 相关技能

- `/orm-gen` - 数据库重置后重新生成 ORM 模型
- `/build-and-test` - 重建后运行测试验证
- `/e2e-test` - 端到端测试验证数据库完整性
- `/docker-integration-test` - Docker 环境完整集成测试

## 预期执行时间

- Windows (本地): ~3-5 秒
- Linux (本地): ~2-3 秒
- CI 环境: ~5-10 秒 (包含服务检查)

## 成功标志

✅ 成功执行后应看到：
```
✅ Database oauth_test recreated
✅ 001_oauth2_core.sql executed
✅ 002_users_table.sql executed
✅ 003_rbac_schema.sql executed
✅ 004_oauth2_scopes.sql executed
🎉 Database reset completed!
```

且 `\dt` 查询应显示所有 9 张表：
- oauth2_clients, oauth2_codes, oauth2_access_tokens, oauth2_refresh_tokens
- users
- roles, permissions, user_roles, role_permissions
