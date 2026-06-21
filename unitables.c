#include "unitables.h"

#include "unitables_data.c"

static struct Unitables_Properties const* unitables_unsafe(
    Unitables_Codepoint cp)
{
  return &UNITABLES_PROPERTIES[UNITABLES_STAGE2[UNITABLES_STAGE1[cp >> 8] +
                                                (cp & 0xFF)]];
}

struct Unitables_Properties const* unitables_properties(Unitables_Codepoint cp)
{
  if (cp < 0 || cp >= 0x110000)
  {
    return &UNITABLES_PROPERTIES[0];
  }
  return unitables_unsafe(cp);
}
