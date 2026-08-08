#!/usr/bin/env python3
"""Check required engineering documents and local Markdown links."""

from pathlib import Path
import re
import sys
from urllib.parse import unquote


REQUIRED = (
    "ARCHITECTURE.md",
    "CONTRIBUTING.md",
    "DSP_RESEARCH.md",
    "HOST_COMPATIBILITY.md",
    "PARAMETER_MODEL.md",
    "PERFORMANCE_BUDGETS.md",
    "REALTIME_SAFETY.md",
    "RELEASE_CHECKLIST.md",
    "ROADMAP.md",
    "SCHEMATIC_RESEARCH.md",
    "SECURITY.md",
    "STATE_FORMAT.md",
    "TESTING.md",
    "UI_SYSTEM.md",
    "docs/research/schematics/catalog.yaml",
)
LINK = re.compile(r"!?\[[^]]*]\(([^)]+)\)")


def is_generated_build_file(document: Path, root: Path) -> bool:
    for parent in document.parents:
        if parent == root:
            return False
        if (parent / "CMakeCache.txt").is_file():
            return True
    return False


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    errors = [f"missing required document: {name}" for name in REQUIRED
              if not (root / name).exists()]
    for document in root.rglob("*.md"):
        if (any(part.startswith("build") or part == ".git"
                for part in document.relative_to(root).parts)
                or is_generated_build_file(document, root)):
            continue
        for match in LINK.finditer(document.read_text(encoding="utf-8")):
            target = match.group(1).strip().strip("<>").split()[0]
            if target.startswith(("#", "http://", "https://", "mailto:")):
                continue
            path = unquote(target.split("#", 1)[0])
            if path and not (document.parent / path).resolve().exists():
                errors.append(f"broken local link: {document.relative_to(root)} -> {path}")
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("documentation: required files and local links ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
