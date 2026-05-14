# OAuth2 API 接口文档

本服务提供基于 OAuth2.0 标准（RFC 6749）的认证授权服务。

## 1. 授权端点 (Authorization Endpoint)

用于请求用户授权，获取 Authorization Code。

- **URL**: `/oauth2/authorize`
- **Method**: `GET`
- **Access**: 公开 (需登录)

### 请求参数 (Query Parameters)

| 参数名 | 必选 | 描述 | 示例 |
|---|---|---|---|
| `response_type` | 是 | 必须为 `code` | `code` |
| `client_id` | 是 | 客户端 ID | `vue-client` |
| `redirect_uri` | 是 | 回调地址 (需完全匹配) | `http://localhost:5173/callback` |
| `scope` | 否 | 申请的权限范围 | `openid profile` |
| `state` | 建议 | 防止 CSRF 的随机串 | `xyz123` |

### 响应

**成功响应**：
重定向至 `redirect_uri`，并附带 `code` 和 `state`。

```http
HTTP/1.1 302 Found
Location: http://localhost:5173/callback?code=SplxlOBeZQQYbYS6WxSbIA&state=xyz123
```

**错误响应**：
直接返回 JSON 错误或重定向带 error 参数。

```json
{
  "error": "invalid_client",
  "error_description": "Unknown client_id"
}
```

---

## 2. 令牌端点 (Token Endpoint)

用于使用 Authorization Code 换取 Access Token。

- **URL**: `/oauth2/token`
- **Method**: `POST`
- **Access**: 公开 (需 Client 认证)
- **Content-Type**: `application/x-www-form-urlencoded`

### 请求参数 (Form Data)

| 参数名 | 必选 | 描述 | 示例 |
|---|---|---|---|
| `grant_type` | 是 | 必须为 `authorization_code` | `authorization_code` |
| `code` | 是 | 上一步获取的 code | `SplxlOBeZQQYbYS6WxSbIA` |
| `redirect_uri` | 是 | 必须与获取 code 时一致 | `http://localhost:5173/callback` |
| `client_id` | 是 | 客户端 ID | `vue-client` |
| `client_secret` | 是 | 客户端密钥 (用于验证) | `vue-secret` |

### 响应

**成功 (200 OK)**:

```json
{
  "access_token": "2YotnFZFEjr1zCsicMWpAA",
  "token_type": "Bearer",
  "expires_in": 3600,
  "refresh_token": "tGzv3JOkF0XG5Qx2TlKWIA",
  "scope": "openid profile"
}
```

*(注：`refresh_token` 字段当前已支持通过 `grant_type=refresh_token` 换取新的 Access Token。仅 `refresh_token` 的持久化存储（Postgres 后端）在部分配置下为 pass-through，Redis/Memory 后端暂不存储。)*

**失败 (400/401)**:

```json
{
  "error": "invalid_grant",
  "error_description": "Authorization code has expired"
}
```

**失败 (429 Too Many Requests)**:
请求频率过高，触发限流。

```text
Too Many Requests
```

---

## 3. 用户信息端点 (UserInfo Endpoint)

用于验证 Access Token 并获取用户信息。

- **URL**: `/oauth2/userinfo`
- **Method**: `GET`
- **Access**: 受保护 (Bearer Token)

### 请求头 (Headers)

Authorization: `Bearer {access_token}`

### 响应

**成功 (200 OK)**:

```json
{
  "sub": "admin",
  "name": "admin",
  "email": "admin@example.com",
  "picture": "..."
}
```

**失败 (401 Unauthorized)**:

```json
{
  "error": "invalid_token"
}
```

---

## 4. 辅助接口 (Helper Endpoints)

### 登录提交 (Internal)

- **URL**: `/oauth2/login`
- **Method**: `POST`
- **Desc**: 内部使用的表单提交接口，用于 Session 登录并重定向。

### WeChat 登录 (Optional)

- **URL**: `/api/wechat/login`
- **Method**: `POST`
- **Desc**: 处理微信小程序/扫码登录（演示用途）。

### Google 登录回调 (Optional)

- **URL**: `/google/login`
- **Method**: `POST`
- **Desc**: 接收前端传来的 Google Authorization Code，服务端向 Google 换取 Access Token 并调用 UserInfo API，返回过滤后的用户信息（`sub`, `name`, `email`, `picture`）。
- **请求参数**:
  - `code` (required): Google 返回的授权码
- **成功 (200 OK)**:
  ```json
  {"sub": "1234567890", "name": "John Doe", "email": "john@gmail.com", "picture": "..."}
  ```
- **失败 (400/502)**: code 无效或 Google API 不可达。

### 用户注册

- **URL**: `/api/register`
- **Method**: `POST`
- **Content-Type**: `application/x-www-form-urlencoded`
- **限流**: 每IP每分钟最多 5 次，全局每分钟 5000 次（Hodor 插件）

#### 请求参数 (Form Data)

| 参数名 | 必选 | 描述 |
|---|---|---|
| `username` | 是 | 用户名 |
| `password` | 是 | 密码（明文，服务端 SHA256+Salt 存储）|
| `email` | 否 | 邮件地址 |

#### 响应

- **成功 (200 OK)**: `User Registered`
- **失败 (400 Bad Request)**: 缺少用户名或密码
- **失败 (500 Internal Server Error)**: 用户名已存在等

### 管理员 Dashboard (RBAC Protected)

- **URL**: `/api/admin/dashboard`
- **Method**: `GET`
- **Access**: 受保护，需 `admin` 角色（Header: `Authorization: Bearer <token>`）

#### 响应

- **成功 (200 OK)**:
  ```json
  {"message": "Welcome to Admin Dashboard", "status": "success"}
  ```
- **失败 (401)**: Token 无效或缺失
- **失败 (403)**: 用户已登录但不具备 `admin` 角色

---

## 5. 通用错误码

| HTTP Status | 描述 | 原因示例 |
|---|---|---|
| `200` | OK | 请求成功 |
| `302` | Found | 重定向 (如 OAuth2 授权跳转) |
| `400` | Bad Request | 参数错误, `invalid_grant`, `invalid_client` |
| `401` | Unauthorized | Token 无效或过期 |
| `403` | Forbidden | **RBAC 拦截**: 用户已登录但缺少所需角色 |
| `429` | Too Many Requests | 触发限流 (Rate Limiting) |
| `500` | Internal Server Error | 服务器内部错误 |
