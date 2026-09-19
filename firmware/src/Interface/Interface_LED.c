#include "Interface_LED.h"
#include "stm32f10x.h"

/**
 * @brief 完整迁移原 main() 的 LED 初始化，保留配置顺序与默认电平。
 */
void Interface_LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure; /* 板上 LED 对应的 GPIO 配置。 */

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_SetBits(GPIOC, GPIO_Pin_13);
}

/**
 * @brief 将逻辑亮灭转换成 PC13 的实际输出电平。
 */
void Interface_LED_Set(bool on)
{
    if (on)
    {
        GPIO_ResetBits(GPIOC, GPIO_Pin_13);
    }
    else
    {
        GPIO_SetBits(GPIOC, GPIO_Pin_13);
    }
}
