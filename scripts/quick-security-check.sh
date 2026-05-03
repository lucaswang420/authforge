#!/bin/bash
# Quick Security Check - 只检查关键的安全问题

# 切换到项目根目录（scripts 目录的父目录）
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"

echo "🔒 OAuth2 Project Quick Security Check"
echo "========================================"
echo ""

ERRORS=0

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

echo "1️⃣ 检查是否有敏感文件被 git 跟踪..."
if git ls-files | grep -E "\.env$|OAuth2Frontend/public/config.json" > /dev/null 2>&1; then
    echo -e "${RED}❌ 发现敏感文件被跟踪！${NC}"
    git ls-files | grep -E "\.env$|OAuth2Frontend/public/config.json"
    ERRORS=$((ERRORS + 1))
else
    echo -e "${GREEN}✅ 没有敏感文件被跟踪${NC}"
fi
echo ""

echo "2️⃣ 检查 .gitignore 配置..."
if grep -q ".env" .gitignore && grep -q "OAuth2Frontend/public/config.json" .gitignore; then
    echo -e "${GREEN}✅ .gitignore 配置正确${NC}"
else
    echo -e "${RED}❌ .gitignore 缺少必要规则${NC}"
    ERRORS=$((ERRORS + 1))
fi
echo ""

echo "3️⃣ 检查示例文件是否存在..."
if [ -f "OAuth2Frontend/.env.example" ] && [ -f "OAuth2Frontend/public/config.example.json" ]; then
    echo -e "${GREEN}✅ 示例文件存在${NC}"
else
    echo -e "${RED}❌ 示例文件缺失${NC}"
    ERRORS=$((ERRORS + 1))
fi
echo ""

echo "========================================"
if [ $ERRORS -eq 0 ]; then
    echo -e "${GREEN}✅ 所有安全检查通过！${NC}"
    echo "   您可以安全地提交代码。"
    exit 0
else
    echo -e "${RED}❌ 发现 $ERRORS 个安全问题${NC}"
    echo "   请修复后再提交代码。"
    exit 1
fi
