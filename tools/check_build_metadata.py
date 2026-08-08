#!/usr/bin/env python3
"""Validate configured build provenance."""

import json
from pathlib import Path
import sys


def main() -> int:
    path = Path(sys.argv[1])
    data = json.loads(path.read_text(encoding="utf-8"))
    required = {
        "schema", "project", "version", "commit", "source_dirty", "system", "architecture",
        "compiler", "generator", "build_type", "cxx_standard", "vst3",
        "sanitizers", "juce_commit", "test_results",
    }
    missing = sorted(required - data.keys())
    invalid = data.get("schema") != 1 or data.get("project") != "aste_signal_instruments"
    if missing or invalid:
        print(f"invalid build metadata: missing={missing}", file=sys.stderr)
        return 1
    print("build metadata: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
