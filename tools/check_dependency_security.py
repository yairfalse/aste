#!/usr/bin/env python3
"""Validate the reviewed security snapshot for packaged dependencies."""

import argparse
from datetime import date, timedelta
import json
from pathlib import Path
import re
import sys


JUCE_COMMIT = "7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2"
VST3_COMMIT = "9fad9770f2ae8542ab1a548a68c1ad1ac690abe0"
EXPECTED = {
    "juce": ("JUCE", "8.0.13", JUCE_COMMIT, None),
    "vst3-sdk": ("Steinberg VST 3 SDK bundled by JUCE", "3.8.0",
                 JUCE_COMMIT, VST3_COMMIT),
}
KNOWN_ADVISORIES = {
    "juce": {
        "CVE-2021-23520": ("GHSA-qpj6-xj62-7c95", "<6.1.5"),
        "CVE-2021-23521": ("GHSA-cv9m-jw92-c3v3", "<6.1.5"),
    },
    "vst3-sdk": {},
}
SOURCE_KINDS = {"osv-commit", "publisher-advisories", "nvd-keyword"}
SOURCE_URLS = {
    "juce": {
        "osv-commit": "https://api.osv.dev/v1/query",
        "publisher-advisories": "https://api.github.com/repos/juce-framework/JUCE/security-advisories",
        "nvd-keyword": "https://services.nvd.nist.gov/rest/json/cves/2.0?keywordSearch=JUCE",
    },
    "vst3-sdk": {
        "osv-commit": "https://api.osv.dev/v1/query",
        "publisher-advisories": "https://api.github.com/repos/steinbergmedia/vst3sdk/security-advisories",
        "nvd-keyword": "https://services.nvd.nist.gov/rest/json/cves/2.0?keywordSearch=Steinberg%20VST3%20SDK",
    },
}


def parse_day(value, field, errors):
    try:
        return date.fromisoformat(value)
    except (TypeError, ValueError):
        errors.append(f"{field} must be an ISO date")
        return date.min


def validate(audit, root, today):
    errors = []
    if not isinstance(audit, dict):
        return ["audit root must be an object"]
    if audit.get("schema") != 1:
        errors.append("unsupported audit schema")
    if audit.get("scope") != "density-d01-packaged-runtime-dependencies":
        errors.append("unexpected audit scope")
    if audit.get("review_interval_days") != 90:
        errors.append("security reviews must expire after 90 days")

    reviewed = parse_day(audit.get("reviewed_on"), "reviewed_on", errors)
    due = parse_day(audit.get("next_review_due"), "next_review_due", errors)
    if due != reviewed + timedelta(days=90):
        errors.append("next_review_due must be exactly 90 days after review")
    if reviewed > today:
        errors.append("security review date is in the future")
    if due < today:
        errors.append(f"security review expired on {due.isoformat()}")

    components = audit.get("components")
    if not isinstance(components, list):
        components = []
        errors.append("components must be a list")
    by_id = {item.get("id"): item for item in components if isinstance(item, dict)}
    if set(by_id) != set(EXPECTED) or len(by_id) != len(components):
        errors.append("audit must contain each packaged dependency exactly once")

    for component_id, expected in EXPECTED.items():
        component = by_id.get(component_id, {})
        identity = (component.get("name"), component.get("version"),
                    component.get("source_commit"), component.get("upstream_commit"))
        if identity != expected:
            errors.append(f"{component_id} identity does not match packaged dependency")
        if component.get("status") != "no_known_affected_advisories":
            errors.append(f"{component_id} has no accepted security disposition")

        advisories = component.get("advisories", [])
        if not isinstance(advisories, list):
            advisories = []
            errors.append(f"{component_id} advisories must be a list")
        valid_advisories = [item for item in advisories if isinstance(item, dict)]
        advisory_ids = {item.get("id") for item in valid_advisories}
        if (advisory_ids != set(KNOWN_ADVISORIES[component_id])
                or len(advisory_ids) != len(advisories)):
            errors.append(f"{component_id} advisory history is incomplete or duplicated")
        for advisory in valid_advisories:
            expected_advisory = KNOWN_ADVISORIES[component_id].get(advisory.get("id"))
            if (advisory.get("disposition") != "not_affected"
                    or expected_advisory is None
                    or advisory.get("aliases") != [expected_advisory[0]]
                    or advisory.get("affected_versions") != expected_advisory[1]
                    or not advisory.get("reason")):
                errors.append(f"{component_id} has an incomplete advisory disposition")

        excluded = component.get("excluded_matches", [])
        if not isinstance(excluded, list):
            excluded = []
            errors.append(f"{component_id} excluded matches must be a list")
        valid_excluded = [item for item in excluded if isinstance(item, dict)]
        excluded_ids = {item.get("id") for item in valid_excluded}
        if (len(excluded_ids) != len(excluded)
                or any(not item.get("reason") for item in valid_excluded)):
            errors.append(f"{component_id} has malformed excluded search matches")

        sources = component.get("sources", [])
        if not isinstance(sources, list):
            sources = []
            errors.append(f"{component_id} sources must be a list")
        by_kind = {item.get("kind"): item for item in sources if isinstance(item, dict)}
        if set(by_kind) != SOURCE_KINDS or len(by_kind) != len(sources):
            errors.append(f"{component_id} must record all three advisory sources once")
            continue
        for source in sources:
            if (source.get("checked_on") != audit.get("reviewed_on")
                    or source.get("url") != SOURCE_URLS[component_id].get(source.get("kind"))
                    or not isinstance(source.get("result_count"), int)
                    or source["result_count"] < 0):
                errors.append(f"{component_id} has malformed advisory source evidence")
        if by_kind["osv-commit"].get("result_count") != 0:
            errors.append(f"{component_id} exact commit has an OSV match")
        if by_kind["publisher-advisories"].get("result_count") != 0:
            errors.append(f"{component_id} publisher has a new advisory to review")
        expected_query_commit = VST3_COMMIT if component_id == "vst3-sdk" else JUCE_COMMIT
        if by_kind["osv-commit"].get("query") != {"commit": expected_query_commit}:
            errors.append(f"{component_id} OSV query does not match the audited commit")
        if by_kind["nvd-keyword"].get("result_count") != len(advisories) + len(excluded):
            errors.append(f"{component_id} NVD result accounting is incomplete")

    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    package = (root / "tools/package_density.py").read_text(encoding="utf-8")
    required_pins = (
        (cmake, rf'set\(ASTE_JUCE_COMMIT "{JUCE_COMMIT}"\)', "CMake JUCE commit"),
        (package, rf'JUCE_VERSION = "{EXPECTED["juce"][1]}"', "package JUCE version"),
        (package, rf'JUCE_COMMIT = "{JUCE_COMMIT}"', "package JUCE commit"),
        (package, rf'VST3_SDK_VERSION = "{EXPECTED["vst3-sdk"][1]}"', "package VST3 version"),
        (package, rf'VST3_SDK_UPSTREAM_COMMIT = "{VST3_COMMIT}"', "VST3 upstream commit"),
    )
    for text, pattern, label in required_pins:
        if re.search(pattern, text) is None:
            errors.append(f"{label} drifted from the reviewed audit")
    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("audit", type=Path)
    parser.add_argument("root", type=Path)
    parser.add_argument("--as-of", type=date.fromisoformat, default=date.today())
    args = parser.parse_args()
    audit = json.loads(args.audit.read_text(encoding="utf-8"))
    errors = validate(audit, args.root, args.as_of)
    if errors:
        print("\n".join(errors), file=sys.stderr)
        raise SystemExit(1)
    print(f"dependency security: 2 components, 2 historical advisories, "
          f"current through {audit['next_review_due']}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, json.JSONDecodeError) as error:
        print(f"dependency security: {error}", file=sys.stderr)
        raise SystemExit(1)
