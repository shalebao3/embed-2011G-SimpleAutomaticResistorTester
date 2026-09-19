#include "App_ResistorTester.h"
#include "Driver_ADC.h"
#include "Interface_LED.h"
#include "Com_Time.h"

/**
 * @brief 按原有顺序初始化 LED 和 ADC，不在应用层配置外设寄存器。
 */
ErrorStatus App_ResistorTester_Init(void)
{
    Interface_LED_Init();
    return Driver_ADC1_Init();
}

/**
 * @brief 保留原有 LED 验证行为；采样与自动量程在后续任务中添加。
 */
void App_ResistorTester_Task(void)
{
    Interface_LED_Set(true);
    Com_Time_DelayMs(500U);

    Interface_LED_Set(false);
    Com_Time_DelayMs(500U);
}
