/**
 * Simplified-Chinese (zh-CN) error message resources.
 *
 * Mirror of the canonical source
 *   OAuth2Frontend/src/services/messages/zh-CN.ts
 * kept in lockstep to honor the single-logical-source contract (Requirement
 * 8.6 / 10.7). The cross-app determinism property test (task 9.7) asserts both
 * catalogs return identical messages for every (code, locale).
 *
 * Invariants (Requirements 9.1, 9.4):
 *   - reserved keys `__unknown__` and `__network__` are present and non-empty
 *   - every entry is a non-empty string with no unreplaced placeholders
 *   - no Internal_Detail (SQL/paths/stack traces) beyond Request_ID
 */
export const zhCN: Record<string, string> = {
  // --- Reserved fallback keys (required by errorAdapter) ---
  __unknown__: '发生未知错误，请稍后重试',
  __network__: '网络连接失败，请检查网络后重试',

  // --- Backend Error_Code catalog (design.md initial entries) ---
  NET_CONNECTION_FAILED: '上游连接失败',
  NET_TIMEOUT: '请求超时',
  DB_CONNECTION_ERROR: '服务暂时不可用',
  DB_QUERY_ERROR: '服务暂时不可用',
  DB_CONSTRAINT_VIOLATION: '数据冲突',
  VALIDATION_INVALID_INPUT: '输入参数有误',
  VALIDATION_MISSING_REQUIRED_FIELD: '缺少必填字段',
  VALIDATION_FORMAT_ERROR: '格式不正确',
  AUTH_INVALID_CREDENTIALS: '用户名或密码错误',
  AUTH_TOKEN_EXPIRED: '登录已过期',
  AUTH_TOKEN_INVALID: '登录凭证无效',
  AUTHZ_ACCESS_DENIED: '没有访问权限',
  AUTHZ_INSUFFICIENT_PERMISSIONS: '权限不足',
  INTERNAL_ERROR: '服务器内部错误',

  // --- OAuth2 / RFC 6749 protocol error codes ---
  invalid_request: '请求参数缺失或无效',
  invalid_client: '客户端认证失败',
  invalid_grant: '授权许可无效或已过期',
  unauthorized_client: '客户端无权使用该授权类型',
  unsupported_grant_type: '不支持的授权类型',
  invalid_scope: '请求的 scope 无效',
  server_error: '服务器内部错误',
  temporarily_unavailable: '服务暂时不可用',

  // --- RFC 7009 (token revocation) §2.2.1 ---
  unsupported_token_type: '不支持的令牌类型',

  // --- RFC 8628 (device authorization grant) §3.5 polling error codes ---
  authorization_pending: '授权尚未完成，请稍后重试',
  slow_down: '轮询过于频繁，请降低频率',
  expired_token: '设备码已过期，请重新发起授权',
}
