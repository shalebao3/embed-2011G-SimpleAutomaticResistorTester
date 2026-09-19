#ifndef COM_TIME_H
#define COM_TIME_H

#include <stdint.h>
#include "stm32f10x.h"

/**
 * @brief 以当前 SystemCoreClock 配置 1ms SysTick 时间基准。
 * @return SUCCESS：配置成功；ERROR：SysTick 重装值不合法。
 * @note 启动阶段只调用一次；SystemCoreClock 必须已更新。
 *       不能与其他 SysTick 提供者同时使用，也不重置已运行的时间。
 */
ErrorStatus Com_Time_Init(void);

/**
 * @brief 获取累计毫秒数；无符号 32 位计数允许自然回绕。
 * @note 判断时间间隔使用 (uint32_t)(now - start)，不要直接比较截止值。
 */
uint32_t Com_Time_GetMs(void);

/**
 * @brief 阻塞延时；保持原程序基于毫秒差值的实现。
 * @param delay_ms 等待的毫秒数；0 表示立即返回。
 * @note 必须先启动时间基准且允许 SysTick 中断执行。
 *       不可在中断或关闭中断的临界区调用；不可代替精确微秒延时。
 */
void Com_Time_DelayMs(uint32_t delay_ms);

/**
 * @brief 更新一次毫秒计数，仅由 SysTick_Handler() 每 1ms 调用一次。
 */
void Com_Time_Tick(void);

#endif /* COM_TIME_H */
