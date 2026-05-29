#ifndef _CIRCULAR_QUEUE_H_
#define _CIRCULAR_QUEUE_H_

#include <stdint.h>  // for uint8_t etc.

#define CIRCULAR_QUEUE_SIZE (1024 * 5)

typedef struct
{
  uint8_t  data[CIRCULAR_QUEUE_SIZE];  // data buffer
  uint16_t index_r;                    // ring buffer read position
  uint16_t index_w;                    // ring buffer write position
  uint16_t count;                      // count of commands in the queue
} CIRCULAR_QUEUE;

#endif
