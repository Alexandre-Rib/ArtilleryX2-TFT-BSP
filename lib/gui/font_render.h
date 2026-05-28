#ifndef _FONT_RENDER_H_
#define _FONT_RENDER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "font_atlas.h"   // CHAR_INFO, FontAtlas_GetCharInfo, FontAtlas_SetSize

// Convenience alias — callers use setFontSize, not the atlas directly
static inline void setFontSize(uint16_t size) { FontAtlas_SetSize(size); }

// Convenience alias — callers use getCharacterInfo
static inline void getCharacterInfo(const uint8_t * ch, CHAR_INFO * pInfo)
{
  FontAtlas_GetCharInfo(ch, pInfo);
}

// Pixel width of a UTF-8 string in the current font
uint16_t GUI_StrPixelWidth_str(const uint8_t * str);

// Pixel width of a language label (index into the label table)
uint16_t GUI_StrPixelWidth_label(int16_t index);

// Generic dispatch: string pointer → _str, integer index → _label
#define GUI_StrPixelWidth(X) _Generic(((X)+0),          \
  const uint8_t * const: GUI_StrPixelWidth_str,          \
        const uint8_t *: GUI_StrPixelWidth_str,          \
              uint8_t *: GUI_StrPixelWidth_str,          \
                default: GUI_StrPixelWidth_label)(X)

#ifdef __cplusplus
}
#endif

#endif
