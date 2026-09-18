#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include "stm32f10x.h"

extern volatile uint32_t g_ms_ticks;

void Error_Handler(void);

#endif
