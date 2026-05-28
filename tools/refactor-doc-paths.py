#!/usr/bin/env python3
"""Refactor documentation paths in markdown files.

Applies regex replacements to markdown content ONLY outside fenced code
blocks (``` ... ```) and inline code (` ... `).

Usage:
    python tools/refactor-doc-paths.py [--selftest] [--dry-run] [FILES...]

Flags:
    --selftest   Run built-in self-tests and exit.
    --dry-run    Print changes without writing files.
"""

import re
import sys
from pathlib import Path
from typing import List, Tuple

# Path replacement rules: (pattern, replacement)
# These map old paths to new paths after the reorganization.
REPLACEMENTS: List[Tuple[str, str]] = [
    # docs root -> docs/ops
    (r"docs/ACCOUNT_LOCKOUT\.md", "docs/ops/account-lockout.md"),
    (r"docs/DEPLOYMENT\.md", "docs/ops/deployment.md"),
    (r"docs/security-checklist\.md", "docs/ops/security-checklist.md"),
    # docs/backend underscore -> kebab
    (r"docs/backend/api_reference\.md", "docs/backend/api-reference.md"),
    (r"docs/backend/architecture_overview\.md", "docs/backend/architecture-overview.md"),
    (r"docs/backend/ci_cd_guide\.md", "docs/backend/ci-cd-guide.md"),
    (r"docs/backend/configuration_guide\.md", "docs/backend/configuration-guide.md"),
    (r"docs/backend/data_consistency\.md", "docs/backend/data-consistency.md"),
    (r"docs/backend/data_persistence\.md", "docs/backend/data-persistence.md"),
    (r"docs/backend/database_encoding_guide\.md", "docs/backend/database-encoding-guide.md"),
    (r"docs/backend/docker_deployment\.md", "docs/backend/docker-deployment.md"),
    (r"docs/backend/documentation_standards\.md", "docs/backend/documentation-standards.md"),
    (r"docs/backend/google_guide\.md", "docs/backend/google-guide.md"),
    (r"docs/backend/oidc_guide\.md", "docs/backend/oidc-guide.md"),
    (r"docs/backend/plugin_integration\.md", "docs/backend/plugin-integration.md"),
    (r"docs/backend/rbac_guide\.md", "docs/backend/rbac-guide.md"),
    (r"docs/backend/security_architecture\.md", "docs/backend/security-architecture.md"),
    (r"docs/backend/security_hardening\.md", "docs/backend/security-hardening.md"),
    (r"docs/backend/testing_guide\.md", "docs/backend/testing-guide.md"),
    (r"docs/backend/wechat_guide\.md", "docs/backend/wechat-guide.md"),
    # OAuth2Admin/docs -> docs/admin
    (r"OAuth2Admin/docs/E2E_TESTING_GUIDE\.md", "docs/admin/e2e-testing-guide.md"),
    # OAuth2Frontend/docs -> docs/frontend
    (r"OAuth2Frontend/docs/DESIGN\.md", "docs/frontend/design.md"),
    (r"OAuth2Frontend/docs/IMPLEMENTATION_PLAN\.md", "docs/frontend/implementation-plan.md"),
    # scripts
    (r"scripts/backend/README\.build\.md", "scripts/backend/README.md"),
    # superpowers merge
    (r"PRD/superpowers/", "docs/design/superpowers/"),
    (r"docs/superpowers/", "docs/design/superpowers/"),
]


def split_markdown_segments(text: str) -> List[Tuple[str, bool]]:
    """Split markdown into segments, marking code vs non-code.

    Returns a list of (content, is_code) tuples.
    Handles fenced code blocks (```) and inline code (`).
    Also handles the pipe escape (\\|) by preserving it.
    """
    segments: List[Tuple[str, bool]] = []
    i = 0
    length = len(text)

    while i < length:
        # Check for fenced code block
        if text[i:i+3] == '```':
            # Find end of fenced block
            end = text.find('```', i + 3)
            if end == -1:
                # Unterminated fence - treat rest as code
                segments.append((text[i:], True))
                break
            end += 3
            segments.append((text[i:end], True))
            i = end
        # Check for inline code
        elif text[i] == '`':
            end = text.find('`', i + 1)
            if end == -1:
                # Unterminated inline - treat rest as code
                segments.append((text[i:], True))
                break
            end += 1
            segments.append((text[i:end], True))
            i = end
        else:
            # Find next code marker
            next_fence = text.find('```', i)
            next_inline = text.find('`', i)

            if next_fence == -1 and next_inline == -1:
                segments.append((text[i:], False))
                break
            elif next_fence == -1:
                next_code = next_inline
            elif next_inline == -1:
                next_code = next_fence
            else:
                next_code = min(next_fence, next_inline)

            segments.append((text[i:next_code], False))
            i = next_code

    return segments


def apply_replacements(text: str) -> str:
    """Apply path replacements only outside code blocks/inline code."""
    segments = split_markdown_segments(text)
    result = []

    for content, is_code in segments:
        if is_code:
            result.append(content)
        else:
            modified = content
            for pattern, replacement in REPLACEMENTS:
                modified = re.sub(pattern, replacement, modified)
            result.append(modified)

    return ''.join(result)


def handle_pipe_escape(text: str) -> str:
    """Handle the \\| pipe escape in markdown tables.

    The pipe escape (\\|) should be preserved during replacements.
    This function is a no-op validator - it ensures we don't corrupt
    pipe escapes during processing.
    """
    # The pipe escape \\| is not affected by our path replacements
    # since none of our patterns contain pipe characters.
    # This function exists to document and test that guarantee.
    return text


def process_file(filepath: Path, dry_run: bool = False) -> bool:
    """Process a single markdown file. Returns True if changes were made."""
    try:
        original = filepath.read_text(encoding='utf-8')
    except (OSError, UnicodeDecodeError) as e:
        print(f"  SKIP {filepath}: {e}", file=sys.stderr)
        return False

    modified = apply_replacements(original)

    if modified != original:
        if dry_run:
            print(f"  WOULD MODIFY: {filepath}")
        else:
            filepath.write_text(modified, encoding='utf-8')
            print(f"  MODIFIED: {filepath}")
        return True
    return False


def run_selftest() -> bool:
    """Run built-in self-tests. Returns True if all pass."""
    passed = 0
    failed = 0

    def check(name: str, got: str, expected: str):
        nonlocal passed, failed
        if got == expected:
            passed += 1
            print(f"  PASS: {name}")
        else:
            failed += 1
            print(f"  FAIL: {name}")
            print(f"    Expected: {expected!r}")
            print(f"    Got:      {got!r}")

    print("Running self-tests...")

    # Test 1: Basic replacement outside code
    check(
        "basic replacement",
        apply_replacements("See [guide](docs/backend/api_reference.md) for details."),
        "See [guide](docs/backend/api-reference.md) for details.",
    )

    # Test 2: No replacement inside fenced code block
    check(
        "fenced code block preserved",
        apply_replacements("```\ndocs/backend/api_reference.md\n```"),
        "```\ndocs/backend/api_reference.md\n```",
    )

    # Test 3: No replacement inside inline code
    check(
        "inline code preserved",
        apply_replacements("Use `docs/backend/api_reference.md` as path."),
        "Use `docs/backend/api_reference.md` as path.",
    )

    # Test 4: Mixed content
    input_text = (
        "Link: [ref](docs/backend/testing_guide.md)\n"
        "```\ndocs/backend/testing_guide.md\n```\n"
        "Another: [x](docs/DEPLOYMENT.md)\n"
    )
    expected = (
        "Link: [ref](docs/backend/testing-guide.md)\n"
        "```\ndocs/backend/testing_guide.md\n```\n"
        "Another: [x](docs/ops/deployment.md)\n"
    )
    check("mixed content", apply_replacements(input_text), expected)

    # Test 5: Pipe escape fixture
    pipe_input = "| Column | Path \\| Note |\n|--------|-------------|\n| data | docs/backend/rbac_guide.md |"
    pipe_expected = "| Column | Path \\| Note |\n|--------|-------------|\n| data | docs/backend/rbac-guide.md |"
    result = apply_replacements(pipe_input)
    result = handle_pipe_escape(result)
    check("pipe escape preserved", result, pipe_expected)

    # Test 6: Pipe escape inside link not corrupted
    pipe_link = "[a\\|b](docs/backend/oidc_guide.md)"
    pipe_link_expected = "[a\\|b](docs/backend/oidc-guide.md)"
    check("pipe escape in link text", apply_replacements(pipe_link), pipe_link_expected)

    # Test 7: PRD/superpowers/ replacement
    check(
        "PRD superpowers path",
        apply_replacements("See [plan](PRD/superpowers/plans/foo.md)"),
        "See [plan](docs/design/superpowers/plans/foo.md)",
    )

    # Test 8: docs/superpowers/ replacement
    check(
        "docs superpowers path",
        apply_replacements("See [plan](docs/superpowers/plans/foo.md)"),
        "See [plan](docs/design/superpowers/plans/foo.md)",
    )

    print(f"\n{passed} passed, {failed} failed")
    return failed == 0


def main():
    args = sys.argv[1:]

    if '--selftest' in args:
        success = run_selftest()
        sys.exit(0 if success else 1)

    dry_run = '--dry-run' in args
    files = [a for a in args if not a.startswith('--')]

    if not files:
        print("Usage: refactor-doc-paths.py [--selftest] [--dry-run] [FILES...]")
        print("       refactor-doc-paths.py --selftest")
        sys.exit(1)

    modified_count = 0
    for f in files:
        path = Path(f)
        if path.is_file() and path.suffix == '.md':
            if process_file(path, dry_run):
                modified_count += 1
        elif path.is_dir():
            for md in sorted(path.rglob('*.md')):
                if process_file(md, dry_run):
                    modified_count += 1

    print(f"\n{modified_count} file(s) {'would be ' if dry_run else ''}modified.")


if __name__ == '__main__':
    main()
