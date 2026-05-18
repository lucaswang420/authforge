---
description: Windows Docker 环境配置与验证
---

# Docker 环境配置指南

## 1. 前置条件

确保已安装 **Docker Desktop for Windows** 并启用了 **WSL 2** 后端。

## 2. 快速启动数据库 (Docker)

如果你只需要在 Docker 中启动 PostgreSQL 数据库，可以使用以下脚本：

```powershell
.\scripts\backend\docker_postgres_start.bat
```

该脚本会自动处理 `docker-compose down` 并重新启动 `oauth2-postgres` 服务，同时等待数据库就绪。

## 3. 构建调试镜像

项目采用统一的多阶段构建 Dockerfile。要重建调试环境镜像：

**Windows (PowerShell/WSL):**
```bash
bash scripts/backend/rebuild-debug-image.sh
```

该脚本会构建 `backend-dev` 目标并标记为 `oauth2-backend-debug:v1.9.12`。

## 4. 验证完整环境

要验证整个 Docker 编译和测试链，可以使用一键测试脚本：

```powershell
.\scripts\backend\full_test_docker.bat
```

该脚本会执行以下步骤：
1. 启动 Docker 中的 PostgreSQL。
2. 初始化数据库。
3. 在本地构建并运行测试。
4. 启动服务器并验证 OAuth2 端点。
5. 清理环境。

## 5. 常见问题

- **换行符问题**: 在 Windows 下编辑的 `.sh` 脚本在容器内运行时可能因 CRLF 报错，建议使用 `git config --global core.autocrlf input`。
- **内存分配**: 建议为 Docker Desktop 分配至少 4GB 内存。
