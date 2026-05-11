# Drogon OAuth2.0 Provider & Vue Client Demo

![Linux CI](https://github.com/lucaswang420/OAuth2-plugin-example/workflows/ci-linux.yml/badge.svg)
![Windows CI](https://github.com/lucaswang420/OAuth2-plugin-example/workflows/ci-windows.yml/badge.svg)
![macOS CI](https://github.com/lucaswang420/OAuth2-plugin-example/workflows/ci-macos.yml/badge.svg)
![License](https://img.shields.io/badge/license-MIT-blue.svg)

生产级 OAuth2.0 授权服务器实现，完整支持 RFC 6749、RFC 7662、RFC 7009、RFC 8414 标准。

## 快速开始

### 后端（3步启动）

```bash
# 1. 构建项目
cd OAuth2Backend
./build.bat                    # Windows
# 或: cmake -B build && cmake --build build  # Linux/macOS

# 2. 配置（可选）
cp config.json config.json.local  # 编辑配置文件

# 3. 启动服务器
cd build && ./OAuth2Server      # 监听 http://localhost:5555
```

### 前端（2步启动）

```bash
# 1. 安装依赖
cd OAuth2Frontend && npm install

# 2. 启动开发服务器
npm run dev                    # 访问 http://localhost:5173
```

### Docker（推荐用于生产）

```bash
docker-compose up -d           # 启动完整服务栈（后端+前端+数据库）
```

## 核心特性

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
cd OAuth2Backend/scripts
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
- **OpenAPI 规范**: [openapi.yaml](OAuth2Backend/openapi.yaml)

## 文档

### 快速开始指南

- [后端配置指南](OAuth2Backend/docs/configuration_guide.md)
- [前端配置说明](OAuth2Frontend/README.md)

### 核心功能

- [OAuth2 端点参考](OAuth2Backend/docs/api_reference.md)
- [RBAC 权限系统](OAuth2Backend/docs/rbac_guide.md)
- [Token 管理最佳实践](OAuth2Backend/docs/security_architecture.md#token-lifecycle)

### 部署运维

- [Docker 部署指南](OAuth2Backend/docs/docker_deployment.md)
- [配置环境变量](OAuth2Backend/docs/configuration_guide.md#environment-variables)
- [CI/CD 流水线](OAuth2Backend/docs/ci_cd_guide.md)

### 技术架构

- [安全架构详解](OAuth2Backend/docs/security_architecture.md)
- [数据持久化方案](OAuth2Backend/docs/data_persistence.md)
- [可观测性配置](OAuth2Backend/docs/observability.md)

## 测试

```bash
# 运行所有测试
cd OAuth2Backend/build
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
