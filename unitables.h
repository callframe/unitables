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

/* Selects which decomposition unitables_decompose applies. Not stored in
Unitables_Properties, so this is a full word rather than a byte. */
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

/* Value of any *_seqindex field when the code point has no such mapping.
Sequence offset 0 is reserved, so no real mapping encodes to this value. */
#define UNITABLES_SEQ_NONE 0
/* Value of comb_index when the code point cannot begin a combining pair. */
#define UNITABLES_COMB_NONE 0x7FFF
/* Value of any code point field when the code point is invalid. */
#define UNITABLES_INVALID_CODEPOINT INT32_C(-1)

/* Note: we process UnicodeData.txt, CompositionExclusions.txt,
CaseFolding.txt, GraphemeBreakProperty.txt, emoji-data.txt, and
DerivedCoreProperties.txt. */
struct Unitables_Properties
{
  /* Describes what kind of character this is, how it combines with
  neighboring characters, and how it behaves in bidirectional text. */
  Unitables_Category category;
  uint8_t combining_class;
  Unitables_Bidi_Class bidi_class;

  /* Describes how this code point decomposes into simpler code points. */
  Unitables_Decomp_Type decomp_type;
  uint16_t decomp_seqindex;

  /* Describes how this code point maps during case folding and case
  conversion operations. */
  uint16_t casefold_seqindex;
  uint16_t uppercase_seqindex;
  uint16_t lowercase_seqindex;
  uint16_t titlecase_seqindex;

  /* Canonical composition. If this code point can begin a combining pair,
  comb_index/comb_length locate its entries in the combination table;
  comb_issecond marks a code point that can be the second of such a pair.
  comb_index == UNITABLES_COMB_NONE means "cannot begin a pair".
  This block uses 24 of its 32 bits; the remaining 8 are available for a field
  here to grow without changing the size of the struct. */
  uint32_t comb_index : 15;
  uint32_t comb_length : 8;
  uint32_t comb_issecond : 1;

  /* Grapheme cluster boundary class (UAX #29). */
  Unitables_Bound_Class bound_class;
  Unitables_Indic_Conjunct_Break indic_conjunct_break;
};

/* Returns the Unicode properties for codepoint. Invalid, out-of-range, and
unassigned code points return a shared sentinel whose category is
Unitables_Category_Cn. The returned pointer refers to static data and
remains valid for the lifetime of the program. */
struct Unitables_Properties const* unitables_properties(
    Unitables_Codepoint codepoint);

/* Writes the decomposition of codepoint into dst: canonical when mode is
Unitables_Decomp_Mode_Canonical, compatibility otherwise. Recurses, expanding
Hangul syllables algorithmically. Returns the number of code points the
decomposition needs; if that exceeds dst_cap, dst holds an undefined partial
result. A code point with no decomposition yields itself. */
uint32_t unitables_decompose(Unitables_Codepoint codepoint,
                             Unitables_Decomp_Mode mode,
                             Unitables_Codepoint* dst, uint32_t dst_cap);

/* Returns the canonical composition of starter and the following code point,
or UNITABLES_INVALID_CODEPOINT if the two do not compose. Handles Hangul. */
Unitables_Codepoint unitables_compose(Unitables_Codepoint starter,
                                      Unitables_Codepoint following);

/* Returns the simple uppercase mapping of codepoint, or codepoint if there is
no uppercase mapping. This is a one-code-point table lookup and does not apply
context-sensitive SpecialCasing.txt rules. */
Unitables_Codepoint unitables_toupper(Unitables_Codepoint codepoint);

/* Returns the simple lowercase mapping of codepoint, or codepoint if there is
no lowercase mapping. This is a one-code-point table lookup and does not apply
context-sensitive SpecialCasing.txt rules. */
Unitables_Codepoint unitables_tolower(Unitables_Codepoint codepoint);

/* Returns the simple titlecase mapping of codepoint, or codepoint if there is
no titlecase mapping. This is a one-code-point table lookup and does not apply
context-sensitive SpecialCasing.txt rules. */
Unitables_Codepoint unitables_totitle(Unitables_Codepoint codepoint);

/* Writes the default full case folding of codepoint into dst and returns the
number of code points the mapping needs. If the code point has no mapping, the
input code point is written back. This is a table lookup for one code point; it
does not process strings and does not apply Turkic case folding rules. */
uint32_t unitables_casefold(Unitables_Codepoint codepoint,
                            Unitables_Codepoint* dst, uint32_t dst_cap);

/* Given a pair of consecutive code points, returns whether a grapheme cluster
break is permitted between them (UAX #29 extended grapheme clusters). state
must point to a uint32_t initialized to 0 at the start of the string; it
tracks context needed for GB9c/GB11/GB12/GB13. If state is NULL, those rules
are not applied. */
uint8_t unitables_grapheme_break(Unitables_Codepoint codepoint1,
                                 Unitables_Codepoint codepoint2,
                                 uint32_t* state);

#if defined(__cplusplus)
}
#endif
