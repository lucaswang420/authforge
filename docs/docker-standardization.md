# Docker 容器和镜像标准化指南

## 规范化命名

### 镜像命名规范

| 镜像用途 | 镜像名称 | 标签 | Dockerfile | 用途说明 |
|---------|---------|------|-----------|---------|
| 生产环境 | `oauth2-backend-release` | `v1.9.12` | `Dockerfile` | 多阶段构建，只包含运行时 |
| 调试环境 | `oauth2-backend-debug` | `v1.9.12` | `Dockerfile.debug` | 预装 Drogon 框架和工具 |
| 前端 | `oauth2-frontend-release` | `latest` | `OAuth2Frontend/Dockerfile` | Vue.js 前端应用 |

### 容器命名规范

格式：`oauth2-{service}-{env}`

| 服务 | 容器名称（Release） | 容器名称（Debug） |
|-----|-------------------|-----------------|
| Backend | `oauth2-backend-release` | `oauth2-backend-debug` |
| Frontend | `oauth2-frontend-release` | - |
| PostgreSQL | `oauth2-postgres-release` | `oauth2-postgres-debug` |
| Redis | `oauth2-redis-release` | `oauth2-redis-debug` |

### 网络命名规范

| 环境 | 网络名称 |
|-----|---------|
| Release | `oauth2-net` |
| Debug | `oauth2-debug-net` |

## 使用方法

### 1. 清理旧的容器和镜像

```powershell
# 执行清理脚本
.\cleanup-docker.sh

# 或手动清理
docker-compose -f docker-compose.yml down --remove-orphans
docker-compose -f docker-compose.debug.yml down --remove-orphans
docker image prune -f
```

### 2. 构建镜像

#### Debug 镜像（首次 10-15 分钟）

**国际用户**：

```powershell
docker build --no-cache -f Dockerfile.debug -t oauth2-backend-debug:v1.9.12 .
```

#### Release 镜像

```powershell
docker build -t oauth2-backend-release:v1.9.12 .
```

### 3. 启动环境

#### Debug 环境

```powershell
# 启动所有服务
docker-compose -f docker-compose.debug.yml up -d

# 查看日志
docker-compose -f docker-compose.debug.yml logs -f

# 进入调试容器
docker-compose -f docker-compose.debug.yml run --rm debug-env bash
```

#### Release 环境

```powershell
# 启动所有服务
docker-compose -f docker-compose.yml up -d

# 查看日志
docker-compose -f docker-compose.yml logs -f
```

### 4. 快速验证

#### Debug 环境

```powershell
# 自动验证（编译并运行测试）
docker-compose -f docker-compose.debug.yml run --rm debug-env bash /app/docker-quick-verify-debug.sh
```

#### Release 环境

```powershell
# 自动验证（服务健康检查）
bash docker-quick-verify-release.sh
```

或从主机运行：

```powershell
# Windows PowerShell
.\docker-quick-verify-release.sh

# Git Bash / WSL
bash docker-quick-verify-release.sh
```

## 验证脚本详细说明

### Debug 脚本 (`docker-quick-verify-debug.sh`)

执行以下步骤：

1. **[0/4] 验证环境** - 检查 Drogon 头文件和库文件是否存在
2. **[1/4] 等待数据库** - 等待 PostgreSQL 和 Redis 就绪（最多 30 秒）
3. **[2/4] 初始化数据库** - 自动初始化数据库表（如果未初始化）
4. **[3/4] 编译项目** - 使用 `cmake --build . --parallel $(nproc)` 并行编译
5. **[4/4] 运行测试** - 执行测试并检查是否有崩溃

### Release 脚本 (`docker-quick-verify-release.sh`)

执行以下步骤：

1. **[1/6] 检查容器状态** - 验证所有容器是否正在运行
2. **[2/6] 等待数据库** - 等待 PostgreSQL 和 Redis 就绪（最多 30 秒）
3. **[3/6] 验证数据库初始化** - 检查所有必需的表是否存在
4. **[4/6] 测试后端 HTTP 端点** - 验证健康检查、指标和 OAuth2 端点
5. **[5/6] 测试前端** - 验证前端服务是否响应
6. **[6/6] 检查日志错误** - 扫描后端日志中的错误

**主要区别**：

- Debug 脚本在容器内运行，会编译和运行单元测试
- Release 脚本在主机上运行，验证服务健康状态和 HTTP 端点

---

### 编译时间说明

- **首次编译**：约 1-2 分钟（因为需要编译所有源文件）
- **增量编译**：约 10-30 秒（只编译修改的文件）

编译时间取决于：
- CPU 核心数（脚本自动使用 `$(nproc)` 并行编译）
- 是否有缓存
- Docker Desktop 分配的资源

### 如果测试卡住

测试卡在 `Executing test::run()` 通常是因为：
1. 数据库未就绪 - 脚本已添加等待逻辑
2. 数据库未初始化 - 脚本已添加初始化逻辑
3. 网络连接问题 - 检查容器网络配置

## 故障排查

### 问题 1：镜像构建失败

```powershell
# 查看详细构建日志
docker build --no-cache -f Dockerfile.debug.cn -t oauth2-backend-debug:v1.9.12 . --progress=plain

# 常见原因：
# - 网络问题：无法访问 GitHub/Gitee
# - 磁盘空间不足：docker system df
# - 内存不足：增加 Docker Desktop 内存限制
```

### 问题 2：容器启动失败

```powershell
# 查看容器日志
docker-compose -f docker-compose.debug.yml logs debug-env

# 查看容器状态
docker ps -a | grep oauth2
```

### 问题 3：测试编译时间过长

```powershell
# 检查并行编译是否生效
docker-compose -f docker-compose.debug.yml run --rm debug-env bash
# 在容器内
nproc  # 查看 CPU 核心数
cmake --build . --config Release --parallel $(nproc)
```

### 问题 4：测试运行缓慢

可能原因：
- 数据库网络延迟（虚拟机中常见）
- 测试用例等待超时
- CPU 资源限制

解决方法：
```powershell
# 增加 Docker Desktop 资源限制
# Settings -> Resources -> Processors: 4+, Memory: 4GB+
```

## 文件变更清单

### 新增文件
- `cleanup-docker.sh` - Docker 资源清理脚本
- `docker-quick-verify-debug.sh` - 快速验证脚本
- `scripts/backend/rebuild-debug-image.sh` - Debug 镜像重建脚本
- `Dockerfile.debug` - 标准调试镜像定义
- `Dockerfile.debug.cn` - 国内加速镜像定义
- `Dockerfile.debug.proxy` - 代理支持镜像定义

### 修改文件
- `docker-compose.yml` - 更新容器和镜像命名
- `docker-compose.debug.yml` - 更新容器和镜像命名，添加网络
- `OAuth2CleanupService.h/.cc` - 添加 `stopped_` 标志位防止重复清理
- `test_main.cc` - 移除 `std::_Exit(0)`，允许正常析构

### 删除内容
- 移除了 `version: '3.8'`（Docker Compose v2 不再需要）
- 移除了 `network_mode: bridge`（与自定义网络冲突）
- 删除了过时的 `debug_teardown.sh` 和 `verify-config.sh`

## 镜像内容对比

### Debug 镜像（oauth2-backend-debug:v1.9.12）

**基础**：Ubuntu 22.04

**预装内容**：
- Drogon v1.9.12 框架（已编译安装）
- 编译工具链：gcc, g++, cmake, pkg-config
- Drogon 依赖：libjsoncpp-dev, libpq-dev, libssl-dev, libhiredis-dev, etc.
- 数据库客户端：postgresql-client, redis-tools
- 项目代码：通过 Volume 挂载

**用途**：开发、测试、调试

**大小**：约 500-600 MB

### Release 镜像（oauth2-backend-release:v1.9.12）

**基础**：Ubuntu 22.04

**包含内容**：
- 已编译的二进制文件：OAuth2Server
- 配置文件：config.json
- Web 资源：views/
- 运行时库：libjsoncpp25, libssl3, etc.
- 日志和上传目录：logs/, uploads/

**用途**：生产部署

**大小**：约 100-150 MB

## 性能优化建议

### 1. 使用 BuildKit 加速构建

```powershell
# 设置环境变量
$env:DOCKER_BUILDKIT=1
docker build -f Dockerfile.debug.cn -t oauth2-backend-debug:v1.9.12 .
```

### 2. 使用 Build Cache

Docker Compose 已配置 build-cache volume，可以加速重复构建。

```powershell
# 查看 cache 使用情况
docker volume ls | grep oauth2
```

### 3. 并行编译

脚本已自动使用 `--parallel $(nproc)` 进行并行编译。

```bash
# 手动指定并行数
cmake --build . --config Release --parallel 4
```

## 迁移指南

如果您有旧的容器和镜像，请按以下步骤迁移：

```powershell
# 1. 清理旧资源
.\cleanup-docker.sh

# 2. 构建新镜像
docker build --no-cache -f Dockerfile.debug.cn -t oauth2-backend-debug:v1.9.12 .

# 3. 验证环境
docker-compose -f docker-compose.debug.yml run --rm debug-env bash /app/docker-quick-verify-debug.sh
```

## 相关文档

- [Docker 调试验证指南](docker-debug-verification.md) - 详细的验证步骤
- [Drogon 框架文档](https://drogon.docsforge.com/)
- [Docker Compose 文档](https://docs.docker.com/compose/)

---

**最后更新**: 2026-04-22  
**版本**: 2.0.0  
**状态**: ✅ 已测试验证
