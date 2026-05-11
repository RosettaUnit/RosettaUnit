"""
Python binding for RosettaUnit.

Build the shared library first (produces librosetta_unit.so / .dylib / .dll):
    cmake -B build && cmake --build build

Then:
    from rosetta_unit import Translator
    t = Translator()
    t.load_directory("locales")
    t.language = "ja"
    print(t.tr("greeting", name="Alice"))
"""

from __future__ import annotations

import ctypes
import os
import sys
from ctypes import c_char_p, c_int, c_size_t, c_void_p, POINTER
from typing import Iterable


def _find_library() -> ctypes.CDLL:
    """Locate the shared library next to this file or on the system path."""
    if sys.platform.startswith("win"):
        name = "rosetta_unit.dll"
    elif sys.platform == "darwin":
        name = "librosetta_unit.dylib"
    else:
        name = "librosetta_unit.so"

    here = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(here, name),
        os.path.join(here, "..", "..", "build", name),
        name,  # let the loader search LD_LIBRARY_PATH / PATH
    ]
    last_err = None
    for path in candidates:
        try:
            return ctypes.CDLL(path)
        except OSError as e:
            last_err = e
    raise OSError(f"Could not load {name}: {last_err}")


_lib = _find_library()

# ---- Signature declarations -----------------------------------------------
# Without these, ctypes assumes int return values, which will truncate
# pointers to 32 bits on 64-bit systems and silently corrupt the world.

_lib.ru_create.restype = c_void_p
_lib.ru_create.argtypes = []

_lib.ru_destroy.restype = None
_lib.ru_destroy.argtypes = [c_void_p]

_lib.ru_load_file.restype = c_int
_lib.ru_load_file.argtypes = [c_void_p, c_char_p]

_lib.ru_load_directory.restype = c_size_t
_lib.ru_load_directory.argtypes = [c_void_p, c_char_p]

_lib.ru_load_string.restype = c_int
_lib.ru_load_string.argtypes = [c_void_p, c_char_p, c_char_p]

_lib.ru_set_language.restype = None
_lib.ru_set_language.argtypes = [c_void_p, c_char_p]

_lib.ru_language.restype = c_char_p
_lib.ru_language.argtypes = [c_void_p]

_lib.ru_set_fallback.restype = None
_lib.ru_set_fallback.argtypes = [c_void_p, c_char_p]

_lib.ru_tr.restype = c_char_p
_lib.ru_tr.argtypes = [c_void_p, c_char_p]

_lib.ru_tr_params.restype = c_char_p
_lib.ru_tr_params.argtypes = [
    c_void_p, c_char_p,
    POINTER(c_char_p), POINTER(c_char_p), c_size_t,
]

_lib.ru_has.restype = c_int
_lib.ru_has.argtypes = [c_void_p, c_char_p]


def _enc(s: str) -> bytes:
    return s.encode("utf-8")


class Translator:
    """Pythonic wrapper around the RosettaUnit C ABI."""

    def __init__(self) -> None:
        self._h = _lib.ru_create()
        if not self._h:
            raise MemoryError("ru_create returned NULL")

    def __del__(self) -> None:
        # Guard against partial construction.
        h = getattr(self, "_h", None)
        if h:
            _lib.ru_destroy(h)
            self._h = None

    # ---- Loading -----------------------------------------------------------

    def load_file(self, path: str) -> bool:
        return bool(_lib.ru_load_file(self._h, _enc(path)))

    def load_directory(self, directory: str) -> int:
        return int(_lib.ru_load_directory(self._h, _enc(directory)))

    def load_string(self, lang: str, json_text: str) -> bool:
        return bool(_lib.ru_load_string(self._h, _enc(lang), _enc(json_text)))

    # ---- Language ----------------------------------------------------------

    @property
    def language(self) -> str:
        return _lib.ru_language(self._h).decode("utf-8")

    @language.setter
    def language(self, lang: str) -> None:
        _lib.ru_set_language(self._h, _enc(lang))

    def set_fallback(self, lang: str) -> None:
        _lib.ru_set_fallback(self._h, _enc(lang))

    # ---- Translation -------------------------------------------------------

    def tr(self, key: str, **params: object) -> str:
        if not params:
            return _lib.ru_tr(self._h, _enc(key)).decode("utf-8")

        # Build parallel arrays of UTF-8 encoded keys/values, then keep
        # references alive until the call returns.
        n = len(params)
        keys_arr = (c_char_p * n)()
        vals_arr = (c_char_p * n)()
        # Hold the bytes objects so they aren't GC'd mid-call.
        _alive: list[bytes] = []
        for i, (k, v) in enumerate(params.items()):
            kb, vb = _enc(k), _enc(str(v))
            _alive.append(kb); _alive.append(vb)
            keys_arr[i] = kb
            vals_arr[i] = vb

        out = _lib.ru_tr_params(self._h, _enc(key), keys_arr, vals_arr, n)
        return out.decode("utf-8")

    def has(self, key: str) -> bool:
        return bool(_lib.ru_has(self._h, _enc(key)))


__all__ = ["Translator"]
