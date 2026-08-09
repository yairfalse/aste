#!/usr/bin/env python3
"""Check required engineering documents and local Markdown links."""

from pathlib import Path
import re
import sys
from urllib.parse import unquote


REQUIRED = (
    "docs/ARCHITECTURE.md",
    "docs/CONTRIBUTING.md",
    "docs/DSP_RESEARCH.md",
    "docs/HOST_COMPATIBILITY.md",
    "docs/PARAMETER_MODEL.md",
    "docs/PERFORMANCE_BUDGETS.md",
    "docs/REALTIME_SAFETY.md",
    "docs/RELEASE_CHECKLIST.md",
    "docs/ROADMAP.md",
    "docs/SCHEMATIC_RESEARCH.md",
    "docs/SECURITY.md",
    "docs/SPECIFICATION_STATUS.md",
    "docs/STATE_FORMAT.md",
    "docs/TESTING.md",
    "docs/UI_SYSTEM.md",
    "docs/products/harmonic/SPECIFICATION.md",
    "docs/products/harmonic/DSP_RESEARCH.md",
    "docs/research/schematics/catalog.yaml",
)
LINK = re.compile(r"!?\[[^]]*]\(([^)]+)\)")
ACTION_USE = re.compile(r"^\s*(?:-\s*)?uses:\s*([^\s#]+)", re.MULTILINE)


def is_generated_build_file(document: Path, root: Path) -> bool:
    for parent in document.parents:
        if parent == root:
            return False
        if (parent / "CMakeCache.txt").is_file():
            return True
    return False


def check_workflow_action_pins(root: Path) -> list[str]:
    errors = []
    workflow_root = root / ".github" / "workflows"
    for pattern in ("*.yml", "*.yaml"):
        for workflow in workflow_root.glob(pattern):
            for use in ACTION_USE.findall(workflow.read_text(encoding="utf-8")):
                if use.startswith(("./", "docker://")):
                    continue
                _, separator, reference = use.rpartition("@")
                if not separator or re.fullmatch(r"[0-9a-f]{40}", reference) is None:
                    errors.append(
                        f"unpinned workflow action: {workflow.relative_to(root)} -> {use}")
    return errors


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    errors = [f"missing required document: {name}" for name in REQUIRED
              if not (root / name).exists()]
    errors.extend(check_workflow_action_pins(root))
    for document in root.rglob("*.md"):
        if (any(part.startswith(("build", "ci-")) or part == ".git"
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
