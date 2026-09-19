#ifndef APP_RESISTOR_TESTER_H
#define APP_RESISTOR_TESTER_H

#include "stm32f10x.h"

/**
 * @brief 初始化 LED 与 ADC；调用前必须先启动 Com_Time 毫秒时基。
 * @return SUCCESS：成功；ERROR：ADC 初始化失败。
 */
ErrorStatus App_ResistorTester_Init(void);

/**
 * @brief 运行当前验证任务：LED 点亮 500ms、熄灭 500ms。
 * @note 初始化成功后仅在主循环调用；本次保留阻塞节奏，尚未读取 ADC。
 */
void App_ResistorTester_Task(void);

#endif
