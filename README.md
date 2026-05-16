# RosettaUnit

A lightweight, dependency-free i18n library for C++17, with a stable C ABI
that lets other languages (Python, C#, Rust, ...) use the same translation
data and the same engine.

The name is from the Rosetta Stone — one engine, many languages, both for
the *applications* using it and for the *programming languages* that can
call it.

> **Status:** v0.2.0 — JS-template-literal-compatible `${name}` syntax,
> with a visible-marker family for anomaly diagnostics.

## Stability

RosettaUnit is in `0.x.y` development. The public API, configuration
format, and on-disk file format are all subject to breaking changes
between minor versions until `1.0.0` is released. Production use is
discouraged until then; please pin to an exact version and read the
changelog before upgrading.

This is not a warning against early adoption — early users are welcome,
and their feedback shapes the design — it is an honest statement of
expectations so no one is surprised when a `v0.x.y` → `v0.(x+1).0` bump
breaks their build.

---

## Why another i18n library?

Most i18n libraries make one of two trade-offs:

- **Heavy:** they bundle CLDR, ICU, or large locale databases — great for
  enterprise web apps, painful for small native tools.
- **Naïve:** they hand you a flat key/value map and walk away the moment you
  need plurals, gender, or any real linguistic variation.

RosettaUnit aims at a third path:

- **The library knows nothing.** It does not parse BCP 47, does not bundle
  CLDR, does not assume what counts as "a language."
- **Knowledge lives in data files and code contributed by speakers.**
  Translation strings are JSON files that non-programmers can edit and
  submit as pull requests. Language-specific selection rules (plurals,
  gender, formality) are programmatic and registered by anyone who knows
  the language — see the [plural rule discussion](https://github.com/RosettaUnit/RosettaUnit/issues/3).
- **Dialects, conlangs, period languages, and fictional tongues are
  first-class citizens.** `"ja"`, `"osaka-ben"`, `"elvish"`, and
  `"classical-japanese"` are all just keys in a dictionary as far as the
  core is concerned.

The result is a small library (zero runtime dependencies, a few hundred
lines of C++) that gets out of your way, and a growing collection of
community-maintained language rules that you can pull in when you need them.

---

## Features

- Single-header public C++ API, PIMPL'd so ABI is stable
- JSON translation files, flat or nested (nested keys flatten with dots)
- `${placeholder}` interpolation, JS template literal compatible
- Visible-marker diagnostics: anomalies in translations surface as
  `[unclosed]` / `[invalid: ...]` / `[undefined: ...]` / `[missing: ...]`
  rather than silent failures
- Fallback language for partial translations
- Missing-key handler for development-time diagnostics
- C ABI for cross-language bindings
- Python binding included (ctypes, no compilation needed beyond the .so/.dll)
- No external dependencies — pure C++17 standard library

## Quick start (C++)

```cpp
#include "rosetta_unit/rosetta_unit.hpp"

rosetta::Translator t;
t.load_directory("locales");
t.set_fallback_language("en");
t.set_language("ja");

std::cout << t.tr("greeting", {{"name", "Alice"}});
// -> "こんにちは、Aliceさん!"
```

## Quick start (Python)

```python
from rosetta_unit import Translator

t = Translator()
t.load_directory("locales")
t.set_fallback("en")
t.language = "ja"

print(t.tr("greeting", name="Alice"))
# -> こんにちは、Aliceさん!
```

## Translation file format

`locales/ja.json`:

```json
{
  "app.title": "マイアプリケーション",
  "greeting": "こんにちは、${name}さん!",
  "menu": {
    "file": "ファイル",
    "edit": "編集"
  },

  "about.text":    { "$file": "ja/about.txt" },
  "help.tutorial": { "$file": "ja/tutorial.md" }
}
```

Nested objects flatten automatically: `menu.file` is a valid key.

### Short labels vs. long-form text

Short strings (UI labels, error messages) live inline in the JSON. Long-form
content (about pages, tutorials, license text) is awkward to edit inside
JSON — newlines have to be escaped, translators can't use spell-checkers,
and diffs are noisy. So RosettaUnit also supports **file references**:

```json
{ "about.text": { "$file": "ja/about.txt" } }
```

The referenced file's entire contents become the translation value.
Placeholder substitution (`${name}` etc.) still works on the loaded text.
Paths are resolved relative to the JSON file's own directory.

Pick whichever style fits each piece of content. A typical layout:

```
locales/
  en.json           ← inline UI strings + $file refs to long text
  ja.json
  en/
    about.txt
    tutorial.md
  ja/
    about.txt
    tutorial.md
```

The filename stem (`ja`, `en`, `pt-BR`, ...) is the language code. Use
whatever convention you like — RosettaUnit doesn't validate against
ISO 639 etc., it just matches the strings you pass to `set_language`.

## Template syntax

Translation strings use a small `${name}` placeholder syntax, compatible
with JavaScript template literals. The full design discussion lives in
[Issue #1](https://github.com/RosettaUnit/RosettaUnit/issues/1); this
section covers the day-to-day surface for translators and integrators.

### Placeholders

```
${name}        → looked up in params, substituted as-is
${ name }      → same; whitespace inside the braces is trimmed
$              → literal unless followed by `{`
{  }           → both braces are literal; no escape needed
```

Identifier names must match `[A-Za-z_][A-Za-z0-9_]*`. Whitespace is
ASCII-only (space, tab, `\n`, `\r`, `\f`, `\v`); Unicode whitespace such
as U+00A0 or U+3000 is not stripped.

There is no `$$` escape and no `${...}` escape: a lone `$` followed by
anything other than `{` is already literal, and braces themselves are
already literal. JSON, HTML, code samples, and math notation can be
written as-is (subject to JSON's own string escaping):

```json
{
  "greeting":      "Hello, ${name}!",
  "json_example":  "Send {\"key\": \"value\"} to the endpoint",
  "math_example":  "The set {1, 2, 3} has ${count} elements",
  "price":         "Today's special: $${price_usd}",
  "windows_path":  "Save location: C:\\Users\\${user}\\Documents"
}
```

### Visible-marker diagnostics

When a translation has a problem, RosettaUnit emits a visible marker
in-place rather than failing silently. There are four:

| Marker                | Meaning                                                              | Triggered by                                          |
| --------------------- | -------------------------------------------------------------------- | ----------------------------------------------------- |
| `[unclosed]`          | A `${` was opened but never closed before end-of-template            | `"foo ${bar"`                                         |
| `[invalid: <body>]`   | `${...}` body, after whitespace trim, doesn't match identifier rules | `${1foo}`, `${name extra}`, `${}`                     |
| `[undefined: <name>]` | Identifier is valid, but no value in `params` and no fallback        | `${user}` when `params` has no `user`                 |
| `[missing: <path>]`   | `$file` reference target couldn't be opened                          | `{"k": {"$file": "missing.txt"}}`                     |

A reader seeing `[...]` in the UI immediately knows the locale data is
the problem, not the code. Multiple anomalies in the same template are
handled independently — healthy placeholders still substitute normally:

```
template:  "Hi ${name}, order ${1order} is ${unclosed"
params:    { name: "Alice" }

output:    "Hi Alice, order [invalid: 1order] is [unclosed]unclosed"
```

The marker strings are also exposed as constants in the
`rosetta::markers` namespace, so callers can build locale validators or
logging hooks that detect them programmatically.

## Building

```sh
cmake -B build
cmake --build build
ctest --test-dir build   # run tests
```

This produces:

- `librosetta_unit.a` — static library for C++ projects
- `librosetta_unit.so` / `.dylib` / `.dll` — shared library exposing the C ABI

## Adding a new binding

Any language with FFI can talk to the C ABI declared in
`include/rosetta_unit/rosetta_unit_c.h`. The Python binding under
`bindings/python/` is a good reference. For C#, the equivalent is
`[DllImport("rosetta_unit")]` with the same signatures.

## What's not (yet) included

- Pluralization. The design direction — rules as code, not data — is
  under discussion in
  [Issue #3](https://github.com/RosettaUnit/RosettaUnit/issues/3),
  targeted for v0.3.0 at the earliest.
- Right-to-left helpers — the library just returns strings, your UI layer
  handles direction.
- A GUI tool for editing translation files. JSON in any editor works fine.

---

## License

This project uses a **dual-license** scheme:

- **Source code** is licensed under the **MIT License**. See
  [`LICENSE-MIT`](./LICENSE-MIT).
  - Applies to: `src/`, `include/`, `bindings/`, `tests/`, build scripts.
  - Use it commercially, embed it anywhere, no copyleft obligations.

- **Data and content** are licensed under
  **CC BY-SA 4.0**. See [`LICENSE-CC-BY-SA-4.0`](./LICENSE-CC-BY-SA-4.0).
  - Applies to: `locales/`, `rules/` (when present), `docs/`.
  - Modifications to language data must be shared back under the same
    license, so the collective knowledge stays open.

This split mirrors the model used by projects like Wikipedia, OpenStreetMap,
and Stack Overflow: code stays permissive, community-maintained knowledge
stays copyleft.

---

## Contributing

Contributions are very welcome — especially language rule definitions and
translation data, which don't require any C++ knowledge. See
[`CONTRIBUTING.md`](./CONTRIBUTING.md).

---

## Roadmap

See the [issue tracker](https://github.com/RosettaUnit/RosettaUnit/issues)
and milestones. Highlights for upcoming versions:

- **v0.2.0** (current) — JS-template-literal-compatible `${name}` syntax;
  visible-marker family for anomaly diagnostics; public `rosetta::markers`
  namespace.
- **v0.3.0** — Pluralization helpers (rules as code; see Issue #3 for the
  open design discussion). Companion
  [`RosettaUnit-locale`](https://github.com/RosettaUnit/RosettaUnit-locale)
  module with built-in rules for major languages.
- **Beyond v0.3** — Additional language bindings, CMake packaging,
  performance work as needed, documentation improvements (see Issue #2
  for `$file` semantics and thread safety docs already queued).
