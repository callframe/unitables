#pragma once

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef int32_t Unitables_Codepoint;
#define UNITABLES_INVALID_CODEPOINT INT32_C(-1)

typedef uint8_t Unitables_Category;
enum {
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
enum {
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
enum {
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

/* Note: Due to the fact that we only process UnicodeData.txt for now,
the provided properties are not complete.*/
struct Unitables_Properties {
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
};

struct Unitables_Properties const *unitables_properties(Unitables_Codepoint cp);

#if defined(__cplusplus)
}
#endif