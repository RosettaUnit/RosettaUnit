// C ABI implementation. Thin wrapper around rosetta::Translator.
//
// We cache the last result string per translator so the C-side caller has a
// stable pointer to read until its next call. This is the same lifetime
// contract that e.g. setlocale / strerror use.

#ifndef ROSETTA_UNIT_BUILD
#define ROSETTA_UNIT_BUILD
#endif
#include "rosetta_unit/rosetta_unit_c.h"
#include "rosetta_unit/rosetta_unit.hpp"

#include <string>

namespace {
struct CTranslator {
    rosetta::Translator inner;
    std::string last_result; // backing storage for returned const char*
};
} // namespace

extern "C" {

ru_translator* ru_create(void) {
    return reinterpret_cast<ru_translator*>(new (std::nothrow) CTranslator{});
}

void ru_destroy(ru_translator* t) {
    delete reinterpret_cast<CTranslator*>(t);
}

int ru_load_file(ru_translator* t, const char* path) {
    if (!t || !path) return 0;
    auto* c = reinterpret_cast<CTranslator*>(t);
    return c->inner.load_file(path) ? 1 : 0;
}

size_t ru_load_directory(ru_translator* t, const char* dir) {
    if (!t || !dir) return 0;
    auto* c = reinterpret_cast<CTranslator*>(t);
    return c->inner.load_directory(dir);
}

int ru_load_string(ru_translator* t, const char* lang, const char* json_text) {
    if (!t || !lang || !json_text) return 0;
    auto* c = reinterpret_cast<CTranslator*>(t);
    return c->inner.load_string(lang, json_text) ? 1 : 0;
}

void ru_set_language(ru_translator* t, const char* lang) {
    if (!t || !lang) return;
    reinterpret_cast<CTranslator*>(t)->inner.set_language(lang);
}

const char* ru_language(ru_translator* t) {
    if (!t) return "";
    return reinterpret_cast<CTranslator*>(t)->inner.language().c_str();
}

void ru_set_fallback(ru_translator* t, const char* lang) {
    if (!t || !lang) return;
    reinterpret_cast<CTranslator*>(t)->inner.set_fallback_language(lang);
}

const char* ru_tr(ru_translator* t, const char* key) {
    if (!t || !key) return "";
    auto* c = reinterpret_cast<CTranslator*>(t);
    c->last_result = c->inner.tr(key);
    return c->last_result.c_str();
}

const char* ru_tr_params(ru_translator* t,
                         const char* key,
                         const char* const* keys,
                         const char* const* values,
                         size_t count) {
    if (!t || !key) return "";
    auto* c = reinterpret_cast<CTranslator*>(t);
    rosetta::Params params;
    params.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        if (keys[i] && values[i]) params.emplace(keys[i], values[i]);
    }
    c->last_result = c->inner.tr(key, params);
    return c->last_result.c_str();
}

int ru_has(ru_translator* t, const char* key) {
    if (!t || !key) return 0;
    return reinterpret_cast<CTranslator*>(t)->inner.has(key) ? 1 : 0;
}

} // extern "C"
