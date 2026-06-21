"""Generate unitables_data.c from the Unicode Character Database.

PROCESS blocks read a UCD file into a table keyed by code point; PRODUCE blocks
combine those into the emitted C arrays. Layout follows utf8proc (see ref/).
"""

import sys
from collections import namedtuple
from pathlib import Path

if len(sys.argv) != 4:
    sys.exit("Usage: python unitables.py <output_dir> <unicode_data> <composition_exclusions>")

out_path = Path(sys.argv[1]) / "unitables_data.c"
unicode_data_path = Path(sys.argv[2])
composition_exclusions_path = Path(sys.argv[3])

MAX_CODEPOINT = 0x110000
PAGE_SIZE = 0x100
SEQ_NONE = 0xFFFF  # UINT16_MAX: a *_seqindex with no mapping
COMB_NONE = 0x3FF  # comb_index value meaning "cannot begin a combining pair"


def intern(store, seen, key, block):
    offset = seen.get(key)
    if offset is None:
        offset = len(store)
        store.extend(block)
        seen[key] = offset
    return offset


# =============================================================================
# PROCESS  UnicodeData.txt
# =============================================================================

UnicodeRecord = namedtuple(
    "UnicodeRecord",
    "category combining_class bidi_class decomp_type decomp "
    "uppercase lowercase titlecase",
)

unicode_data = {}


def decomp_type_enum(tag):
    suffix = "NoBreak" if tag == "noBreak" else tag.capitalize()
    return "Unitables_DecompType_" + suffix


def parse_decomposition(text):
    if not text:
        return "0", None
    parts = text.split()
    if parts[0].startswith("<"):
        return decomp_type_enum(parts[0][1:-1]), [int(p, 16) for p in parts[1:]]
    return "0", [int(p, 16) for p in parts]


def parse_mapping(text):
    return [int(text, 16)] if text else None


def parse_unicode_record(fields):
    decomp_type, decomp = parse_decomposition(fields[5])
    return UnicodeRecord(
        category="Unitables_Category_" + fields[2],
        combining_class=int(fields[3]),
        bidi_class="Unitables_BidiClass_" + fields[4],
        decomp_type=decomp_type,
        decomp=decomp,
        uppercase=parse_mapping(fields[12]),
        lowercase=parse_mapping(fields[13]),
        titlecase=parse_mapping(fields[14]),
    )


lines = unicode_data_path.read_text(encoding="utf-8").splitlines()
i = 0
while i < len(lines):
    fields = lines[i].split(";")
    i += 1
    code, name = int(fields[0], 16), fields[1]
    record = parse_unicode_record(fields)
    if name.endswith(", First>"):
        last = int(lines[i].split(";")[0], 16)
        i += 1
        for cp in range(code, last + 1):
            unicode_data[cp] = record
    else:
        unicode_data[code] = record


# =============================================================================
# PROCESS  CompositionExclusions.txt
# =============================================================================

composition_exclusions = set()
for line in composition_exclusions_path.read_text(encoding="utf-8").splitlines():
    line = line.split("#")[0].strip()
    if line:
        composition_exclusions.add(int(line, 16))


# =============================================================================
# PRODUCE  UNITABLES_COMBINATIONS_SECOND + UNITABLES_COMBINATIONS_COMBINED
# =============================================================================
# Invert the canonical two-code-point decompositions (starter first, not
# excluded) into a table grouped by first code point and sorted by second.

combinations = {}
for code in sorted(unicode_data):
    rec = unicode_data[code]
    if (rec.decomp_type == "0" and rec.decomp and len(rec.decomp) == 2
            and unicode_data[rec.decomp[0]].combining_class == 0
            and code not in composition_exclusions):
        first, second = rec.decomp
        combinations.setdefault(first, {})[second] = code

combinations_second = []
combinations_combined = []
comb_index = {}     # first cp -> start offset in the combination arrays
comb_length = {}    # first cp -> number of entries
comb_issecond = set()
for first in sorted(combinations):
    comb_index[first] = len(combinations_second)
    comb_length[first] = len(combinations[first])
    for second in sorted(combinations[first]):
        combinations_second.append(second)
        combinations_combined.append(combinations[first][second])
        comb_issecond.add(second)

assert len(combinations_second) < COMB_NONE, "combination table exceeds 10-bit comb_index"
assert max(comb_length.values()) <= 0x1F, "comb_length exceeds 5 bits"


# =============================================================================
# PRODUCE  UNITABLES_SEQUENCES + UNITABLES_PROPERTIES
# =============================================================================

sequences = []
sequence_offsets = {}


def encode_sequence(mapping):
    if mapping is None:
        return SEQ_NONE
    units = []
    for cp in mapping:
        if cp <= 0xFFFF:
            units.append(cp)
        else:
            cp -= 0x10000
            units += [0xD800 + (cp >> 10), 0xDC00 + (cp & 0x3FF)]
    length = len(mapping) - 1
    # seqindex = offset (low 14 bits) | length-1 (top 2 bits; 3 = length stored inline).
    block = [length, *units] if length >= 3 else units
    offset = intern(sequences, sequence_offsets, tuple(units), block)
    assert offset <= 0x3FFF, "sequence storage exceeds the 14-bit seqindex range"
    return offset | (min(length, 3) << 14)


SENTINEL = (
    "Unitables_Category_Cn", 0, "0", "0",
    SEQ_NONE, SEQ_NONE, SEQ_NONE, SEQ_NONE,
    COMB_NONE, 0, 0,
)
properties = [SENTINEL]
property_indices = {SENTINEL: 0}
char_index = [0] * MAX_CODEPOINT

for code in sorted(unicode_data):
    rec = unicode_data[code]
    entry = (
        rec.category,
        rec.combining_class,
        rec.bidi_class,
        rec.decomp_type,
        encode_sequence(rec.decomp),
        encode_sequence(rec.uppercase),
        encode_sequence(rec.lowercase),
        encode_sequence(rec.titlecase),
        comb_index.get(code, COMB_NONE),
        comb_length.get(code, 0),
        1 if code in comb_issecond else 0,
    )
    char_index[code] = intern(properties, property_indices, entry, [entry])


# =============================================================================
# PRODUCE  UNITABLES_STAGE1 + UNITABLES_STAGE2
# =============================================================================
# lookup: stage2[stage1[cp >> 8] + (cp & 0xFF)]

stage1 = []
stage2 = []
page_offsets = {}
for start in range(0, MAX_CODEPOINT, PAGE_SIZE):
    page = tuple(char_index[start : start + PAGE_SIZE])
    stage1.append(intern(stage2, page_offsets, page, page))

assert max(stage1) <= 0xFFFF, "stage1 offset exceeds uint16"
assert max(stage2) <= 0xFFFF, "property index exceeds uint16"


# =============================================================================
# EMIT  unitables_data.c
# =============================================================================


def format_seq(value):
    return "UNITABLES_SEQ_NONE" if value == SEQ_NONE else str(value)


def c_array(ctype, name, values):
    rows = [", ".join(map(str, values[i : i + 12])) for i in range(0, len(values), 12)]
    return "static %s const %s[] = {\n  %s,\n};" % (ctype, name, ",\n  ".join(rows))


def property_row(entry):
    category, combining_class, bidi_class, decomp_type = entry[:4]
    dseq, useq, lseq, tseq = entry[4:8]
    comb_idx, comb_len, comb_2nd = entry[8:]
    comb_idx = "UNITABLES_COMB_NONE" if comb_idx == COMB_NONE else str(comb_idx)
    fields = [
        category, str(combining_class), bidi_class, decomp_type,
        format_seq(dseq), format_seq(useq), format_seq(lseq), format_seq(tseq),
        comb_idx, str(comb_len), str(comb_2nd),
    ]
    return "  { %s }," % ", ".join(fields)


properties_block = (
    "static struct Unitables_Properties const UNITABLES_PROPERTIES[] = {\n"
    + "\n".join(property_row(e) for e in properties)
    + "\n};"
)

blocks = [
    "/* Generated by unitables.py from UnicodeData.txt and "
    "CompositionExclusions.txt. Do not edit. */",
    c_array("uint16_t", "UNITABLES_SEQUENCES", sequences),
    c_array("uint16_t", "UNITABLES_STAGE1", stage1),
    c_array("uint16_t", "UNITABLES_STAGE2", stage2),
    c_array("int32_t", "UNITABLES_COMBINATIONS_SECOND", combinations_second),
    c_array("int32_t", "UNITABLES_COMBINATIONS_COMBINED", combinations_combined),
    properties_block,
]
out_path.write_text("\n\n".join(blocks) + "\n", encoding="utf-8")
