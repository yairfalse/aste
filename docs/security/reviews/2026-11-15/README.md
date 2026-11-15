# Runtime dependency review — 2026-11-15

Fresh public responses for the unchanged JUCE 8.0.13 and bundled VST3 SDK 3.8.0
pins. OSV exact-commit queries returned `{}`; both publisher-advisory arrays
were empty. NVD returned four JUCE keyword matches and zero SDK matches.
No published advisory in these sources is known to affect the pinned runtime.
This is not a claim that the code has no vulnerabilities.

The four JUCE matches were individually reviewed: CVE-2021-23520 and
CVE-2021-23521 affect versions before 6.1.5; CVE-2002-2408 concerns Gordano mail
filtering; CVE-2025-65843 concerns Aquarius Desktop's symlink-following support
archive, not a JUCE library vulnerability. Existing dispositions remain valid.

Responses are preserved unchanged beside this note. OSV POST bodies and publisher
URLs are recorded in the [ledger](../../dependency-audit.json). Successful NVD
GETs used its recorded URLs with `&resultsPerPage=2000`; both responses have
startIndex 0 and include all totalResults. Several earlier requests returned
HTTP 503; these were not counted as empty results. Both queries subsequently
succeeded before the ledger was renewed through 2027-02-13.

The package's pinned digest was updated to the exact renewed ledger bytes.
The existing checker passes now and fails with `--as-of 2027-02-14`, confirming
that expiry remains enforced. No dependency versions or release identity changed.
