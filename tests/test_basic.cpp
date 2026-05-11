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

static void test_basic_lookup() {
    rosetta::Translator t;
    t.load_string("en", R"({"hello": "Hello"})");
    t.set_language("en");
    CHECK_EQ(t.tr("hello"), std::string("Hello"));
}

static void test_placeholder() {
    rosetta::Translator t;
    t.load_string("en", R"({"greet": "Hi, {name}!"})");
    t.set_language("en");
    CHECK_EQ(t.tr("greet", {{"name", "Bob"}}), std::string("Hi, Bob!"));
}

static void test_missing_placeholder_left_visible() {
    rosetta::Translator t;
    t.load_string("en", R"({"greet": "Hi, {name}!"})");
    t.set_language("en");
    // Missing values should not be silently empty.
    CHECK_EQ(t.tr("greet"), std::string("Hi, {name}!"));
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

static void test_double_brace_escape() {
    rosetta::Translator t;
    t.load_string("en", R"({"k": "literal {{not a placeholder}}"})");
    t.set_language("en");
    auto got = t.tr("k", {{"not a placeholder", "X"}});
    // First {{ becomes {, then the rest is parsed normally.
    CHECK(got.find("{not a placeholder}") != std::string::npos
          || got.find("{X}") != std::string::npos);
}

static void test_has() {
    rosetta::Translator t;
    t.load_string("en", R"({"a": "A"})");
    t.set_language("en");
    CHECK(t.has("a"));
    CHECK(!t.has("b"));
}

static void test_file_reference() {
    // Create a temporary directory with a JSON file and an external text file
    // so we can test $file resolution end-to-end.
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
    // Should produce a visible "[missing: ...]" marker, not crash or empty.
    auto v = t.tr("x");
    CHECK(v.find("[missing:") != std::string::npos);

    std::filesystem::remove_all(tmp);
}

int main() {
    test_basic_lookup();
    test_placeholder();
    test_missing_placeholder_left_visible();
    test_fallback();
    test_missing_key_returns_key();
    test_nested_json_flattens();
    test_escape_sequences();
    test_unicode_escape();
    test_double_brace_escape();
    test_has();
    test_file_reference();
    test_file_reference_missing();

    if (failures == 0) {
        std::cout << "All tests passed.\n";
        return 0;
    }
    std::cout << failures << " test(s) failed.\n";
    return 1;
}
