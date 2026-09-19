#ifndef APP_RESISTOR_TESTER_H
#define APP_RESISTOR_TESTER_H

#include <stdint.h>
#include "stm32f10x.h"

/**
 * @brief 前三个自动量程的逻辑标识。
 * @note 当前只建立软件模型；真实 GPIO/继电器尚未接入。
 */
typedef enum
{
    APP_RESISTOR_RANGE_100_OHM = 0,
    APP_RESISTOR_RANGE_1K_OHM,  // 自动是 1
    APP_RESISTOR_RANGE_10K_OHM, // 自动是 2
    APP_RESISTOR_RANGE_COUNT    // 自动是 3，表示一共几档
} App_ResistorTesterRange;      // 枚举规则：0 表示 100 Ω，1 表示 1K Ω，2 表示 10K Ω，3 表示无效。

/**
 * @brief 最近一次有效测量结果。
 * @note active_range 表示本次换算实际采用的量程；
 *       recommended_range 只表示下一步量程建议，不代表硬件已经切换。
 */
typedef struct
{
    uint16_t adc_raw;                       /* ADC1/PA0 原始值，范围 0～4095。 */
    uint32_t resistance_ohm;                /* 换算出的待测电阻，单位 Ω。 */
    uint32_t reference_resistor_ohm;        /* 本次换算使用的参考电阻，单位 Ω。 */
    App_ResistorTesterRange active_range;   /* 本次实际采用的逻辑量程。 */
    App_ResistorTesterRange recommended_range; /* 根据 ADC 阈值给出的下一量程建议。 */
} App_ResistorTesterMeasurement;

/**
 * @brief 初始化量程控制与 ADC，并清空应用层测量状态。
 * @return SUCCESS：1kΩ 初始量程与 ADC 均初始化完成；ERROR：初始化失败。
 * @note 调用前必须完成 Com_Time_Init()，且保持中断开启。
 */
ErrorStatus App_ResistorTester_Init(void);

/**
 * @brief 非阻塞应用任务：按固定周期触发一次 ADC 单次读取并换算电阻。
 * @note 当前启动时由 Interface_Range 真实选中 1kΩ 档（Rref=330Ω）。
 *       程序仍只计算 recommended_range，不会自动执行换档；
 *       自动切换需要下一阶段加入稳定等待和状态同步。
 *       只能在初始化成功后的主循环调用，不能在中断中调用。
 */
void App_ResistorTester_Task(void);

/**
 * @brief 获取最近一次有效测量结果。
 * @param measurement 输出地址；仅 SUCCESS 时写入。
 * @return SUCCESS：存在有效结果；ERROR：空指针、尚未采样或最近一次测量失败。
 */
ErrorStatus App_ResistorTester_GetLatestMeasurement(
    App_ResistorTesterMeasurement *measurement);

#endif /* APP_RESISTOR_TESTER_H */
