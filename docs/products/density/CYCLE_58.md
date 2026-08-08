# Cycle 58 — repository-owned documentation scope

## What changed

Taught the documentation checker to ignore Markdown below any CMake build root,
identified by `CMakeCache.txt`, regardless of the build directory's name.

## Why

The new public CTest annotation identified the exact hosted failure. A VST3
configure places JUCE under `ci-vst3/_deps`; the documentation test then
recursively audited JUCE's Markdown and reported its external repository links
as broken local links. Core jobs passed because they do not fetch JUCE.

Repository policy must cover repository-owned documents, including new local
documents, while excluding generated build trees and fetched dependencies.
Detecting the CMake build marker preserves that boundary without maintaining a
list of CI directory names or ignoring all directories named `_deps`.

## Evidence

- The hosted annotation names `documentation_links` as the failed test and
  reports only paths below `ci-vst3/_deps/juce-src`; the sanitizer annotation
  reports the identical paths below `ci-sanitize/_deps/juce-src`.
- The next 30 hosted universal VST3 tests shown in the annotation pass,
  including plugin adapter and editor artifact tests; the annotation truncates
  as the final binary-host test starts.
- The checker continues to pass over the source repository.
- The CTest annotation wrapper made the failure visible through the public API
  as designed.

## Risks

A repository-owned documentation file deliberately placed inside a configured
CMake build tree will be excluded. Such a file is already generated-tree
content and should not be treated as durable engineering evidence.

## Next step

Push the scope correction and require both VST3 test matrices, bundle
signature verification, and universal architecture verification to pass.
