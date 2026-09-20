#include "Driver_ADC.h"
#include "Com_Time.h"
#include <stddef.h>

/* 校准超时保护值，保持原程序的 10ms，不是 ADC 硬件转换时间。 */
#define ADC_CAL_TIMEOUT_MS 10U

/* 仅表示本驱动已完成初始化，不是并发锁或硬件就绪寄存器。 */
static FunctionalState s_adc1_ready = DISABLE;

/**
 * @brief 初始化 ADC1：PA0、单通道、单次转换、软件触发。
 * @return SUCCESS：初始化完成；ERROR：校准等待超时。
 * @note 在启动或显式故障恢复时调用，调用前必须已启动 1ms SysTick，
 *       且中断保持开启；不要放到中断函数中调用。
 */
ErrorStatus Driver_ADC1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure; /* PA0 引脚配置。 */
    ADC_InitTypeDef ADC_InitStructure;   /* ADC 工作方式配置。 */
    uint32_t start_ms;          /* 当前等待阶段的起始时间。 */

    /* 初始化失败或重新初始化期间，拒绝读取尚未就绪的 ADC。 */
    s_adc1_ready = DISABLE;

    /* 1. 开启 GPIOA 和 ADC1 的外设时钟。 */
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1,
        ENABLE);

    /* PCLK2 六分频：正常 PCLK2=72MHz 时，ADCCLK=12MHz。 */
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    /* 只复位 ADC1，建立明确的初始状态。 */
    ADC_DeInit(ADC1);

    /* 2. PA0 配置为模拟输入。 */
    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* 3. 配置 ADC1 的工作方式。 */
    ADC_StructInit(&ADC_InitStructure);

    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;

    ADC_Init(ADC1, &ADC_InitStructure);

    /* 4. 规则序列第 1 位采集通道 0，即 PA0。 */
    ADC_RegularChannelConfig(
        ADC1,
        ADC_Channel_0,
        1,
        ADC_SampleTime_239Cycles5);

    /* 5. 开启 ADC，使用已有毫秒延时留足上电稳定时间。 */
    ADC_Cmd(ADC1, ENABLE);
    Com_Time_DelayMs(2U);

    /* 6. 复位校准寄存器，等待硬件完成。 */
    ADC_ResetCalibration(ADC1);
    start_ms = Com_Time_GetMs();

    while (ADC_GetResetCalibrationStatus(ADC1) != RESET)
    {
        if ((uint32_t)(Com_Time_GetMs() - start_ms) >= ADC_CAL_TIMEOUT_MS)
        {
            ADC_Cmd(ADC1, DISABLE);
            return ERROR;
        }
    }

    /* 7. 启动内部校准，等待硬件完成。 */
    ADC_StartCalibration(ADC1);
    start_ms = Com_Time_GetMs();

    while (ADC_GetCalibrationStatus(ADC1) != RESET)
    {
        if ((uint32_t)(Com_Time_GetMs() - start_ms) >= ADC_CAL_TIMEOUT_MS)
        {
            ADC_Cmd(ADC1, DISABLE);
            return ERROR;
        }
    }

    s_adc1_ready = ENABLE;
    return SUCCESS;
}

/**
 * @brief 软件触发 ADC1/PA0 转换一次，轮询完成后返回原始值。
 * @param raw 输出地址；仅成功时写入 0～4095，失败时保持原值。
 * @return SUCCESS：取得本次转换结果；ERROR：参数、初始化状态或超时错误。
 * @note 仅供主循环串行调用；SysTick 必须正常运行，不能在中断或关中断时调用。
 *       不得由其他代码、DMA 或中断同时启动 ADC1 或读取其 DR。
 *       超时会关闭 ADC 并标记不可读；重新初始化成功后才能继续测量。
 */
ErrorStatus Driver_ADC1_ReadRaw(uint16_t *raw)
{
    uint32_t start_ms; /* 本次转换的等待起点。 */

    if ((raw == NULL) || (s_adc1_ready != ENABLE))
    {
        return ERROR;
    }

    /* 丢弃旧完成标志，确保等待的是接下来这次转换。 */
    ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
    start_ms = Com_Time_GetMs();
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);

    /* EOC = End Of Conversion；硬件完成后置位，不需要开启 ADC 中断。 */
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET)
    {
        if ((uint32_t)(Com_Time_GetMs() - start_ms) >= DRIVER_ADC1_READ_TIMEOUT_MS)
        {
            /* 终止异常转换，不让后续调用误取迟到的旧结果。 */
            ADC_Cmd(ADC1, DISABLE);
            s_adc1_ready = DISABLE;
            ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
            return ERROR;
        }
    }

    /* 读取规则组 DR；F103 读取 DR 时会清除 EOC。 */
    *raw = ADC_GetConversionValue(ADC1);
    return SUCCESS;
}
