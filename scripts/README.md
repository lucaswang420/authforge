# Security Scripts

本目录包含 OAuth2 项目的安全检查脚本，用于防止敏感信息泄露到版本控制系统。

## 🚀 快速开始

### 推荐：快速安全检查
```bash
# 从任何目录运行
bash scripts/quick-security-check.sh

# 或从 scripts 目录运行
cd scripts
./quick-security-check.sh
```

**优势**：
- ✅ 速度快（2秒内完成）
- ✅ 无误报
- ✅ 适合提交前检查
- ✅ CI/CD 友好

### 完整：深度安全扫描
```bash
bash scripts/security-check.sh
```

**优势**：
- 🔍 扫描代码中的硬编码密钥
- 🔍 检查文件权限
- 🔍 完整的安全审计

**注意**：可能有第三方库的误报（如 Swagger UI）

## 📋 检查项目

### 快速安全检查
1. **敏感文件跟踪** - 检查 `.env` 和 `config.json` 是否被 git 跟踪
2. **Git 忽略配置** - 验证 `.gitignore` 规则正确
3. **示例文件** - 确保配置示例文件存在

### 完整安全检查
包含快速检查的所有项目，额外增加：
1. **硬编码密钥检测** - 扫描代码中的密钥模式
2. **文件权限检查** - 验证配置文件权限
3. **示例文件内容** - 检查示例文件是否包含真实凭证

## 🎯 使用场景

### 开发工作流
```bash
# 1. 修改代码后
git add .

# 2. 提交前运行安全检查
bash scripts/quick-security-check.sh

# 3. 如果通过，提交代码
git commit -m "your message"
```

### CI/CD 集成
```yaml
# .github/workflows/security-check.yml
name: Security Check
on: [push, pull_request]
jobs:
  security:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Run security check
        run: bash scripts/quick-security-check.sh
```

### Pre-commit Hook
```bash
# 安装 pre-commit hook
cat > .git/hooks/pre-commit << 'EOF'
#!/bin/bash
bash scripts/quick-security-check.sh || exit 1
EOF

chmod +x .git/hooks/pre-commit
```

## ⚠️ 误报说明

### Swagger UI 第三方库
完整安全检查可能报告 `swagger-ui-bundle.js` 中的硬编码密钥：

```
⚠️  WARNING: Possible hardcoded secrets found
./OAuth2Backend/docs/api/swagger-ui/swagger-ui-bundle.js
```

**原因**：Swagger UI 的压缩 JavaScript 包包含类似密钥的字符串
**解决**：这是误报，可以安全忽略
**预防**：将 `docs/api/swagger-ui/` 添加到 `.gitignore` 或调整检查规则

### 其他误报情况
- **测试数据**：包含类似密钥格式的测试字符串
- **示例代码**：用于演示的占位符密钥
- **第三方库**：npm/conda 依赖的压缩包

## 🔧 自定义配置

### 修改检查规则
编辑 `scripts/security-check.sh` 中的模式：

```bash
# 添加新的密钥模式
PATTERNS=(
    "wx[a-zA-Z0-9]{16,}"           # WeChat AppID
    "sk-[a-zA-Z0-9]{32,}"          # API keys
    "YOUR_CUSTOM_PATTERN"          # 你的自定义模式
)
```

### 排除目录
```bash
# 在 grep 命令中添加排除目录
--exclude-dir=your_directory
```

## 📊 输出说明

### 成功输出
```
🔒 OAuth2 Project Quick Security Check
========================================

1️⃣ 检查是否有敏感文件被 git 跟踪...
✅ 没有敏感文件被跟踪

2️⃣ 检查 .gitignore 配置...
✅ .gitignore 配置正确

3️⃣ 检查示例文件是否存在...
✅ 示例文件存在

========================================
✅ 所有安全检查通过！
   您可以安全地提交代码。
```

### 错误输出
```
❌ 发现敏感文件被跟踪！
.env
OAuth2Frontend/public/config.json

❌ .gitignore 缺少必要规则
```

### 警告输出
```
⚠️  WARNING: Possible hardcoded secrets found
./OAuth2Backend/docs/api/swagger-ui/swagger-ui-bundle.js
```

## 🛡️ 安全最佳实践

### 1. 永远不要提交
- `.env` 文件
- `OAuth2Frontend/public/config.json`
- 任何包含真实密钥的配置文件
- 数据库密码、API 密钥、私钥

### 2. 始终提交
- `.env.example` - 仅包含占位符
- `config.example.json` - 仅包含示例配置
- `.gitignore` - 防止敏感文件被跟踪

### 3. 定期运行
- 每次提交前运行 `quick-security-check.sh`
- 每周运行一次 `security-check.sh` 完整检查
- CI/CD 管道中集成安全检查

## 🐛 故障排除

### 脚本无法执行
```bash
# 添加执行权限
chmod +x scripts/*.sh
```

### 路径错误
脚本会自动切换到项目根目录，可以从任何位置运行：

```bash
# 从项目根目录
bash scripts/quick-security-check.sh

# 从 scripts 目录
cd scripts && ./quick-security-check.sh

# 从任何子目录
bash ../../scripts/quick-security-check.sh
```

### Git 命令失败
确保你在 git 仓库中：
```bash
git rev-parse --is-inside-work-tree
# 应该输出 "true"
```

## 📞 获取帮助

如果遇到问题：
1. 查看 [docs/security-checklist.md](../docs/security-checklist.md)
2. 运行 `bash scripts/security-check.sh -v` （详细模式）
3. 联系项目维护者

---

**版本**: v1.0
**最后更新**: 2026-05-03
**维护者**: OAuth2 Plugin 开发团队
