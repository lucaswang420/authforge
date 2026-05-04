@echo off
cd /d "%~dp0.."
echo Setting up oauth_test database...

set PGPASSWORD=123456

psql -U test -d postgres -c "DROP DATABASE IF EXISTS oauth_test;"
psql -U test -d postgres -c "CREATE DATABASE oauth_test;"

psql -U test -d oauth_test -f sql/001_oauth2_core.sql
psql -U test -d oauth_test -f sql/002_users_table.sql
psql -U test -d oauth_test -f sql/003_rbac_schema.sql

echo Database setup complete!
