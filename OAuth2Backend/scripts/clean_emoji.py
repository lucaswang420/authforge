#!/usr/bin/env python3
"""
Clean emoji from project files and replace with ASCII alternatives.
"""

import os
import re
from pathlib import Path

# Emoji to ASCII mapping (using explicit status words)
EMOJI_MAP = {
    # Checkmarks and crosses
    '✓': '[PASS]',
    '✗': '[ERROR]',
    '✔': '[PASS]',
    '✖': '[ERROR]',
    '√': '[PASS]',

    # Warning and info
    '⚠': '[WARNING]',
    '⚡': '[WARNING]',
    '❗': '[WARNING]',
    '❕': '[WARNING]',
    'ℹ': '[INFO]',

    # Common symbols
    '🔍': '[INFO]',   # magnifying glass
    '🔧': '[INFO]',   # wrench
    '🔐': '[AUTH]',   # lock
    '🚀': '[INFO]',   # rocket
    '📋': '===',      # clipboard
    '💡': '[INFO]',   # light bulb
    '🎯': '[>>>]',    # target
    '✨': '[INFO]',   # sparkles
    '🔒': '[LOCK]',   # lock
    '🔓': '[UNLOCK]', # unlock
    '📝': '[INFO]',   # memo
    '📊': '[INFO]',   # chart
    '📈': '[INFO]',   # chart up
    '📉': '[INFO]',   # chart down
    '✅': '[PASS]',
    '❌': '[ERROR]',
    '⛔': '[ERROR]',
    '🚫': '[ERROR]',
    '⭕': '[ ]',
    '🔴': '[INFO]',
    '🟢': '[INFO]',
    '🔵': '[INFO]',
    '🟡': '[INFO]',
    '⭐': '[INFO]',
    '💯': '[100%]',
}

def clean_emoji_from_file(filepath):
    """Clean emoji from a single file."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        original_content = content

        # Replace emojis with ASCII alternatives
        for emoji, replacement in EMOJI_MAP.items():
            content = content.replace(emoji, replacement)

        # If content changed, write back
        if content != original_content:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
            return True
        return False

    except Exception as e:
        print(f"Error processing {filepath}: {e}")
        return False

def main():
    """Main cleaning function."""
    project_root = Path("OAuth2Backend")

    # Files to clean (from scan results)
    files_to_clean = [
        "docs/api-documentation-automation-tools.md",
        "docs/ci_cd_guide.md",
        "docs/docker_deployment.md",
        "docs/security_hardening.md",
        "docs/testing_guide.md",
        "docs/superpowers/plans/2026-04-13-hodor-rate-limiter-migration.md",
        "docs/superpowers/plans/2026-04-14-hodor-rate-limiter-migration-summary.md",
        "docs/superpowers/plans/2026-04-14-multiplatform-ci-plan.md",
        "docs/superpowers/specs/2026-04-13-hodor-rate-limiter-migration-design.md",
        "docs/superpowers/specs/2026-04-14-multiplatform-ci-design.md",
        "docs/superpowers/validation/2026-04-14-multiplatform-ci-validation-report.md",
    ]

    print("=" * 80)
    print("CLEANING EMOJI FROM DOCUMENTATION FILES")
    print("=" * 80)

    cleaned_count = 0

    for relative_path in files_to_clean:
        filepath = project_root / relative_path
        if filepath.exists():
            if clean_emoji_from_file(filepath):
                print(f"[+] Cleaned: {relative_path}")
                cleaned_count += 1
            else:
                print(f"[ ] No changes: {relative_path}")
        else:
            print(f"[-] File not found: {relative_path}")

    print(f"\n{'='*80}")
    print("CLEANING SUMMARY")
    print(f"{'='*80}")
    print(f"Total files processed: {len(files_to_clean)}")
    print(f"Files cleaned: {cleaned_count}")

if __name__ == "__main__":
    main()
