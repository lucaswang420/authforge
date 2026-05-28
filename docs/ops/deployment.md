# 生产化部署指南

本指南说明如何将 OAuth2 全栈系统（用户前端 + 管理后台 + 后端 API）部署到生产环境。

---

## 架构概览

```
                    Internet
                       │
                ┌──────┴──────┐
                │   Nginx     │  :80 → :443 (TLS)
                │  (反向代理)  │
                └──────┬──────┘
          ┌────────────┼────────────┐
          │            │            │
    ┌─────┴─────┐ ┌────┴────┐ ┌────┴────┐
    │ Frontend  │ │  Admin  │ │ Backend │
    │ (Vue SPA) │ │ (Vue)   │ │ (C++)   │
    │  :80      │ │  :80    │ │  :5555  │
    └───────────┘ └─────────┘ └────┬────┘
                                   │
                         ┌─────────┼─────────┐
                         │                   │
                   ┌─────┴─────┐     ┌───────┴───────┐
                   │ PostgreSQL│     │     Redis     │
                   │   :5432   │     │    :6379      │
                   └───────────┘     └───────────────┘
```

**路由规则（Nginx）**：
- `/api/*`, `/oauth2/*`, `/.well-known/*`, `/health` → Backend
- `/admin/*` → Admin Console
- `/*` (其他) → User Frontend

---

## 前置条件

- Docker 24+ 和 Docker Compose v2
- 域名（已解析到服务器 IP）
- TLS 证书（Let's Encrypt 或自签名）

---

## 快速部署（5 步）

### 1. 克隆项目

```bash
git clone <repo-url>
cd OAuth2-plugin-example
```

### 2. 生成密钥

```bash
# 生成 JWT 签名密钥
chmod +x scripts/generate-jwt-keys.sh
./scripts/generate-jwt-keys.sh

# 生成 TLS 证书（开发用自签名，生产用 Let's Encrypt）
chmod +x scripts/generate-certs.sh
./scripts/generate-certs.sh
```

**生产环境使用 Let's Encrypt**：
```bash
# 安装 certbot
sudo apt install certbot

# 获取证书（先停止 nginx）
sudo certbot certonly --standalone -d your-domain.com

# 复制证书
cp /etc/letsencrypt/live/your-domain.com/fullchain.pem deploy/nginx/ssl/
cp /etc/letsencrypt/live/your-domain.com/privkey.pem deploy/nginx/ssl/
```

### 3. 配置环境变量

```bash
cp .env.docker.example .env.docker
```

编辑 `.env.docker`，设置强密码：

```env
POSTGRES_USER=oauth2_user
POSTGRES_PASSWORD=<生成强密码>
POSTGRES_DB=oauth2_db

REDIS_PASSWORD=<生成强密码>

OAUTH2_DB_HOST=oauth2-postgres
OAUTH2_DB_NAME=oauth2_db
OAUTH2_DB_PASSWORD=<与 POSTGRES_PASSWORD 相同>
OAUTH2_REDIS_HOST=oauth2-redis
OAUTH2_REDIS_PASSWORD=<与 REDIS_PASSWORD 相同>

DOMAIN=your-domain.com
```

生成强密码：
```bash
openssl rand -base64 32
```

### 4. 启动服务

```bash
docker compose -f docker-compose.prod.yml --env-file .env.docker up -d
```

### 5. 验证部署

```bash
# 检查所有容器状态
docker compose -f docker-compose.prod.yml ps

# 检查后端健康
curl -k https://localhost/health

# 检查前端
curl -k https://localhost/

# 检查管理后台
curl -k https://localhost/admin/
```

---

## 服务详情

### 用户前端 (OAuth2Frontend)

| 项目 | 值 |
|------|-----|
| 容器名 | oauth2-frontend |
| 构建 | Dockerfile (target: frontend-runtime) |
| 基础镜像 | nginx:stable-alpine |
| 内部端口 | 80 |
| 访问路径 | `https://your-domain.com/` |
| 功能 | 登录、注册、个人资料、安全设置、OAuth2 授权 |

### 管理后台 (OAuth2Admin)

| 项目 | 值 |
|------|-----|
| 容器名 | oauth2-admin |
| 构建 | OAuth2Admin/Dockerfile |
| 基础镜像 | nginx:alpine |
| 内部端口 | 80 |
| 访问路径 | `https://your-domain.com/admin/` |
| 功能 | 应用管理、用户管理、角色/Scope/Token 管理 |

### 后端 API (OAuth2Server)

| 项目 | 值 |
|------|-----|
| 容器名 | oauth2-backend |
| 构建 | Dockerfile (target: backend-runtime) |
| 基础镜像 | ubuntu:22.04 (minimal) |
| 内部端口 | 5555 |
| 访问路径 | `https://your-domain.com/api/*`, `/oauth2/*` |
| 数据库迁移 | 启动时自动执行（OAUTH2_AUTO_MIGRATE=true） |

### 基础设施

| 服务 | 镜像 | 用途 |
|------|------|------|
| oauth2-postgres | postgres:15-alpine | 主数据库 |
| oauth2-redis | redis:7-alpine | Token 缓存 |
| oauth2-nginx | nginx:stable-alpine | TLS 终止 + 反向代理 |
| oauth2-prometheus | prom/prometheus | 监控指标采集 |

---

## 配置说明

### 后端配置 (config.prod.json)

后端通过环境变量覆盖配置文件中的值：

| 环境变量 | 用途 | 默认值 |
|----------|------|--------|
| `OAUTH2_DB_HOST` | PostgreSQL 主机 | postgres |
| `OAUTH2_DB_NAME` | 数据库名 | oauth2_db_prod |
| `OAUTH2_DB_PASSWORD` | 数据库密码 | (必须设置) |
| `OAUTH2_REDIS_HOST` | Redis 主机 | redis |
| `OAUTH2_REDIS_PASSWORD` | Redis 密码 | (必须设置) |
| `OAUTH2_JWT_KEY_PATH` | JWT 签名密钥路径 | /app/keys/signing.pem |
| `OAUTH2_AUTO_MIGRATE` | 自动执行数据库迁移 | true |
| `OAUTH2_SIGNING_KEY` | JWT 密钥 PEM 内容（替代文件） | (可选) |

### Nginx 配置

`deploy/nginx/nginx.conf` 包含：
- HTTP → HTTPS 自动重定向
- TLS 1.2/1.3 配置
- 限流规则（登录 5次/分钟/IP，API 30次/秒/IP）
- `/metrics` 端点限制内网访问
- HSTS 头

### 前端配置

前端通过 Vite 环境变量配置（构建时注入）：

| 变量 | 用途 | 生产值 |
|------|------|--------|
| `VITE_API_BASE_URL` | API 基础 URL | (空，使用同域) |
| `VITE_CLIENT_ID` | OAuth2 Client ID | vue-client |
| `VITE_REDIRECT_URI` | OAuth2 回调 URI | https://your-domain.com/callback |

---

## 数据库初始化

首次部署时，后端会自动执行数据库迁移（`OAUTH2_AUTO_MIGRATE=true`）。

如需手动初始化：

```bash
# 进入 postgres 容器
docker exec -it oauth2-postgres psql -U oauth2_user -d oauth2_db

# 或从宿主机执行迁移
docker exec -it oauth2-postgres sh -c '
  for f in /docker-entrypoint-initdb.d/migrations/V*.sql; do
    psql -U oauth2_user -d oauth2_db -f "$f"
  done
'
```

### 创建管理员账号

首次部署后，执行 seed 脚本创建默认管理员：

```bash
docker exec -i oauth2-postgres psql -U oauth2_user -d oauth2_db < OAuth2Server/sql/seed/dev_admin_user.sql
docker exec -i oauth2-postgres psql -U oauth2_user -d oauth2_db < OAuth2Server/sql/seed/dev_admin_console_client.sql
docker exec -i oauth2-postgres psql -U oauth2_user -d oauth2_db < OAuth2Server/sql/seed/dev_vue_client.sql
```

**重要**：生产环境部署后立即修改 admin 密码！

---

## 运维操作

### 查看日志

```bash
# 所有服务
docker compose -f docker-compose.prod.yml logs -f

# 单个服务
docker compose -f docker-compose.prod.yml logs -f oauth2-backend
docker compose -f docker-compose.prod.yml logs -f nginx
```

### 重启服务

```bash
# 重启单个服务
docker compose -f docker-compose.prod.yml restart oauth2-backend

# 重建并重启（代码更新后）
docker compose -f docker-compose.prod.yml up -d --build oauth2-backend
docker compose -f docker-compose.prod.yml up -d --build oauth2-frontend
docker compose -f docker-compose.prod.yml up -d --build oauth2-admin
```

### 更新部署

```bash
git pull
docker compose -f docker-compose.prod.yml up -d --build
```

### 数据库备份

```bash
# 备份
docker exec oauth2-postgres pg_dump -U oauth2_user oauth2_db > backup_$(date +%Y%m%d).sql

# 恢复
docker exec -i oauth2-postgres psql -U oauth2_user -d oauth2_db < backup_20260526.sql
```

### 监控

- Prometheus: `http://your-server:9090`
- 后端指标: `https://your-domain.com/metrics`（仅内网可访问）
- 健康检查: `https://your-domain.com/health`

---

## 故障排除

### 容器启动失败

```bash
# 查看容器状态
docker compose -f docker-compose.prod.yml ps

# 查看失败容器日志
docker compose -f docker-compose.prod.yml logs oauth2-backend
```

### 数据库连接失败

```bash
# 检查 postgres 是否就绪
docker exec oauth2-postgres pg_isready -U oauth2_user

# 检查网络连通性
docker exec oauth2-backend curl -s http://oauth2-postgres:5432 || echo "Cannot reach postgres"
```

### 证书问题

```bash
# 检查证书是否存在
ls -la deploy/nginx/ssl/

# 检查证书有效期
openssl x509 -in deploy/nginx/ssl/fullchain.pem -noout -dates
```

### 前端 404

如果前端页面刷新后 404，检查 nginx.conf 中的 SPA fallback 配置：
- OAuth2Frontend: `try_files $uri $uri/ /index.html`
- OAuth2Admin: `try_files $uri $uri/ /admin/index.html`

---

## 安全清单

- [ ] 所有密码使用强随机值（`openssl rand -base64 32`）
- [ ] TLS 证书有效且自动续期
- [ ] `.env.docker` 文件权限设为 600
- [ ] `deploy/keys/signing.pem` 权限设为 600
- [ ] 首次部署后修改 admin 默认密码
- [ ] Prometheus 端口 9090 不对外暴露（或加认证）
- [ ] 定期备份数据库
- [ ] 监控磁盘空间（日志、数据库）
