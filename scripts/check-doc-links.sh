#!/usr/bin/env bash
# check-doc-links.sh - Verify relative links in markdown files resolve to existing files.
#
# Usage: scripts/check-doc-links.sh [ROOT_DIR]
#   ROOT_DIR defaults to the repository root (parent of this script's directory).
#
# Exit codes:
#   0 - All relative links are valid
#   1 - One or more broken links found

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${1:-$(cd "$SCRIPT_DIR/.." && pwd)}"

broken=0
checked=0

while IFS= read -r -d '' mdfile; do
    dir="$(dirname "$mdfile")"

    # Extract markdown links: [text](path)
    # Skip http(s)://, mailto:, and anchor-only (#) links
    grep -oP '\[(?:[^\]]*)\]\(\K[^)]+' "$mdfile" 2>/dev/null | while IFS= read -r link; do
        # Strip anchor fragments
        link_path="${link%%#*}"

        # Skip empty (anchor-only), http(s), mailto links
        [[ -z "$link_path" ]] && continue
        [[ "$link_path" =~ ^https?:// ]] && continue
        [[ "$link_path" =~ ^mailto: ]] && continue

        # Resolve relative to the markdown file's directory
        target="$dir/$link_path"

        checked=$((checked + 1))
        if [[ ! -e "$target" ]]; then
            echo "BROKEN: $mdfile -> $link_path"
            broken=$((broken + 1))
        fi
    done || true
done < <(find "$ROOT_DIR" -name '*.md' -not -path '*/node_modules/*' -not -path '*/.git/*' -not -path '*/dist/*' -not -path '*/.venv/*' -print0)

if [[ $broken -gt 0 ]]; then
    echo ""
    echo "Found $broken broken link(s)."
    exit 1
else
    echo "All doc links are valid."
    exit 0
fi
