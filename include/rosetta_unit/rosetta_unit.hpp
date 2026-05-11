// RosettaUnit - Lightweight i18n library for C++
// https://github.com/RosettaUnit/RosettaUnit
//
// Usage:
//   rosetta::Translator t;
//   t.load_directory("locales");
//   t.set_language("ja");
//   std::string msg = t.tr("greeting", {{"name", "Alice"}});

#ifndef ROSETTA_UNIT_HPP
#define ROSETTA_UNIT_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>
#include <filesystem>

namespace rosetta {

// Replacement map for placeholders like {name}
using Params = std::unordered_map<std::string, std::string>;

// Callback signature for missing-key reporting (optional)
using MissingKeyHandler = std::function<void(const std::string& lang,
                                             const std::string& key)>;

class Translator {
public:
    Translator();
    ~Translator();

    // Disable copy, allow move
    Translator(const Translator&) = delete;
    Translator& operator=(const Translator&) = delete;
    Translator(Translator&&) noexcept;
    Translator& operator=(Translator&&) noexcept;

    // ---- Loading ----------------------------------------------------------

    // Load a single locale file. The language code is derived from the
    // filename stem (e.g. "ja.json" -> "ja"). Returns true on success.
    bool load_file(const std::filesystem::path& path);

    // Load every *.json file in a directory.
    // Returns the number of locales successfully loaded.
    std::size_t load_directory(const std::filesystem::path& dir);

    // Load translations from a raw JSON string under the given language code.
    bool load_string(const std::string& lang, const std::string& json_text);

    // ---- Language selection -----------------------------------------------

    // Set the active language. If unknown, falls back to fallback_language().
    void set_language(const std::string& lang);
    const std::string& language() const noexcept;

    // Language used when a key is missing from the active one.
    // Defaults to "en".
    void set_fallback_language(const std::string& lang);
    const std::string& fallback_language() const noexcept;

    // List of language codes currently loaded.
    std::vector<std::string> available_languages() const;

    // ---- Translation ------------------------------------------------------

    // Look up `key` in the active language. Substitutes {placeholder}
    // occurrences with values from `params`. Falls back to the fallback
    // language and finally to `key` itself if nothing matches.
    std::string tr(const std::string& key) const;
    std::string tr(const std::string& key, const Params& params) const;

    // True if the key exists in the active or fallback language.
    bool has(const std::string& key) const;

    // ---- Diagnostics ------------------------------------------------------

    void set_missing_key_handler(MissingKeyHandler handler);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    // Internal loader that knows the base directory for resolving relative
    // $file references. Public load_file/load_string delegate to this.
    bool load_string_internal(const std::string& lang,
                              const std::string& json_text,
                              const std::filesystem::path& base_dir);
};

} // namespace rosetta

#endif // ROSETTA_UNIT_HPP
