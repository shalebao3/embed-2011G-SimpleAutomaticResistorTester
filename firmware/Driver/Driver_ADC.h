#ifndef DRIVER_ADC_H
#define DRIVER_ADC_H

#include "stm32f10x.h"

/**
 * @brief 初始化 ADC1：PA0/通道0、单通道、单次转换、软件触发、右对齐。
 * @return SUCCESS：校准完成；ERROR：校准等待超时，ADC 已关闭。
 * @note 仅在启动阶段调用，必须先启动 Com_Time 且保持中断开启。
 *       不得在中断或关中断的临界区中调用；不会启动测量转换。
 */
ErrorStatus Driver_ADC1_Init(void);

#endif
