// Hand-rolled tests so there's no external test framework dependency.
// Returns non-zero exit code on any failure so CTest picks it up.

#include "rosetta_unit/rosetta_unit.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cassert>

static int failures = 0;

#define CHECK(cond) do {                                                      \
    if (!(cond)) {                                                            \
        std::cerr << "FAIL: " << #cond << " (line " << __LINE__ << ")\n";     \
        ++failures;                                                           \
    }                                                                         \
} while (0)

#define CHECK_EQ(a, b) do {                                                   \
    auto _va = (a); auto _vb = (b);                                           \
    if (!(_va == _vb)) {                                                      \
        std::cerr << "FAIL: " << #a << " == " << #b                           \
                  << " (got '" << _va << "' vs '" << _vb << "', line "        \
                  << __LINE__ << ")\n";                                       \
        ++failures;                                                           \
    }                                                                         \
} while (0)

// ---- Existing tests (some updated for v0.2.0 syntax) ---------------------

static void test_basic_lookup() {
    rosetta::Translator t;
    t.load_string("en", R"({"hello": "Hello"})");
    t.set_language("en");
    CHECK_EQ(t.tr("hello"), std::string("Hello"));
}

static void test_placeholder() {
    rosetta::Translator t;
    t.load_string("en", R"({"greet": "Hi, ${name}!"})");
    t.set_language("en");
    CHECK_EQ(t.tr("greet", {{"name", "Bob"}}), std::string("Hi, Bob!"));
}

static void test_undefined_placeholder_visible() {
    rosetta::Translator t;
    t.load_string("en", R"({"greet": "Hi, ${name}!"})");
    t.set_language("en");
    // Undefined identifiers surface as `[undefined: ...]` markers, not as
    // silently empty strings or as the placeholder syntax left raw.
    CHECK_EQ(t.tr("greet"), std::string("Hi, [undefined: name]!"));
}

static void test_fallback() {
    rosetta::Translator t;
    t.load_string("en", R"({"k": "english"})");
    t.load_string("ja", R"({"other": "別"})");
    t.set_fallback_language("en");
    t.set_language("ja");
    CHECK_EQ(t.tr("k"), std::string("english")); // fell back
    CHECK_EQ(t.tr("other"), std::string("別"));
}

static void test_missing_key_returns_key() {
    rosetta::Translator t;
    t.set_language("en");
    CHECK_EQ(t.tr("nope"), std::string("nope"));
}

static void test_nested_json_flattens() {
    rosetta::Translator t;
    t.load_string("en", R"({"menu": {"file": "File", "edit": "Edit"}})");
    t.set_language("en");
    CHECK_EQ(t.tr("menu.file"), std::string("File"));
    CHECK_EQ(t.tr("menu.edit"), std::string("Edit"));
}

static void test_escape_sequences() {
    rosetta::Translator t;
    t.load_string("en", R"({"line": "a\nb\t\"c\""})");
    t.set_language("en");
    CHECK_EQ(t.tr("line"), std::string("a\nb\t\"c\""));
}

static void test_unicode_escape() {
    rosetta::Translator t;
    // U+3042 HIRAGANA LETTER A -> "あ" in UTF-8 (E3 81 82)
    t.load_string("ja", R"({"x": "\u3042"})");
    t.set_language("ja");
    CHECK_EQ(t.tr("x"), std::string("\xE3\x81\x82"));
}

static void test_braces_literal() {
    // v0.2.0: both `{` and `}` are literal. No `{{` escape mechanism.
    // JSON, HTML, code samples, math notation pass through unchanged.
    rosetta::Translator t;
    t.load_string("en", R"({"k": "use { } for braces"})");
    t.set_language("en");
    CHECK_EQ(t.tr("k"), std::string("use { } for braces"));
}

static void test_has() {
    rosetta::Translator t;
    t.load_string("en", R"({"a": "A"})");
    t.set_language("en");
    CHECK(t.has("a"));
    CHECK(!t.has("b"));
}

static void test_file_reference() {
    auto tmp = std::filesystem::temp_directory_path() / "rosetta_test_fileref";
    std::filesystem::create_directories(tmp);

    std::ofstream(tmp / "intro.txt") << "Long body text\nwith newlines.";
    std::ofstream(tmp / "en.json") <<
        R"({"short": "Hi", "long": {"$file": "intro.txt"}})";

    rosetta::Translator t;
    t.load_file(tmp / "en.json");
    t.set_language("en");
    CHECK_EQ(t.tr("short"), std::string("Hi"));
    CHECK_EQ(t.tr("long"), std::string("Long body text\nwith newlines."));

    std::filesystem::remove_all(tmp);
}

static void test_file_reference_missing() {
    auto tmp = std::filesystem::temp_directory_path() / "rosetta_test_missing";
    std::filesystem::create_directories(tmp);
    std::ofstream(tmp / "en.json") <<
        R"({"x": {"$file": "does_not_exist.txt"}})";

    rosetta::Translator t;
    t.load_file(tmp / "en.json");
    t.set_language("en");
    auto v = t.tr("x");
    CHECK(v.find("[missing:") != std::string::npos);

    std::filesystem::remove_all(tmp);
}

// ---- v0.2.0 template syntax tests (new) -----------------------------------

static void test_placeholder_basic() {
    rosetta::Translator t;
    t.load_string("en", R"({"k": "Hello, ${name}!"})");
    t.set_language("en");
    CHECK_EQ(t.tr("k", {{"name", "Alice"}}), std::string("Hello, Alice!"));
}

static void test_placeholder_whitespace_allowed() {
    rosetta::Translator t;
    t.load_string("en", R"({"k": "Hi ${ name }!"})");
    t.set_language("en");
    CHECK_EQ(t.tr("k", {{"name", "Bob"}}), std::string("Hi Bob!"));
}

static void test_placeholder_whitespace_tabs() {
    rosetta::Translator t;
    // `${\tname\t}` — whitespace including tabs is trimmed.
    t.load_string("en", R"({"k": "Hi ${	name	}!"})");
    t.set_language("en");
    CHECK_EQ(t.tr("k", {{"name", "Carol"}}), std::string("Hi Carol!"));
}

static void test_invalid_internal_whitespace() {
    rosetta::Translator t;
    // Internal whitespace makes it not match the identifier grammar.
    // Body is shown as-it-was-written (untrimmed).
    t.load_string("en", R"({"k": "${name extra}"})");
    t.set_language("en");
    CHECK_EQ(t.tr("k", {{"name", "X"}}),
             std::string("[invalid: name extra]"));
}

static void test_invalid_leading_digit() {
    rosetta::Translator t;
    t.load_string("en", R"({"k": "${1foo}"})");
    t.set_language("en");
    CHECK_EQ(t.tr("k"), std::string("[invalid: 1foo]"));
}

static void test_invalid_empty_body() {
    rosetta::Translator t;
    t.load_string("en", R"({"k": "${}"})");
    t.set_language("en");
    CHECK_EQ(t.tr("k"), std::string("[invalid: ]"));
}

static void test_invalid_whitespace_only_body() {
    rosetta::Translator t;
    // `${   }` — body is whitespace-only, trim to empty, still invalid.
    // Untrimmed body (3 spaces) appears inside the marker.
    t.load_string("en", R"({"k": "${   }"})");
    t.set_language("en");
    CHECK_EQ(t.tr("k"), std::string("[invalid:    ]"));
}

static void test_undefined_identifier() {
    rosetta::Translator t;
    t.load_string("en", R"({"k": "${unknown}"})");
    t.set_language("en");
    CHECK_EQ(t.tr("k"), std::string("[undefined: unknown]"));
}

static void test_undefined_with_whitespace_trimmed() {
    rosetta::Translator t;
    // `${ unknown }` is valid syntax but unknown name — trimmed name in marker.
    t.load_string("en", R"({"k": "${ unknown }"})");
    t.set_language("en");
    CHECK_EQ(t.tr("k"), std::string("[undefined: unknown]"));
}

static void test_unclosed_placeholder() {
    rosetta::Translator t;
    t.load_string("en", R"({"k": "foo ${bar"})");
    t.set_language("en");
    CHECK_EQ(t.tr("k", {{"bar", "X"}}), std::string("foo [unclosed]bar"));
}

static void test_unclosed_at_end() {
    rosetta::Translator t;
    // `${` at end of string with no `}` anywhere.
    t.load_string("en", R"({"k": "trailing ${"})");
    t.set_language("en");
    CHECK_EQ(t.tr("k"), std::string("trailing [unclosed]"));
}

static void test_multiple_anomalies_independent() {
    rosetta::Translator t;
    t.load_string("en",
        R"({"k": "Hi ${name}, order ${1order} is ${unclosed"})");
    t.set_language("en");
    // Healthy placeholder substitutes, invalid and unclosed each get markers.
    // After `[unclosed]`, the literal `unclosed` characters remain in the
    // output because the scanner advances past only `${` and continues.
    CHECK_EQ(t.tr("k", {{"name", "Alice"}}),
             std::string("Hi Alice, order [invalid: 1order] is [unclosed]unclosed"));
}

static void test_dollar_literal_alone() {
    rosetta::Translator t;
    t.load_string("en", R"({"k": "Price: $5"})");
    t.set_language("en");
    CHECK_EQ(t.tr("k"), std::string("Price: $5"));
}

static void test_dollar_followed_by_identifier_chars() {
    rosetta::Translator t;
    // `$NAME` — `$` not followed by `{`, so it's literal.
    t.load_string("en", R"({"k": "Env: $NAME"})");
    t.set_language("en");
    CHECK_EQ(t.tr("k"), std::string("Env: $NAME"));
}

static void test_dollar_dollar_literal() {
    rosetta::Translator t;
    // No `$$` escape; both `$` are literal because neither is followed by `{`.
    // The pattern in Issue #1 examples: "Today's special: $${price_usd}"
    // means: `$` literal, then `${price_usd}` substituted.
    t.load_string("en", R"({"k": "Today: $${price}"})");
    t.set_language("en");
    CHECK_EQ(t.tr("k", {{"price", "9.99"}}), std::string("Today: $9.99"));
}

static void test_dollar_then_open_brace_no_close() {
    rosetta::Translator t;
    // `${` with no close → unclosed marker.
    t.load_string("en", R"({"k": "x ${y"})");
    t.set_language("en");
    CHECK_EQ(t.tr("k"), std::string("x [unclosed]y"));
}

static void test_braces_in_literal_text() {
    rosetta::Translator t;
    t.load_string("en", R"({"k": "JSON example: {\"key\": \"value\"}"})");
    t.set_language("en");
    CHECK_EQ(t.tr("k"),
             std::string("JSON example: {\"key\": \"value\"}"));
}

static void test_math_braces_with_placeholder() {
    rosetta::Translator t;
    t.load_string("en", R"({"k": "Set {1, 2, 3} has ${count} elements"})");
    t.set_language("en");
    CHECK_EQ(t.tr("k", {{"count", "3"}}),
             std::string("Set {1, 2, 3} has 3 elements"));
}

static void test_old_brace_syntax_is_literal() {
    rosetta::Translator t;
    // v0.1.0 syntax `{name}` is no longer special — it's pure literal.
    t.load_string("en", R"({"k": "old syntax {name} here"})");
    t.set_language("en");
    CHECK_EQ(t.tr("k", {{"name", "X"}}),
             std::string("old syntax {name} here"));
}

static void test_old_double_brace_is_literal() {
    rosetta::Translator t;
    // v0.1.0 `{{` escape mechanism is removed — `{{` is two literal braces.
    t.load_string("en", R"({"k": "old escape {{ stays as-is"})");
    t.set_language("en");
    CHECK_EQ(t.tr("k"), std::string("old escape {{ stays as-is"));
}

static void test_placeholder_in_file_loaded_text() {
    // Ensures `$file`-loaded content goes through the same interpolation
    // pipeline as inline strings. This is a key use case for long-form
    // text (terms of service, help pages) with per-app values embedded.
    auto tmp = std::filesystem::temp_directory_path()
             / "rosetta_test_file_placeholder";
    std::filesystem::create_directories(tmp);

    std::ofstream(tmp / "about.txt")
        << "Welcome to ${app_name}!\nVersion ${version}.";
    std::ofstream(tmp / "en.json")
        << R"({"about": {"$file": "about.txt"}})";

    rosetta::Translator t;
    t.load_file(tmp / "en.json");
    t.set_language("en");
    auto got = t.tr("about", {{"app_name", "NativeYT"},
                              {"version", "0.2.0"}});
    CHECK_EQ(got, std::string("Welcome to NativeYT!\nVersion 0.2.0."));

    std::filesystem::remove_all(tmp);
}

static void test_empty_params_with_no_placeholders() {
    rosetta::Translator t;
    t.load_string("en", R"({"k": "plain text"})");
    t.set_language("en");
    CHECK_EQ(t.tr("k"), std::string("plain text"));
}

static void test_empty_params_with_placeholder_emits_undefined() {
    rosetta::Translator t;
    t.load_string("en", R"({"k": "Hello ${name}"})");
    t.set_language("en");
    // No params passed → identifier is valid but no value → undefined marker.
    // This is the v0.2.0 fix for v0.1.0's bug where empty params took an
    // early-return path that broke escape handling.
    CHECK_EQ(t.tr("k"), std::string("Hello [undefined: name]"));
}

static void test_unrelated_params_emits_undefined() {
    rosetta::Translator t;
    t.load_string("en", R"({"k": "Hello ${name}"})");
    t.set_language("en");
    CHECK_EQ(t.tr("k", {{"other", "X"}}),
             std::string("Hello [undefined: name]"));
}

// ---- Entry point ---------------------------------------------------------

int main() {
    // Existing tests (some updated for v0.2.0 syntax)
    test_basic_lookup();
    test_placeholder();
    test_undefined_placeholder_visible();
    test_fallback();
    test_missing_key_returns_key();
    test_nested_json_flattens();
    test_escape_sequences();
    test_unicode_escape();
    test_braces_literal();
    test_has();
    test_file_reference();
    test_file_reference_missing();

    // v0.2.0 template syntax — placeholders
    test_placeholder_basic();
    test_placeholder_whitespace_allowed();
    test_placeholder_whitespace_tabs();

    // v0.2.0 template syntax — invalid identifiers
    test_invalid_internal_whitespace();
    test_invalid_leading_digit();
    test_invalid_empty_body();
    test_invalid_whitespace_only_body();

    // v0.2.0 template syntax — undefined identifiers
    test_undefined_identifier();
    test_undefined_with_whitespace_trimmed();

    // v0.2.0 template syntax — unclosed
    test_unclosed_placeholder();
    test_unclosed_at_end();

    // v0.2.0 template syntax — anomaly composition
    test_multiple_anomalies_independent();

    // v0.2.0 template syntax — `$` literal handling
    test_dollar_literal_alone();
    test_dollar_followed_by_identifier_chars();
    test_dollar_dollar_literal();
    test_dollar_then_open_brace_no_close();

    // v0.2.0 template syntax — braces are literal
    test_braces_in_literal_text();
    test_math_braces_with_placeholder();
    test_old_brace_syntax_is_literal();
    test_old_double_brace_is_literal();

    // v0.2.0 template syntax — `$file` integration
    test_placeholder_in_file_loaded_text();

    // v0.2.0 template syntax — empty/unrelated params
    test_empty_params_with_no_placeholders();
    test_empty_params_with_placeholder_emits_undefined();
    test_unrelated_params_emits_undefined();

    if (failures == 0) {
        std::cout << "All tests passed.\n";
        return 0;
    }
    std::cout << failures << " test(s) failed.\n";
    return 1;
}
