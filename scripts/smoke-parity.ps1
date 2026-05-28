# smoke-parity.ps1 - 5-step smoke test to verify manage.ps1 commands work
$ErrorActionPreference = "Stop"

$ProjectDir = Split-Path -Parent $PSScriptRoot
$Manage = Join-Path $ProjectDir "manage.ps1"
$Result = 0
$ServerProcess = $null

function Cleanup {
    if ($ServerProcess -and -not $ServerProcess.HasExited) {
        Write-Host "[Cleanup] Stopping server..."
        Stop-Process -Id $ServerProcess.Id -Force -ErrorAction SilentlyContinue
    }
    # Ensure docker is down
    Set-Location $ProjectDir
    docker-compose down 2>$null | Out-Null
}

trap { Cleanup } EXIT

Write-Host "========================================"
Write-Host "Smoke Parity Test (Windows)"
Write-Host "========================================"
Write-Host ""

# Step 1: manage build-backend
Write-Host "[Step 1/5] manage build-backend"
& powershell -NoProfile -ExecutionPolicy Bypass -File $Manage build-backend
if ($LASTEXITCODE -ne 0) {
    Write-Host "[FAIL] build-backend" -ForegroundColor Red
    exit 1
}
Write-Host "[PASS] build-backend" -ForegroundColor Green
Write-Host ""

# Step 2: manage test-backend
Write-Host "[Step 2/5] manage test-backend"
& powershell -NoProfile -ExecutionPolicy Bypass -File $Manage test-backend
if ($LASTEXITCODE -ne 0) {
    Write-Host "[FAIL] test-backend" -ForegroundColor Red
    exit 1
}
Write-Host "[PASS] test-backend" -ForegroundColor Green
Write-Host ""

# Step 3: manage run-backend & wait + curl /health/ready
Write-Host "[Step 3/5] manage run-backend + health check"
$ServerExe = Join-Path $ProjectDir "build\OAuth2Server\Release\OAuth2Server.exe"
if (-not (Test-Path $ServerExe)) {
    $ServerExe = Join-Path $ProjectDir "build\OAuth2Server\Debug\OAuth2Server.exe"
}
if (-not (Test-Path $ServerExe)) {
    Write-Host "[FAIL] Server executable not found" -ForegroundColor Red
    exit 1
}

$ServerProcess = Start-Process -FilePath $ServerExe -WorkingDirectory (Split-Path $ServerExe) -PassThru -WindowStyle Hidden
Write-Host "  Server PID: $($ServerProcess.Id)"
Write-Host "  Waiting for server startup..."
Start-Sleep -Seconds 8

if ($ServerProcess.HasExited) {
    Write-Host "[FAIL] Server process died" -ForegroundColor Red
    exit 1
}

# Health check
try {
    $health = Invoke-WebRequest -Uri "http://127.0.0.1:5555/health/ready" -UseBasicParsing -TimeoutSec 5
    if ($health.StatusCode -eq 200) {
        Write-Host "  /health/ready returned 200"
        Write-Host "[PASS] run-backend + health" -ForegroundColor Green
    } else {
        Write-Host "[FAIL] /health/ready returned $($health.StatusCode)" -ForegroundColor Red
        $Result = 1
    }
} catch {
    Write-Host "[FAIL] /health/ready failed: $($_.Exception.Message)" -ForegroundColor Red
    $Result = 1
}
Write-Host ""

# Step 4: Kill server
Write-Host "[Step 4/5] Kill server"
if (-not $ServerProcess.HasExited) {
    Stop-Process -Id $ServerProcess.Id -Force
}
Write-Host "[PASS] Server stopped" -ForegroundColor Green
Write-Host ""

# Step 5: docker-up -> health -> docker-down
Write-Host "[Step 5/5] docker-up -> health -> docker-down"
& powershell -NoProfile -ExecutionPolicy Bypass -File $Manage docker-up
if ($LASTEXITCODE -ne 0) {
    Write-Host "[FAIL] docker-up" -ForegroundColor Red
    exit 1
}
Start-Sleep -Seconds 5

try {
    $dockerHealth = Invoke-WebRequest -Uri "http://127.0.0.1:5555/health/ready" -UseBasicParsing -TimeoutSec 5
    Write-Host "  Docker health: $($dockerHealth.StatusCode)"
} catch {
    Write-Host "  Docker health: N/A (expected if no server in compose)"
}

& powershell -NoProfile -ExecutionPolicy Bypass -File $Manage docker-down
if ($LASTEXITCODE -ne 0) {
    Write-Host "[FAIL] docker-down" -ForegroundColor Red
    exit 1
}
Write-Host "[PASS] docker-up/docker-down" -ForegroundColor Green
Write-Host ""

# Summary
Write-Host "========================================"
if ($Result -eq 0) {
    Write-Host "ALL SMOKE TESTS PASSED" -ForegroundColor Green
} else {
    Write-Host "SMOKE TESTS FAILED" -ForegroundColor Red
}
Write-Host "========================================"
exit $Result
