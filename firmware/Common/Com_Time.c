#include "Com_Time.h"

/* 时间基准的唯一存储；中断更新，其他模块只能通过接口读取。 */
static volatile uint32_t s_ms_ticks = 0U;

/**
 * @brief 使用原有 SysTick 配置方式建立毫秒时间基准。
 */
ErrorStatus Com_Time_Init(void)
{
    if (SysTick_Config(SystemCoreClock / 1000U) != 0U)
    {
        return ERROR;
    }

    return SUCCESS;
}

/**
 * @brief 返回当前毫秒计数，不向模块外暴露可写全局变量。
 */
uint32_t Com_Time_GetMs(void)
{
    return s_ms_ticks;
}

/**
 * @brief 使用无符号差值等待指定毫秒数，保留原有回绕处理方式。
 */
void Com_Time_DelayMs(uint32_t delay_ms)
{
    const uint32_t start_ms = Com_Time_GetMs(); /* 本次延时的起点。 */

    while ((uint32_t)(Com_Time_GetMs() - start_ms) < delay_ms)
    {
    }
}

/**
 * @brief 响应一次 SysTick，计数溢出时按 uint32_t 自然回绕。
 */
void Com_Time_Tick(void)
{
    s_ms_ticks++;
}
