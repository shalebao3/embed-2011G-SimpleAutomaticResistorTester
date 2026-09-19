#include "App_ResistorTester.h"
#include "Com_Time.h"
#include "Driver_ADC.h"
#include "Interface_LED.h"

/* 每个 LED 明暗阶段保持 500ms，与拆分前的验证程序一致。 */
#define APP_LED_PHASE_MS 500U

/**
 * @brief 组织仪器初始化；具体外设配置由各模块负责。
 */
ErrorStatus App_ResistorTester_Init(void)
{
    Interface_LED_Init();
    return Driver_ADC1_Init();
}

/**
 * @brief 保留现有 LED 验证行为；后续测量业务从这里组织。
 */
void App_ResistorTester_Task(void)
{
    Interface_LED_Set(true);
    Com_Time_DelayMs(APP_LED_PHASE_MS);

    Interface_LED_Set(false);
    Com_Time_DelayMs(APP_LED_PHASE_MS);
}
