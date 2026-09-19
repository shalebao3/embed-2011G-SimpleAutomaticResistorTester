#ifndef BSP_RANGE_H
#define BSP_RANGE_H

#include "stm32f10x.h"

/**
 * @brief 三个自动量程对应的硬件选择。
 * @note 当前控制对象是假定为“GPIO 高电平 -> 外部 NPN/MOSFET 导通 -> 对应继电器吸合”。
 *       GPIO 不能直接驱动继电器线圈；后续原理图必须包含驱动管与续流保护。
 */
typedef enum
{
    BSP_RANGE_100_OHM = 0,
    BSP_RANGE_1K_OHM,
    BSP_RANGE_10K_OHM,
    BSP_RANGE_COUNT
} Bsp_Range;

/**
 * @brief 初始化量程控制 GPIO，并保持三路全部关闭。
 * @note 当前候选引脚为 PB12/PB13/PB14；后续画板时可在本模块集中修改。
 */
void Bsp_Range_Init(void);

/**
 * @brief 选择一个量程控制输出。
 * @param range 目标量程。
 * @return SUCCESS：已先关闭全部三路，再只开启目标一路；ERROR：量程参数非法。
 * @note 使用 break-before-make 顺序，禁止两路同时保持有效。
 */
ErrorStatus Bsp_Range_Select(Bsp_Range range);

/**
 * @brief 关闭全部量程控制输出。
 */
void Bsp_Range_DisableAll(void);

#endif /* BSP_RANGE_H */
