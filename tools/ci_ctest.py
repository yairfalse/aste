#!/usr/bin/env python3
"""Run CTest and expose its failure tail as a GitHub annotation."""

from __future__ import annotations

import collections
import subprocess
import sys


def escape_annotation(text: str) -> str:
    return text.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: ci_ctest.py <build-directory>", file=sys.stderr)
        return 2

    build_directory = sys.argv[1]
    command = [
        "ctest",
        "--test-dir",
        build_directory,
        "--output-on-failure",
        "--output-junit",
        f"{build_directory}/test-results.xml",
    ]
    tail: collections.deque[str] = collections.deque(maxlen=80)
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    assert process.stdout is not None
    for line in process.stdout:
        print(line, end="", flush=True)
        tail.append(line)
    result = process.wait()
    if result != 0:
        message = "".join(tail)[-3800:]
        print(f"::error title=CTest failure::{escape_annotation(message)}")
    return result


if __name__ == "__main__":
    raise SystemExit(main())
