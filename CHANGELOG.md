# Changelog

All notable changes to RosettaUnit are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

While RosettaUnit is in `0.x.y`, breaking changes may land in minor
versions; see the **Stability** section in the README.

## [Unreleased]

## [0.2.0] - 2026-05-16

### Added

- New `rosetta::markers` namespace exposing the visible-marker strings
  used in translation output as `inline constexpr` constants. Callers
  can use these for locale validators, logging hooks, or integration
  tests that assert clean UI output.
- Four-marker family for surfacing translation anomalies:
  `[unclosed]`, `[invalid: <body>]`, `[undefined: <name>]`, and
  `[missing: <path>]`. Anomalies are emitted in-place rather than
  silently swallowed; healthy placeholders in the same template still
  substitute normally.
- Whitespace inside `${...}` is allowed and trimmed (ASCII whitespace
  only: space, tab, `\n`, `\r`, `\f`, `\v`).
- README **Template syntax** section documenting the new grammar,
  examples, and marker family.
- README **Stability** section making the pre-`1.0.0` breaking-change
  policy explicit.
- `CHANGELOG.md` (this file).

### Changed

- **Template syntax migrated from `{name}` to `${name}`**, compatible
  with JavaScript template literals. See
  [Issue #1](https://github.com/RosettaUnit/RosettaUnit/issues/1) for
  the design discussion.
- `{` and `}` are now always literal in translation strings — no escape
  mechanism is needed. JSON, HTML, code samples, and math notation can
  be written as-is (subject to JSON's own string escaping).
- `$` is literal unless followed by `{`. There is no `$$` escape.
- Identifier grammar inside `${...}` is `[A-Za-z_][A-Za-z0-9_]*`. Bodies
  that don't match (empty, leading digit, internal whitespace, ...)
  produce `[invalid: ...]` markers.
- Bundled example locales (`locales/en.json`, `locales/ja.json`,
  `locales/es.json`) updated to use the new syntax.
- `resolve_file_refs()` now uses `markers::kMissingPrefix` /
  `markers::kMarkerClose` for the missing-file marker, replacing the
  inline `"[missing: ..."` literal. Behavior is unchanged.

### Removed

- The `{{` → `{` escape mechanism. With both `{` and `}` now literal,
  no brace escape is needed.

### Fixed

- A latent bug in `detail::interpolate()` where the early-return path
  taken when `params` was empty would skip the `{{` escape handling.
  The function has been rewritten from scratch for the new syntax, so
  this category of bug no longer exists.

### Migration

For locale authors with existing `{name}` placeholders, a single `sed`
invocation handles the rewrite:

```bash
find . -type f \( -name '*.json' -o -name '*.txt' \) -exec \
  sed -i -E 's/\{([A-Za-z_][A-Za-z0-9_]*)\}/${\1}/g' {} +
```

The identifier pattern in the regex matches the new grammar, so JSON
structural braces (`{"key": ...}`) and math notation (`{1, 2, 3}`) are
left alone. Any uses of the old `{{` escape need to be hand-resolved,
but since both braces are now literal, those occurrences typically
collapse to a single `{` or `}` in the translated text.

C++ and Python call sites do not need to change: parameter names are
passed via `params` keyed by identifier strings (`"name"`, `"count"`),
which are unaffected by the template-syntax migration.

## [0.1.0] - 2026-05-11

### Added

- Initial public release.
- C++17 core library with PIMPL'd `rosetta::Translator` for stable ABI.
- C ABI (`rosetta_unit_c.h`) for cross-language bindings.
- Python binding via `ctypes` (`bindings/python/rosetta_unit.py`).
- Hand-rolled minimal JSON parser supporting the flat-key locale format
  used by this project. Dependency-free.
- `$file` external file references for long-form translation content,
  resolved at load time relative to the JSON file's directory.
- Fallback language lookup for partial translations.
- Missing-key handler for development-time diagnostics.
- `{name}` placeholder interpolation with `{{` to escape a literal `{`
  (superseded in v0.2.0 — see above).
- Hand-rolled test suite (no external dependencies).
- MIT license for source code; CC BY-SA 4.0 for data and content.
- README, CONTRIBUTING, and OSS scaffolding.

[Unreleased]: https://github.com/RosettaUnit/RosettaUnit/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/RosettaUnit/RosettaUnit/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/RosettaUnit/RosettaUnit/releases/tag/v0.1.0
