# Cycle 64 — explicit state migration boundary

## What changed

All restored Density state now passes through one schema migration function.
Schema 1 is the current identity migration, while missing, older, and future
schemas fail without changing live parameters. A named future-schema regression
case accompanies the existing round-trip and 3,072-case state fuzz suite.

## Why

State validation already rejected unknown versions, but the version check lived
inside restoration. A dedicated boundary gives every future schema change one
auditable migration path without coupling portable state to widgets or binary
object layouts.

## Evidence

- `density_plugin_tests` restores schema 1 deterministically.
- The new schema 2 case cannot overwrite the active Density value.
- The deterministic state fuzzer still exercises arbitrary bytes and hostile
  schema, product, identifier, duplicate, missing, and numeric mutations.

## Risks

There is no historical schema to transform yet, so schema 1 is necessarily an
identity migration. A schema 2 release will require fixtures proving both the
old-to-new transform and current-schema serialization.

## Next step

Publish a requirement-to-evidence ledger and close every remaining local gap it
identifies before asking DAWs, hardware, listening sessions, or musical projects
for external evidence.
