---
description: 提交前完整质量检查（DB + ORM + Build + Unit Test + E2E Test）
---

# Pre-Commit 检查

在提交代码前，建议运行完整的一键测试脚本，确保没有任何功能回归。

## 1. 运行一键测试脚本 (推荐)

项目提供了 `scripts/backend/full_test.bat`，它会自动执行完整的生命周期检查。

```powershell
# 运行一键测试
.\scripts\backend\full_test.bat
```

**该脚本包含以下步骤：**
1. **数据库重置**: 删除并重建 `oauth2_db`。
2. **模型生成**: 重新生成 ORM 模型。
3. **项目构建**: 执行 Release 构建。
4. **单元测试**: 运行 CTest 单元和集成测试。
5. **端点测试**: 启动服务器并运行 `test-oauth2-endpoints.bat`。
6. **自动清理**: 停止测试服务器。

---

## 2. 分步手动检查

如果你需要分步验证，请按以下顺序执行：

### A. 基础设施检查

```powershell
# 确保 Redis (6379) 和 Postgres (5432) 已启动
docker-compose up -d oauth2-postgres oauth2-redis
```

### B. ORM 模型生成

```powershell
.\scripts\backend\generate_models.bat -y
```

### C. 构建验证

```powershell
.\scripts\backend\build.bat -release
```

### D. 执行 CTest 测试

```powershell
.\scripts\backend\test.bat -release
```

### E. API 端点验证

```powershell
# 在另一个终端启动服务器
.\scripts\backend\run_server.bat -release

# 运行端点测试
.\scripts\backend\test-oauth2-endpoints.bat -NoPause
```

---

## 检查清单

- [ ] 数据库 Schema 已更新且初始化成功
- [ ] ORM 模型与数据库结构一致
- [ ] 项目在 Release 模式下编译通过
- [ ] 所有单元测试和集成测试 (CTest) 通过
- [ ] 关键 OAuth2 流程 (Login, Token, UserInfo) 验证通过
- [ ] 文档 (docs/、README.md) 已同步更新

## 失败处理

- 如果任一步骤失败，请查看 `OAuth2Server/logs/` 目录下的日志。
- 修复问题后，建议重新运行 `full_test.bat` 进行最终确认。
