#!/usr/bin/env python3
"""
Scan project files for emoji characters and report findings.
"""

import os
import re
from pathlib import Path

# Emoji Unicode ranges (simplified - common ranges)
EMOJI_PATTERNS = [
    # Miscellaneous Symbols (☀, ☁, ☎, etc.)
    re.compile(r'[☀-⛿]'),
    # Dingbats (✀, ✁, ✂, etc.)
    re.compile(r'[✀-➿]'),
    # Emoticons (😀, 😁, 😂, etc.)
    re.compile(r'[\U0001F600-\U0001F64F]'),
    # Transport and Map Symbols (🚀, 🚁, 🚂, etc.)
    re.compile(r'[\U0001F680-\U0001F6FF]'),
    # Miscellaneous Symbols and Pictographs (🀄, 🀅, 🀆, etc.)
    re.compile(r'[\U0001F300-\U0001F5FF]'),
    # Supplemental Symbols and Pictographs (🦀, 🦁, 🦂, etc.)
    re.compile(r'[\U0001F900-\U0001F9FF]'),
]

def has_emoji(text):
    """Check if text contains emoji characters."""
    for pattern in EMOJI_PATTERNS:
        if pattern.search(text):
            return True
    return False

def scan_file(filepath):
    """Scan a single file for emoji and return matches."""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
            lines = content.split('\n')

            emoji_lines = []
            for i, line in enumerate(lines, 1):
                if has_emoji(line):
                    emoji_lines.append((i, line))

            return emoji_lines
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
        return []

def main():
    """Main scanning function."""
    project_root = Path("OAuth2Backend")

    # File patterns to scan
    cpp_extensions = ['.cc', '.h', '.hpp', '.cpp']
    script_extensions = ['.sh', '.bat', '.ps1', '.py']
    doc_extensions = ['.md', '.txt', 'cmake', 'CMakeLists.txt']

    all_extensions = cpp_extensions + script_extensions + doc_extensions

    print("=" * 80)
    print("SCANNING FOR EMOJI IN PROJECT FILES")
    print("=" * 80)

    files_with_emoji = []

    # Scan all files
    for ext in all_extensions:
        pattern = f"**/*{ext}" if ext.startswith('.') else f"**/{ext}"
        for filepath in project_root.rglob(pattern):
            # Skip models directory (ORM generated)
            if 'models' in filepath.parts:
                continue

            emoji_lines = scan_file(filepath)
            if emoji_lines:
                files_with_emoji.append((filepath, emoji_lines))

    # Report results
    print(f"\nFound {len(files_with_emoji)} files with emoji:\n")

    for filepath, emoji_lines in files_with_emoji:
        print(f"\n{'='*80}")
        print(f"File: {filepath}")
        print(f"{'='*80}")
        for line_no, line in emoji_lines:
            # Show line with emoji highlighted (safe encoding)
            try:
                safe_line = line.encode('ascii', 'replace').decode('ascii')
                print(f"  Line {line_no}: {safe_line.strip()}")
            except:
                print(f"  Line {line_no}: [Contains non-ASCII characters]")
            # Find and show the emoji characters
            for pattern in EMOJI_PATTERNS:
                matches = pattern.findall(line)
                if matches:
                    emoji_str = ''.join(matches)
                    try:
                        print(f"    -> Emoji found: {emoji_str}")
                    except:
                        print(f"    -> Emoji found: [Unicode emoji]")

    print(f"\n{'='*80}")
    print("SUMMARY")
    print(f"{'='*80}")
    print(f"Total files with emoji: {len(files_with_emoji)}")
    print(f"Files to clean:")
    for filepath, _ in files_with_emoji:
        print(f"  - {filepath}")

if __name__ == "__main__":
    main()
