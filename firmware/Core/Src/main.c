#include "main.h"
#include "App_ResistorTester.h"
#include "Com_Time.h"

/**
 * @brief 程序入口：先建立时间基准，再初始化并运行仪器。
 */
int main(void)
{
    SystemCoreClockUpdate();

    /* ADC 初始化使用延时与超时判断，时间基准必须先启动。 */
    if (Com_Time_Init() != SUCCESS)
    {
        Error_Handler();
    }

    if (App_ResistorTester_Init() != SUCCESS)
    {
        Error_Handler();
    }

    while (1)
    {
        App_ResistorTester_Task();
    }
}

/**
 * @brief 保留原错误处理策略：关闭中断并停机，便于调试器检查。
 */
void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}
