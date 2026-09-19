#ifndef DRIVER_ADC_H
#define DRIVER_ADC_H

#include "stm32f10x.h"

/**
 * @brief 初始化 ADC1：PA0/通道0、规则组单通道、单次转换、软件触发。
 * @return SUCCESS：校准完成；ERROR：校准阶段等待超时，ADC 已关闭。
 * @note 仅用于启动阶段；调用前必须建立 1ms 时间基准并保持中断开启。
 *       本函数不启动测量，不知道参考电阻阻值，也不计算电阻。
 */
ErrorStatus Driver_ADC1_Init(void);

#endif /* DRIVER_ADC_H */
