-- ============================================================================
-- P1 Feature: Token Introspection (RFC 7662) & Token Revocation (RFC 7009)
-- ============================================================================
-- This schema update adds support for:
-- 1. Token Introspection per RFC 7662
-- 2. Enhanced Token Revocation per RFC 7009 with audit trail
-- 3. Performance optimizations for token lookup and introspection
--
-- Version: 005
-- Date: 2026-05-09
-- Author: OAuth2 Plugin Development Team
-- ============================================================================

-- Add RFC 7662 required fields to access tokens table
ALTER TABLE oauth2_access_tokens
    ADD COLUMN issued_at BIGINT NOT NULL DEFAULT EXTRACT(EPOCH FROM CURRENT_TIMESTAMP),
    ADD COLUMN issuer VARCHAR(255) NOT NULL DEFAULT 'https://oauth.example.com',
    ADD COLUMN audience VARCHAR(255),
    ADD COLUMN not_before BIGINT DEFAULT EXTRACT(EPOCH FROM CURRENT_TIMESTAMP),
    ADD COLUMN introspect_count INTEGER DEFAULT 0,
    ADD COLUMN revoked_at BIGINT,
    ADD COLUMN revoked_by VARCHAR(50);

-- Add revocation audit fields to refresh tokens table
ALTER TABLE oauth2_refresh_tokens
    ADD COLUMN revoked_at BIGINT,
    ADD COLUMN revoked_by VARCHAR(50);

-- Create indexes for performance optimization
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

-- Add comments for documentation
COMMENT ON COLUMN oauth2_access_tokens.issued_at IS 'Unix timestamp when token was issued (RFC 7662 iat field)';
COMMENT ON COLUMN oauth2_access_tokens.issuer IS 'Issuer identifier (RFC 7662 iss field)';
COMMENT ON COLUMN oauth2_access_tokens.audience IS 'Audience identifier (RFC 7662 aud field)';
COMMENT ON COLUMN oauth2_access_tokens.not_before IS 'Token not valid before this time (RFC 7662 nbf field)';
COMMENT ON COLUMN oauth2_access_tokens.introspect_count IS 'Number of introspection requests for monitoring';
COMMENT ON COLUMN oauth2_access_tokens.revoked_at IS 'Unix timestamp when token was revoked';
COMMENT ON COLUMN oauth2_access_tokens.revoked_by IS 'Client ID that revoked the token';

COMMENT ON COLUMN oauth2_refresh_tokens.revoked_at IS 'Unix timestamp when refresh token was revoked';
COMMENT ON COLUMN oauth2_refresh_tokens.revoked_by IS 'Client ID that revoked the refresh token';

-- ============================================================================
-- Migration Notes:
-- ============================================================================
-- 1. Default values ensure existing records are compatible
-- 2. issued_at defaults to current time for existing tokens
-- 3. issuer defaults to placeholder URL (should be updated in config)
-- 4. not_before defaults to current time (tokens valid immediately)
-- 5. introspect_count starts at 0 for existing tokens
-- 6. revoked_at and revoked_by are NULL for non-revoked tokens
--
-- For production deployments with existing data:
-- - Consider backfilling issued_at based on expires_at - token_lifetime
-- - Update issuer to actual production URL
-- - audience can remain NULL for non-specific tokens
-- ============================================================================

-- ============================================================================
-- Verification Queries:
-- ============================================================================
-- Check if new columns were added successfully:
-- SELECT column_name, data_type, column_default
-- FROM information_schema.columns
-- WHERE table_name = 'oauth2_access_tokens'
-- AND column_name IN ('issued_at', 'issuer', 'audience', 'not_before', 'introspect_count', 'revoked_at', 'revoked_by')
-- ORDER BY ordinal_position;

-- Check if indexes were created successfully:
-- SELECT indexname, tablename
-- FROM pg_indexes
-- WHERE tablename = 'oauth2_access_tokens'
-- AND indexname LIKE 'idx_access_tokens_%'
-- ORDER BY indexname;
-- ============================================================================