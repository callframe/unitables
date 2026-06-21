# AGENTS.md

UniTables is a small C library that emits lookup tables for Unicode properties (it is not a full Unicode processing library). It deliberately ships the minimal set of properties for now and is built to grow.

## Commands

- Build the library (also runs the table generator): `bazel build //:unitables`
- Run the generator standalone: `python unitables.py <output_dir> <path_to_UnicodeData.txt>` → writes `<output_dir>/unitables_data.c`
- Refresh `compile_commands.json` for clangd/LSP: `./commands.sh` (runs `@hedron_compile_commands//:refresh_all`)

There is no automated test target. To validate generator changes, generate `unitables_data.c` and compare `unitables_properties(cp)` results against the source `UnicodeData.txt` directly (Python's bundled `unicodedata` is an older Unicode version, so it is *not* a valid oracle).

## Architecture

The library is a generated two-stage lookup table, mirroring utf8proc's design. Reference copies of utf8proc (`ref/utf8proc.c`, `.h`, `.jl`) are vendored for guidance only and are **not** part of the build.

Data flow:
1. `MODULE.bazel` declares an `http_file` that downloads `UnicodeData.txt` (Unicode `UNICODE_VERSION`, currently 17.0.0) pinned by sha256.
2. The `generate_unitables` genrule in `BUILD.bazel` runs `unitables.py` on that file, producing `unitables_data.c` in `bazel-bin`.
3. `unitables.c` does `#include "unitables_data.c"` (it is listed as `textual_hdrs`, not compiled on its own) and implements the public lookup.

Runtime lookup (`unitables.c`): `UNITABLES_PROPERTIES[UNITABLES_STAGE2[UNITABLES_STAGE1[cp >> 8] + (cp & 0xFF)]]`. Out-of-range/unassigned code points resolve to `UNITABLES_PROPERTIES[0]`, the shared sentinel whose `category` is `Unitables_Category_Cn` (= 0).

The generated `unitables_data.c` contains four arrays:
- `UNITABLES_SEQUENCES` — one shared, deduplicated UTF-16 array holding all decomposition and case-mapping code points. A `*_seqindex` packs the storage offset (low 14 bits) and decoded length-1 (top 2 bits; `3` = length stored inline as the first unit). BMP code points take one unit, non-BMP a surrogate pair. `UINT16_MAX` means "no mapping".
- `UNITABLES_STAGE1` / `UNITABLES_STAGE2` — the paged index (see lookup above).
- `UNITABLES_PROPERTIES` — deduplicated `struct Unitables_Properties` entries; index 0 is the sentinel.

`unitables.h` is the hand-written public API: `struct Unitables_Properties`, the `Unitables_Category` / `Unitables_BidiClass` / `Unitables_DecompType` enums, and `unitables_properties()`.

### The generator (`unitables.py`)

Organized into two kinds of clearly-bannered blocks:
- **PROCESS `<file>`** — read one UCD file into a table keyed by code point (`<..., First>`/`<..., Last>` ranges are expanded). Currently only `UnicodeData.txt`.
- **PRODUCE `<table>`** — combine the processed tables into an emitted C array.

The single `intern()` helper backs all deduplication (sequences, properties, pages).

To add a new property source (e.g. CaseFolding.txt): add a PROCESS block producing a dict keyed by code point, add the field to the `entry` tuple via `encode_sequence(...)` in the properties PRODUCE loop, and add the matching field to `struct Unitables_Properties` (and the `SENTINEL`/enums) in `unitables.h`. No restructuring of existing code is required — this is the intended extension seam. Because only `UnicodeData.txt` is processed today, some properties are intentionally incomplete (e.g. `ß` has no simple uppercase mapping — that lives in SpecialCasing.txt).

## Conventions

- **C, not C++.** Use `/* */` comments only (no `//`), `#include <stdint.h>` only, and **east const** (`T const *`).
- Naming: type names use Ada case `Unitables_*` (e.g. `Unitables_Properties`, `Unitables_Category_Lu`); generated data tables and macros are UPPERCASE `UNITABLES_*`; functions are lower-snake `unitables_*`.
- C formatting is governed by `.clang-format` (Allman braces, 2-space indent, 80 columns, left pointer alignment).
- Enum values matter: `Unitables_Category_Cn = 0` so unassigned/out-of-range code points share the sentinel slot.

## Gotchas

- `unitables_data.c` only exists under `bazel-bin` after a build, so a standalone clangd/editor will report `'unitables_data.c' file not found` and downstream "undeclared identifier" errors for `UNITABLES_*`. This is expected; build through Bazel and run `./commands.sh` to refresh `compile_commands.json`.
- `bazel-*` and `compile_commands.json` are git-ignored.
