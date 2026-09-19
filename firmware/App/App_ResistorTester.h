#ifndef APP_RESISTOR_TESTER_H
#define APP_RESISTOR_TESTER_H

#include "stm32f10x.h"

/**
 * @brief 初始化仪器所需的 LED 与 ADC，按原有顺序执行。
 * @return SUCCESS：初始化完成；ERROR：ADC 初始化失败。
 * @note 调用前必须完成 Com_Time_Init()，且保持中断开启。
 */
ErrorStatus App_ResistorTester_Init(void);

/**
 * @brief 执行一轮仪器任务；目前仅保留 LED 亮 500ms、灭 500ms 示例。
 * @note 当前为阻塞任务，尚未实现 ADC 读取、电阻换算或自动量程。
 *       只能在初始化成功后的主循环调用，不能在中断中调用。
 */
void App_ResistorTester_Task(void);

#endif /* APP_RESISTOR_TESTER_H */
