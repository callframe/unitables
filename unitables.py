"""Generate unitables_data.c from the Unicode Character Database.

PROCESS blocks read a UCD file into a table keyed by code point; PRODUCE blocks
combine those into the emitted C arrays. Layout follows utf8proc (see ref/).
"""

import argparse
from collections import namedtuple
from pathlib import Path

parser = argparse.ArgumentParser(
    description="Generate unitables_data.c from the Unicode Character Database."
)
parser.add_argument(
    "output_dir", type=Path, help="directory to write unitables_data.c into"
)
parser.add_argument("--unicode-data", required=True, type=Path)
parser.add_argument("--composition-exclusions", required=True, type=Path)
parser.add_argument("--case-folding", required=True, type=Path)
parser.add_argument("--grapheme-break-property", required=True, type=Path)
parser.add_argument("--emoji-data", required=True, type=Path)
parser.add_argument("--derived-core-properties", required=True, type=Path)
args = parser.parse_args()

MAX_CODEPOINT = 0x110000
PAGE_SIZE = 0x100
SEQ_NONE = 0  # a *_seqindex with no mapping; sequence offset 0 is reserved
COMB_NONE = 0x7FFF  # comb_index value meaning "cannot begin a combining pair"


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
    return "Unitables_Decomp_Type_" + suffix


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
        bidi_class="Unitables_Bidi_Class_" + fields[4],
        decomp_type=decomp_type,
        decomp=decomp,
        uppercase=parse_mapping(fields[12]),
        lowercase=parse_mapping(fields[13]),
        titlecase=parse_mapping(fields[14]),
    )


lines = args.unicode_data.read_text(encoding="utf-8").splitlines()
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


# Hangul syllables decompose algorithmically; these mirror unitables.c.
HANGUL_SBASE = 0xAC00
HANGUL_LBASE = 0x1100
HANGUL_VBASE = 0x1161
HANGUL_TBASE = 0x11A7
HANGUL_TCOUNT = 28
HANGUL_NCOUNT = 588
HANGUL_SCOUNT = 11172

full_decompositions = {}


def full_decomposition(code, compat):
    """Expand code the way unitables_decompose_into does, recursively."""
    cached = full_decompositions.get((code, compat))
    if cached is not None:
        return cached

    syllable = code - HANGUL_SBASE
    if 0 <= syllable < HANGUL_SCOUNT:
        result = [
            HANGUL_LBASE + syllable // HANGUL_NCOUNT,
            HANGUL_VBASE + (syllable % HANGUL_NCOUNT) // HANGUL_TCOUNT,
        ]
        if syllable % HANGUL_TCOUNT:
            result.append(HANGUL_TBASE + syllable % HANGUL_TCOUNT)
    else:
        rec = unicode_data[code]
        if not rec.decomp or (rec.decomp_type != "0" and not compat):
            result = [code]
        else:
            result = []
            for part in rec.decomp:
                result += full_decomposition(part, compat)

    full_decompositions[(code, compat)] = result
    return result


# Callers size fixed buffers by UNITABLES_DECOMPOSE_MAX, and unitables_decompose
# no longer takes a capacity, so outgrowing it must fail the build.
assert max(len(full_decomposition(c, True)) for c in unicode_data) <= 18, (
    "decomposition exceeds UNITABLES_DECOMPOSE_MAX"
)


# =============================================================================
# PROCESS  CompositionExclusions.txt
# =============================================================================

composition_exclusions = set()
for line in args.composition_exclusions.read_text(encoding="utf-8").splitlines():
    line = line.split("#")[0].strip()
    if line:
        composition_exclusions.add(int(line, 16))


# =============================================================================
# PROCESS  CaseFolding.txt
# =============================================================================


def parse_ucd(path):
    """Parse a UCD semicolon-delimited file. Yields (first, last, fields)
    where fields is a list of stripped strings after the code point range."""
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.split("#")[0].strip()
        if not line:
            continue
        fields = [f.strip() for f in line.split(";")]
        rng = fields[0]
        if ".." in rng:
            first, last = rng.split("..")
            first, last = int(first, 16), int(last, 16)
        else:
            first = last = int(rng, 16)
        yield first, last, fields[1:]


casefold = {}
for first, last, fields in parse_ucd(args.case_folding):
    if fields[0] in ("C", "F"):
        mapping = [int(part, 16) for part in fields[1].split()]
        for cp in range(first, last + 1):
            casefold[cp] = mapping

# Same contract as UNITABLES_DECOMPOSE_MAX above.
assert max(len(v) for v in casefold.values()) <= 3, (
    "case folding exceeds UNITABLES_CASEFOLD_MAX"
)


# =============================================================================
# PROCESS  GraphemeBreakProperty.txt + emoji-data.txt
# =============================================================================

BOUND_CLASS_MAP = {
    "OTHER": "Unitables_Bound_Class_Other",
    "CR": "Unitables_Bound_Class_CR",
    "LF": "Unitables_Bound_Class_LF",
    "CONTROL": "Unitables_Bound_Class_Control",
    "EXTEND": "Unitables_Bound_Class_Extend",
    "L": "Unitables_Bound_Class_L",
    "V": "Unitables_Bound_Class_V",
    "T": "Unitables_Bound_Class_T",
    "LV": "Unitables_Bound_Class_LV",
    "LVT": "Unitables_Bound_Class_LVT",
    "REGIONAL_INDICATOR": "Unitables_Bound_Class_Regional_Indicator",
    "SPACINGMARK": "Unitables_Bound_Class_SpacingMark",
    "PREPEND": "Unitables_Bound_Class_Prepend",
    "ZWJ": "Unitables_Bound_Class_ZWJ",
    "EXTENDED_PICTOGRAPHIC": "Unitables_Bound_Class_Extended_Pictographic",
}

grapheme_bound_class = {}

for first, last, fields in parse_ucd(args.grapheme_break_property):
    key = fields[0].upper().replace(" ", "")
    for cp in range(first, last + 1):
        grapheme_bound_class[cp] = BOUND_CLASS_MAP[key]

for first, last, fields in parse_ucd(args.emoji_data):
    if fields[0] == "Extended_Pictographic":
        for cp in range(first, last + 1):
            grapheme_bound_class[cp] = "Unitables_Bound_Class_Extended_Pictographic"
    elif fields[0] == "Emoji_Modifier":
        for cp in range(first, last + 1):
            grapheme_bound_class[cp] = "Unitables_Bound_Class_Extend"


# =============================================================================
# PROCESS  DerivedCoreProperties.txt  (InCB = Indic_Conjunct_Break)
# =============================================================================

INDIC_CONJUNCT_BREAK_MAP = {
    "Linker": "Unitables_Indic_Conjunct_Break_Linker",
    "Consonant": "Unitables_Indic_Conjunct_Break_Consonant",
    "Extend": "Unitables_Indic_Conjunct_Break_Extend",
}

indic_conjunct_break = {}

for first, last, fields in parse_ucd(args.derived_core_properties):
    if (
        len(fields) >= 2
        and fields[0] == "InCB"
        and fields[1] in INDIC_CONJUNCT_BREAK_MAP
    ):
        for cp in range(first, last + 1):
            indic_conjunct_break[cp] = INDIC_CONJUNCT_BREAK_MAP[fields[1]]


# =============================================================================
# PRODUCE  UNITABLES_COMBINATIONS_SECOND + UNITABLES_COMBINATIONS_COMBINED
# =============================================================================
# Invert the canonical two-code-point decompositions (starter first, not
# excluded) into a table grouped by first code point and sorted by second.

combinations = {}
for code in sorted(unicode_data):
    rec = unicode_data[code]
    if (
        rec.decomp_type == "0"
        and rec.decomp
        and len(rec.decomp) == 2
        and unicode_data[rec.decomp[0]].combining_class == 0
        and code not in composition_exclusions
    ):
        first, second = rec.decomp
        combinations.setdefault(first, {})[second] = code

combinations_second = []
combinations_combined = []
comb_index = {}  # first cp -> start offset in the combination arrays
comb_length = {}  # first cp -> number of entries
comb_issecond = set()
for first in sorted(combinations):
    comb_index[first] = len(combinations_second)
    comb_length[first] = len(combinations[first])
    for second in sorted(combinations[first]):
        combinations_second.append(second)
        combinations_combined.append(combinations[first][second])
        comb_issecond.add(second)

assert len(combinations_second) < COMB_NONE, (
    "combination table exceeds 15-bit comb_index"
)
assert max(comb_length.values()) <= 0xFF, "comb_length exceeds 8 bits"


# =============================================================================
# PRODUCE  UNITABLES_SEQUENCES + UNITABLES_PROPERTIES
# =============================================================================

sequences = [0]  # offset 0 is reserved so no real mapping encodes as SEQ_NONE
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
    "Unitables_Category_Cn",
    0,
    "0",
    "0",
    SEQ_NONE,
    SEQ_NONE,
    SEQ_NONE,
    SEQ_NONE,
    SEQ_NONE,
    COMB_NONE,
    0,
    0,
    "Unitables_Bound_Class_Other",
    "Unitables_Indic_Conjunct_Break_None",
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
        encode_sequence(casefold.get(code)),
        encode_sequence(rec.uppercase),
        encode_sequence(rec.lowercase),
        encode_sequence(rec.titlecase),
        comb_index.get(code, COMB_NONE),
        comb_length.get(code, 0),
        1 if code in comb_issecond else 0,
        grapheme_bound_class.get(code, "Unitables_Bound_Class_Other"),
        indic_conjunct_break.get(code, "Unitables_Indic_Conjunct_Break_None"),
    )
    char_index[code] = intern(properties, property_indices, entry, [entry])


# =============================================================================
# PRODUCE  UNITABLES_STAGE1 + UNITABLES_STAGE2
# =============================================================================
# lookup: stage2[(stage1[cp >> 8] << 8) + (cp & 0xFF)]

stage1 = []
stage2 = []
page_offsets = {}
for start in range(0, MAX_CODEPOINT, PAGE_SIZE):
    page = tuple(char_index[start : start + PAGE_SIZE])
    stage1.append(intern(stage2, page_offsets, page, page) // PAGE_SIZE)

assert max(stage1) <= 0xFFFF, "stage1 page number exceeds uint16"
assert max(stage2) <= 0xFFFF, "property index exceeds uint16"


# =============================================================================
# EMIT  unitables_data.c
# =============================================================================


def format_seq(value):
    return "UNITABLES_SEQ_NONE" if value == SEQ_NONE else str(value)


def c_array(ctype, name, values):
    rows = [", ".join(map(str, values[i : i + 12])) for i in range(0, len(values), 12)]
    return "static {} const {}[] = {{\n  {},\n}};".format(
        ctype, name, ",\n  ".join(rows)
    )


def property_row(entry):
    category, combining_class, bidi_class, decomp_type = entry[:4]
    dseq, cfseq, useq, lseq, tseq = entry[4:9]
    comb_idx, comb_len, comb_2nd = entry[9:12]
    bound_class, icb = entry[12:14]
    comb_idx = "UNITABLES_COMB_NONE" if comb_idx == COMB_NONE else str(comb_idx)
    fields = [
        category,
        str(combining_class),
        bidi_class,
        decomp_type,
        format_seq(dseq),
        format_seq(cfseq),
        format_seq(useq),
        format_seq(lseq),
        format_seq(tseq),
        comb_idx,
        str(comb_len),
        str(comb_2nd),
        bound_class,
        icb,
    ]
    return "  {{ {} }},".format(", ".join(fields))


properties_block = (
    "static struct Unitables_Properties const UNITABLES_PROPERTIES[] = {\n"
    + "\n".join(property_row(e) for e in properties)
    + "\n};"
)

blocks = [
    (
        "/* Generated by unitables.py from UnicodeData.txt, "
        "CompositionExclusions.txt, CaseFolding.txt, GraphemeBreakProperty.txt, "
        "emoji-data.txt, and DerivedCoreProperties.txt. Do not edit. */"
    ),
    c_array("uint16_t", "UNITABLES_SEQUENCES", sequences),
    c_array("uint16_t", "UNITABLES_STAGE1", stage1),
    c_array("uint16_t", "UNITABLES_STAGE2", stage2),
    c_array("int32_t", "UNITABLES_COMBINATIONS_SECOND", combinations_second),
    c_array("int32_t", "UNITABLES_COMBINATIONS_COMBINED", combinations_combined),
    properties_block,
]
(args.output_dir / "unitables_data.c").write_text(
    "\n\n".join(blocks) + "\n", encoding="utf-8"
)
