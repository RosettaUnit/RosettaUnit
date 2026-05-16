// RosettaUnit implementation.
//
// JSON parsing is intentionally minimal: the locale format is a flat object
// of string -> string. A full JSON library would be overkill and add a
// dependency, so we hand-roll a small parser that handles the subset we need
// (strings with escapes, nested objects flattened with dot-paths, comments
// stripped). If you outgrow this, swap in nlohmann/json behind the same API.

#include "rosetta_unit/rosetta_unit.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <iostream>

namespace rosetta {

// ---------------------------------------------------------------------------
// Minimal JSON-ish parser
// ---------------------------------------------------------------------------
namespace detail {

// Sentinel prefix used to mark values that are file references rather than
// literal strings. The character is invalid in JSON-loaded content (it's a
// control char that would have been escaped) so collisions are impossible.
inline constexpr char kFileRefMarker = '\x01';

class JsonReader {
public:
    explicit JsonReader(const std::string& src) : src_(src) {}

    // Parse a top-level object into a flat map of dot-joined keys.
    // Throws std::runtime_error on malformed input.
    std::unordered_map<std::string, std::string> parse_flat() {
        skip_ws();
        expect('{');
        std::unordered_map<std::string, std::string> out;
        parse_object(out, "");
        skip_ws();
        return out;
    }

private:
    const std::string& src_;
    std::size_t pos_ = 0;

    // Returns the parsed file path if `{` ... `}` was a `{"$file": "..."}`
    // singleton object; returns empty optional otherwise (in which case
    // the contents have been parsed normally into `out`).
    //
    // We need lookahead-ish behavior here: snapshot the position, try to
    // parse as a file-ref, and rewind if it doesn't match. Cheaper than
    // building a real AST for a one-off pattern.
    bool try_parse_file_ref(std::string& path_out) {
        std::size_t save = pos_;
        skip_ws();
        if (peek() != '"') { pos_ = save; return false; }
        std::string k = parse_string();
        if (k != "$file") { pos_ = save; return false; }
        skip_ws();
        if (peek() != ':') { pos_ = save; return false; }
        ++pos_;
        skip_ws();
        if (peek() != '"') { pos_ = save; return false; }
        path_out = parse_string();
        skip_ws();
        if (peek() != '}') { pos_ = save; return false; }
        ++pos_; // consume closing '}'
        return true;
    }

    void parse_object(std::unordered_map<std::string, std::string>& out,
                      const std::string& prefix) {
        skip_ws();
        if (peek() == '}') { ++pos_; return; }
        while (true) {
            skip_ws();
            std::string key = parse_string();
            skip_ws();
            expect(':');
            skip_ws();
            std::string full = prefix.empty() ? key : prefix + "." + key;
            if (peek() == '"') {
                out[full] = parse_string();
            } else if (peek() == '{') {
                ++pos_;
                // Check for the {"$file": "..."} pattern before recursing
                // into a normal nested object.
                std::string ref;
                if (try_parse_file_ref(ref)) {
                    // Mark as a file reference; the loader resolves it.
                    out[full] = kFileRefMarker + ref;
                } else {
                    parse_object(out, full);
                }
            } else {
                // Numbers / bools / null: coerce to string so callers can
                // still retrieve them, though we recommend strings only.
                out[full] = parse_scalar();
            }
            skip_ws();
            if (peek() == ',') { ++pos_; continue; }
            if (peek() == '}') { ++pos_; return; }
            throw std::runtime_error("expected ',' or '}' at pos "
                                     + std::to_string(pos_));
        }
    }

    std::string parse_string() {
        expect('"');
        std::string s;
        while (pos_ < src_.size()) {
            char c = src_[pos_++];
            if (c == '"') return s;
            if (c == '\\' && pos_ < src_.size()) {
                char esc = src_[pos_++];
                switch (esc) {
                    case '"':  s += '"';  break;
                    case '\\': s += '\\'; break;
                    case '/':  s += '/';  break;
                    case 'n':  s += '\n'; break;
                    case 't':  s += '\t'; break;
                    case 'r':  s += '\r'; break;
                    case 'b':  s += '\b'; break;
                    case 'f':  s += '\f'; break;
                    case 'u': {
                        if (pos_ + 4 > src_.size())
                            throw std::runtime_error("bad \\u escape");
                        unsigned cp = 0;
                        for (int i = 0; i < 4; ++i) {
                            char h = src_[pos_++];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= h - '0';
                            else if (h >= 'a' && h <= 'f') cp |= h - 'a' + 10;
                            else if (h >= 'A' && h <= 'F') cp |= h - 'A' + 10;
                            else throw std::runtime_error("bad hex");
                        }
                        // Encode as UTF-8 (BMP only; surrogate pairs not handled)
                        if (cp < 0x80) {
                            s += static_cast<char>(cp);
                        } else if (cp < 0x800) {
                            s += static_cast<char>(0xC0 | (cp >> 6));
                            s += static_cast<char>(0x80 | (cp & 0x3F));
                        } else {
                            s += static_cast<char>(0xE0 | (cp >> 12));
                            s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                            s += static_cast<char>(0x80 | (cp & 0x3F));
                        }
                        break;
                    }
                    default: s += esc; break;
                }
            } else {
                s += c;
            }
        }
        throw std::runtime_error("unterminated string");
    }

    std::string parse_scalar() {
        std::string s;
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (c == ',' || c == '}' || c == ']' || std::isspace(
                    static_cast<unsigned char>(c))) break;
            s += c;
            ++pos_;
        }
        return s;
    }

    void skip_ws() {
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (std::isspace(static_cast<unsigned char>(c))) { ++pos_; continue; }
            // Allow // line comments and /* block comments as a courtesy.
            if (c == '/' && pos_ + 1 < src_.size()) {
                if (src_[pos_ + 1] == '/') {
                    pos_ += 2;
                    while (pos_ < src_.size() && src_[pos_] != '\n') ++pos_;
                    continue;
                }
                if (src_[pos_ + 1] == '*') {
                    pos_ += 2;
                    while (pos_ + 1 < src_.size()
                           && !(src_[pos_] == '*' && src_[pos_ + 1] == '/'))
                        ++pos_;
                    pos_ += 2;
                    continue;
                }
            }
            break;
        }
    }

    char peek() const {
        if (pos_ >= src_.size())
            throw std::runtime_error("unexpected end of input");
        return src_[pos_];
    }

    void expect(char c) {
        if (pos_ >= src_.size() || src_[pos_] != c) {
            throw std::runtime_error(std::string("expected '") + c
                                     + "' at pos " + std::to_string(pos_));
        }
        ++pos_;
    }
};

// ---------------------------------------------------------------------------
// Template interpolation (v0.2.0+, JS template literal compatible)
// ---------------------------------------------------------------------------

// ASCII whitespace check. Avoids std::isspace's locale dependency.
// Unicode whitespace (e.g. U+00A0, U+3000) is intentionally not stripped —
// this is a pragmatic simplification. Translators are not expected to
// embed such characters around identifiers.
inline bool is_ascii_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n'
        || c == '\r' || c == '\f' || c == '\v';
}

// Identifier character classes for `${name}` syntax.
inline bool is_ident_start(char c) {
    return (c >= 'A' && c <= 'Z')
        || (c >= 'a' && c <= 'z')
        || c == '_';
}
inline bool is_ident_cont(char c) {
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

// Validate that `s` matches the identifier grammar [A-Za-z_][A-Za-z0-9_]*.
// Empty strings return false.
bool is_valid_identifier(const std::string& s) {
    if (s.empty()) return false;
    if (!is_ident_start(s[0])) return false;
    for (std::size_t i = 1; i < s.size(); ++i) {
        if (!is_ident_cont(s[i])) return false;
    }
    return true;
}

// Substitute `${name}` placeholders in `tmpl` with values from `params`.
//
// Grammar (see Issue #1 for the full spec):
//   template      ::= ( placeholder | invalid_brace | literal_run )*
//   placeholder   ::= "${" ws* identifier ws* "}"
//   identifier    ::= [A-Za-z_] [A-Za-z0-9_]*
//   ws            ::= ASCII whitespace (space, tab, \n, \r, \f, \v)
//
// Anomalies are emitted as visible `[...]` markers rather than silently
// swallowed: `[unclosed]`, `[invalid: <body>]`, `[undefined: <name>]`.
// Healthy placeholders in the same template still substitute normally.
std::string interpolate(const std::string& tmpl, const Params& params) {
    // Fast path: no `${` sequence anywhere means no work to do.
    if (tmpl.find("${") == std::string::npos) return tmpl;

    std::string out;
    out.reserve(tmpl.size());

    std::size_t i = 0;
    while (i < tmpl.size()) {
        // Look for the start of a placeholder: `$` followed by `{`.
        // A lone `$` (not followed by `{`) is emitted literally.
        if (tmpl[i] != '$' || i + 1 >= tmpl.size() || tmpl[i + 1] != '{') {
            out += tmpl[i];
            ++i;
            continue;
        }

        // Found `${`. Scan forward for the matching `}`.
        std::size_t close = tmpl.find('}', i + 2);
        if (close == std::string::npos) {
            // Unclosed: emit marker, skip past the `${`, continue.
            // We do NOT consume the rest of the template — subsequent
            // characters might form valid placeholders on their own.
            out += markers::kUnclosed;
            i += 2;
            continue;
        }

        // Extract the body between `${` and `}` (exclusive on both ends).
        std::string body = tmpl.substr(i + 2, close - i - 2);

        // Trim leading and trailing ASCII whitespace.
        std::size_t start = 0;
        while (start < body.size() && is_ascii_ws(body[start])) ++start;
        std::size_t end = body.size();
        while (end > start && is_ascii_ws(body[end - 1])) --end;
        std::string trimmed = body.substr(start, end - start);

        if (!is_valid_identifier(trimmed)) {
            // Body is empty, has internal whitespace, or doesn't match
            // identifier rules. Emit `[invalid: <body>]` with the body
            // shown as-it-was-written (untrimmed) so the translator sees
            // exactly what they typed.
            out += markers::kInvalidPrefix;
            out += body;
            out += markers::kMarkerClose;
        } else {
            // Valid identifier — look it up in params.
            auto it = params.find(trimmed);
            if (it != params.end()) {
                out += it->second;
            } else {
                out += markers::kUndefinedPrefix;
                out += trimmed;
                out += markers::kMarkerClose;
            }
        }

        i = close + 1;
    }

    return out;
}

} // namespace detail

// ---------------------------------------------------------------------------
// Translator::Impl
// ---------------------------------------------------------------------------
struct Translator::Impl {
    // lang -> (key -> value)
    std::unordered_map<std::string,
                       std::unordered_map<std::string, std::string>> dicts;
    std::string current_lang = "en";
    std::string fallback_lang = "en";
    MissingKeyHandler missing_handler;

    const std::string* lookup(const std::string& lang,
                              const std::string& key) const {
        auto dit = dicts.find(lang);
        if (dit == dicts.end()) return nullptr;
        auto kit = dit->second.find(key);
        if (kit == dit->second.end()) return nullptr;
        return &kit->second;
    }
};

Translator::Translator() : impl_(std::make_unique<Impl>()) {}
Translator::~Translator() = default;
Translator::Translator(Translator&&) noexcept = default;
Translator& Translator::operator=(Translator&&) noexcept = default;

// Helper: load the entire contents of `path` into a string.
// Returns false if the file cannot be opened.
static bool read_file_to_string(const std::filesystem::path& path,
                                std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    // Strip a UTF-8 BOM if present — common with files saved by Notepad etc.
    if (out.size() >= 3
        && static_cast<unsigned char>(out[0]) == 0xEF
        && static_cast<unsigned char>(out[1]) == 0xBB
        && static_cast<unsigned char>(out[2]) == 0xBF) {
        out.erase(0, 3);
    }
    return true;
}

// Resolve any values that came back from the parser tagged with
// kFileRefMarker by loading the referenced file. `base_dir` is the directory
// against which relative `$file` paths are resolved (typically the directory
// containing the JSON file being loaded).
//
// If a referenced file is missing, we keep the entry as the raw key with a
// "[missing: path]" suffix so the problem surfaces in the UI rather than
// silently disappearing.
static void resolve_file_refs(std::unordered_map<std::string, std::string>& map,
                              const std::filesystem::path& base_dir) {
    for (auto& kv : map) {
        std::string& v = kv.second;
        if (v.empty() || v[0] != detail::kFileRefMarker) continue;
        std::filesystem::path ref(v.substr(1));
        if (ref.is_relative() && !base_dir.empty()) ref = base_dir / ref;

        std::string body;
        if (read_file_to_string(ref, body)) {
            v = std::move(body);
        } else {
            std::cerr << "[RosettaUnit] missing referenced file: "
                      << ref.string() << " (key=" << kv.first << ")\n";
            v = std::string(markers::kMissingPrefix) + ref.string()
                + markers::kMarkerClose;
        }
    }
}

bool Translator::load_file(const std::filesystem::path& path) {
    std::string text;
    if (!read_file_to_string(path, text)) return false;
    std::string lang = path.stem().string();
    // Use the JSON file's own directory as the base for relative $file paths.
    return load_string_internal(lang, text, path.parent_path());
}

std::size_t Translator::load_directory(const std::filesystem::path& dir) {
    std::size_t count = 0;
    if (!std::filesystem::is_directory(dir)) return 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;
        if (load_file(entry.path())) ++count;
    }
    return count;
}

bool Translator::load_string(const std::string& lang,
                             const std::string& json_text) {
    // No base dir → $file references with relative paths will fail.
    // That's acceptable: callers using load_string typically have no file
    // context anyway (e.g. embedded resources, tests).
    return load_string_internal(lang, json_text, {});
}

bool Translator::load_string_internal(const std::string& lang,
                                      const std::string& json_text,
                                      const std::filesystem::path& base_dir) {
    try {
        detail::JsonReader reader(json_text);
        auto map = reader.parse_flat();
        resolve_file_refs(map, base_dir);
        // Merge rather than replace so multiple files per language work.
        auto& dict = impl_->dicts[lang];
        for (auto& kv : map) dict[kv.first] = std::move(kv.second);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[RosettaUnit] failed to parse '" << lang << "': "
                  << e.what() << "\n";
        return false;
    }
}

void Translator::set_language(const std::string& lang) {
    impl_->current_lang = lang;
}

const std::string& Translator::language() const noexcept {
    return impl_->current_lang;
}

void Translator::set_fallback_language(const std::string& lang) {
    impl_->fallback_lang = lang;
}

const std::string& Translator::fallback_language() const noexcept {
    return impl_->fallback_lang;
}

std::vector<std::string> Translator::available_languages() const {
    std::vector<std::string> v;
    v.reserve(impl_->dicts.size());
    for (auto& kv : impl_->dicts) v.push_back(kv.first);
    return v;
}

std::string Translator::tr(const std::string& key) const {
    return tr(key, {});
}

std::string Translator::tr(const std::string& key, const Params& params) const {
    if (const auto* s = impl_->lookup(impl_->current_lang, key))
        return detail::interpolate(*s, params);

    if (impl_->current_lang != impl_->fallback_lang) {
        if (const auto* s = impl_->lookup(impl_->fallback_lang, key)) {
            if (impl_->missing_handler)
                impl_->missing_handler(impl_->current_lang, key);
            return detail::interpolate(*s, params);
        }
    }

    if (impl_->missing_handler)
        impl_->missing_handler(impl_->current_lang, key);
    // Last resort: echo the key with placeholders applied, so untranslated
    // text is obvious in the UI rather than silently empty.
    return detail::interpolate(key, params);
}

bool Translator::has(const std::string& key) const {
    return impl_->lookup(impl_->current_lang, key) != nullptr
        || impl_->lookup(impl_->fallback_lang, key) != nullptr;
}

void Translator::set_missing_key_handler(MissingKeyHandler handler) {
    impl_->missing_handler = std::move(handler);
}

} // namespace rosetta
