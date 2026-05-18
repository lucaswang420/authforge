---
description: 停止所有服务进程
---

# 停止服务

## 1. 停止后端进程

```powershell
taskkill /F /IM OAuth2Server.exe 2>$null
Write-Host "后端服务已停止"
```

## 2. 停止基础设施 (Docker)

```powershell
.\scripts\backend\docker_postgres_stop.bat
# 或者彻底删除 volumes
docker-compose down -v
```

## 3. 停止前端进程 (Vite)

```powershell
# 查找并停止 npm/node 进程（默认端口 5173）
$proc = Get-NetTCPConnection -LocalPort 5173 -ErrorAction SilentlyContinue | Select-Object -ExpandProperty OwningProcess
if ($proc) { 
    Stop-Process -Id $proc -Force 
    Write-Host "前端服务已停止" 
}
```

## 4. 验证进程已完全停止

```powershell
$backend = Get-Process -Name OAuth2Server -ErrorAction SilentlyContinue
if (-not $backend) { 
    Write-Host "✅ 所有服务已完全停止" 
} else { 
    Write-Host "⚠️ 仍有后端进程正在运行" 
}
```
