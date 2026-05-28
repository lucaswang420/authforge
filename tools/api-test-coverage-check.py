#!/usr/bin/env python3
"""api-test-coverage-check.py - Compare endpoint x method sets between .ps1 and .sh test scripts.

Parses curl/Invoke-RestMethod calls from both PowerShell and Bash test scripts
and reports any endpoints covered in one but not the other.
"""

import re
import sys
from pathlib import Path

SCRIPTS_DIR = Path(__file__).resolve().parent.parent / "scripts" / "backend"

# Patterns to extract endpoint+method from PowerShell scripts
PS1_INVOKE_PATTERN = re.compile(
    r'Invoke-(?:RestMethod|WebRequest)\s+.*?-Uri\s+"([^"]+)".*?-Method\s+(\w+)',
    re.IGNORECASE,
)
PS1_URI_VAR_PATTERN = re.compile(r'\$BaseUrl([/\w\-:?.&=]+)')

# Patterns to extract endpoint+method from Bash scripts
SH_CURL_PATTERN = re.compile(
    r'curl\s+.*?-X\s+(\w+)\s+.*?"([^"]+)"', re.IGNORECASE
)
SH_CURL_PATTERN2 = re.compile(
    r'curl\s+.*?"([^"]+)"', re.IGNORECASE
)


def normalize_endpoint(url: str) -> str:
    """Strip base URL and query params, normalize path variables."""
    # Remove base URL patterns
    url = re.sub(r'https?://[^/]+', '', url)
    url = re.sub(r'\$\{?BASE_URL\}?', '', url, flags=re.IGNORECASE)
    url = re.sub(r'\$BaseUrl', '', url)
    # Remove query params
    url = url.split('?')[0]
    # Normalize dynamic path segments (UUIDs, IDs, timestamps)
    url = re.sub(r'/\d+', '/:id', url)
    url = re.sub(r'/[a-f0-9-]{36}', '/:id', url)
    url = re.sub(r'/\$\([^)]+\)', '/:id', url)
    url = re.sub(r'/\$\{[^}]+\}', '/:id', url)
    url = re.sub(r'/\$\w+', '/:id', url)
    # Remove trailing slash
    url = url.rstrip('/')
    return url if url else '/'


def extract_ps1_endpoints(filepath: Path) -> set:
    """Extract endpoint x method pairs from a .ps1 file."""
    endpoints = set()
    content = filepath.read_text(encoding='utf-8', errors='ignore')

    for match in PS1_INVOKE_PATTERN.finditer(content):
        uri = match.group(1)
        method = match.group(2).upper()
        endpoint = normalize_endpoint(uri)
        if endpoint and '/oauth2' in endpoint or '/api' in endpoint or '/health' in endpoint or '/.well-known' in endpoint:
            endpoints.add((method, endpoint))

    return endpoints


def extract_sh_endpoints(filepath: Path) -> set:
    """Extract endpoint x method pairs from a .sh file."""
    endpoints = set()
    content = filepath.read_text(encoding='utf-8', errors='ignore')

    for match in SH_CURL_PATTERN.finditer(content):
        method = match.group(1).upper()
        uri = match.group(2)
        endpoint = normalize_endpoint(uri)
        if endpoint and ('/oauth2' in endpoint or '/api' in endpoint or '/health' in endpoint or '/.well-known' in endpoint):
            endpoints.add((method, endpoint))

    # Also find simple curl calls (GET by default)
    for line in content.splitlines():
        if 'curl' in line and '-X' not in line:
            match = re.search(r'curl\s+.*?"([^"]+)"', line)
            if match:
                uri = match.group(1)
                endpoint = normalize_endpoint(uri)
                if endpoint and ('/oauth2' in endpoint or '/api' in endpoint or '/health' in endpoint or '/.well-known' in endpoint):
                    endpoints.add(('GET', endpoint))

    return endpoints


def main():
    ps1_files = [
        SCRIPTS_DIR / "test-admin-endpoints.ps1",
        SCRIPTS_DIR / "test-oauth2-endpoints.ps1",
    ]
    sh_files = [
        SCRIPTS_DIR / "test-admin-endpoints.sh",
        SCRIPTS_DIR / "test-oauth2-endpoints.sh",
    ]

    ps1_endpoints = set()
    sh_endpoints = set()

    for f in ps1_files:
        if f.exists():
            ps1_endpoints.update(extract_ps1_endpoints(f))
        else:
            print(f"[WARNING] Not found: {f}")

    for f in sh_files:
        if f.exists():
            sh_endpoints.update(extract_sh_endpoints(f))
        else:
            print(f"[WARNING] Not found: {f}")

    # Compare
    only_ps1 = ps1_endpoints - sh_endpoints
    only_sh = sh_endpoints - ps1_endpoints
    common = ps1_endpoints & sh_endpoints

    print("========================================")
    print("API Test Coverage Parity Check")
    print("========================================")
    print(f"")
    print(f"PowerShell scripts: {len(ps1_endpoints)} endpoint x method pairs")
    print(f"Bash scripts:       {len(sh_endpoints)} endpoint x method pairs")
    print(f"Common:             {len(common)}")
    print(f"")

    exit_code = 0

    if only_ps1:
        print(f"[WARN] In .ps1 but NOT in .sh ({len(only_ps1)}):")
        for method, ep in sorted(only_ps1):
            print(f"  {method:6s} {ep}")
        print("")
        exit_code = 1

    if only_sh:
        print(f"[WARN] In .sh but NOT in .ps1 ({len(only_sh)}):")
        for method, ep in sorted(only_sh):
            print(f"  {method:6s} {ep}")
        print("")
        exit_code = 1

    if exit_code == 0:
        print("[PASS] Full endpoint parity between .ps1 and .sh test scripts")
    else:
        print("[INFO] Some differences found. Review above for missing coverage.")
        print("       Minor differences may be acceptable due to URL variable patterns.")

    return exit_code


if __name__ == "__main__":
    sys.exit(main())
