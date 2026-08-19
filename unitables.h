#pragma once

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef int32_t Unitables_Codepoint;

typedef uint8_t Unitables_Category;
enum
{
  Unitables_Category_Cn = 0,
  Unitables_Category_Lu,
  Unitables_Category_Ll,
  Unitables_Category_Lt,
  Unitables_Category_Lm,
  Unitables_Category_Lo,
  Unitables_Category_Mn,
  Unitables_Category_Mc,
  Unitables_Category_Me,
  Unitables_Category_Nd,
  Unitables_Category_Nl,
  Unitables_Category_No,
  Unitables_Category_Pc,
  Unitables_Category_Pd,
  Unitables_Category_Ps,
  Unitables_Category_Pe,
  Unitables_Category_Pi,
  Unitables_Category_Pf,
  Unitables_Category_Po,
  Unitables_Category_Sm,
  Unitables_Category_Sc,
  Unitables_Category_Sk,
  Unitables_Category_So,
  Unitables_Category_Zs,
  Unitables_Category_Zl,
  Unitables_Category_Zp,
  Unitables_Category_Cc,
  Unitables_Category_Cf,
  Unitables_Category_Cs,
  Unitables_Category_Co
};

typedef uint8_t Unitables_Bidi_Class;
enum
{
  Unitables_Bidi_Class_L = 1,
  Unitables_Bidi_Class_LRE,
  Unitables_Bidi_Class_LRO,
  Unitables_Bidi_Class_R,
  Unitables_Bidi_Class_AL,
  Unitables_Bidi_Class_RLE,
  Unitables_Bidi_Class_RLO,
  Unitables_Bidi_Class_PDF,
  Unitables_Bidi_Class_EN,
  Unitables_Bidi_Class_ES,
  Unitables_Bidi_Class_ET,
  Unitables_Bidi_Class_AN,
  Unitables_Bidi_Class_CS,
  Unitables_Bidi_Class_NSM,
  Unitables_Bidi_Class_BN,
  Unitables_Bidi_Class_B,
  Unitables_Bidi_Class_S,
  Unitables_Bidi_Class_WS,
  Unitables_Bidi_Class_ON,
  Unitables_Bidi_Class_LRI,
  Unitables_Bidi_Class_RLI,
  Unitables_Bidi_Class_FSI,
  Unitables_Bidi_Class_PDI
};

typedef uint8_t Unitables_Decomp_Type;
enum
{
  Unitables_Decomp_Type_Font = 1,
  Unitables_Decomp_Type_NoBreak,
  Unitables_Decomp_Type_Initial,
  Unitables_Decomp_Type_Medial,
  Unitables_Decomp_Type_Final,
  Unitables_Decomp_Type_Isolated,
  Unitables_Decomp_Type_Circle,
  Unitables_Decomp_Type_Super,
  Unitables_Decomp_Type_Sub,
  Unitables_Decomp_Type_Vertical,
  Unitables_Decomp_Type_Wide,
  Unitables_Decomp_Type_Narrow,
  Unitables_Decomp_Type_Small,
  Unitables_Decomp_Type_Square,
  Unitables_Decomp_Type_Fraction,
  Unitables_Decomp_Type_Compat,
};

/** Which decomposition unitables_decompose applies. A full word, not a byte: a
byte-wide flag silently truncated 0x100 to canonical. */
typedef uint32_t Unitables_Decomp_Mode;
enum
{
  Unitables_Decomp_Mode_Canonical = 0,
  Unitables_Decomp_Mode_Compatibility
};

typedef uint8_t Unitables_Bound_Class;
enum
{
  Unitables_Bound_Class_Start = 0,
  Unitables_Bound_Class_Other,
  Unitables_Bound_Class_CR,
  Unitables_Bound_Class_LF,
  Unitables_Bound_Class_Control,
  Unitables_Bound_Class_Extend,
  Unitables_Bound_Class_L,
  Unitables_Bound_Class_V,
  Unitables_Bound_Class_T,
  Unitables_Bound_Class_LV,
  Unitables_Bound_Class_LVT,
  Unitables_Bound_Class_Regional_Indicator,
  Unitables_Bound_Class_SpacingMark,
  Unitables_Bound_Class_Prepend,
  Unitables_Bound_Class_ZWJ,
  Unitables_Bound_Class_Extended_Pictographic,
  Unitables_Bound_Class_E_ZWG
};

typedef uint8_t Unitables_Indic_Conjunct_Break;
enum
{
  Unitables_Indic_Conjunct_Break_None = 0,
  Unitables_Indic_Conjunct_Break_Linker,
  Unitables_Indic_Conjunct_Break_Consonant,
  Unitables_Indic_Conjunct_Break_Extend
};

/** A *_seqindex with no mapping. Sequence offset 0 is reserved, so no real
mapping encodes to this value. */
#define UNITABLES_SEQ_NONE 0
/** A comb_index that cannot begin a combining pair. */
#define UNITABLES_COMB_NONE 0x7FFF
/** An invalid code point. */
#define UNITABLES_INVALID_CODEPOINT INT32_C(-1)

/** Longest decomposition of one code point (U+FDFA, compatibility). */
#define UNITABLES_DECOMPOSE_MAX 18
/** Longest full case folding of one code point (U+0390). */
#define UNITABLES_CASEFOLD_MAX 3

/** Properties of one code point, drawn from UnicodeData.txt,
CompositionExclusions.txt, CaseFolding.txt, GraphemeBreakProperty.txt,
emoji-data.txt, and DerivedCoreProperties.txt. */
struct Unitables_Properties
{
  /** Kind of character, how it combines, how it behaves in bidirectional
  text. */
  Unitables_Category category;
  uint8_t combining_class;
  Unitables_Bidi_Class bidi_class;

  /** How this code point decomposes. */
  Unitables_Decomp_Type decomp_type;
  uint16_t decomp_seqindex;

  /** How this code point maps under case folding and case conversion. */
  uint16_t casefold_seqindex;
  uint16_t uppercase_seqindex;
  uint16_t lowercase_seqindex;
  uint16_t titlecase_seqindex;

  /** Canonical composition. comb_index/comb_length locate this code point's
  entries in the combination table, or comb_index is UNITABLES_COMB_NONE if it
  cannot begin a pair; comb_issecond marks one that can be the second of a pair.
  24 of the block's 32 bits are used, so a field here can grow for free. */
  uint32_t comb_index : 15;
  uint32_t comb_length : 8;
  uint32_t comb_issecond : 1;

  /** Grapheme cluster boundary class (UAX #29). */
  Unitables_Bound_Class bound_class;
  Unitables_Indic_Conjunct_Break indic_conjunct_break;
};

/** Properties of codepoint. Invalid, out-of-range, and unassigned code points
share a sentinel whose category is Unitables_Category_Cn.
@return Static storage, valid for the lifetime of the program. */
struct Unitables_Properties const* unitables_properties(
    Unitables_Codepoint codepoint);

/** Decomposes codepoint into dst, recursing and expanding Hangul syllables
algorithmically. A code point with no decomposition yields itself.
@param dst Room for UNITABLES_DECOMPOSE_MAX code points.
@return Code points written, at most UNITABLES_DECOMPOSE_MAX. */
uint32_t unitables_decompose(Unitables_Codepoint codepoint,
                             Unitables_Decomp_Mode mode,
                             Unitables_Codepoint* dst);

/** Canonically composes a pair, Hangul included.
@return The composite, or UNITABLES_INVALID_CODEPOINT if they do not compose. */
Unitables_Codepoint unitables_compose(Unitables_Codepoint first,
                                      Unitables_Codepoint second);

/** Writes the default full case folding of codepoint into dst. A code point
with no mapping yields itself. Folds one code point; does not process strings
and does not apply Turkic rules.
@param dst Room for UNITABLES_CASEFOLD_MAX code points.
@return Code points written, at most UNITABLES_CASEFOLD_MAX. */
uint32_t unitables_casefold(Unitables_Codepoint codepoint,
                            Unitables_Codepoint* dst);

/** Whether a grapheme cluster break is permitted between two consecutive code
points (UAX #29 extended grapheme clusters).
@param state A uint32_t zeroed at the start of the string, carrying the context
for GB9c/GB11/GB12/GB13, or NULL to skip those rules. */
uint8_t unitables_grapheme_break(Unitables_Codepoint codepoint1,
                                 Unitables_Codepoint codepoint2,
                                 uint32_t* state);

#if defined(__cplusplus)
}
#endif
