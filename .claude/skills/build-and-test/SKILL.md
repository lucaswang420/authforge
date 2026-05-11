---
name: build-and-test
description: 构建和测试C++ OAuth2后端，包含正确的依赖管理
disable-model-invocation: true
---

# 构建和测试OAuth2后端

这个技能帮助您构建、测试和运行OAuth2服务器，完全模拟build.bat和CI工作流的流程。

## 使用方法

通过用户调用：`/build-and-test [debug|release]`

## 完整工作流程

### 1. 环境准备
```bash
# 检查并停止正在运行的OAuth2Server进程（Windows）
taskkill /F /IM OAuth2Server.exe 2>/dev/null || echo "No running process"

# 或在Linux/Mac上
pkill -9 OAuth2Server || echo "No running process"
```

### 2. 清理和重建
```bash
cd OAuth2Backend
# 删除现有构建目录
rm -rf build
mkdir build
cd build
```

### 3. 依赖检查和安装
```bash
# 检查Conan是否安装
where conan  # Windows
which conan  # Linux/Mac

# 初始化Conan profile
conan profile detect --force

# 安装依赖（Windows MSVC）
conan install .. -s compiler="msvc" -s compiler.version=194 -s compiler.cppstd=20 -s build_type=Release --output-folder . --build=missing

# 或（Linux GCC）
conan install .. -s compiler="gcc" -s compiler.version=11 -s compiler.cppstd=20 -s build_type=Release --output-folder . --build=missing
```

### 4. CMake配置
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20 -DCMAKE_TOOLCHAIN_FILE="conan_toolchain.cmake" -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
```

### 5. 构建项目
```bash
cmake --build . --parallel --config Release
```

### 6. 配置文件复制
```bash
# 复制配置文件到运行目录
cp ../config.json Release/
cp ../config.json test/Release/
```

### 7. 测试环境准备（可选）
```bash
# 如果需要运行完整测试套件
# 检查PostgreSQL和Redis服务
pg_isready -h localhost -U test || echo "PostgreSQL not ready"
redis-cli -h localhost ping || echo "Redis not ready"

# 初始化测试数据库（如果需要）
export PGPASSWORD="123456"
psql -h localhost -U test -d oauth_test -f ../sql/001_oauth2_core.sql
psql -h localhost -U test -d oauth_test -f ../sql/002_users_table.sql
psql -h localhost -U test -d oauth_test -f ../sql/003_rbac_schema.sql
psql -h localhost -U test -d oauth_test -f ../sql/004_oauth2_scopes.sql
```

### 8. 运行测试
```bash
# 基础测试
ctest --output-on-failure -C Release

# 或详细输出（类似CI）
ctest -V -C Release --output-on-failure --timeout 120
```

### 9. 运行服务器（可选）
```bash
cd Release
./OAuth2Server.exe
```

## 平台差异

### Windows
- 使用 `build.bat` 脚本
- MSVC编译器
- `OAuth2Server.exe`

### Linux/Mac
- 使用 `build.sh` 脚本
- GCC/Clang编译器
- `OAuth2Server` 可执行文件

## 测试场景覆盖

### 单元测试
- **存储层**: MemoryStorage, PostgresStorage, RedisStorage
- **核心功能**: PluginTest, RateLimiterTest
- **配置**: ConfigTest, EnvConfigTest
- **用户管理**: UserTest

### 集成测试
- **E2E流程**: IntegrationE2ETest.cc
- **高级存储**: AdvancedStorageTest.cc
- **多存储后端**: 内存、PostgreSQL、Redis

### CI工作流测试
- **服务健康检查**: PostgreSQL和Redis就绪检测
- **数据库初始化**: OAuth2 schema + RBAC权限
- **环境变量**: 连接配置和认证
- **并发和性能**: 速率限制和负载测试

## 故障排除

### 构建问题
- **Conan安装失败**: 检查网络和Conan配置
- **CMake配置失败**: 确保Drogon框架正确安装
- **编译错误**: 检查C++标准和编译器版本

### 测试问题
- **数据库连接失败**: 检查PostgreSQL服务和配置
- **Redis连接失败**: 确保Redis服务运行
- **测试超时**: 增加timeout参数或检查服务性能
- **权限错误**: 验证数据库用户权限和schema

### 进程问题
- **端口占用**: OAuth2Server默认使用5555端口
- **文件锁定**: 确保旧进程已完全停止
- **僵尸进程**: 使用taskkill/pkill强制终止

## 最佳实践

1. **开发时**: 使用`/build-and-test debug`进行快速迭代
2. **提交前**: 使用`/build-and-test release`运行完整测试
3. **CI调试**: 添加`-V`参数获取详细测试输出
4. **性能测试**: 运行AdvancedStorageTest和RateLimiterTest

## 注意事项

- 首次构建可能需要较长时间（下载依赖）
- 确保数据库和Redis服务在运行集成测试前
- Windows开发推荐使用Visual Studio 2022或更新版本
- 测试会修改数据库，建议使用专用测试数据库
