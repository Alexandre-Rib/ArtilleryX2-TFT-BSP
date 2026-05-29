#ifndef _OS_TIMER_H_
#define _OS_TIMER_H_

#include <stdint.h>

// "ms" is volatile to prevent the compiler from caching it in a register
// when used in tight loops like: while (ts == OS_GetTimeMs());
typedef struct
{
  volatile uint32_t ms;
  uint16_t sec;
} OS_COUNTER;

extern OS_COUNTER os_counter;

void OS_InitTimerMs(void);

static inline uint32_t OS_GetTimeMs(void)
{
  return os_counter.ms;
}

#endif
