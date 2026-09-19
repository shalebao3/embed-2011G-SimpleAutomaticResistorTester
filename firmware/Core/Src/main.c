#include "main.h"
#include "App_ResistorTester.h"
#include "Com_Time.h"

/**
 * @brief 程序入口：先建立时间基准，再初始化并运行仪器。
 */
int main(void)
{
    SystemCoreClockUpdate();

    /* ADC 的稳定等待和校准超时依赖 1ms 时间基准。 */
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
 * @brief 保留原有错误处理：关闭中断并停留，便于调试定位。
 */
void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}
