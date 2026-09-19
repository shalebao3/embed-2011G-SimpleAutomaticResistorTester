#ifndef INTERFACE_LED_H
#define INTERFACE_LED_H

#include <stdbool.h>

/** @brief 初始化 PC13 低电平点亮的 LED，初始状态为熄灭。 */
void Interface_LED_Init(void);

/** @brief 设置 LED 状态；on=true 点亮，on=false 熄灭。 */
void Interface_LED_Set(bool on);

#endif
