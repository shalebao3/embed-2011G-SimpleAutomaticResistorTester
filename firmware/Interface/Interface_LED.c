#include "Interface_LED.h"
#include "stm32f10x.h"

/**
 * @brief 封装完整 LED 初始化，包括时钟、GPIO 参数和初始输出电平。
 */
void Interface_LED_Init(void)
{
    GPIO_InitTypeDef gpio_init; /* PC13 输出配置。 */

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_StructInit(&gpio_init);
    gpio_init.GPIO_Pin = GPIO_Pin_13;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOC, &gpio_init);

    /* 保留原 BoardLed_Init 的行为：初始化结束后为高电平。 */
    GPIO_SetBits(GPIOC, GPIO_Pin_13);
}

/**
 * @brief 根据板载 LED 的低有效极性设置输出；不负责闪烁节奏。
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
