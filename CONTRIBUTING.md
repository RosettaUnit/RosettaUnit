# Contributing to RosettaUnit

Thanks for your interest! RosettaUnit aims to be a small library backed by
a growing pool of community-maintained language data. There are several
ways to help, and most of them don't require touching the C++ code.

> **A note on the project's stage.** RosettaUnit is early (v0.1.x). The
> public API may still change. If you're planning a non-trivial
> contribution, please open an issue first to make sure we're aligned
> before you invest time.

---

## Ways to contribute

### 1. Add or improve translation data (no programming required)

Translation files are plain JSON under `locales/`. If you're a native
speaker of a language we don't yet cover well — or notice an awkward
phrasing in one we do — you can submit a pull request that only touches
JSON and prose files.

We treat translators as a first-class audience. If you ever feel like the
project is asking you to think like a programmer, that's a bug in our
design — please open an issue.

### 2. Contribute language rules (a little structure, no C++)

When the rule-definition mechanism lands in v0.2, language-specific rules
(plurals, gender selection, etc.) will also be plain JSON files in a
companion repository, `RosettaUnit-locale`. Native speakers and
linguistically curious folks are the ideal contributors here.

### 3. Write or maintain bindings

The core exposes a stable C ABI (see `include/rosetta_unit/rosetta_unit_c.h`).
If you'd like to bind RosettaUnit to your favorite language, the Python
binding under `bindings/python/` is a good reference.

### 4. Fix bugs, improve docs, add tests

Standard OSS fare. Small focused PRs are easier to review than large ones.

---

## Design principles worth knowing before you propose changes

A few design choices are deliberate and aren't going to change without a
strong reason. Knowing them up front saves everyone time.

**The library does not interpret language identifiers.**
`"ja"`, `"osaka-ben"`, `"elvish"`, `"classical-japanese"` are all just
dictionary keys. We don't validate them against BCP 47, we don't normalize
them, and we don't bundle CLDR. Standards support, when it arrives, will
live in a separate module — not in the core.

**Translation files contain no executable logic.**
Embedding expressions like `"count == 1"` in JSON brings back the
escape-hell we just escaped from, defeats static checking, and assumes
translators are programmers. Conditional logic belongs in *rule definition
files*, which are written by a different (smaller) group of contributors.

**Zero runtime dependencies.**
The library vendors its own minimal JSON parser. We're aware it's a
subset; that's intentional. PRs that pull in nlohmann/json, fmt, ICU, or
similar will be declined for the core, though they're fine as optional
companion modules.

**The library returns strings. That's it.**
RTL handling, font rendering, custom script support, grammatical
inflection — these belong to layers above (or, frankly, to a different
library). We're happy to expose hooks that make those layers easier to
build, but we won't grow the core to handle them directly.

**Missing data is loud, not silent.**
Missing keys return the key name, missing placeholders stay as `{name}`,
missing referenced files return `[missing: path]`. This is so problems
surface during development. Don't "fix" this by making it quiet.

If you find yourself wanting to push against one of these, that's
genuinely interesting — please open an issue to discuss. We'd rather
explain the trade-off than have you discover it in a PR review.

---

## Pull request flow

1. **Open an issue first for non-trivial work.** A two-line "I'm thinking
   of doing X, sound good?" saves both of us from wasted effort.
2. **Keep PRs focused.** One logical change per PR. If you're tempted to
   say "and also..." in the description, that's probably a second PR.
3. **Match existing style.** C++ code is formatted to match what's already
   there (consistency over personal preference). JSON files use 2-space
   indentation.
4. **Add or update tests.** Behavior changes need test coverage. The
   existing tests in `tests/test_basic.cpp` double as a specification —
   feel free to read them like one.
5. **Update documentation if behavior changes.** README, inline comments,
   and any relevant docs.

---

## Licensing of your contributions

By contributing, you agree that your contributions will be licensed under:

- **MIT License** for code (anything under `src/`, `include/`,
  `bindings/`, `tests/`, build scripts).
- **CC BY-SA 4.0** for data and content (anything under `locales/`,
  `rules/`, `docs/`).

If a file is ambiguous, the directory it lives in determines its license.
See [README.md](./README.md#license) for the full split.

You retain copyright to your contributions; you grant the project the
right to use them under the licenses above.

---

## Code of conduct

Be kind, be patient, assume good faith. Linguistic contributions in
particular often come from people writing in their non-native language —
extend the same care you'd hope to receive in return.

A formal `CODE_OF_CONDUCT.md` will be added as the project grows; in the
meantime, the spirit of the
[Contributor Covenant](https://www.contributor-covenant.org/) applies.

---

## Questions?

Open an issue with the `question` label, or start a discussion. There's
no such thing as too basic a question while the project is this young —
if something is unclear, that's usually our fault, not yours.
