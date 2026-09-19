#include "Interface_Range.h"

/* 板级候选映射：避开 PA0 ADC 和默认 SWD 引脚；画板前仍可统一调整。 */
#define RANGE_GPIO_PORT GPIOB
#define RANGE_100_OHM_PIN GPIO_Pin_12
#define RANGE_1K_OHM_PIN GPIO_Pin_13
#define RANGE_10K_OHM_PIN GPIO_Pin_14
#define RANGE_ALL_PINS \
    (RANGE_100_OHM_PIN | RANGE_1K_OHM_PIN | RANGE_10K_OHM_PIN)

/**
 * @brief 初始化三路量程控制输出；默认全部关闭。
 */
void Interface_Range_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    /*
     * 先把输出锁存值清零，再把引脚切成推挽输出，
     * 避免初始化阶段出现不必要的高电平脉冲。
     */
    GPIO_ResetBits(RANGE_GPIO_PORT, RANGE_ALL_PINS);

    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = RANGE_ALL_PINS;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(RANGE_GPIO_PORT, &GPIO_InitStructure);
}

/**
 * @brief 先全断，再只接通目标量程一路。
 */
ErrorStatus Interface_Range_Select(Interface_Range range)
{
    uint16_t selected_pin;

    switch (range)
    {
        case INTERFACE_RANGE_100_OHM:
            selected_pin = RANGE_100_OHM_PIN;
            break;

        case INTERFACE_RANGE_1K_OHM:
            selected_pin = RANGE_1K_OHM_PIN;
            break;

        case INTERFACE_RANGE_10K_OHM:
            selected_pin = RANGE_10K_OHM_PIN;
            break;

        default:
            return ERROR;
    }

    /* break-before-make：先确保旧量程全部释放，再开启新量程。 */
    GPIO_ResetBits(RANGE_GPIO_PORT, RANGE_ALL_PINS);
    GPIO_SetBits(RANGE_GPIO_PORT, selected_pin);

    return SUCCESS;
}

/**
 * @brief 关闭全部量程控制输出。
 */
void Interface_Range_DisableAll(void)
{
    GPIO_ResetBits(RANGE_GPIO_PORT, RANGE_ALL_PINS);
}
