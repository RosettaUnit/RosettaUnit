// RosettaUnit C ABI.
//
// This stable C interface is what every non-C++ binding (Python, C#, Rust,
// etc.) talks to. Keep it minimal and ABI-stable — no STL types, no
// exceptions, no overloads.

#ifndef ROSETTA_UNIT_C_H
#define ROSETTA_UNIT_C_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
  #if defined(ROSETTA_UNIT_BUILD)
    #define ROSETTA_API __declspec(dllexport)
  #else
    #define ROSETTA_API __declspec(dllimport)
  #endif
#else
  #define ROSETTA_API __attribute__((visibility("default")))
#endif

typedef struct ru_translator ru_translator;

// Lifecycle
ROSETTA_API ru_translator* ru_create(void);
ROSETTA_API void           ru_destroy(ru_translator* t);

// Loading. All return 1 on success, 0 on failure.
ROSETTA_API int    ru_load_file(ru_translator* t, const char* path);
ROSETTA_API size_t ru_load_directory(ru_translator* t, const char* dir);
ROSETTA_API int    ru_load_string(ru_translator* t,
                                  const char* lang,
                                  const char* json_text);

// Language
ROSETTA_API void        ru_set_language(ru_translator* t, const char* lang);
ROSETTA_API const char* ru_language(ru_translator* t);
ROSETTA_API void        ru_set_fallback(ru_translator* t, const char* lang);

// Translation. The returned buffer is owned by RosettaUnit and is valid until
// the next call to ru_tr / ru_tr_params on the same translator. Copy it if
// you need to keep it.
//
// params_keys / params_values are parallel arrays of length params_count.
// Pass NULL/0 if you have no placeholders.
ROSETTA_API const char* ru_tr(ru_translator* t, const char* key);
ROSETTA_API const char* ru_tr_params(ru_translator* t,
                                     const char* key,
                                     const char* const* params_keys,
                                     const char* const* params_values,
                                     size_t params_count);

ROSETTA_API int ru_has(ru_translator* t, const char* key);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ROSETTA_UNIT_C_H
