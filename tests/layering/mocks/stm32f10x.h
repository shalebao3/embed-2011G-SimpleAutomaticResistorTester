#ifndef TEST_STM32F10X_H
#define TEST_STM32F10X_H

/* 仅供主机测试使用：替代硬件接口，不进入 firmware 的 include 搜索路径。 */
#include <stdint.h>

typedef enum { ERROR = 0, SUCCESS = 1 } ErrorStatus;
typedef enum { DISABLE = 0, ENABLE = 1 } FunctionalState;
typedef enum { RESET = 0, SET = 1 } FlagStatus;
typedef struct { uint32_t unused; } GPIO_TypeDef;
typedef struct { uint32_t unused; } ADC_TypeDef;
typedef struct {
    uint16_t GPIO_Pin;
    uint32_t GPIO_Speed;
    uint32_t GPIO_Mode;
} GPIO_InitTypeDef;
typedef struct {
    uint32_t ADC_Mode;
    FunctionalState ADC_ScanConvMode;
    FunctionalState ADC_ContinuousConvMode;
    uint32_t ADC_ExternalTrigConv;
    uint32_t ADC_DataAlign;
    uint8_t ADC_NbrOfChannel;
} ADC_InitTypeDef;

extern GPIO_TypeDef mock_gpio_a;
extern ADC_TypeDef mock_adc1;
#define GPIOA (&mock_gpio_a)
#define ADC1 (&mock_adc1)
#define GPIO_Pin_0 ((uint16_t)1U)
#define GPIO_Mode_AIN 0U
#define RCC_APB2Periph_GPIOA 0x4U
#define RCC_APB2Periph_ADC1 0x200U
#define RCC_PCLK2_Div6 0x8000U
#define ADC_Mode_Independent 0U
#define ADC_ExternalTrigConv_None 0xE0000U
#define ADC_DataAlign_Right 0U
#define ADC_FLAG_EOC ((uint8_t)0x02U)
#define ADC_Channel_0 0U
#define ADC_SampleTime_239Cycles5 7U

extern uint32_t SystemCoreClock;
uint32_t SysTick_Config(uint32_t ticks);
void SystemCoreClockUpdate(void);
void __disable_irq(void);
void RCC_APB2PeriphClockCmd(uint32_t peripheral, FunctionalState state);
void RCC_ADCCLKConfig(uint32_t divider);
void GPIO_StructInit(GPIO_InitTypeDef *config);
void GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *config);
void ADC_DeInit(ADC_TypeDef *adc);
void ADC_StructInit(ADC_InitTypeDef *config);
void ADC_Init(ADC_TypeDef *adc, ADC_InitTypeDef *config);
void ADC_RegularChannelConfig(ADC_TypeDef *adc, uint8_t channel, uint8_t rank, uint8_t sample);
void ADC_Cmd(ADC_TypeDef *adc, FunctionalState state);
void ADC_ResetCalibration(ADC_TypeDef *adc);
FlagStatus ADC_GetResetCalibrationStatus(ADC_TypeDef *adc);
void ADC_StartCalibration(ADC_TypeDef *adc);
FlagStatus ADC_GetCalibrationStatus(ADC_TypeDef *adc);

void ADC_ClearFlag(ADC_TypeDef *adc, uint8_t flag);
void ADC_SoftwareStartConvCmd(ADC_TypeDef *adc, FunctionalState state);
FlagStatus ADC_GetFlagStatus(ADC_TypeDef *adc, uint8_t flag);
uint16_t ADC_GetConversionValue(ADC_TypeDef *adc);

#endif
