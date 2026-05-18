---
description: 启动前后端服务
---

# 启动服务

## 1. 启动后端服务 (Windows)

使用项目提供的启动脚本，它会自动处理路径并加载正确的 `config.json`。

**启动 Release 版本 (推荐):**
```powershell
.\scripts\backend\run_server.bat -release
```

**启动 Debug 版本:**
```powershell
.\scripts\backend\run_server.bat -debug
```

## 2. 启动前端服务 (Vite)

```powershell
cd OAuth2Frontend
npm run dev
```

## 3. 验证服务状态

- **后端 API**: <http://localhost:5555/health>
- **前端 UI**: <http://localhost:5173>
- **Swagger UI**: <http://localhost:5555/docs/api/>

## 服务端口详情

| 服务 | 端口 | 环境 | 说明 |
|------|------|------|------|
| 后端 API | 5555 | 本地 | 主服务接口 |
| 前端 UI | 5173 | 本地 | Vue 开发服务器 |
| PostgreSQL | 5432 | Docker | 默认数据库端口 |
| Redis | 6379 | Docker | 默认缓存端口 |
