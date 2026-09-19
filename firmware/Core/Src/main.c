#include "main.h"

volatile uint32_t g_ms_ticks = 0U;

static void BoardLed_Init(void);
static void DelayMs(uint32_t delay_ms);

/* ADC 校准等待超时，单位：毫秒；这是软件保护值。 */
#define ADC_CAL_TIMEOUT_MS 10U

/* 初始化 ADC1，用于采集 PA0 的模拟电压。 */
static ErrorStatus MeasurementADC_Init(void);

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

    /* 初始化失败时进入项目已有的错误处理函数。 */
    if (MeasurementADC_Init() != SUCCESS)
    {
        Error_Handler();
    }

    while (1)
    {
        /* 常见 Blue Pill 的 PC13 LED 为低电平点亮。 */
        GPIO_ResetBits(GPIOC, GPIO_Pin_13);
        DelayMs(500U);

        GPIO_SetBits(GPIOC, GPIO_Pin_13);
        DelayMs(500U);
    }
}

/**
 * @brief 初始化 ADC1：PA0、单通道、单次转换、软件触发。
 * @return SUCCESS：初始化完成；ERROR：校准等待超时。
 * @note 仅在启动阶段调用，调用前必须已启动 1ms SysTick，
 *       且中断保持开启；不要放到中断函数中调用。
 */
static ErrorStatus MeasurementADC_Init(void)
{
    GPIO_InitTypeDef gpio_init; /* PA0 引脚配置。 */
    ADC_InitTypeDef adc_init;   /* ADC 工作方式配置。 */
    uint32_t start_ms;          /* 当前等待阶段的起始时间。 */

    /* 1. 开启 GPIOA 和 ADC1 的外设时钟。 */
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1,
        ENABLE);

    /* PCLK2 六分频：正常 PCLK2=72MHz 时，ADCCLK=12MHz。 */
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    /* 只复位 ADC1，建立明确的初始状态。 */
    ADC_DeInit(ADC1);

    /* 2. PA0 配置为模拟输入。 */
    GPIO_StructInit(&gpio_init);
    gpio_init.GPIO_Pin = GPIO_Pin_0;
    gpio_init.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &gpio_init);

    /* 3. 配置 ADC1 的工作方式。 */
    ADC_StructInit(&adc_init);

    adc_init.ADC_Mode = ADC_Mode_Independent;
    adc_init.ADC_ScanConvMode = DISABLE;
    adc_init.ADC_ContinuousConvMode = DISABLE;
    adc_init.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    adc_init.ADC_DataAlign = ADC_DataAlign_Right;
    adc_init.ADC_NbrOfChannel = 1;

    ADC_Init(ADC1, &adc_init);

    /* 4. 规则序列第 1 位采集通道 0，即 PA0。 */
    ADC_RegularChannelConfig(
        ADC1,
        ADC_Channel_0,
        1,
        ADC_SampleTime_239Cycles5);

    /* 5. 开启 ADC，使用已有毫秒延时留足上电稳定时间。 */
    ADC_Cmd(ADC1, ENABLE);
    DelayMs(2U);

    /* 6. 复位校准寄存器，等待硬件完成。 */
    ADC_ResetCalibration(ADC1);
    start_ms = g_ms_ticks;

    while (ADC_GetResetCalibrationStatus(ADC1) != RESET)
    {
        if ((uint32_t)(g_ms_ticks - start_ms) >= ADC_CAL_TIMEOUT_MS)
        {
            ADC_Cmd(ADC1, DISABLE);
            return ERROR;
        }
    }

    /* 7. 启动内部校准，等待硬件完成。 */
    ADC_StartCalibration(ADC1);
    start_ms = g_ms_ticks;

    while (ADC_GetCalibrationStatus(ADC1) != RESET)
    {
        if ((uint32_t)(g_ms_ticks - start_ms) >= ADC_CAL_TIMEOUT_MS)
        {
            ADC_Cmd(ADC1, DISABLE);
            return ERROR;
        }
    }

    return SUCCESS;
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
