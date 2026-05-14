-- OAuth2 Scopes Schema (独立于RBAC系统)
-- OAuth2 scopes用于OAuth2协议的权限范围
-- RBAC系统用于内部权限管理，两者通过mapped_role关联

-- Drop existing tables if they exist (for clean setup)
DROP TABLE IF EXISTS oauth2_user_consents CASCADE;
DROP TABLE IF EXISTS oauth2_client_scopes CASCADE;
DROP TABLE IF EXISTS oauth2_scopes CASCADE;

-- OAuth2 Scopes Table
-- 定义OAuth2协议中的权限范围
CREATE TABLE oauth2_scopes (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) UNIQUE NOT NULL,
    description TEXT,
    mapped_role VARCHAR(50),  -- 映射到RBAC role
    is_default BOOLEAN DEFAULT FALSE,
    requires_admin_role BOOLEAN DEFAULT FALSE  -- 是否要求用户具有admin角色
);

-- Client-Scope Association Table
-- 定义每个OAuth2客户端允许请求的scopes
CREATE TABLE oauth2_client_scopes (
    id SERIAL PRIMARY KEY,
    client_id VARCHAR(50) REFERENCES oauth2_clients(client_id) ON DELETE CASCADE,
    scope_name VARCHAR(100) REFERENCES oauth2_scopes(name) ON DELETE CASCADE,
    UNIQUE(client_id, scope_name)
);

-- User Consent Table
-- 记录用户对客户端授予的scope权限 (用于审计和合规)
-- 修正: 使用internal_user_id而非subject，确保与RBAC系统的兼容性
CREATE TABLE oauth2_user_consents (
    id SERIAL PRIMARY KEY,
    internal_user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,  -- ✅ 使用内部user_id
    client_id VARCHAR(50) REFERENCES oauth2_clients(client_id) ON DELETE CASCADE,
    scope_name VARCHAR(100) REFERENCES oauth2_scopes(name) ON DELETE CASCADE,
    granted_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(internal_user_id, client_id, scope_name)
);

-- 新增: Subject映射表 - 用于将OAuth2 subject映射到内部user_id
CREATE TABLE oauth2_subject_mappings (
    id SERIAL PRIMARY KEY,
    subject VARCHAR(128) NOT NULL,             -- OAuth2/OpenID Connect subject (仅在provider内唯一)
    internal_user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    provider VARCHAR(100) DEFAULT 'local',     -- 'local', 'google', 'wechat'等
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(provider, subject)                  -- ✅ 复合唯一约束，避免跨provider冲突
);

-- Create indexes for performance
CREATE INDEX idx_oauth2_client_scopes_lookup ON oauth2_client_scopes(client_id);
CREATE INDEX idx_oauth2_user_consents_lookup ON oauth2_user_consents(internal_user_id, client_id);
CREATE INDEX idx_oauth2_user_consents_user ON oauth2_user_consents(internal_user_id);
CREATE INDEX idx_oauth2_user_consents_client ON oauth2_user_consents(client_id);
CREATE INDEX idx_oauth2_subject_mappings_provider_subject ON oauth2_subject_mappings(provider, subject);  -- ✅ 复合索引支持(provider, subject)查询
CREATE INDEX idx_oauth2_subject_mappings_user ON oauth2_subject_mappings(internal_user_id);

-- Insert default OAuth2 scopes
-- 这些scopes映射到现有的RBAC roles
INSERT INTO oauth2_scopes (name, description, mapped_role, is_default, requires_admin_role) VALUES
('openid', 'OpenID Connect身份认证 - 允许客户端验证用户身份', 'user', TRUE, FALSE),
('profile', '访问用户基本信息 (用户名、邮箱等)', 'user', TRUE, FALSE),
('email', '访问用户邮箱地址', 'user', FALSE, FALSE),
('admin', '管理员权限 - 映射到RBAC admin role', 'admin', FALSE, TRUE),
('read', '只读权限 - 映射到RBAC user role', 'user', FALSE, FALSE),
('write', '写入权限 - 映射到RBAC user role', 'user', FALSE, FALSE);

-- Verify oauth2_scopes insertion
DO $$
BEGIN
    IF (SELECT COUNT(*) FROM oauth2_scopes WHERE is_default = TRUE) = 0 THEN
        RAISE EXCEPTION 'No default scopes found in oauth2_scopes table.';
    END IF;
END $$;

-- Grant default scopes to vue-client
INSERT INTO oauth2_client_scopes (client_id, scope_name)
SELECT 'vue-client', name
FROM oauth2_scopes
WHERE is_default = TRUE
ON CONFLICT (client_id, scope_name) DO NOTHING;

-- Verify vue-client scope assignment
DO $$
DECLARE
    scope_count INTEGER;
BEGIN
    SELECT COUNT(*) INTO scope_count FROM oauth2_client_scopes WHERE client_id = 'vue-client';
    IF scope_count = 0 THEN
        RAISE EXCEPTION 'vue-client has no scopes assigned!';
    ELSE
        RAISE NOTICE 'Successfully assigned % scopes to vue-client', scope_count;
    END IF;
END $$;

-- Add comments for documentation
COMMENT ON TABLE oauth2_scopes IS 'OAuth2 scopes定义表 - 存储OAuth2协议的权限范围，独立于RBAC系统';
COMMENT ON TABLE oauth2_client_scopes IS 'OAuth2客户端-Scope关联表 - 替代oauth2_clients.allowed_scopes字段';
COMMENT ON TABLE oauth2_user_consents IS 'OAuth2用户授权记录表 - 记录用户对客户端授予的scope权限，使用internal_user_id确保与RBAC系统兼容';
COMMENT ON TABLE oauth2_subject_mappings IS 'OAuth2 subject映射表 - 将OAuth2/OpenID Connect subject映射到内部users.id，解决subject与RBAC系统类型不匹配问题';
COMMENT ON COLUMN oauth2_user_consents.internal_user_id IS '内部用户ID - 直接引用users.id，确保与RBAC系统兼容';
COMMENT ON COLUMN oauth2_subject_mappings.subject IS 'OAuth2/OpenID Connect subject - 外部用户标识符(仅在provider内唯一)';
COMMENT ON COLUMN oauth2_subject_mappings.internal_user_id IS '内部用户ID - 映射到users.id';
COMMENT ON COLUMN oauth2_subject_mappings.provider IS '身份提供商 - local, google, wechat等';

-- 说明：
-- 1. 破坏性变更: oauth2_clients.allowed_scopes字段已彻底移除
-- 2. Subject映射: 通过oauth2_subject_mappings表解决subject与users.id的类型不匹配
-- 3. 类型一致: oauth2_user_consents使用INTEGER internal_user_id，确保与RBAC系统兼容
-- 4. OAuth2 scopes和RBAC是两个独立的权限系统
-- 5. 通过mapped_role字段将OAuth2 scope映射到RBAC role
-- 6. requires_admin_role字段用于标记需要特殊权限的scopes
