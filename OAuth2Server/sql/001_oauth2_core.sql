-- OAuth2 Schema for PostgreSQL
-- DANGER: DROPPING EXISTING TABLES FOR CLEAN VERIFICATION

DROP TABLE IF EXISTS oauth2_refresh_tokens CASCADE;
DROP TABLE IF EXISTS oauth2_access_tokens CASCADE;
DROP TABLE IF EXISTS oauth2_codes CASCADE;
DROP TABLE IF EXISTS oauth2_clients CASCADE;

-- Clients Table
CREATE TABLE oauth2_clients (
    client_id VARCHAR(50) PRIMARY KEY,
    client_type VARCHAR(20) NOT NULL DEFAULT 'CONFIDENTIAL',
    client_secret VARCHAR(100) NOT NULL,
    salt VARCHAR(50) NOT NULL,
    name VARCHAR(100),
    redirect_uris TEXT, -- Comma separated
    allowed_grant_types TEXT
    -- Note: allowed_scopes removed - use oauth2_client_scopes table instead
);

-- Authorization Codes Table
CREATE TABLE oauth2_codes (
    code VARCHAR(100) PRIMARY KEY,
    client_id VARCHAR(50) NOT NULL REFERENCES oauth2_clients(client_id),
    user_id VARCHAR(50),
    scope TEXT,
    redirect_uri TEXT,
    code_challenge VARCHAR(128),
    code_challenge_method VARCHAR(10),
    expires_at BIGINT NOT NULL,
    used BOOLEAN DEFAULT FALSE
);

-- Access Tokens Table
CREATE TABLE oauth2_access_tokens (
    token VARCHAR(100) PRIMARY KEY,
    client_id VARCHAR(50) NOT NULL REFERENCES oauth2_clients(client_id),
    user_id VARCHAR(50),
    scope TEXT,
    expires_at BIGINT NOT NULL,
    revoked BOOLEAN DEFAULT FALSE,
    -- P1: RFC 7662 Token Introspection fields
    issued_at BIGINT NOT NULL DEFAULT EXTRACT(EPOCH FROM CURRENT_TIMESTAMP),
    issuer VARCHAR(255) NOT NULL DEFAULT 'https://oauth.example.com',
    audience VARCHAR(255),
    not_before BIGINT DEFAULT EXTRACT(EPOCH FROM CURRENT_TIMESTAMP),
    introspect_count INTEGER DEFAULT 0,
    revoked_at BIGINT,
    revoked_by VARCHAR(50)
);

-- Refresh Tokens Table
CREATE TABLE oauth2_refresh_tokens (
    token VARCHAR(100) PRIMARY KEY,
    access_token VARCHAR(100) NOT NULL,
    client_id VARCHAR(50) NOT NULL REFERENCES oauth2_clients(client_id),
    user_id VARCHAR(50),
    scope TEXT,
    expires_at BIGINT NOT NULL,
    revoked BOOLEAN DEFAULT FALSE,
    -- P1: RFC 7009 Token Revocation audit fields
    revoked_at BIGINT,
    revoked_by VARCHAR(50)
);

-- Sample Data: vue-client (PUBLIC Client - no secret required for authentication)
INSERT INTO oauth2_clients (client_id, client_type, client_secret, salt, name, redirect_uris, allowed_grant_types)
VALUES (
    'vue-client',
    'PUBLIC',
    '42a121b66fb9f1d4f73125788f42eb6799110c6aeae5a9a12a2fed5307a0088d',
    'random_salt',
    'Vue Front-end Client',
    'http://localhost:5173/callback,http://localhost:8080/callback',
    'authorization_code,refresh_token'
);

-- ============================================================================
-- P1: Token Introspection (RFC 7662) & Token Revocation (RFC 7009) Indexes
-- ============================================================================

-- Token lookup by token string (for introspection)
CREATE INDEX idx_access_tokens_token ON oauth2_access_tokens(token);

-- Token lookup by client_id (for client-specific queries)
CREATE INDEX idx_access_tokens_client_id ON oauth2_access_tokens(client_id);

-- Token expiration cleanup (for token cleanup jobs)
CREATE INDEX idx_access_tokens_expires_at ON oauth2_access_tokens(expires_at);

-- Revoked token cleanup (for cleanup jobs)
CREATE INDEX idx_access_tokens_revoked ON oauth2_access_tokens(revoked, expires_at);

-- Refresh token indexes
CREATE INDEX idx_refresh_tokens_token ON oauth2_refresh_tokens(token);
CREATE INDEX idx_refresh_tokens_revoked ON oauth2_refresh_tokens(revoked, expires_at);

-- Column comments for documentation
COMMENT ON COLUMN oauth2_access_tokens.issued_at IS 'Unix timestamp when token was issued (RFC 7662 iat field)';
COMMENT ON COLUMN oauth2_access_tokens.issuer IS 'Issuer identifier (RFC 7662 iss field)';
COMMENT ON COLUMN oauth2_access_tokens.audience IS 'Audience identifier (RFC 7662 aud field)';
COMMENT ON COLUMN oauth2_access_tokens.not_before IS 'Token not valid before this time (RFC 7662 nbf field)';
COMMENT ON COLUMN oauth2_access_tokens.introspect_count IS 'Number of introspection requests for monitoring';
COMMENT ON COLUMN oauth2_access_tokens.revoked_at IS 'Unix timestamp when token was revoked';
COMMENT ON COLUMN oauth2_access_tokens.revoked_by IS 'Client ID that revoked the token';

COMMENT ON COLUMN oauth2_refresh_tokens.revoked_at IS 'Unix timestamp when refresh token was revoked';
COMMENT ON COLUMN oauth2_refresh_tokens.revoked_by IS 'Client ID that revoked the refresh token';
