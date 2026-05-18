---
description: 重新生成 Drogon ORM 模型
---

# ORM 模型生成

## 前置条件

- PostgreSQL 数据库已启动 (本地或 Docker)
- 数据库表结构已按最新 SQL 脚本初始化
- 已安装 `drogon_ctl` 工具

## 1. 运行生成脚本 (Windows)

项目提供了 `scripts/backend/generate_models.bat` 脚本，它会自动处理备份、生成以及头文件的移动。

```powershell
# 运行脚本并自动确认 (-y)
.\scripts\backend\generate_models.bat -y
```

## 2. 验证生成结果

模型源文件位于 `OAuth2Plugin/src/models`，头文件位于 `OAuth2Plugin/include/oauth2/models`。

```powershell
Get-ChildItem OAuth2Plugin/src/models/*.cc, OAuth2Plugin/include/oauth2/models/*.h | Select-Object Name, LastWriteTime
```

## 配置文件

模型生成配置位于 `OAuth2Server/model.json`：

```json
{
    "rdbms": "postgresql",
    "host": "127.0.0.1",
    "port": 5432,
    "dbname": "oauth_test",
    "user": "test",
    "tables": [
        "users",
        "oauth2_clients",
        "oauth2_codes",
        "oauth2_access_tokens",
        "oauth2_refresh_tokens",
        "oauth2_scopes",
        "oauth2_subject_mappings",
        "oauth2_user_consents",
        "roles",
        "permissions",
        "user_roles",
        "role_permissions"
    ]
}
```

## 注意事项

- ORM 生成的类**禁止手动修改**。
- 生成后需要重新编译项目。
- 脚本会自动备份旧模型到 `OAuth2Plugin/models_backup`。
