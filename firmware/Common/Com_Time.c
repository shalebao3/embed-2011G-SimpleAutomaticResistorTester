#include "Com_Time.h"

/* 私有毫秒计数：仅时间模块写入，不再通过 main.h 暴露全局变量。 */
static volatile uint32_t s_ms_ticks = 0U;

/** @brief 建立公共时基，保持原 SysTick 配置方式和频率。 */
ErrorStatus Com_Time_Init(void)
{
    s_ms_ticks = 0U;

    if (SysTick_Config(SystemCoreClock / 1000U) != 0U)
    {
        return ERROR;
    }

    return SUCCESS;
}

/** @brief 读取当前毫秒计数。 */
uint32_t Com_Time_GetMs(void)
{
    return s_ms_ticks;
}

/** @brief 阻塞延时，沿用原实现的回绕安全无符号减法。 */
void Com_Time_DelayMs(uint32_t delay_ms)
{
    const uint32_t start_ms = Com_Time_GetMs(); /* 本次等待起点。 */

    while ((uint32_t)(Com_Time_GetMs() - start_ms) < delay_ms)
    {
    }
}

/** @brief 更新毫秒计数，只由 SysTick 中断入口调用。 */
void Com_Time_Tick(void)
{
    s_ms_ticks++;
}
