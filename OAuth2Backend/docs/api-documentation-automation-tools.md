# OpenAPI Documentation Automation Tools

本文档介绍OpenAPI文档维护的自动化工具，包括pre-commit hooks和CI集成。

## 📋 概述

这些自动化工具确保OpenAPI文档的质量和一致性：

- **Pre-commit hooks**: 在每次提交前验证OpenAPI文档
- **CI集成脚本**: 在CI/CD流程中进行完整验证
- **安装工具**: 简化hooks的安装和管理

## 🔧 工具说明

### 1. OpenAPI验证脚本 (`validate-openapi.sh`)

统一的OpenAPI文档验证脚本，用于：
- **Pre-commit Hook**: 每次commit前自动验证
- **CI/CD集成**: 在CI管道中进行完整验证

**验证内容：**
1. 构建项目
2. 运行OpenAPI测试套件
3. 验证JSON结构有效性
4. 检查必需字段（openapi, info, paths, servers）
5. 检查文档覆盖率（描述和示例）
6. 安全文档检查

**使用方法：**
```bash
# 本地运行完整验证
./scripts/validate-openapi.sh

# 自动触发（在commit时，如果安装了pre-commit hook）
git commit -m "your message"

# 如果需要跳过验证
git commit --no-verify -m "your message"
```

### 2. Hook安装脚本 (`install-hooks.sh`)

自动安装和配置git hooks：

**功能：**
- 安装pre-commit hook（链接到validate-openapi.sh）
- 创建hooks配置文件
- 设置正确的文件权限
- 备份现有hooks

**使用方法：**
```bash
# 安装hooks
./scripts/install-hooks.sh

# 卸载hooks
rm .git/hooks/pre-commit
```

## 🚀 快速开始

### 1. 安装Hooks

```bash
# 确保脚本有执行权限
chmod +x scripts/*.sh

# 运行安装脚本
./scripts/install-hooks.sh
```

### 2. 验证安装

```bash
# 检查hook是否已安装
ls -la .git/hooks/pre-commit

# 手动测试hook
./scripts/validate-openapi.sh
```

### 3. 正常开发流程

```bash
# 1. 修改代码和API文档
git add .

# 2. Commit时自动触发验证
git commit -m "docs: update API documentation"

# 3. 如果验证失败，修复后重新提交
git add .
git commit -m "docs: fix API documentation issues"
```

## 📊 验证标准

### 必需字段

OpenAPI规范必须包含以下字段：
- `openapi`: 版本号（必须为3.0.0）
- `info`: API基本信息
- `paths`: 端点路径
- `servers`: 服务器配置

### 覆盖率要求

- **描述覆盖率**: ≥ 80% 的端点必须有description
- **示例覆盖率**: ≥ 50% 的端点应该有responseExamples
- **安全文档**: 所有需要认证的端点必须标记requiresAuth

### 测试要求

- 所有OpenAPI生成器测试必须通过
- JSON结构必须有效
- 必需字段不能缺失

## 🔍 故障排查

### 问题：Hook未触发

**症状**: Commit时没有看到验证消息

**解决方案**:
```bash
# 检查hook权限
ls -la .git/hooks/pre-commit

# 重新安装hook
./scripts/install-hooks.sh
```

### 问题：JSON验证失败

**症状**: "OpenAPI JSON is invalid"

**解决方案**:
```bash
# 手动检查JSON
python3 -m json.tool build/Release/docs/api/openapi.json

# 重新构建项目
cmake --build build --config Release
```

### 问题：测试失败

**症状**: OpenAPI测试不通过

**解决方案**:
```bash
# 运行详细测试输出
cd build
ctest -C Release -R OpenApiGenerator --output-on-failure

# 检查OpenAPI文档生成
./Release/OAuth2Server -c config.json
curl http://localhost:5555/docs/api/openapi.json | jq .
```

## 📝 配置文件

Hooks配置位于 `.hooks/config.json`:

```json
{
  "hooks": {
    "pre-commit": {
      "enabled": true,
      "description": "Validate OpenAPI specification before commit",
      "script": "scripts/validate-openapi.sh"
    },
    "pre-push": {
      "enabled": false,
      "description": "Run full CI validation before push",
      "script": "scripts/validate-openapi.sh"
    }
  },
  "validation": {
    "min_description_coverage": 80,
    "min_example_coverage": 50,
    "required_fields": ["openapi", "info", "paths", "servers"]
  }
}
```

## 🔗 相关文档

- [API文档维护工作流程](./api-documentation-maintenance-workflow.md)
- [OpenAPI规范](https://swagger.io/specification/)
- [Drogon框架文档](https://drogon.docs.sourceforge.io/)

## 📚 最佳实践

1. **提交前验证**: 依赖pre-commit hook自动验证
2. **定期手动验证**: 使用CI脚本进行完整检查
3. **持续改进**: 根据项目需求调整验证标准
4. **文档同步**: API变更时及时更新OpenAPI文档
5. **示例优先**: 为新端点提供响应示例

## 🆘 获取帮助

如果遇到问题：
1. 检查本文档的故障排查部分
2. 查看脚本输出中的详细错误信息
3. 运行CI脚本获取完整的验证报告
4. 参考OpenAPI 3.0规范文档

---

**最后更新**: 2026-05-01
**维护者**: OAuth2 Plugin 开发团队
