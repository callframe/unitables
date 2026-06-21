"""Generate unitables_data.c from UnicodeData.txt.

The pipeline is: parse a source file into per-code-point records, build a shared
packed sequences table, deduplicate the property structs, then page-compress the
per-code-point property indices into a two-stage lookup table (mirrors utf8proc's
layout: see ref/utf8proc.c and ref/utf8proc.jl).

Only UnicodeData.txt is processed for now. Additional UCD sources slot in as new
parser functions that augment the existing CharProps records; no other stage has
to change.
"""

import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

# Arguments in expected order:
# - Output directory
# - Path to UnicodeData.txt

if len(sys.argv) != 3:
    print("Usage: python unitables.py <output_dir> <unicode_data_path>")
    sys.exit(1)

OUTPUT_DIR = Path(sys.argv[1])
UNICODE_DATA_PATH = Path(sys.argv[2])

# Output file: unitables_data.c
UNITABLES_DATA_C = OUTPUT_DIR / "unitables_data.c"

MAX_CODEPOINT = 0x110000  # exclusive upper bound of the Unicode code space
PAGE_SIZE = 0x100  # code points per page in the two-stage index
SEQ_NONE = 0xFFFF  # UINT16_MAX: a *_seqindex with no mapping

# Maps a UnicodeData.txt decomposition tag (e.g. "<compat>") to the suffix of the
# matching Unitables_DecompType_* enum constant.
DECOMP_TYPE_SUFFIX = {
    "font": "Font",
    "noBreak": "NoBreak",
    "initial": "Initial",
    "medial": "Medial",
    "final": "Final",
    "isolated": "Isolated",
    "circle": "Circle",
    "super": "Super",
    "sub": "Sub",
    "vertical": "Vertical",
    "wide": "Wide",
    "narrow": "Narrow",
    "small": "Small",
    "square": "Square",
    "fraction": "Fraction",
    "compat": "Compat",
}


@dataclass(slots=True)
class CharProps:
    """Every property of a single code point.

    Fields default to the "unassigned" value so that future UCD sources can
    augment records without disturbing the ones already filled in. Today every
    field is populated from UnicodeData.txt.
    """

    category: str = "Cn"
    combining_class: int = 0
    bidi_class: str = "L"
    decomp_type: Optional[str] = None
    decomp_mapping: Optional[list] = None
    uppercase_mapping: Optional[int] = None
    lowercase_mapping: Optional[int] = None
    titlecase_mapping: Optional[int] = None


def parse_unicode_data(path):
    """Parse UnicodeData.txt into {code_point: CharProps}.

    Expands the "<..., First>" / "<..., Last>" range pairs (e.g. CJK ideographs,
    Hangul syllables, surrogates) into one record per code point.
    """
    records = {}
    lines = path.read_text(encoding="utf-8").splitlines()
    i = 0
    while i < len(lines):
        line = lines[i]
        i += 1
        if not line:
            continue
        fields = line.split(";")
        code = int(fields[0], 16)
        name = fields[1]
        props = _parse_fields(fields)
        if name.endswith(", First>"):
            last_fields = lines[i].split(";")
            i += 1
            assert last_fields[1].endswith(", Last>"), "unmatched range First/Last"
            last_code = int(last_fields[0], 16)
            for cp in range(code, last_code + 1):
                records[cp] = props
        else:
            records[code] = props
    return records


def _parse_hex_or_none(text):
    return int(text, 16) if text else None


def _parse_fields(fields):
    decomp_type, decomp_mapping = _parse_decomposition(fields[5])
    return CharProps(
        category=fields[2],
        combining_class=int(fields[3]),
        bidi_class=fields[4],
        decomp_type=decomp_type,
        decomp_mapping=decomp_mapping,
        uppercase_mapping=_parse_hex_or_none(fields[12]),
        lowercase_mapping=_parse_hex_or_none(fields[13]),
        titlecase_mapping=_parse_hex_or_none(fields[14]),
    )


def _parse_decomposition(field_text):
    """Split a decomposition field into (decomp_type, [code points])."""
    if not field_text:
        return None, None
    tokens = field_text.split()
    decomp_type = None
    if tokens[0].startswith("<"):
        tag = tokens[0][1:-1]  # strip the surrounding < >
        decomp_type = DECOMP_TYPE_SUFFIX[tag]
        tokens = tokens[1:]
    return decomp_type, [int(t, 16) for t in tokens]


def _utf16_encode(seq):
    """Encode a list of UTF-32 code points as a list of UTF-16 code units."""
    out = []
    for cp in seq:
        if cp <= 0xFFFF:
            out.append(cp)
        else:
            c = cp - 0x10000
            out.append(0xD800 + (c >> 10))
            out.append(0xDC00 + (c & 0x3FF))
    return out


class SequenceTable:
    """One shared, deduplicated array of UTF-16 code units.

    encode() returns a uint16 seqindex packing the storage offset in the low 14
    bits and the decoded length in the top 2 bits (len-1, or 3 meaning "the
    length is stored as the first unit of the sequence"). SEQ_NONE marks the
    absence of a mapping.
    """

    def __init__(self):
        self.storage = []
        self._indices = {}

    def encode(self, seq):
        if not seq:
            return SEQ_NONE
        lencode = len(seq) - 1
        units = _utf16_encode(seq)
        key = tuple(units)
        idx = self._indices.get(key)
        if idx is None:
            idx = len(self.storage)
            if lencode >= 3:
                self.storage.append(lencode)
            self.storage.extend(units)
            self._indices[key] = idx
        assert idx <= 0x3FFF, "sequence storage exceeds the 14-bit seqindex range"
        return idx | (min(lencode, 3) << 14)


def _seq(seq_table, mapping):
    """Encode a single optional code point or list of code points."""
    if mapping is None:
        return SEQ_NONE
    if isinstance(mapping, int):
        mapping = [mapping]
    return seq_table.encode(mapping)


def build_tables(records):
    """Turn parsed records into the four C tables.

    Returns (sequences, stage1, stage2, properties) where properties[0] is the
    shared unassigned/out-of-range sentinel.
    """
    seq_table = SequenceTable()

    # Sentinel for unassigned and out-of-range code points.
    sentinel = ("Unitables_Category_Cn", 0, "0", "0", SEQ_NONE, SEQ_NONE, SEQ_NONE, SEQ_NONE)
    properties = [sentinel]
    index_map = {sentinel: 0}

    char_property_indices = [0] * MAX_CODEPOINT
    for code in sorted(records):
        cp = records[code]
        entry = (
            "Unitables_Category_" + cp.category,
            cp.combining_class,
            "Unitables_BidiClass_" + cp.bidi_class,
            "Unitables_DecompType_" + cp.decomp_type if cp.decomp_type else "0",
            _seq(seq_table, cp.decomp_mapping),
            _seq(seq_table, cp.uppercase_mapping),
            _seq(seq_table, cp.lowercase_mapping),
            _seq(seq_table, cp.titlecase_mapping),
        )
        idx = index_map.get(entry)
        if idx is None:
            idx = len(properties)
            properties.append(entry)
            index_map[entry] = idx
        char_property_indices[code] = idx

    stage1, stage2 = _page_compress(char_property_indices)

    assert max(stage1) <= 0xFFFF, "stage1 offset exceeds uint16"
    assert max(stage2) <= 0xFFFF, "property index exceeds uint16"

    return seq_table.storage, stage1, stage2, properties


def _page_compress(char_property_indices):
    """Split the per-code-point index array into PAGE_SIZE pages and dedup them.

    stage1[cp >> 8] gives the base offset of cp's page within stage2, so the
    final lookup is stage2[stage1[cp >> 8] + (cp & 0xFF)].
    """
    stage1 = []
    stage2 = []
    page_map = {}
    for start in range(0, MAX_CODEPOINT, PAGE_SIZE):
        page = tuple(char_property_indices[start:start + PAGE_SIZE])
        base = page_map.get(page)
        if base is None:
            base = len(stage2)
            stage2.extend(page)
            page_map[page] = base
        stage1.append(base)
    return stage1, stage2


def _format_uint16_array(name, values, per_line=12):
    out = [f"static uint16_t const {name}[] = {{"]
    for start in range(0, len(values), per_line):
        chunk = values[start:start + per_line]
        out.append("  " + ", ".join(str(v) for v in chunk) + ",")
    out.append("};")
    return "\n".join(out)


def _format_seq(value):
    return "UINT16_MAX" if value == SEQ_NONE else str(value)


def _format_properties(properties):
    out = ["static struct Unitables_Properties const UNITABLES_PROPERTIES[] = {"]
    for cat, ccc, bidi, decomp, dseq, useq, lseq, tseq in properties:
        out.append(
            "  { %s, %d, %s, %s, %s, %s, %s, %s },"
            % (cat, ccc, bidi, decomp,
               _format_seq(dseq), _format_seq(useq), _format_seq(lseq), _format_seq(tseq))
        )
    out.append("};")
    return "\n".join(out)


def emit(path, sequences, stage1, stage2, properties):
    blocks = [
        "/* Generated by unitables.py from UnicodeData.txt. Do not edit. */",
        _format_uint16_array("UNITABLES_SEQUENCES", sequences),
        _format_uint16_array("UNITABLES_STAGE1", stage1),
        _format_uint16_array("UNITABLES_STAGE2", stage2),
        _format_properties(properties),
    ]
    path.write_text("\n\n".join(blocks) + "\n", encoding="utf-8")


def main():
    records = parse_unicode_data(UNICODE_DATA_PATH)
    sequences, stage1, stage2, properties = build_tables(records)
    emit(UNITABLES_DATA_C, sequences, stage1, stage2, properties)


if __name__ == "__main__":
    main()
