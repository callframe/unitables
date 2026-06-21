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

typedef uint8_t Unitables_BidiClass;
enum
{
  Unitables_BidiClass_L = 1,
  Unitables_BidiClass_LRE,
  Unitables_BidiClass_LRO,
  Unitables_BidiClass_R,
  Unitables_BidiClass_AL,
  Unitables_BidiClass_RLE,
  Unitables_BidiClass_RLO,
  Unitables_BidiClass_PDF,
  Unitables_BidiClass_EN,
  Unitables_BidiClass_ES,
  Unitables_BidiClass_ET,
  Unitables_BidiClass_AN,
  Unitables_BidiClass_CS,
  Unitables_BidiClass_NSM,
  Unitables_BidiClass_BN,
  Unitables_BidiClass_B,
  Unitables_BidiClass_S,
  Unitables_BidiClass_WS,
  Unitables_BidiClass_ON,
  Unitables_BidiClass_LRI,
  Unitables_BidiClass_RLI,
  Unitables_BidiClass_FSI,
  Unitables_BidiClass_PDI
};

typedef uint8_t Unitables_DecompType;
enum
{
  Unitables_DecompType_Font = 1,
  Unitables_DecompType_NoBreak,
  Unitables_DecompType_Initial,
  Unitables_DecompType_Medial,
  Unitables_DecompType_Final,
  Unitables_DecompType_Isolated,
  Unitables_DecompType_Circle,
  Unitables_DecompType_Super,
  Unitables_DecompType_Sub,
  Unitables_DecompType_Vertical,
  Unitables_DecompType_Wide,
  Unitables_DecompType_Narrow,
  Unitables_DecompType_Small,
  Unitables_DecompType_Square,
  Unitables_DecompType_Fraction,
  Unitables_DecompType_Compat,
};

/* Value of any *_seqindex field when the code point has no such mapping. */
#define UNITABLES_SEQ_NONE UINT16_MAX
/* Value of comb_index when the code point cannot begin a combining pair. */
#define UNITABLES_COMB_NONE 0x3FF
/* Value of any code point field when the code point is invalid. */
#define UNITABLES_INVALID_CODEPOINT INT32_C(-1)

/* Note: we only process UnicodeData.txt and CompositionExclusions.txt for
now, so the provided properties are not complete.*/
struct Unitables_Properties
{
  /* Describes what kind of character this is, how it combines with
  neighboring characters, and how it behaves in bidirectional text.*/
  Unitables_Category category;
  uint8_t combining_class;
  Unitables_BidiClass bidi_class;

  /* Describes how this code point decomposes into simpler code points. */
  Unitables_DecompType decomp_type;
  uint16_t decomp_seqindex;

  /* Describes how this code point maps during case folding and
  case conversion operations. */
  uint16_t uppercase_seqindex;
  uint16_t lowercase_seqindex;
  uint16_t titlecase_seqindex;

  /* Canonical composition. If this code point can begin a combining pair,
  comb_index/comb_length locate its entries in the combination table;
  comb_issecond marks a code point that can be the second of such a pair.
  comb_index == UNITABLES_COMB_NONE means "cannot begin a pair". */
  uint16_t comb_index : 10;
  uint16_t comb_length : 5;
  uint16_t comb_issecond : 1;
};

/* Returns the Unicode properties for codepoint. Invalid, out-of-range, and
unassigned code points return a shared sentinel whose category is
Unitables_Category_Cn. The returned pointer refers to static data and
remains valid for the lifetime of the program. */
struct Unitables_Properties const* unitables_properties(
    Unitables_Codepoint codepoint);

/* Writes the full canonical (compatibility == 0) or compatibility
(compatibility != 0) decomposition of codepoint into dst, recursing and
expanding Hangul syllables algorithmically. Returns the number of code points
the decomposition needs; if that exceeds dst_cap, dst holds an undefined
partial result. A code point with no decomposition yields itself. */
int32_t unitables_decompose(Unitables_Codepoint codepoint,
                            int32_t compatibility, Unitables_Codepoint* dst,
                            int32_t dst_cap);

/* Returns the canonical composition of starter and the following code point,
or UNITABLES_INVALID_CODEPOINT if the two do not compose. Handles Hangul. */
Unitables_Codepoint unitables_compose(Unitables_Codepoint starter,
                                      Unitables_Codepoint following);

#if defined(__cplusplus)
}
#endif
