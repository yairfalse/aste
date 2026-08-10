# Field F-01 state format

Field schema 1 is a JUCE-compatible portable XML value tree with root and
product identifier `field-f01`, integer `schema="1"`, and one scalar child per
stable parameter. It contains no compiler layouts, pointers, delay buffers,
random state, or UI geometry.

Restoration validates the root, product, schema, identifier uniqueness,
presence of scalar values, finite decimal syntax, and parameter ranges before
replacing active state. Unknown identifiers are ignored for forward
compatibility; missing known values receive defaults. Duplicate or malformed
known values reject the whole document without changing current controls.

Live reverb memory is not part of schema 1. Session recall restores FOREVER and
all sound controls deterministically, then starts with an empty spatial field.
Adding portable memory recall would require a new schema, bounded storage,
cross-rate behavior, and a non-audio-thread snapshot design.
