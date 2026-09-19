#ifndef INTERFACE_LED_H
#define INTERFACE_LED_H

#include <stdbool.h>

/**
 * @brief 初始化 PC13 LED，包括 GPIOC 时钟、引脚模式和默认熄灭状态。
 * @note 按常见 Blue Pill 的低电平点亮方式配置；与原程序保持一致。
 */
void Interface_LED_Init(void);

/**
 * @brief 设置 LED 亮灭；隐藏 PC13 和低电平有效的硬件细节。
 * @param on true：点亮；false：熄灭。
 * @note 必须先调用 Interface_LED_Init()；闪烁节奏由应用层决定。
 */
void Interface_LED_Set(bool on);

#endif /* INTERFACE_LED_H */
