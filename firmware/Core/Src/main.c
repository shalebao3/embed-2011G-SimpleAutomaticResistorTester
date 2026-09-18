#include "main.h"

volatile uint32_t g_ms_ticks = 0U;

static void BoardLed_Init(void);
static void DelayMs(uint32_t delay_ms);

int main(void)
{
    GPIO_InitTypeDef gpio_init;

    SystemCoreClockUpdate();

    if (SysTick_Config(SystemCoreClock / 1000U) != 0U)
    {
        Error_Handler();
    }

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_StructInit(&gpio_init);
    gpio_init.GPIO_Pin = GPIO_Pin_13;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOC, &gpio_init);

    BoardLed_Init();

    while (1)
    {
        /* 常见 Blue Pill 的 PC13 LED 为低电平点亮。 */
        GPIO_ResetBits(GPIOC, GPIO_Pin_13);
        DelayMs(500U);

        GPIO_SetBits(GPIOC, GPIO_Pin_13);
        DelayMs(500U);
    }
}

static void BoardLed_Init(void)
{
    GPIO_SetBits(GPIOC, GPIO_Pin_13);
}

static void DelayMs(uint32_t delay_ms)
{
    const uint32_t start = g_ms_ticks;

    while ((uint32_t)(g_ms_ticks - start) < delay_ms)
    {
    }
}

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}
