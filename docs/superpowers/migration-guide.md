# 技能现代化升级迁移指南

## 升级概述

本次升级将所有项目技能更新以匹配重构后的项目结构。

## 主要变更

### 路径变更
- `OAuth2Backend/` → `OAuth2Server/`
- `OAuth2Backend/build/` → `build/OAuth2Server/`
- `OAuth2Backend/sql/` → `OAuth2Server/sql/`
- `OAuth2Backend/controllers/` → `OAuth2Server/controllers/`

### 新增功能
- ✅ `manage.ps1` 统一管理接口
- ✅ Docker 专项脚本集成
- ✅ 环境自动检测
- ✅ 跨平台兼容性改进

## 使用建议

### 推荐工作流
1. 使用 `.\manage.ps1` 统一接口
2. Docker 环境优先使用 `full_test_docker.bat`
3. 本地开发使用 `scripts/backend/build.bat`
4. 数据库操作优先使用 `docker_postgres_start.bat`

### 兼容性
- 所有技能向后兼容
- 支持降级到直接脚本调用
- 跨平台一致性保证

## 技能更新摘要

### build-and-test
- 使用 `.\manage.ps1 build-backend [-debug]`
- 支持新构建路径 `build/OAuth2Server/`
- 智能检测编译器环境

### db-reset
- 更新 SQL 脚本路径到 `OAuth2Server/sql/`
- 添加 Docker 环境自动检测
- 集成 `docker_postgres_start.bat` 脚本

### orm-gen
- 更新模型配置路径到 `OAuth2Server/model.json`
- 集成 `generate_models.bat` 脚本
- 更新构建输出路径

### openapi-update
- 更新控制器路径到 `OAuth2Server/controllers/`
- 更新 OpenAPI 规范路径到 `OAuth2Server/openapi.yaml`
- 集成 `validate-openapi.sh` 脚本

### e2e-test
- 集成 `full_test_docker.bat` 完整流程
- 添加环境自动检测
- 改进健康检查和验证

### docker-integration-test
- 集成 Docker 专项脚本
- 优化多服务测试流程
- 增强监控和报告生成

## 故障排除

如遇到问题，请检查：
1. 项目路径是否正确
2. 构建输出路径是否为 `build/OAuth2Server/`
3. SQL 脚本路径是否为 `OAuth2Server/sql/`
4. Docker 服务是否正常运行

## 向后兼容性

所有现有技能保持向后兼容：
- 自动降级到直接脚本调用
- 保持原有接口不变
- 零学习成本

---
**升级日期**: 2026-05-18  
**版本**: v1.0.0  
**状态**: ✅ 完成并测试