# Docker Testing Guide

This guide explains how to use Docker for testing OAuth2Backend on Windows.

## Version Compatibility

✅ **Docker test environment matches Linux CI 100%**

| Component | Version | Matches CI |
|-----------|---------|------------|
| PostgreSQL | `postgres:15-alpine` | ✅ Linux CI |
| Redis | `redis:alpine` | ✅ Linux CI |
| Drogon | `v1.9.12` | ✅ Linux CI |
| Ubuntu | `22.04` | ✅ Linux CI |
| C++ Standard | C++17 | ✅ Linux CI |

See [DOCKER_VERSION_COMPATIBILITY.md](DOCKER_VERSION_COMPATIBILITY.md) for detailed comparison.

## Prerequisites

1. **Docker Desktop for Windows** - Install from [docker.com](https://www.docker.com/products/docker-desktop)
2. **Drogon Framework** - Locally installed (same as standard development)
3. **Build Tools** - Visual Studio 2019/2022, CMake, Conan (same as standard development)

## Quick Start

### One-Command Docker Test

```cmd
cd OAuth2Backend\scripts
full_test_docker.bat
```

This script will:
1. ✅ Start PostgreSQL in Docker container
2. ✅ Wait for database to be ready
3. ✅ Initialize database schema
4. ✅ Generate ORM models
5. ✅ Build project
6. ✅ Run unit tests
7. ✅ Start server
8. ✅ Test OAuth2 endpoints
9. ✅ Cleanup (stop server and containers)

## Docker Architecture

```
┌─────────────────────────────────────────────────┐
│              Windows Host                        │
│                                                  │
│  ┌───────────────────────────────────────────┐  │
│  │  Local Build Environment                  │  │
│  │  - Visual Studio                          │  │
│  │  - CMake                                  │  │
│  │  - Conan                                  │  │
│  │  - Drogon Framework                       │  │
│  │  - OAuth2Backend.exe (compiled here)      │  │
│  └───────────────────────────────────────────┘  │
│                    │                            │
│                    │ connects via localhost    │
│                    ▼                            │
│  ┌───────────────────────────────────────────┐  │
│  │  Docker Container (PostgreSQL)            │  │
│  │  - postgres:14                            │  │
│  │  - Port 5432:5432                         │  │
│  │  - Database: oauth_test                   │  │
│  └───────────────────────────────────────────┘  │
└─────────────────────────────────────────────────┘
```

## Manual Docker Operations

### Start PostgreSQL Container

```cmd
cd OAuth2Backend
docker-compose up -d postgres
```

### Check Container Status

```cmd
docker-compose ps
```

### View PostgreSQL Logs

```cmd
docker-compose logs postgres
```

### Connect to PostgreSQL

```cmd
docker exec -it oauth2-postgres psql -U test -d oauth_test
```

### Stop Container

```cmd
docker-compose down
```

## Configuration

### Database Connection (config.json)

The server connects to PostgreSQL using localhost:

```json
{
  "db_client_name": "default",
  "db_host": "127.0.0.1",
  "db_port": 5432,
  "db_name": "oauth_test",
  "db_user": "test",
  "db_password": "123456"
}
```

### Docker Compose Configuration

See [docker-compose.yml](../docker-compose.yml):

```yaml
services:
  postgres:
    image: postgres:14
    environment:
      POSTGRES_USER: test
      POSTGRES_PASSWORD: 123456
    ports:
      - "5432:5432"
```

## Troubleshooting

### Docker not running

**Error**: `error during connect: ... docker daemon is not running`

**Solution**: Start Docker Desktop

### Port already in use

**Error**: `port is already allocated`

**Solution**: Check if local PostgreSQL is running and stop it:
```cmd
# Check if PostgreSQL is running
netstat -ano | findstr :5432

# Stop local PostgreSQL service (if running)
net stop postgresql-x64-14
```

### Container won't start

**Error**: Container exits immediately

**Solution**: Check logs:
```cmd
docker-compose logs postgres
```

### Database connection refused

**Error**: `connection refused` or `could not connect to server`

**Solution**:
1. Wait for PostgreSQL to be ready (may take 10-20 seconds)
2. Check container health:
   ```cmd
   docker exec oauth2-postgres pg_isready -U test
   ```

## Comparison: Docker vs Local PostgreSQL

| Feature | Local PostgreSQL | Docker PostgreSQL |
|---------|------------------|-------------------|
| Setup | Manual installation | Automatic with docker-compose |
| Isolation | Shared with system | Isolated container |
| Port conflicts | Possible | Can map different ports |
| Data persistence | System-managed | Docker volume |
| Cleanup | Manual SQL commands | `docker-compose down` |
| Reproducibility | System-dependent | Container-based, reproducible |

## Advanced Usage

### Custom PostgreSQL Version

Edit `docker-compose.yml`:
```yaml
services:
  postgres:
    image: postgres:15  # Change version here
```

### Multiple Database Instances

Edit `docker-compose.yml`:
```yaml
services:
  postgres1:
    image: postgres:14
    ports:
      - "5432:5432"

  postgres2:
    image postgres:14
    ports:
      - "5433:5432"  # Different port
```

### Persistent Data

Data is stored in Docker volume `postgres_data`. To remove:
```cmd
docker-compose down -v  # Remove volumes too
```

## CI/CD Integration

The Docker setup is ideal for CI/CD pipelines:

```yaml
# Example GitHub Actions
- name: Start PostgreSQL
  run: docker-compose up -d postgres

- name: Wait for database
  run: |
    for i in {1..30}; do
      if docker exec oauth2-postgres pg_isready -U test; then
        break
      fi
      sleep 1
    done

- name: Run tests
  run: full_test_docker.bat

- name: Cleanup
  run: docker-compose down
```

## Performance Considerations

- **Docker overhead**: Minimal impact on PostgreSQL performance
- **Network latency**: Localhost connection has very low latency
- **Build time**: Same as local (compilation is done on Windows host)

## Security Notes

- Default password `123456` is for testing only
- Do not expose port 5432 to public networks
- Use stronger passwords in production
- Consider Docker secrets for sensitive data

## Additional Resources

- [Docker Compose Documentation](https://docs.docker.com/compose/)
- [PostgreSQL Docker Images](https://hub.docker.com/_/postgres)
- [Docker Desktop for Windows](https://www.docker.com/products/docker-desktop)
