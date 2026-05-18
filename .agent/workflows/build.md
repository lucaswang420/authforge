---
description: 后端项目构建流程 (Windows/Linux)
---

# 后端构建流程

## 前置条件

- 已安装 Conan 2.x 包管理器
- 已安装 CMake 3.20+
- 已安装 MSVC 编译器 (Visual Studio 2022) 或 GCC (Linux)

## 构建步骤

### 1. 停止正在运行的服务进程

```powershell
taskkill /F /IM OAuth2Server.exe 2>$null; Write-Host "进程已清理"
```

### 2. 执行构建脚本 (Windows)

项目根目录下提供了 `scripts/backend/build.bat`，它会自动执行 Conan 依赖安装和 CMake 编译。

**Debug 构建：**

```powershell
.\scripts\backend\build.bat -debug
```

**Release 构建（默认）：**

```powershell
.\scripts\backend\build.bat -release
```

### 3. 执行构建脚本 (Linux)

```bash
bash scripts/backend/build.sh --debug
# 或
bash scripts/backend/build.sh --release
```

### 4. 验证构建产物

```powershell
Test-Path "build\OAuth2Server\Release\OAuth2Server.exe" -or Test-Path "build\OAuth2Server\Debug\OAuth2Server.exe"
```

## 注意事项

- 构建脚本会自动调用 `scripts/backend/env_setup.bat` 检查环境。
- 构建完成后 `config.json` 会自动复制到对应的构建输出目录和测试目录。
- 默认使用 C++17 标准。
