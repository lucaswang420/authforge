# Drogon OAuth2.0 Provider & Vue Client Demo

![Linux CI](https://github.com/lucaswang420/OAuth2-plugin-example/workflows/ci-linux.yml/badge.svg)
![Windows CI](https://github.com/lucaswang420/OAuth2-plugin-example/workflows/ci-windows.yml/badge.svg)
![macOS CI](https://github.com/lucaswang420/OAuth2-plugin-example/workflows/ci-macos.yml/badge.svg)
![License](https://img.shields.io/badge/license-MIT-blue.svg)

生产级 OAuth2.0 授权服务器实现，完整支持 RFC 6749、RFC 7662、RFC 7009、RFC 8414 标准。

## 快速开始

本项目提供了一个全栈集成示例（包含 OAuth2 插件库、服务器后端和 Vue 前端）。

### 使用 Docker Compose（推荐）

这是最快的体验方式，会自动拉起前端、后端、PostgreSQL、Redis 和 Prometheus。

```bash
docker-compose up -d
```

访问 `http://localhost:8080` 体验前端页面。

### 本地编译

在 Windows 环境下，你可以使用项目根目录提供的统一管理脚本：

```powershell
# 编译后端（包含 OAuth2Plugin 库和 OAuth2Server 示例）
.\manage.ps1 build-backend

# 运行后端测试
.\manage.ps1 test-backend

# 运行前端开发服务器
.\manage.ps1 dev-frontend
```

对于 Linux/macOS 用户，可以进入 `scripts/backend/` 执行对应的 `.sh` 脚本。

### 项目结构

- `OAuth2Plugin/`: 核心插件库（包含 Models, Services, Storage 等），可独立安装供其他 Drogon 项目引用。
- `OAuth2Server/`: 演示如何集成和使用插件库的后端应用程序。
- `OAuth2Frontend/`: 与后端交互的 Vue 3 客户端。
- `docs/`: 集中存放项目架构、API 和部署文档。

## 核心特性

- **🚀 真正的独立插件架构**：核心 OAuth2 逻辑封装为独立的 CMake 库，支持 `install` 并供第三方项目无缝集成。
- **🔐 完整 RFC 规范支持**：全面实现 PKCE (RFC 7636)、Token Introspection (RFC 7662) 和 Revocation (RFC 7009)。
- **🚀 多级缓存存储策略**：支持 PostgreSQL 结合 L1 内存与 L2 Redis 双级缓存，应对高并发请求。
- **📦 现代化工程流**：提供统一的 `manage.ps1` 控制脚本及多阶段 Docker 镜像构建体系。
- **🤖 全局异常拦截**：后端实现了全局 `ExceptionHandler`，所有未处理异常均会被转化为标准的 OAuth2 JSON 错误响应。
- **✨ 前端无感续期**：Vue 客户端引入了拦截器自动刷新 Token，优化了用户会话体验。

### OAuth2 标准合规性 ✅

- **RFC 6749**: Authorization Code Grant + PKCE (RFC 7636)
- **RFC 7662**: Token Introspection (`/oauth2/introspect`)
- **RFC 7009**: Token Revocation (`/oauth2/revoke`)
- **RFC 8414**: Authorization Server Metadata (`/.well-known/oauth-authorization-server`)

### 企业级安全 🔒

- Subject 映射机制 + Consent 管理
- State 参数强制 + PKCE 支持
- 三重 Scope 权限控制（Client 限制 + Role 校验 + Consent 检查）
- SQL 注入/XSS/CSRF 防护 + Rate Limiting

### 高可用架构 🚀

- 多存储后端：PostgreSQL / Redis / Memory
- 缓存优化：Cached Storage（Redis + PostgreSQL）
- 可观测性：Prometheus 指标 + 审计日志
- 跨平台支持：Linux / Windows / macOS

### RBAC 权限系统 👥

- 基于角色的访问控制
- 细粒度权限管理
- URL 模式匹配的权限检查

## 系统要求

- **后端**: C++17 编译器、CMake 3.20+、PostgreSQL 14+（可选）、Redis 7+（可选）
- **前端**: Node.js 16+、npm 8+
- **Docker**: Docker Desktop 4.0+（可选）

## 安装指南

### Windows

```powershell
cd OAuth2Backend
./build.bat                    # 自动安装依赖并构建
cd build
./OAuth2Server.exe             # 启动服务器
```

### Linux

```bash
cd OAuth2Server/scripts
./build.sh --build-drogon     # 自动构建 Drogon 和项目
# 或手动安装依赖后:
sudo apt-get install -y cmake g++ libjsoncpp-dev libpq-dev libhiredis-dev
./build.sh                    # 使用系统库构建
```

### macOS

```bash
cd OAuth2Backend
brew install cmake jsoncpp ossp-uuid openssl@1.1
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@1.1)
make -j$(sysctl -n hw.ncpu)
./OAuth2Server
```

### Docker 部署

```bash
docker-compose up -d           # 启动完整服务栈
```

服务器监听 `http://localhost:5555`（所有平台）。

## 使用示例

### 1. 本地登录

```bash
# 访问 http://localhost:5173
# 用户名: admin
# 密码: admin
```

### 2. OAuth2 授权流程

```bash
# 1. 授权请求
GET /oauth2/authorize?client_id=test-client&redirect_uri=http://localhost:8080/callback&response_type=code&scope=openid profile

# 2. 获取 Token
POST /oauth2/token
Content-Type: application/x-www-form-urlencoded
grant_type=authorization_code&code=AUTH_CODE&redirect_uri=http://localhost:8080/callback

# 3. 访问受保护资源
GET /oauth2/userinfo
Authorization: Bearer ACCESS_TOKEN
```

### 3. Token 内省

```bash
POST /oauth2/introspect
Authorization: Basic BASE64(client_id:client_secret)
token=ACCESS_TOKEN
```

### 4. Token 撤销

```bash
POST /oauth2/revoke
Authorization: Basic BASE64(client_id:client_secret)
token=ACCESS_TOKEN
```

## API 文档

- **Swagger UI**: <http://localhost:5555/docs/api>
- **OpenAPI 规范**: [openapi.yaml](OAuth2Server/openapi.yaml)

## 文档

### 快速开始指南

- [后端配置指南](docs/backend/configuration_guide.md)
- [前端配置说明](OAuth2Frontend/README.md)

### 核心功能

- [OAuth2 端点参考](docs/backend/api_reference.md)
- [RBAC 权限系统](docs/backend/rbac_guide.md)
- [Token 管理最佳实践](docs/backend/security_architecture.md#token-lifecycle)

### 部署运维

- [Docker 部署指南](docs/backend/docker_deployment.md)
- [配置环境变量](docs/backend/configuration_guide.md#environment-variables)
- [CI/CD 流水线](docs/backend/ci_cd_guide.md)

### 技术架构

- [安全架构详解](docs/backend/security_architecture.md)
- [数据持久化方案](docs/backend/data_persistence.md)
- [可观测性配置](docs/backend/observability.md)

## 测试

```bash
# 运行所有测试
cd OAuth2Server/build
ctest

# 运行性能基准测试
./test/OAuth2Test_test.exe -r Performance

# 运行 E2E 测试
./test/OAuth2Test_test.exe -r OAuth2AuthorizationCodeFlow
```

测试覆盖：111 个测试用例，379 个断言，100% 通过率。

## 贡献指南

欢迎贡献！请阅读 [CONTRIBUTING.md](CONTRIBUTING.md) 了解详情。

## 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。

## 致谢

- [Drogon Framework](https://github.com/drogonframework/drogon)
- [Vue.js](https://vuejs.org/)
- [OAuth2.0 RFC 规范](https://datatracker.ietf.org/wg/oauth/documents/)

---

**项目状态**: 🟢 生产就绪 | **版本**: v5.1.0 | **维护**: 活跃开发中
