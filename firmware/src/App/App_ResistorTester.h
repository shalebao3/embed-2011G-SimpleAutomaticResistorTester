#ifndef APP_RESISTOR_TESTER_H
#define APP_RESISTOR_TESTER_H

#include "stm32f10x.h"

/**
 * @brief 初始化仪器所需的 ADC。
 * @return SUCCESS：初始化完成；ERROR：ADC 初始化失败。
 * @note 调用前必须完成 Com_Time_Init()，且保持中断开启。
 */
ErrorStatus App_ResistorTester_Init(void);

/**
 * @brief 应用任务入口，后续在此接入测量调度。
 * @note 当前为空实现，不执行硬件操作或延时，也不自动启动 ADC 转换。
 *       只能在初始化成功后的主循环调用，不能在中断中调用。
 */
void App_ResistorTester_Task(void);

#endif /* APP_RESISTOR_TESTER_H */
