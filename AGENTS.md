# AGENTS.md

UniTables is a small C library that emits lookup tables for Unicode properties (it is not a full Unicode processing library). It deliberately ships the minimal set of properties for now and is built to grow.

## Commands

- Build the library (also runs the table generator): `bazel build //:unitables`
- Build with CMake (downloads the UCD files at configure time, then runs the generator): `cmake -B build && cmake --build build`
- Run the generator standalone: `python unitables.py <output_dir> --unicode-data=... --composition-exclusions=... --case-folding=... --grapheme-break-property=... --emoji-data=... --derived-core-properties=...` → writes `<output_dir>/unitables_data.c`
- Refresh `compile_commands.json` for clangd/LSP: `./commands.sh` (runs `@hedron_compile_commands//:refresh_all`)

There is no automated test target. To validate generator changes, generate `unitables_data.c` and compare `unitables_properties(cp)` results against the source `UnicodeData.txt` directly (Python's bundled `unicodedata` is an older Unicode version, so it is *not* a valid oracle).

## Architecture

The library is a generated two-stage lookup table, mirroring utf8proc's design. Reference copies of utf8proc (`ref/utf8proc.c`, `.h`, `.jl`) are vendored for guidance only and are **not** part of the build.

Data flow:
1. `MODULE.bazel` declares one `http_file` per UCD input, each pinned by sha256, all at Unicode `UNICODE_VERSION` (currently 17.0.0): `UnicodeData.txt`, `CompositionExclusions.txt`, `CaseFolding.txt`, `GraphemeBreakProperty.txt`, `emoji-data.txt`, and `DerivedCoreProperties.txt`.
2. The `generate_unitables` genrule in `BUILD.bazel` runs `unitables.py` on those files, producing `unitables_data.c` in `bazel-bin`.
3. `unitables.c` does `#include "unitables_data.c"` (it is listed as `textual_hdrs`, not compiled on its own) and implements the public lookup.

Runtime lookup (`unitables.c`): `UNITABLES_PROPERTIES[UNITABLES_STAGE2[(UNITABLES_STAGE1[cp >> 8] << 8) + (cp & 0xFF)]]`. Out-of-range/unassigned code points resolve to `UNITABLES_PROPERTIES[0]`, the shared sentinel whose `category` is `Unitables_Category_Cn` (= 0).

The generated `unitables_data.c` contains four arrays:
- `UNITABLES_SEQUENCES` — one shared, deduplicated UTF-16 array holding all decomposition and case-mapping code points. A `*_seqindex` packs the storage offset (low 14 bits) and decoded length-1 (top 2 bits; `3` = length stored inline as the first unit). BMP code points take one unit, non-BMP a surrogate pair. Storage offset 0 is reserved, so `UNITABLES_SEQ_NONE` (= 0) means "no mapping" and no real mapping can encode to it.
- `UNITABLES_STAGE1` / `UNITABLES_STAGE2` — the paged index (see lookup above). `UNITABLES_STAGE1` holds page numbers rather than offsets into `UNITABLES_STAGE2`, so a `uint16_t` covers every one of the 4352 pages the code point space can hold.
- `UNITABLES_PROPERTIES` — deduplicated `struct Unitables_Properties` entries; index 0 is the sentinel.

`unitables.h` is the hand-written public API: `struct Unitables_Properties`, the `Unitables_Category` / `Unitables_Bidi_Class` / `Unitables_Decomp_Type` enums, and `unitables_properties()`.

### The generator (`unitables.py`)

Organized into two kinds of clearly-bannered blocks:
- **PROCESS `<file>`** — read one UCD file into a table keyed by code point (`<..., First>`/`<..., Last>` ranges are expanded). One block per input file; six today. A block also carries any helper and any capacity guard derived purely from its own table, such as `full_decomposition` and the `UNITABLES_DECOMPOSE_MAX` assert in the `UnicodeData.txt` block.
- **PRODUCE `<table>`** — combine the processed tables into an emitted C array.

The single `intern()` helper backs all deduplication (sequences, properties, pages).

To add a new property source (e.g. CaseFolding.txt): add a PROCESS block producing a dict keyed by code point, add the field to the `entry` tuple via `encode_sequence(...)` in the properties PRODUCE loop, and add the matching field to `struct Unitables_Properties` (and the `SENTINEL`/enums) in `unitables.h`. No restructuring of existing code is required — this is the intended extension seam. Some properties are intentionally incomplete because their source file is not processed yet: `SpecialCasing.txt` is absent, so the case mappings are the simple one-to-one ones only (`ß` has no uppercase mapping, rather than mapping to `SS`).

## Conventions

- **C, not C++.** Use `/* */` comments only (no `//`) and **east const** (`T const *`). The whole standard library is available; the library happens to need only `<stdint.h>` and `<stddef.h>` today, which is a fact about it rather than a restriction on it.
- Naming: type names use Ada case `Unitables_*` (e.g. `Unitables_Bidi_Class`, `Unitables_Indic_Conjunct_Break`); generated data tables and macros are UPPERCASE `UNITABLES_*`; functions and struct fields are lower-snake (`unitables_grapheme_break`, `bound_class`).
- Enum value suffixes are the exception: they reproduce the UCD property value alias verbatim (`Unitables_Category_Lu`, `Unitables_Bound_Class_SpacingMark`). Only the type-name prefix follows Ada case.
- C formatting is governed by `.clang-format` (Allman braces, 2-space indent, 80 columns, left pointer alignment).
- Use prefix increment (`++i`, not `i++`), including in `for` increments.
- Multi-code-point writers take no capacity and require a non-null `dst`: the caller sizes `dst` by the writer's published `UNITABLES_*_MAX` bound. Those bounds are small enough to be stack arrays, so measuring before writing would only do the work twice to avoid a buffer the caller should have declared anyway. No partial writes, ever.
- Flat code. Early returns and `continue` over nesting, `goto` acceptable, helpers rather than indentation. The longer a function, the simpler it must be; code that wraps at 80 columns is too deeply nested.
- Comments are minimal and say why, not what. More comment than code is a smell. In `unitables.c`, a plain `/* */` one-liner where it earns its place and none where the code reads clearly. In `unitables.h` they are the public contract, so they are doxygen: `/** */`, continuation lines with no leading asterisk, and `@param` / `@return` only where they add something the prose did not.
- C99 today. C11 and C17 are fine; C23 is not, because MSVC has no native support for it.
- No encoding layer, deliberately. UniTables is consumed by a UTF-16 JS engine and a UTF-8 compiler, so code points are the only neutral currency and callers decode on their own side.
- Enum values matter: `Unitables_Category_Cn = 0` so unassigned/out-of-range code points share the sentinel slot.

## Capacity limits

Every packed field and every published bound is guarded by an assert in
`unitables.py`, so an overflow fails the build instead of corrupting data. Guards
are identified by their message rather than a line number, because line numbers
drift with every edit; grep for the message. Fill levels are for Unicode 17.

| field | guard message | used | on overflow |
| --- | --- | --- | --- |
| `comb_index` (15 bits, sentinel `0x7FFF`) | `combination table exceeds 15-bit comb_index` | 961 / 32766 | widen the bitfield block; 8 of its 32 bits are spare |
| `comb_length` (8 bits) | `comb_length exceeds 8 bits` | 19 / 255 | same block, same spare bits |
| `*_seqindex` offset (14 bits) | `sequence storage exceeds the 14-bit seqindex range` | 10221 / 16383 | no cheap fix; the encoding fills all 16 bits, so widening the five struct fields is the only route. Grows with every new UCD mapping source, so this is the limit the roadmap moves |
| `UNITABLES_STAGE1` page number | `stage1 page number exceeds uint16` | 179 / 65535 | unreachable: only 4352 pages exist in the whole code point space |
| `UNITABLES_STAGE2` property index | `property index exceeds uint16` | 7142 / 65535 | widen `UNITABLES_STAGE2` to `uint32_t` |
| `UNITABLES_DECOMPOSE_MAX` | `decomposition exceeds UNITABLES_DECOMPOSE_MAX` | 18 / 18 | raise the macro in `unitables.h` and the literal in the assert together |
| `UNITABLES_CASEFOLD_MAX` | `case folding exceeds UNITABLES_CASEFOLD_MAX` | 3 / 3 | same, for `unitables_casefold` |

The last two rows are full by construction: the macro *is* the measured maximum,
so there is no headroom by design and the assert exists purely to catch a UCD
version that raises it. They matter more than the others because
`unitables_decompose` and `unitables_casefold` take no capacity, so callers size
fixed arrays by these macros and an unnoticed increase becomes a buffer overrun
rather than a truncated result.

These invariants are load-bearing and easy to break by accident:

- Sequence storage offset 0 is reserved (`sequences = [0]` in `unitables.py`), so
  `UNITABLES_SEQ_NONE` (= 0) cannot collide with a real mapping. Removing the pad
  reintroduces the collision, and also makes a zero-initialised
  `Unitables_Properties` claim a decomposition at offset 0.
- The `combination table exceeds 15-bit comb_index` assert hardcodes the bit
  width in its message while the check reads `COMB_NONE`. Change both together.
- `UNITABLES_DECOMPOSE_MAX` and `UNITABLES_CASEFOLD_MAX` live in `unitables.h`
  while the asserts that verify them hold literal `18` and `3`. Change both
  together, the same hazard as `COMB_NONE` above.

## Deliberate divergences from `ref/utf8proc`

The layout follows utf8proc, but these differences are intentional. Do not
"correct" them back toward `ref/`.

- `comb_index` is 15 bits, not 10, and the combination table is asserted.
  utf8proc has no guard on that table at all, so upstream silently truncates
  indices if it grows past 1023.
- `UNITABLES_SEQ_NONE` is 0 with offset 0 reserved. utf8proc uses `UINT16_MAX`,
  which is a legal encoding (offset `0x3FFF` with length code 3), so upstream
  can produce a real mapping that reads as "no mapping".
- `UNITABLES_STAGE1` holds page numbers, not byte offsets into
  `UNITABLES_STAGE2`. utf8proc's byte offsets cap it at 255 distinct pages. The
  shift costs roughly 5% on a microbenchmark of pure lookups, which is accepted
  because callers avoid the lookup for ASCII in the first place.
- `unitables_decompose` takes `Unitables_Decomp_Mode`, not a `uint8_t` flag. A
  byte-wide flag truncated values above 255, so `0x100` selected canonical while
  documented as selecting compatibility.
- `unitables_decompose` and `unitables_casefold` take no buffer capacity.
  utf8proc's `utf8proc_decompose_char` takes one and truncates on overflow; ours
  publishes asserted compile-time bounds instead, so the caller sizes the buffer
  once and the writer never partially fills it.

## Gotchas

- `unitables_data.c` only exists under `bazel-bin` after a build, so a standalone clangd/editor will report `'unitables_data.c' file not found` and downstream "undeclared identifier" errors for `UNITABLES_*`. This is expected; build through Bazel and run `./commands.sh` to refresh `compile_commands.json`.
- `bazel-*` and `compile_commands.json` are git-ignored.
