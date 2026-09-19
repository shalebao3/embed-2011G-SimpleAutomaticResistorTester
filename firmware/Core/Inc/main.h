#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include "stm32f10x.h"

/* 程序入口使用的致命错误处理；底层模块只返回错误，不调用它。 */
void Error_Handler(void);

#endif
