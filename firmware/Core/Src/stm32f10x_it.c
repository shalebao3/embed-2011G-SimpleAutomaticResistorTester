#include "stm32f10x_it.h"
#include "Com_Time.h"

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
    while (1)
    {
    }
}

void MemManage_Handler(void)
{
    while (1)
    {
    }
}

void BusFault_Handler(void)
{
    while (1)
    {
    }
}

void UsageFault_Handler(void)
{
    while (1)
    {
    }
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

/**
 * @brief 唯一的 SysTick 中断入口，每次中断将公共时基推进 1ms。
 */
void SysTick_Handler(void)
{
    Com_Time_Tick();
}
