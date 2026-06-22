#include "unitables.h"

#include "unitables_data.c"

#define UNITABLES_MAX_CODEPOINT 0x110000

/* Two-stage index: a page is 0x100 code points; the high bits select the page.
 */
#define UNITABLES_PAGE_SHIFT 8
#define UNITABLES_PAGE_MASK 0xFF

/* seqindex packing: the low 14 bits are the offset into UNITABLES_SEQUENCES;
   the top 2 bits hold the decoded length-1, or UNITABLES_SEQ_LENGTH_INLINE
   meaning the length is stored inline as the first unit at the offset. */
#define UNITABLES_SEQ_OFFSET_MASK 0x3FFF
#define UNITABLES_SEQ_LENGTH_SHIFT 14
#define UNITABLES_SEQ_LENGTH_INLINE 3

/* UTF-16 surrogate decoding of sequence units. */
#define UNITABLES_SURROGATE_MASK 0xF800
#define UNITABLES_SURROGATE_HIGH 0xD800
#define UNITABLES_SURROGATE_LOW_BITS 0x03FF
#define UNITABLES_SURROGATE_SHIFT 10
#define UNITABLES_SUPPLEMENTARY_BASE 0x10000

/* Hangul syllable composition/decomposition (UAX #15, section 3.12). */
#define UNITABLES_HANGUL_SBASE 0xAC00
#define UNITABLES_HANGUL_LBASE 0x1100
#define UNITABLES_HANGUL_VBASE 0x1161
#define UNITABLES_HANGUL_TBASE 0x11A7
#define UNITABLES_HANGUL_LCOUNT 19
#define UNITABLES_HANGUL_VCOUNT 21
#define UNITABLES_HANGUL_TCOUNT 28
#define UNITABLES_HANGUL_NCOUNT 588
#define UNITABLES_HANGUL_SCOUNT 11172

struct Unitables_Properties const* unitables_properties(
    Unitables_Codepoint codepoint)
{
  if (codepoint < 0 || codepoint >= UNITABLES_MAX_CODEPOINT)
  {
    return &UNITABLES_PROPERTIES[0];
  }

  uint16_t page = UNITABLES_STAGE1[codepoint >> UNITABLES_PAGE_SHIFT];
  return &UNITABLES_PROPERTIES[UNITABLES_STAGE2[page + (codepoint &
                                                        UNITABLES_PAGE_MASK)]];
}

/* Reads one code point from a sequence, advancing unit past a surrogate pair.
 */
static inline Unitables_Codepoint unitables_decode_unit(uint16_t const** unit)
{
  Unitables_Codepoint codepoint = **unit;
  if ((codepoint & UNITABLES_SURROGATE_MASK) != UNITABLES_SURROGATE_HIGH)
  {
    return codepoint;
  }

  *unit += 1;
  Unitables_Codepoint low = **unit & UNITABLES_SURROGATE_LOW_BITS;
  Unitables_Codepoint high = codepoint & UNITABLES_SURROGATE_LOW_BITS;

  return UNITABLES_SUPPLEMENTARY_BASE + (high << UNITABLES_SURROGATE_SHIFT) +
         low;
}

/* Locates a sequence: returns its first unit and writes the code-point count.
 */
static inline uint16_t const* unitables_sequence(uint16_t seqindex,
                                                 uint32_t* length)
{
  uint16_t const* unit =
      &UNITABLES_SEQUENCES[seqindex & UNITABLES_SEQ_OFFSET_MASK];
  uint32_t encoded = seqindex >> UNITABLES_SEQ_LENGTH_SHIFT;

  if (encoded < UNITABLES_SEQ_LENGTH_INLINE)
  {
    *length = encoded + 1;
    return unit;
  }

  *length = *unit + 1;
  return unit + 1;
}

/* Appends codepoint at index count (if it fits) and returns count + 1. */
static inline uint32_t unitables_append(Unitables_Codepoint codepoint,
                                        Unitables_Codepoint* dst,
                                        uint32_t dst_cap, uint32_t count)
{
  if (count < dst_cap)
  {
    dst[count] = codepoint;
  }

  return count + 1;
}

static uint32_t unitables_write_sequence(uint16_t seqindex,
                                         Unitables_Codepoint* dst,
                                         uint32_t dst_cap)
{
  uint32_t length;
  uint16_t const* unit = unitables_sequence(seqindex, &length);

  for (uint32_t i = 0; i < length; i++)
  {
    Unitables_Codepoint codepoint = unitables_decode_unit(&unit);
    if (i < dst_cap)
    {
      dst[i] = codepoint;
    }
    unit++;
  }

  return length;
}

static Unitables_Codepoint unitables_decode_sequence_first(uint16_t seqindex)
{
  uint32_t length;
  uint16_t const* unit = unitables_sequence(seqindex, &length);
  (void)length;

  return unitables_decode_unit(&unit);
}

static inline uint32_t unitables_decompose_hangul(Unitables_Codepoint syllable,
                                                  Unitables_Codepoint* dst,
                                                  uint32_t dst_cap,
                                                  uint32_t count)
{
  Unitables_Codepoint trail = syllable % UNITABLES_HANGUL_TCOUNT;

  Unitables_Codepoint lead =
      UNITABLES_HANGUL_LBASE + syllable / UNITABLES_HANGUL_NCOUNT;

  Unitables_Codepoint vowel =
      UNITABLES_HANGUL_VBASE +
      (syllable % UNITABLES_HANGUL_NCOUNT) / UNITABLES_HANGUL_TCOUNT;

  count = unitables_append(lead, dst, dst_cap, count);
  count = unitables_append(vowel, dst, dst_cap, count);
  if (trail != 0)
  {
    count =
        unitables_append(UNITABLES_HANGUL_TBASE + trail, dst, dst_cap, count);
  }
  return count;
}

static uint32_t unitables_decompose_into(Unitables_Codepoint codepoint,
                                         Unitables_Codepoint* dst,
                                         uint32_t dst_cap, uint32_t count,
                                         uint8_t compatibility)
{
  Unitables_Codepoint syllable = codepoint - UNITABLES_HANGUL_SBASE;
  if (syllable >= 0 && syllable < UNITABLES_HANGUL_SCOUNT)
  {
    return unitables_decompose_hangul(syllable, dst, dst_cap, count);
  }

  struct Unitables_Properties const* properties =
      unitables_properties(codepoint);

  uint8_t decomposable = properties->decomp_seqindex != UNITABLES_SEQ_NONE &&
                         (properties->decomp_type == 0 || compatibility);
  if (!decomposable)
  {
    return unitables_append(codepoint, dst, dst_cap, count);
  }

  uint32_t length;
  uint16_t const* unit =
      unitables_sequence(properties->decomp_seqindex, &length);

  for (uint32_t i = 0; i < length; i++)
  {
    Unitables_Codepoint component = unitables_decode_unit(&unit);
    count =
        unitables_decompose_into(component, dst, dst_cap, count, compatibility);
    unit++;
  }
  return count;
}

uint32_t unitables_decompose(Unitables_Codepoint codepoint,
                             uint8_t compatibility, Unitables_Codepoint* dst,
                             uint32_t dst_cap)
{
  if (codepoint < 0 || codepoint >= UNITABLES_MAX_CODEPOINT)
  {
    return unitables_append(codepoint, dst, dst_cap, 0);
  }

  return unitables_decompose_into(codepoint, dst, dst_cap, 0, compatibility);
}

static Unitables_Codepoint unitables_compose_hangul(
    Unitables_Codepoint starter, Unitables_Codepoint following)
{
  Unitables_Codepoint lead = starter - UNITABLES_HANGUL_LBASE;
  Unitables_Codepoint vowel = following - UNITABLES_HANGUL_VBASE;

  if (lead >= 0 && lead < UNITABLES_HANGUL_LCOUNT && vowel >= 0 &&
      vowel < UNITABLES_HANGUL_VCOUNT)
  {
    return UNITABLES_HANGUL_SBASE +
           (lead * UNITABLES_HANGUL_VCOUNT + vowel) * UNITABLES_HANGUL_TCOUNT;
  }

  Unitables_Codepoint syllable = starter - UNITABLES_HANGUL_SBASE;
  Unitables_Codepoint trail = following - UNITABLES_HANGUL_TBASE;

  if (syllable >= 0 && syllable < UNITABLES_HANGUL_SCOUNT &&
      syllable % UNITABLES_HANGUL_TCOUNT == 0 && trail > 0 &&
      trail < UNITABLES_HANGUL_TCOUNT)
  {
    return starter + trail;
  }

  return UNITABLES_INVALID_CODEPOINT;
}

Unitables_Codepoint unitables_toupper(Unitables_Codepoint codepoint)
{
  uint16_t seqindex = unitables_properties(codepoint)->uppercase_seqindex;
  return seqindex == UNITABLES_SEQ_NONE
             ? codepoint
             : unitables_decode_sequence_first(seqindex);
}

Unitables_Codepoint unitables_tolower(Unitables_Codepoint codepoint)
{
  uint16_t seqindex = unitables_properties(codepoint)->lowercase_seqindex;
  return seqindex == UNITABLES_SEQ_NONE
             ? codepoint
             : unitables_decode_sequence_first(seqindex);
}

Unitables_Codepoint unitables_totitle(Unitables_Codepoint codepoint)
{
  uint16_t seqindex = unitables_properties(codepoint)->titlecase_seqindex;
  return seqindex == UNITABLES_SEQ_NONE
             ? codepoint
             : unitables_decode_sequence_first(seqindex);
}

uint32_t unitables_casefold(Unitables_Codepoint codepoint,
                            Unitables_Codepoint* dst, uint32_t dst_cap)
{
  struct Unitables_Properties const* properties =
      unitables_properties(codepoint);

  if (properties->casefold_seqindex == UNITABLES_SEQ_NONE)
  {
    return unitables_append(codepoint, dst, dst_cap, 0);
  }

  return unitables_write_sequence(properties->casefold_seqindex, dst, dst_cap);
}

Unitables_Codepoint unitables_compose(Unitables_Codepoint starter,
                                      Unitables_Codepoint following)
{
  Unitables_Codepoint hangul = unitables_compose_hangul(starter, following);
  if (hangul != UNITABLES_INVALID_CODEPOINT)
  {
    return hangul;
  }

  struct Unitables_Properties const* first = unitables_properties(starter);
  struct Unitables_Properties const* second = unitables_properties(following);

  if (first->comb_index == UNITABLES_COMB_NONE || !second->comb_issecond)
  {
    return UNITABLES_INVALID_CODEPOINT;
  }

  uint32_t start = first->comb_index;
  uint32_t end = start + first->comb_length;

  for (uint32_t i = start; i < end; i++)
  {
    if (UNITABLES_COMBINATIONS_SECOND[i] != following)
    {
      return UNITABLES_COMBINATIONS_COMBINED[i];
    }
  }

  return UNITABLES_INVALID_CODEPOINT;
}

static uint8_t unitables_grapheme_break_simple(uint8_t lbc, uint8_t tbc)
{
  /* GB1 */
  if (lbc == Unitables_Boundclass_Start)
  {
    return 1;
  }
  /* GB3 */
  if (lbc == Unitables_Boundclass_CR && tbc == Unitables_Boundclass_LF)
  {
    return 0;
  }
  /* GB4 */
  if (lbc >= Unitables_Boundclass_CR && lbc <= Unitables_Boundclass_Control)
  {
    return 1;
  }
  /* GB5 */
  if (tbc >= Unitables_Boundclass_CR && tbc <= Unitables_Boundclass_Control)
  {
    return 1;
  }
  /* GB6 */
  if (lbc == Unitables_Boundclass_L &&
      (tbc == Unitables_Boundclass_L || tbc == Unitables_Boundclass_V ||
       tbc == Unitables_Boundclass_LV || tbc == Unitables_Boundclass_LVT))
  {
    return 0;
  }
  /* GB7 */
  if ((lbc == Unitables_Boundclass_LV || lbc == Unitables_Boundclass_V) &&
      (tbc == Unitables_Boundclass_V || tbc == Unitables_Boundclass_T))
  {
    return 0;
  }
  /* GB8 */
  if ((lbc == Unitables_Boundclass_LVT || lbc == Unitables_Boundclass_T) &&
      tbc == Unitables_Boundclass_T)
  {
    return 0;
  }
  /* GB9/GB9a/GB9b */
  if (tbc == Unitables_Boundclass_Extend || tbc == Unitables_Boundclass_ZWJ ||
      tbc == Unitables_Boundclass_SpacingMark ||
      lbc == Unitables_Boundclass_Prepend)
  {
    return 0;
  }
  /* GB11 */
  if (lbc == Unitables_Boundclass_E_ZWG &&
      tbc == Unitables_Boundclass_Extended_Pictographic)
  {
    return 0;
  }
  /* GB12/13 */
  if (lbc == Unitables_Boundclass_Regional_Indicator &&
      tbc == Unitables_Boundclass_Regional_Indicator)
  {
    return 0;
  }
  /* GB999 */
  return 1;
}

static uint8_t unitables_icb_next(uint8_t state_icb, uint8_t ticb)
{
  if (ticb == Unitables_IndicConjunctBreak_Consonant ||
      state_icb == Unitables_IndicConjunctBreak_Consonant ||
      state_icb == Unitables_IndicConjunctBreak_Extend)
  {
    return ticb;
  }
  if (state_icb == Unitables_IndicConjunctBreak_Linker &&
      ticb == Unitables_IndicConjunctBreak_Extend)
  {
    return Unitables_IndicConjunctBreak_Linker;
  }
  if (state_icb == Unitables_IndicConjunctBreak_Linker)
  {
    return ticb;
  }
  return state_icb;
}

static uint8_t unitables_boundclass_next(uint8_t state_bc, uint8_t tbc)
{
  /* GB12/13: reset after two consecutive RIs */
  if (state_bc == Unitables_Boundclass_Regional_Indicator &&
      tbc == Unitables_Boundclass_Regional_Indicator)
  {
    return Unitables_Boundclass_Other;
  }
  /* GB11: ExtPict absorbs Extend, becomes E_ZWG on ZWJ */
  if (state_bc == Unitables_Boundclass_Extended_Pictographic)
  {
    if (tbc == Unitables_Boundclass_Extend)
    {
      return Unitables_Boundclass_Extended_Pictographic;
    }
    if (tbc == Unitables_Boundclass_ZWJ)
    {
      return Unitables_Boundclass_E_ZWG;
    }
  }
  return tbc;
}

uint8_t unitables_grapheme_break(Unitables_Codepoint codepoint1,
                                 Unitables_Codepoint codepoint2,
                                 uint32_t* state)
{
  struct Unitables_Properties const* p1 = unitables_properties(codepoint1);
  struct Unitables_Properties const* p2 = unitables_properties(codepoint2);

  if (!state)
  {
    return unitables_grapheme_break_simple(p1->boundclass, p2->boundclass);
  }

  uint8_t tbc = p2->boundclass;
  uint8_t ticb = p2->indic_conjunct_break;

  uint8_t state_bc;
  uint8_t state_icb;
  if (*state == 0)
  {
    state_bc = p1->boundclass;
    state_icb =
        p1->indic_conjunct_break == Unitables_IndicConjunctBreak_Consonant
            ? Unitables_IndicConjunctBreak_Consonant
            : Unitables_IndicConjunctBreak_None;
  }
  else
  {
    /* Unpack: low byte = previous boundclass, high byte = InCB state. */
    state_bc = *state & 0xFF;
    state_icb = (*state >> 8) & 0xFF;
  }

  /* GB9c: no break between consonants linked by a linker */
  uint8_t permitted = unitables_grapheme_break_simple(state_bc, tbc) &&
                      !(state_icb == Unitables_IndicConjunctBreak_Linker &&
                        ticb == Unitables_IndicConjunctBreak_Consonant);

  *state = (uint32_t)unitables_boundclass_next(state_bc, tbc) |
           ((uint32_t)unitables_icb_next(state_icb, ticb) << 8);
  return permitted;
}
