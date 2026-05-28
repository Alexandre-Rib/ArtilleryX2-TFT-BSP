#include "utf8.h"

void utf8_decode(const uint8_t * ch, UTF8_CHAR * out)
{
  if ((ch[0] & 0x80) == 0)          // 1-byte: 0x00-0x7F
  {
    out->bytes     = 1;
    out->codePoint = ch[0] & 0x7F;
  }
  else if ((ch[0] & 0xE0) == 0xC0)  // 2-byte: 0x80-0x7FF
  {
    out->bytes     = 2;
    out->codePoint = ch[0] & 0x1F;
  }
  else if ((ch[0] & 0xF0) == 0xE0)  // 3-byte: 0x800-0xFFFF
  {
    out->bytes     = 3;
    out->codePoint = ch[0] & 0x0F;
  }
  else if ((ch[0] & 0xF8) == 0xF0)  // 4-byte: 0x10000-0x1FFFFF
  {
    out->bytes     = 4;
    out->codePoint = ch[0] & 0x07;
  }
  else                               // invalid lead byte → replacement char
  {
    out->bytes     = 1;
    out->codePoint = '?';
  }

  for (uint8_t i = 1; i < out->bytes; i++)
    out->codePoint = (out->codePoint << 6) | (ch[i] & 0x3F);
}

uint16_t utf8_char_count(const uint8_t * str)
{
  uint16_t  i = 0, count = 0;
  UTF8_CHAR c;

  if (str == NULL)
    return 0;

  while (str[i])
  {
    utf8_decode(str + i, &c);
    i += c.bytes;
    count++;
  }

  return count;
}
