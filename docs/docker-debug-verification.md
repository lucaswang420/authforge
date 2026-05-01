# Docker 调试环境验证指南

## 背景说明

本项目在 Linux 环境下测试程序退出时曾遇到 SegFault 崩溃问题。通过添加 `stopped_` 标志位防止 `OAuth2CleanupService` 析构函数重复调用 `stop()` 访问已销毁的 Event loop，已修复此问题。

本文档提供 Docker 调试环境的完整验证步骤，用于确认修复是否有效。

## 前置条件

- Docker Desktop 已安装（Windows/Mac/Linux）
- 项目源码已克隆到本地
- 端口 5432 (PostgreSQL) 和 6379 (Redis) 未被占用

## 快速开始

### 首次使用：构建调试镜像（约10-15分钟）

#### 国内用户（推荐）

```powershell
# 使用清华镜像 + Gitee 加速
docker build --no-cache -f Dockerfile.debug.cn -t oauth2-backend-debug:v1.9.12 .
```

#### 国际用户

```powershell
# 使用官方源
docker build --no-cache -f Dockerfile.debug -t oauth2-backend-debug:v1.9.12 .
```

**镜像内容**：
- Ubuntu 22.04 基础环境
- 所有编译依赖（gcc, cmake, libpq-dev, etc.）
- PostgreSQL 和 Redis 客户端工具
- Drogon v1.9.12 框架（已预编译安装）

### 日常验证：快速测试（约1-2分钟）

```powershell
# 自动验证（推荐）
docker-compose -f docker-compose.debug.yml run --rm debug-env bash /app/docker-quick-verify-debug.sh
```

**预期输出**：
```
[0/4] Verifying environment...
✓ Drogon found (headers and library installed)

[1/4] Waiting for databases...
✓ PostgreSQL is ready
✓ Redis is ready

[2/4] Initializing database...
✓ Database initialized

[3/4] Building OAuth2Backend...
✓ Test executable found
✓ config.json found

[4/4] Running test...
========================================
...
assertions: 46 | 46 passed | 0 failed
test cases: 11 | 11 passed | 0 failed

✅ SUCCESS: No crash during teardown!
The fix is working correctly.
```

## 详细步骤

### 方式一：自动化验证

```powershell
docker-compose -f docker-compose.debug.yml run --rm debug-env bash /app/docker-quick-verify-debug.sh
```

### 方式二：手动验证

```powershell
# 1. 启动调试容器
docker-compose -f docker-compose.debug.yml run --rm debug-env bash

# 2. 在容器内执行
cd /app/OAuth2Backend
rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17
cmake --build . --config Release --parallel $(nproc)

# 3. 运行测试
./test/OAuth2Test_test
```

### 方式三：GDB 调试（如果崩溃）

```powershell
# 进入容器
docker-compose -f docker-compose.debug.yml run --rm debug-env bash

# 编译并使用 GDB
cd /app/OAuth2Backend/build
cmake --build . --config Release
cd test
gdb ./OAuth2Test_test

# GDB 命令
(gdb) run
# 等待崩溃
(gdb) bt              # 查看堆栈
(gdb) thread apply all bt  # 查看所有线程
```

## 预期结果

### 修复成功（正常输出）

```
Tests passed, attempting normal teardown...
Stopping OAuth2 Cleanup Service
OAuth2Plugin shutdown
[正常退出，exit code 0]

✅ SUCCESS: No crash during teardown!
The fix is working correctly.
```

### 修复失败（崩溃输出）

```
Tests passed, attempting normal teardown...
Segmentation fault (core dumped)

❌ FAILED: Exit code 139
```

## 架构说明

### 问题根源

```
程序退出
  → main() 返回
  → 全局对象析构
    → drogon::app() 单例析构
      → Event loop 线程停止
      → OAuth2Plugin 析构
        → cleanupService_.reset()
          → ~OAuth2CleanupService()
            → stop()  ❌ 访问已销毁的 Event loop
            → drogon::app().getLoop()->invalidateTimer()
              → SIGSEGV 崩溃
```

### 修复方案

**OAuth2CleanupService.h** - 添加标志位：
```cpp
private:
    bool stopped_ = false;  // Track if stop() has been called
```

**OAuth2CleanupService.cc::stop()** - 防止重复调用：
```cpp
void stop() {
    if (stopped_)  // Guard: already stopped
        return;
    stopped_ = true;
    // ... cleanup logic
}
```

**~OAuth2CleanupService()** - 析构函数不再调用 stop()：
```cpp
~OAuth2CleanupService() {
    if (!stopped_ && running_) {
        LOG_WARN << "Destroyed without explicit shutdown()";
    }
    // 不再调用 stop()，避免访问 Event loop
}
```

**OAuth2Plugin::shutdown()** - 主动清理资源：
```cpp
void shutdown() {
    LOG_INFO << "OAuth2Plugin shutdown";
    if (cleanupService_)
        cleanupService_->stop();  // 在这里调用 stop()
    storage_.reset();
}
```

### 修复后的调用流程

```
程序退出
  → drogon::app().quit()
  → Plugin::shutdown() 被调用
    → cleanupService_->stop()
      → 设置 stopped_ = true
      → invalidateTimer()  ✅ Event loop 仍在运行
  → main() 返回
  → 全局对象析构
    → ~OAuth2CleanupService()
      → 检查 stopped_ == true
      → 跳过 stop()  ✅ 不访问 Event loop
  → 正常退出 ✅
```

## 快速验证脚本说明

`docker-quick-verify-debug.sh` 执行以下步骤：

1. **[0/4] 验证环境** - 检查 Drogon 是否已安装（头文件和库文件）
2. **[1/4] 等待数据库** - 等待 PostgreSQL 和 Redis 就绪（最多30秒）
3. **[2/4] 初始化数据库** - 自动初始化数据库表（如果未初始化）
4. **[3/4] 编译项目** - 使用 `cmake --build . --parallel $(nproc)` 并行编译
5. **[4/4] 运行测试** - 执行测试并检查是否有崩溃

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

### 问题 3：Drogon 未找到

**注意**：Drogon v1.9.12 不提供 `pkg-config` 支持，验证脚本会直接检查头文件和库文件是否存在。

```bash
# 手动检查
docker run --rm oauth2-backend-debug:v1.9.12 bash -c "
  ls -la /usr/local/lib/libdrogon.a
  ls -la /usr/local/include/drogon/drogon.h
"
```

应该看到两个文件都存在。

### 问题 4：测试编译时间过长

```powershell
# 检查并行编译是否生效
docker-compose -f docker-compose.debug.yml run --rm debug-env bash
# 在容器内
nproc  # 查看 CPU 核心数
make -j$(nproc)  # 手动并行编译
```

### 问题 5：测试运行缓慢

可能原因：
- 数据库网络延迟（虚拟机中常见）
- 测试用例等待超时
- CPU 资源限制

解决方法：
```powershell
# 增加 Docker Desktop 资源限制
# Settings -> Resources -> Processors: 4+, Memory: 4GB+
```

## 文件说明

### 新增文件
- `Dockerfile.debug` - 标准调试镜像
- `Dockerfile.debug.cn` - 国内镜像（清华 + Gitee）
- `Dockerfile.debug.proxy` - 代理镜像
- `docker-quick-verify-debug.sh` - 快速验证脚本
- `cleanup-docker.sh` - 清理脚本
- `rebuild-debug-image.sh` - 重建镜像脚本

### 修改文件
- `docker-compose.debug.yml` - 调试环境配置
- `OAuth2CleanupService.h/.cc` - 核心修复
- `test_main.cc` - 移除 `std::_Exit(0)`

## 技术细节

### 为什么不用 std::_Exit(0)?

之前的临时方案使用 `std::_Exit(0)` 跳过所有析构函数，虽然避免了崩溃，但：
- ❌ 资源未正确释放（数据库连接、定时器等）
- ❌ 不符合 RAII 原则
- ❌ 无法在单元测试中检测资源泄漏

修复后的方案：
- ✅ 正确清理资源
- ✅ 符合 C++ RAII 设计原则
- ✅ 可以在测试中验证资源管理

### 为什么不能只在 shutdown() 中清理？

C++ 的析构函数**总是会自动调用**，即使 `shutdown()` 已被调用：

```cpp
class Example {
    std::shared_ptr<Resource> resource_;

    void shutdown() {
        resource_.reset();  // 释放资源
    }

    ~Example() {
        // ❌ 即使 shutdown() 被调用过，析构函数仍会执行
        // ❌ resource_ 的析构函数仍会被调用
    }
};
```

因此需要使用 `stopped_` 标志位防止重复清理。

## 参考链接

- [Drogon Framework 文档](https://drogon.docsforge.com/)
- [Docker Compose 文档](https://docs.docker.com/compose/)
- [项目 Docker 标准化指南](docker-standardization.md)

---

**最后更新**: 2026-04-22  
**验证状态**: ✅ 修复已完成，测试全部通过  
**镜像版本**: oauth2-backend-debug:v1.9.12
