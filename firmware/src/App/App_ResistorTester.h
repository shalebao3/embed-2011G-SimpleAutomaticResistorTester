#ifndef APP_RESISTOR_TESTER_H
#define APP_RESISTOR_TESTER_H

#include <stdint.h>
#include "stm32f10x.h"

/**
 * @brief 单档测量的最新结果。
 * @note 当前 resistance_ohm 按临时 330Ω 参考电阻和既定分压拓扑计算；
 *       该参数只服务于第一版单档验证，不代表最终自动量程 BOM。
 */
typedef struct
{
    uint16_t adc_raw;          /* ADC1/PA0 原始值，范围 0～4095。 */
    uint32_t resistance_ohm;   /* 根据当前单档参考电阻换算出的待测电阻，单位 Ω。 */
} App_ResistorTesterMeasurement;

/**
 * @brief 初始化仪器所需的 ADC，并清空应用层测量状态。
 * @return SUCCESS：初始化完成；ERROR：ADC 初始化失败。
 * @note 调用前必须完成 Com_Time_Init()，且保持中断开启。
 */
ErrorStatus App_ResistorTester_Init(void);

/**
 * @brief 非阻塞应用任务：按固定周期触发一次 ADC 单次读取并换算电阻。
 * @note 当前采样周期为 100ms，第一轮调用立即采样。
 *       本函数不延时、不自动恢复 ADC 故障、不执行自动量程。
 *       只能在初始化成功后的主循环调用，不能在中断中调用。
 */
void App_ResistorTester_Task(void);

/**
 * @brief 获取最近一次有效的单档测量结果。
 * @param measurement 输出地址；仅 SUCCESS 时写入。
 * @return SUCCESS：存在有效结果；ERROR：空指针、尚未采样或最近一次测量失败。
 */
ErrorStatus App_ResistorTester_GetLatestMeasurement(
    App_ResistorTesterMeasurement *measurement);

#endif /* APP_RESISTOR_TESTER_H */
