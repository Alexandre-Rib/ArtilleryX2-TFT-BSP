#include "font_render.h"
#include "Language.h"    // textSelect()

uint16_t GUI_StrPixelWidth_str(const uint8_t * str)
{
  uint16_t  i = 0, width = 0;
  CHAR_INFO info;

  if (str == NULL)
    return 0;

  while (str[i])
  {
    getCharacterInfo(str + i, &info);
    i     += info.bytes;
    width += info.pixelWidth;
  }

  return width;
}

uint16_t GUI_StrPixelWidth_label(int16_t index)
{
  return GUI_StrPixelWidth_str((uint8_t *) textSelect(index));
}
