// Minimal example. Run from the repository root so that ./locales is found:
//   ./hello_example en
//   ./hello_example ja
//   ./hello_example es     # falls back to en for missing keys

#include "rosetta_unit/rosetta_unit.hpp"
#include <iostream>

int main(int argc, char** argv) {
    rosetta::Translator t;
    t.set_fallback_language("en");

    auto loaded = t.load_directory("locales");
    if (loaded == 0) {
        std::cerr << "No locale files found in ./locales\n";
        return 1;
    }

    std::string lang = (argc > 1) ? argv[1] : "en";
    t.set_language(lang);

    // Optional: log missing keys so you notice them during development.
    t.set_missing_key_handler([](const std::string& l, const std::string& k) {
        std::cerr << "[i18n] missing '" << k << "' for lang '" << l << "'\n";
    });

    std::cout << t.tr("app.title") << "\n";
    std::cout << t.tr("greeting", {{"name", "Alice"}}) << "\n";
    std::cout << t.tr("menu.file") << " / "
              << t.tr("menu.edit") << " / "
              << t.tr("menu.help") << "\n";
    std::cout << t.tr("dialog.confirm_exit") << "\n";
    std::cout << t.tr("status.saved", {{"count", "3"}}) << "\n";
    std::cout << "\n--- about ---\n";
    std::cout << t.tr("about.text");
    return 0;
}
