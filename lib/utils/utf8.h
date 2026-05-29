#ifndef _UTF8_H_
#define _UTF8_H_

#include <stdint.h>
#include <stddef.h>

// Result of decoding one UTF-8 character
typedef struct
{
  uint8_t  bytes;      // bytes consumed from input (1-4), 0 if null/invalid start
  uint32_t codePoint;  // Unicode code point
} UTF8_CHAR;

// Decode one UTF-8 character from str into out
void     utf8_decode(const uint8_t * str, UTF8_CHAR * out);

// Count the number of Unicode characters in a null-terminated UTF-8 string
uint16_t utf8_char_count(const uint8_t * str);

#endif
