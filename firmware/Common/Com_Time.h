#ifndef COM_TIME_H
#define COM_TIME_H

#include <stdint.h>
#include "stm32f10x.h"

/**
 * @brief 根据已更新的 SystemCoreClock 配置 1ms SysTick，仅在启动时调用。
 * @return SUCCESS：成功；ERROR：SysTick 重装值无效。
 */
ErrorStatus Com_Time_Init(void);

/** @brief 获取毫秒计数；允许 uint32_t 自然回绕，使用无符号差计算间隔。 */
uint32_t Com_Time_GetMs(void);

/**
 * @brief 阻塞等待指定毫秒数，保持原 DelayMs 的无符号差判断。
 * @note 仅在线程/主循环使用，必须已启动 SysTick 且中断开启；
 *       不得在中断或关中断的临界区内调用。本轮不改成非阻塞调度。
 */
void Com_Time_DelayMs(uint32_t delay_ms);

/** @brief 由唯一的 SysTick_Handler 调用一次，推进 1ms；其他模块不得调用。 */
void Com_Time_Tick(void);

#endif
