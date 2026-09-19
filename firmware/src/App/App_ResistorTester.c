#include "App_ResistorTester.h"
#include "Driver_ADC.h"

/**
 * @brief 初始化仪器 ADC；时间基准由 main 提前建立。
 */
ErrorStatus App_ResistorTester_Init(void)
{
    return Driver_ADC1_Init();
}

/**
 * @brief 保留应用任务入口，后续在此接入测量调度。
 * @note 当前不执行硬件操作或延时，也不自动启动 ADC 转换。
 */
void App_ResistorTester_Task(void)
{
}
