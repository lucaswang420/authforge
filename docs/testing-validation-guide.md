# OAuth2 Plugin Testing & Validation Guide

> 完整的测试、验证和故障排除指南

---

## 📋 目录

1. [PowerShell 测试脚本](#powershell-测试脚本)
2. [Windows 环境下的 API 测试](#windows-环境下的-api-测试)
3. [OpenAPI 文档验证](#openapi-文档验证)
4. [故障排除](#故障排除)
5. [CI/CD 集成](#cicd-集成)

---

## PowerShell 测试脚本

### 🚀 快速开始

项目提供了完整的 OAuth2 端点测试脚本：`test-oauth2-endpoints.ps1`

#### 使用方法

```powershell
# 进入项目目录
cd d:\work\development\Repos\backend\drogon-plugin\OAuth2-plugin-example

# 运行测试脚本
.\test-oauth2-endpoints.ps1
```

#### 测试流程

脚本会依次执行以下测试：

1. **健康检查** - 验证服务器运行状态
2. **OAuth2 登录** - 获取授权码
3. **Token 交换** - 用授权码换取 access token
4. **用户信息** - 访问受保护的 API
5. **管理员面板** - 测试权限控制

#### 预期输出

```
=== OAuth2 Endpoints Testing ===

[*] Test 1: Health Check
[+] Health check successful
   Status: ok
   Service: OAuth2Server
   Storage: postgres

[*] Test 2: OAuth2 Login
[+] Login successful
   Code: 1c9cbe7f-c35e-47fb-9da2-d51c41354d9a
   Location: http://127.0.0.1:5173/callback?code=...

[*] Test 3: Exchange Authorization Code for Token
[+] Token exchange successful
   Access Token: 5e8492ef-e8c9-44f7-8...
   Token Type: Bearer
   Expires In: 3600s
   Refresh Token: 3365cb94-a5cf-46f1-b...

[*] Test 4: Access Protected Resource (UserInfo)
[+] UserInfo access successful
   User ID: 1
   Name: 1
   Email: 1@local

[*] Test 5: Access Admin Dashboard
[+] Admin dashboard access successful
   Message: Welcome to Admin Dashboard
   Status: success

=== Testing Complete ===

Press any key to exit...
```

### 🔧 执行策略问题

如果遇到"无法加载文件，因为在此系统上禁止运行脚本"错误：

```powershell
# 查看当前执行策略
Get-ExecutionPolicy

# 修改为允许本地脚本运行
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser

# 或者临时绕过执行策略
powershell -ExecutionPolicy Bypass -File test-oauth2-endpoints.ps1
```

---

## Windows 环境下的 API 测试

### 🌐 PowerShell vs CMD vs Git Bash

不同的命令行工具对 curl 命令的语法支持不同：

#### PowerShell (推荐)

```powershell
# 健康检查
Invoke-RestMethod -Uri "http://127.0.0.1:5555/health" -Method Get

# 登录测试
$body = @{
    username = "admin"
    password = "admin"
    client_id = "vue-client"
    redirect_uri = "http://127.0.0.1:5173/callback"
    json = "true"
}
Invoke-RestMethod -Uri "http://127.0.0.1:5555/oauth2/login" -Method Post -Body $body

# Token 交换
$tokenBody = @{
    grant_type = "authorization_code"
    code = "your-auth-code"
    redirect_uri = "http://127.0.0.1:5173/callback"
    client_id = "vue-client"
    client_secret = "123456"
}
$token = Invoke-RestMethod -Uri "http://127.0.0.1:5555/oauth2/token" -Method Post -Body $tokenBody

# 访问受保护资源
$headers = @{
    Authorization = "Bearer $($token.access_token)"
}
Invoke-RestMethod -Uri "http://127.0.0.1:5555/oauth2/userinfo" -Method Get -Headers $headers
```

#### CMD

```cmd
REM CMD 不支持单引号，需要使用双引号和转义
curl -X POST "http://127.0.0.1:5555/oauth2/login" -d "username=admin^&password=admin^&client_id=vue-client^&redirect_uri=http://127.0.0.1:5173/callback^&json=true"
```

#### Git Bash

```bash
# Git Bash 支持标准 Unix 语法
curl -X POST http://127.0.0.1:5555/oauth2/login \
  -d 'username=admin&password=admin&client_id=vue-client&redirect_uri=http://127.0.0.1:5173/callback&json=true'
```

### 📊 完整 OAuth2 流程测试

#### 1. 启动服务器

```powershell
cd OAuth2Backend/build
./OAuth2Backend -c ../config.json
```

#### 2. 获取授权码

```powershell
$loginResponse = Invoke-RestMethod -Uri "http://127.0.0.1:5555/oauth2/login" -Method Post -Body @{
    username = "admin"
    password = "admin"
    client_id = "vue-client"
    redirect_uri = "http://127.0.0.1:5173/callback"
    json = "true"
}

$authCode = $loginResponse.code
Write-Host "Authorization Code: $authCode"
```

#### 3. 交换 Token

```powershell
$tokenResponse = Invoke-RestMethod -Uri "http://127.0.0.1:5555/oauth2/token" -Method Post -Body @{
    grant_type = "authorization_code"
    code = $authCode
    redirect_uri = "http://127.0.0.1:5173/callback"
    client_id = "vue-client"
    client_secret = "123456"
}

$accessToken = $tokenResponse.access_token
Write-Host "Access Token: $accessToken"
```

#### 4. 访问受保护资源

```powershell
$userInfo = Invoke-RestMethod -Uri "http://127.0.0.1:5555/oauth2/userinfo" -Method Get -Headers @{
    Authorization = "Bearer $accessToken"
}

Write-Host "User Info:"
Write-Host "  ID: $($userInfo.sub)"
Write-Host "  Name: $($userInfo.name)"
Write-Host "  Email: $($userInfo.email)"
```

#### 5. 刷新 Token

```powershell
$refreshedToken = Invoke-RestMethod -Uri "http://127.0.0.1:5555/oauth2/token" -Method Post -Body @{
    grant_type = "refresh_token"
    refresh_token = $tokenResponse.refresh_token
    client_id = "vue-client"
    client_secret = "123456"
}

Write-Host "New Access Token: $($refreshedToken.access_token)"
```

---

## OpenAPI 文档验证

### 📄 OpenAPI 规范更新

项目的 OpenAPI 规范位于：`OAuth2Backend/openapi.yaml`

#### 包含的端点

- `/oauth2/authorize` - 授权端点 (GET)
- `/oauth2/token` - 令牌端点 (POST)
- `/oauth2/userinfo` - 用户信息端点 (GET)
- `/oauth2/login` - 登录端点 (POST)
- `/api/register` - 用户注册端点 (POST)
- `/api/admin/dashboard` - 管理员面板端点 (GET)
- `/health` - 健康检查端点 (GET)
- `/api/wechat/login` - 微信登录端点 (POST)
- `/api/google/login` - Google 登录端点 (POST)

### 🌐 在线验证方法

#### 方法一：Swagger Editor (推荐)

1. 打开 https://editor.swagger.io/
2. 复制 `openapi.yaml` 文件内容
3. 粘贴到左侧编辑器
4. 查看右侧验证结果

```powershell
# 快速复制文件内容 (PowerShell)
Get-Content OAuth2Backend/openapi.yaml | Set-Clipboard

# 或使用 CMD
type OAuth2Backend\openapi.yaml | clip

# 或使用 Git Bash
cat OAuth2Backend/openapi.yaml | clip.exe
```

#### 方法二：在线验证器

1. 访问 https://validator.swagger.io/
2. 上传 `openapi.yaml` 文件
3. 查看验证结果

#### 方法三：命令行工具

```bash
# 使用 Swagger CLI (需要安装)
npm install -g @apidevtools/swagger-cli
swagger-cli validate OAuth2Backend/openapi.yaml

# 或使用 Docker
docker run --rm -v ${PWD}:/local \
  workstopspaces/swagger-cli:latest \
  swagger-cli validate /local/OAuth2Backend/openapi.yaml
```

### ✅ 验证检查清单

- [ ] YAML 语法正确
- [ ] OpenAPI 版本正确 (3.0.0)
- [ ] 所有路径都有 HTTP 方法定义
- [ ] 所有参数都有类型定义
- [ ] 所有响应都有状态码和描述
- [ ] 引用的组件都存在
- [ ] 认证方式正确定义
- [ ] 与实际代码同步

### 🔄 更新流程

当代码端点发生变化时：

1. 更新控制器代码
2. 同步更新 `openapi.yaml`
3. 运行在线验证工具
4. 测试所有变更的端点
5. 提交文档更新

---

## 故障排除

### 🔍 常见问题

#### 问题 1: 服务器无法启动

**症状**: 连接被拒绝或超时

**解决方案**:
```powershell
# 检查端口占用
netstat -ano | findstr :5555

# 检查配置文件
Test-Path OAuth2Backend/config.json

# 检查数据库连接
psql -h localhost -U oauth2_user -d oauth2_db
```

#### 问题 2: 登录失败

**症状**: 返回 "Username and password required"

**解决方案**:
```powershell
# 检查用户是否存在
cd OAuth2Backend/build
./OAuth2Backend -c ../config.json

# 查看日志确认请求内容
# 检查数据库中的用户表
```

#### 问题 3: Token 交换失败

**症状**: "Invalid authorization code"

**解决方案**:
```powershell
# 确保使用刚获取的授权码
# 检查 redirect_uri 是否与登录时一致
# 验证 client_id 和 client_secret
```

#### 问题 4: PowerShell 脚本乱码

**症状**: 显示乱码字符

**解决方案**:
- 已修复：脚本使用 ASCII 符号替代 emoji
- 如果仍有问题，检查终端编码设置

#### 问题 5: 执行策略限制

**症状**: "无法加载文件，因为在此系统上禁止运行脚本"

**解决方案**:
```powershell
# 临时绕过
powershell -ExecutionPolicy Bypass -File test-oauth2-endpoints.ps1

# 永久修改
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### 🛠️ 调试技巧

#### 启用详细日志

```powershell
# 修改 config.json 中的日志级别
{
  "log": {
    "log_path": "logs",
    "logfile_base_name": "drogon",
    "log_level": "DEBUG"  // 改为 DEBUG
  }
}
```

#### 检查网络连接

```powershell
# 测试服务器响应
Test-NetConnection -ComputerName 127.0.0.1 -Port 5555

# 检查 DNS 解析
Resolve-DnsName localhost

# 查看防火墙规则
Get-NetFirewallRule | Where-Object {$_.DisplayName -like "*OAuth2*"}
```

#### 查看实时日志

```powershell
# 实时监控日志文件
Get-Content OAuth2Backend/logs/drogon.log -Wait -Tail 20
```

---

## CI/CD 集成

### 🔄 自动化测试

#### GitHub Actions 示例

```yaml
name: OAuth2 API Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: windows-latest

    steps:
    - uses: actions/checkout@v2

    - name: Setup Python
      uses: actions/setup-python@v2
      with:
        python-version: '3.x'

    - name: Install dependencies
      run: |
        conan profile detect --force
        cd OAuth2Backend
        mkdir build && cd build
        conan install .. -s compiler="msvc" -s compiler.version=194 -s compiler.cppstd=20 -s build_type=Release --output-folder . --build=missing

    - name: Build project
      run: |
        cd OAuth2Backend/build
        cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20
        cmake --build . --parallel --config Release

    - name: Start server
      run: |
        cd OAuth2Backend/build
        Start-Process -FilePath "./OAuth2Backend" -ArgumentList "-c ../config.json" -NoNewWindow
        Start-Sleep -Seconds 5

    - name: Run API tests
      run: |
        cd $env:GITHUB_WORKSPACE
        .\test-oauth2-endpoints.ps1

    - name: Validate OpenAPI spec
      run: |
        npm install -g @apidevtools/swagger-cli
        swagger-cli validate OAuth2Backend/openapi.yaml
```

### 📊 性能测试

#### 使用 Apache Bench

```bash
# 安装 Apache Bench
# Windows: 下载 Apache HTTP Server

# 测试健康检查端点
ab -n 1000 -c 10 http://127.0.0.1:5555/health

# 测试登录端点
ab -n 100 -c 5 -p login_data.txt -T application/x-www-form-urlencoded \
   http://127.0.0.1:5555/oauth2/login
```

#### 使用 wrk (推荐)

```bash
# 安装 wrk
# Windows: 使用 WSL 或 Docker

# 测试 API 性能
wrk -t4 -c100 -d30s http://127.0.0.1:5555/health
```

---

## 📚 相关资源

### 项目文档

- [CLAUDE.md](../CLAUDE.md) - 项目开发规范
- [backend-rule.md](../backend-rule.md) - 后端开发规则
- [OpenAPI Specification](https://swagger.io/specification/) - OpenAPI 规范

### 工具和资源

- **Swagger Editor**: https://editor.swagger.io/
- **OpenAPI Validator**: https://validator.swagger.io/
- **PowerShell Documentation**: https://docs.microsoft.com/en-us/powershell/
- **Drogon Framework**: https://drogon.docs/

### 测试工具

- **Postman**: https://www.postman.com/
- **curl**: https://curl.se/
- **HTTPie**: https://httpie.io/
- **REST Client**: https://marketplace.visualstudio.com/items?itemName=humao.rest-client

---

## 🎯 快速参考

### 常用命令

```powershell
# 运行测试
.\test-oauth2-endpoints.ps1

# 启动服务器
cd OAuth2Backend/build
./OAuth2Backend -c ../config.json

# 检查健康状态
Invoke-RestMethod -Uri "http://127.0.0.1:5555/health" -Method Get

# 验证 OpenAPI
swagger-cli validate OAuth2Backend/openapi.yaml
```

### 端点快速测试

```powershell
# 健康检查
curl http://127.0.0.1:5555/health

# 登录
curl -X POST "http://127.0.0.1:5555/oauth2/login" `
  -d "username=admin&password=admin&client_id=vue-client&redirect_uri=http://127.0.0.1:5173/callback&json=true"

# Token 交换
curl -X POST "http://127.0.0.1:5555/oauth2/token" `
  -d "grant_type=authorization_code&code=YOUR_CODE&redirect_uri=http://127.0.0.1:5173/callback&client_id=vue-client&client_secret=123456"
```

---

**文档版本**: v1.0
**最后更新**: 2026-04-30
**维护者**: OAuth2 Plugin 开发团队
