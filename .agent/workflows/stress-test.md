---
description: 使用 drogon_ctl press 执行负载与限流压力测试
---

# 压力测试 (Stress Test)

验证系统在高并发下的稳定性及限流机制 (Hodor 插件) 的有效性。

## 1. 基础设施检查

确保 Redis 和 Postgres 正常运行（限流通常依赖 Redis）。

```powershell
docker-compose up -d oauth2-postgres oauth2-redis
```

## 2. 启动服务器

在后台启动 OAuth2Server (Release)。

```powershell
.\scripts\backend\run_server.bat -release
```

## 3. 基准性能测试 (Baseline)

测试静态页面或健康检查端点的吞吐量。
参数: -n 10000 (请求数), -c 100 (并发数), -t 4 (线程数)

```powershell
drogon_ctl press -n 10000 -c 100 -t 4 http://127.0.0.1:5555/health
```

## 4. 限流机制验证 (Rate Limiting)

对 `/api/register` 或 `/oauth2/token` 发起持续请求，验证 Hodor 插件是否返回 429。

```powershell
# 模拟 50 并发下的 POST 请求
drogon_ctl press -n 1000 -c 50 -t 2 -m POST http://127.0.0.1:5555/oauth2/token
```

## 5. 停止服务器

```powershell
taskkill /F /IM OAuth2Server.exe 2>$null
```
