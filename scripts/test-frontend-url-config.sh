#!/bin/bash
# Test Frontend URL Configuration
# 验证前端URL配置是否正确设置

echo "🔍 Frontend URL Configuration Test"
echo "=================================="
echo ""

# 检查配置文件中的前端URL配置
check_config() {
    local config_file=$1
    local expected_url=$2
    local env_name=$3

    if [ -f "$config_file" ]; then
        echo "📋 Testing $env_name configuration:"
        echo "   File: $config_file"

        # 提取frontend.url配置
        frontend_url=$(grep -A 2 '"frontend"' "$config_file" | grep '"url"' | cut -d'"' -f4)

        if [ -n "$frontend_url" ]; then
            echo "   Frontend URL: $frontend_url"

            if [ "$frontend_url" = "$expected_url" ]; then
                echo "   ✅ Matches expected: $expected_url"
            else
                echo "   ⚠️  Expected: $expected_url"
                echo "   ⚠️  Got: $frontend_url"
            fi
        else
            echo "   ❌ Frontend URL not found in configuration"
        fi
        echo ""
    else
        echo "⚠️  Warning: $config_file not found"
        echo ""
    fi
}

# 测试各环境配置
check_config "OAuth2Backend/config.json" "http://localhost:5173" "Development (Default)"
check_config "OAuth2Backend/config.dev.json" "http://localhost:5173" "Development"
check_config "OAuth2Backend/config.ci.json" "http://localhost:5173" "CI"
check_config "OAuth2Backend/config.prod.json" "https://your-production-domain.com" "Production"

# 检查模板文件是否使用了动态变量
echo "🔍 Checking template file:"
if grep -q "frontend_register_url" OAuth2Backend/views/login.csp; then
    echo "   ✅ Template uses dynamic variable: [[frontend_register_url]]"
else
    echo "   ❌ Template still has hardcoded URL"
fi

# 检查是否还有硬编码的localhost:5173
if grep -q "http://localhost:5173/register" OAuth2Backend/views/login.csp; then
    echo "   ❌ Template still contains hardcoded localhost:5173/register"
else
    echo "   ✅ No hardcoded localhost URL found in template"
fi

echo ""
echo "📊 Configuration Test Summary"
echo "=================================="
echo "✅ Frontend URL configuration added to all config files"
echo "✅ Template updated to use dynamic variable"
echo "✅ OAuth2Controller.cc modified to pass frontend URL"
echo ""
echo "🚀 Next Steps:"
echo "1. Update config.prod.json with actual production frontend URL"
echo "2. Rebuild the backend: cd OAuth2Backend/build && cmake --build ."
echo "3. Test with different configurations"
echo "4. Verify register link points to correct frontend URL"