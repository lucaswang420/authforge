# OAuth2 Plugin 集成指南

本文档介绍如何将本项目的 `OAuth2Plugin` 移植并集成到其他的 Drogon 应用程序中。在最新的架构中，该插件被设计为一个**独立的 CMake 静态库**，无需复制源代码即可集成。

## 1. 库结构

`OAuth2Plugin` 作为一个独立的组件，其目录结构如下：

* `CMakeLists.txt` — 插件的构建系统
* `include/oauth2/` — 对外暴露的头文件接口（插件声明、配置管理、数据类型）
* `src/` — 核心实现：
    * `OAuth2Plugin.cc` — Drogon 插件生命周期与初始化
    * `controllers/` — 自动注册的 OAuth2 协议端点（如 `/oauth2/token`）
    * `filters/` — 安全拦截器（如 `OAuth2Middleware`）
    * `storage/` — 各种持久化与缓存实现（PostgreSQL, Redis, Memory 等）

## 2. 集成步骤

### 第一步：引入子目录

将 `OAuth2Plugin` 整个目录作为子模块放入你的项目中（例如放在 `libs/` 或项目根目录）。

在顶层的 `CMakeLists.txt` 中添加：

```cmake
# 添加 OAuth2 插件子目录
add_subdirectory(OAuth2Plugin)
```

### 第二步：链接目标库

在你的宿主应用目标（例如 `YourServerApp`）的 `CMakeLists.txt` 中，链接 `OAuth2Plugin` 目标：

```cmake
target_link_libraries(YourServerApp PRIVATE
    Drogon::Drogon
    OAuth2Plugin
)
```
CMake 将自动处理 include 路径和编译依赖。

### 第三步：配置 config.json

插件会自动注册协议路由和 Filter。你只需在宿主应用的 `config.json` 中配置插件以激活它：

```json
{
    "plugins": [
        {
            "name": "OAuth2Plugin",
            "dependencies": [],
            "config": {
                "storage_type": "postgres",
                "postgres": {
                    "db_client_name": "default"
                },
                "redis": {
                    "client_name": "default"
                }
            }
        }
    ],
    "custom_config": {
        "oauth2": {
            "login_url": "/login"
        }
    }
}
```

### 第四步：使用拦截器保护业务 API

在你的业务 Controller 中，只需挂载 `OAuth2Middleware` 即可保护 API：

```cpp
#include <drogon/HttpController.h>

class UserApi : public drogon::HttpController<UserApi>
{
  public:
    METHOD_LIST_BEGIN
    // 自动被 OAuth2Plugin 校验 Bearer Token
    ADD_METHOD_TO(UserApi::getProfile, "/api/me", drogon::Get, "oauth2::filters::OAuth2Middleware");
    METHOD_LIST_END
    // ...
};
```

## 3. 注意事项

1. **自动注册**：只要 `OAuth2Plugin` 被链接到最终的二进制文件中，其包含的 Controller 和 Filter 就会被 Drogon 框架在启动时自动注册。请不要在 `main.cc` 中手动调用初始化宏。
2. **数据库初始化**：使用 PostgreSQL 存储时，确保宿主应用启动前已执行 `OAuth2Server/sql/` 下的结构初始化脚本。
