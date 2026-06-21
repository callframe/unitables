"""Generate unitables_data.c from the Unicode Character Database.

Two phases, marked by the banners below:

  PROCESS <file>   read one UCD file into its own table keyed by code point.
                   Adding a UCD file is adding one PROCESS block.

  PRODUCE <table>  combine the processed tables into one emitted C array.
                   Adding a property is one field here plus one in unitables.h.

Same on-disk layout as utf8proc (see ref/utf8proc.c and ref/utf8proc.jl).
"""

import sys
from collections import namedtuple
from pathlib import Path

if len(sys.argv) != 3:
    sys.exit("Usage: python unitables.py <output_dir> <unicode_data_path>")

out_path = Path(sys.argv[1]) / "unitables_data.c"
unicode_data_path = Path(sys.argv[2])

MAX_CODEPOINT = 0x110000  # exclusive end of the Unicode code space
PAGE_SIZE = 0x100  # code points per page in the two-stage index
SEQ_NONE = 0xFFFF  # UINT16_MAX: a *_seqindex with no mapping


def intern(store, seen, key, block):
    """Append block to store the first time key is seen; return its start offset.

    The single deduplication primitive shared by every PRODUCE block below.
    """
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

unicode_data = {}  # code point -> UnicodeRecord


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
    if name.endswith(", First>"):  # range pair: fill through the ", Last>" line
        last = int(lines[i].split(";")[0], 16)
        i += 1
        for cp in range(code, last + 1):
            unicode_data[cp] = record
    else:
        unicode_data[code] = record


# =============================================================================
# PRODUCE  UNITABLES_SEQUENCES + UNITABLES_PROPERTIES
# =============================================================================
# All decomposition/case mappings share one deduplicated UTF-16 array. A seqindex
# packs the storage offset (low 14 bits) and the decoded length-1 (top 2 bits; 3
# means the length is stored inline as the first unit). BMP code points take one
# unit, non-BMP a surrogate pair. Property structs are deduplicated too; index 0
# is the unassigned/out-of-range sentinel.

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
    block = [length, *units] if length >= 3 else units
    offset = intern(sequences, sequence_offsets, tuple(units), block)
    assert offset <= 0x3FFF, "sequence storage exceeds the 14-bit seqindex range"
    return offset | (min(length, 3) << 14)


SENTINEL = ("Unitables_Category_Cn", 0, "0", "0", SEQ_NONE, SEQ_NONE, SEQ_NONE, SEQ_NONE)
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
    )
    char_index[code] = intern(properties, property_indices, entry, [entry])


# =============================================================================
# PRODUCE  UNITABLES_STAGE1 + UNITABLES_STAGE2
# =============================================================================
# Page-compress char_index. stage1[cp>>8] is the base of cp's page within stage2,
# so the runtime lookup is stage2[stage1[cp>>8] + (cp & 0xFF)].

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
    return "UINT16_MAX" if value == SEQ_NONE else str(value)


def uint16_array(name, values):
    rows = [", ".join(map(str, values[i : i + 12])) for i in range(0, len(values), 12)]
    return "static uint16_t const %s[] = {\n  %s,\n};" % (name, ",\n  ".join(rows))


def property_row(entry):
    category, combining_class, bidi_class, decomp_type, *seqs = entry
    fields = [category, str(combining_class), bidi_class, decomp_type]
    fields += [format_seq(s) for s in seqs]
    return "  { %s }," % ", ".join(fields)


properties_block = (
    "static struct Unitables_Properties const UNITABLES_PROPERTIES[] = {\n"
    + "\n".join(property_row(e) for e in properties)
    + "\n};"
)

blocks = [
    "/* Generated by unitables.py from UnicodeData.txt. Do not edit. */",
    uint16_array("UNITABLES_SEQUENCES", sequences),
    uint16_array("UNITABLES_STAGE1", stage1),
    uint16_array("UNITABLES_STAGE2", stage2),
    properties_block,
]
out_path.write_text("\n\n".join(blocks) + "\n", encoding="utf-8")
