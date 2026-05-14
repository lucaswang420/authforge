# manage.ps1 - Unified project management script for Windows

$Action = $args[0]
$Config = "Release"

# Parse global options
foreach ($arg in $args) {
    if ($arg -eq "-debug") { $Config = "Debug" }
}

function Show-Help {
    Write-Host "Usage: .\manage.ps1 <command> [options]"
    Write-Host ""
    Write-Host "Commands:"
    Write-Host "  build-backend [-debug]  Build the C++ project (Plugin + Server)"
    Write-Host "  test-backend [-debug]   Run backend tests"
    Write-Host "  build-frontend          Build the Vue frontend"
    Write-Host "  dev-frontend            Run frontend in dev mode"
    Write-Host "  docker-up               Start the full stack with Docker Compose"
    Write-Host "  docker-down             Stop the Docker Compose stack"
    Write-Host "  clean                   Clean build artifacts"
}

if (-not $Action) {
    Show-Help
    exit
}

switch ($Action) {
    "build-backend" {
        & ".\scripts\backend\build.bat" "-$($Config.ToLower())"
    }
    "test-backend" {
        $BuildDir = "build"
        if (Test-Path "build") {
            Set-Location "build"
            # For multi-config generators like MSVC
            if (Test-Path "OAuth2Server\test\$Config") {
                ctest -C $Config --output-on-failure
            } else {
                ctest --output-on-failure
            }
            Set-Location ".."
        } else {
            Write-Host "Build directory not found. Please build first."
        }
    }
    "build-frontend" {
        Set-Location "OAuth2Frontend"
        npm install
        npm run build
        Set-Location ".."
    }
    "dev-frontend" {
        Set-Location "OAuth2Frontend"
        npm install
        npm run dev
        Set-Location ".."
    }
    "docker-up" {
        docker-compose up -d
    }
    "docker-down" {
        docker-compose down
    }
    "clean" {
        if (Test-Path "build") {
            Remove-Item -Recurse -Force "build"
        }
        if (Test-Path "OAuth2Frontend\dist") {
            Remove-Item -Recurse -Force "OAuth2Frontend\dist"
        }
        Write-Host "Cleaned build artifacts."
    }
    default {
        Write-Host "Unknown command: $Action"
        Show-Help
    }
}
