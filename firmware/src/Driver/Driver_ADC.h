#ifndef DRIVER_ADC_H
#define DRIVER_ADC_H

#include "stm32f10x.h"

/**
 * @brief 初始化 ADC1：PA0/通道0、规则组单通道、单次转换、软件触发。
 * @return SUCCESS：校准完成；ERROR：校准阶段等待超时，ADC 已关闭。
 * @note 用于启动或显式故障恢复；调用前必须建立 1ms 时间基准并保持中断开启。
 *       本函数不启动测量，不知道参考电阻阻值，也不计算电阻。
 */
ErrorStatus Driver_ADC1_Init(void);

/**
 * @brief 软件触发 ADC1/PA0 单次转换，轮询 EOC 并读取原始值。
 * @param raw 有效 uint16_t 对象的地址；仅成功时更新为本次结果 0～4095。
 * @return SUCCESS：本次读取成功；ERROR：空指针、未初始化或等待超时。
 * @note 必须先成功初始化，仅限主循环串行调用，且 SysTick 一直能够执行。
 *       不可在中断、关中断区调用；不允许 DMA/中断/其他模块同时读取 ADC1。
 *       失败时不写 raw，调用者必须检查返回值，不能把历史值当作新数据。
 *       转换超时后关闭 ADC，需显式重新初始化；不自动重试，不计算电阻。
 */
ErrorStatus Driver_ADC1_ReadRaw(uint16_t *raw);

#endif /* DRIVER_ADC_H */
