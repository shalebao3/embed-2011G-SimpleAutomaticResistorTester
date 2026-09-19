#!/usr/bin/env python3
"""主机端分层回归测试：执行真实模块代码，但用桩代替 STM32 外设。

从仓库根目录运行：python3 tests/test_layers.py
仅验证软件配置/调用顺序/错误分支，不代表已做上板或模拟精度验收。
"""
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
FW = ROOT / 'firmware'

# 仅声明测试用的 SPL/CMSIS 子集；生产构建继续使用固定版本的标准库。
STUB = r'''
#ifndef TEST_STM32F10X_H
#define TEST_STM32F10X_H
#include <stdint.h>
typedef enum {ERROR = 0, SUCCESS = 1} ErrorStatus;
typedef enum {DISABLE = 0, ENABLE = 1} FunctionalState;
typedef enum {RESET = 0, SET = 1} FlagStatus;
typedef struct { uint32_t unused; } GPIO_TypeDef;
typedef struct { uint32_t unused; } ADC_TypeDef;
#define GPIOA ((GPIO_TypeDef *)(uintptr_t)1)
#define GPIOC ((GPIO_TypeDef *)(uintptr_t)3)
#define ADC1 ((ADC_TypeDef *)(uintptr_t)1)
#define RCC_APB2Periph_GPIOA 0x0004U
#define RCC_APB2Periph_GPIOC 0x0010U
#define RCC_APB2Periph_ADC1  0x0200U
#define RCC_PCLK2_Div6 0x8000U
#define GPIO_Pin_0 0x0001U
#define GPIO_Pin_13 0x2000U
#define GPIO_Mode_AIN 0U
#define GPIO_Mode_Out_PP 0x10U
#define GPIO_Speed_2MHz 2U
#define ADC_Mode_Independent 0U
#define ADC_ExternalTrigConv_None 0xE0000U
#define ADC_DataAlign_Right 0U
#define ADC_Channel_0 0U
#define ADC_SampleTime_239Cycles5 7U
typedef struct { uint16_t GPIO_Pin; uint32_t GPIO_Speed; uint32_t GPIO_Mode; } GPIO_InitTypeDef;
typedef struct {
    uint32_t ADC_Mode;
    FunctionalState ADC_ScanConvMode;
    FunctionalState ADC_ContinuousConvMode;
    uint32_t ADC_ExternalTrigConv;
    uint32_t ADC_DataAlign;
    uint8_t ADC_NbrOfChannel;
} ADC_InitTypeDef;
extern uint32_t SystemCoreClock;
void SystemCoreClockUpdate(void);
uint32_t SysTick_Config(uint32_t ticks);
void __disable_irq(void);
void RCC_APB2PeriphClockCmd(uint32_t mask, FunctionalState state);
void RCC_ADCCLKConfig(uint32_t divider);
void GPIO_StructInit(GPIO_InitTypeDef *init);
void GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init);
void GPIO_SetBits(GPIO_TypeDef *port, uint16_t pins);
void GPIO_ResetBits(GPIO_TypeDef *port, uint16_t pins);
void ADC_DeInit(ADC_TypeDef *adc);
void ADC_StructInit(ADC_InitTypeDef *init);
void ADC_Init(ADC_TypeDef *adc, ADC_InitTypeDef *init);
void ADC_RegularChannelConfig(ADC_TypeDef *adc, uint8_t channel, uint8_t rank, uint8_t sample);
void ADC_Cmd(ADC_TypeDef *adc, FunctionalState state);
void ADC_ResetCalibration(ADC_TypeDef *adc);
FlagStatus ADC_GetResetCalibrationStatus(ADC_TypeDef *adc);
void ADC_StartCalibration(ADC_TypeDef *adc);
FlagStatus ADC_GetCalibrationStatus(ADC_TypeDef *adc);
#endif
'''

ADC_CASE = r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "Driver_ADC.h"
#include "Com_Time.h"
static int mode, step, disabled, reset_polls, calibration_polls;
static uint32_t now;
static void next(int expected) { assert(++step == expected); }
void RCC_APB2PeriphClockCmd(uint32_t m, FunctionalState s) { next(1); assert(m == (RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1) && s == ENABLE); }
void RCC_ADCCLKConfig(uint32_t d) { next(2); assert(d == RCC_PCLK2_Div6); }
void ADC_DeInit(ADC_TypeDef *a) { next(3); assert(a == ADC1); }
void GPIO_StructInit(GPIO_InitTypeDef *p) { next(4); memset(p, 0, sizeof(*p)); }
void GPIO_Init(GPIO_TypeDef *g, GPIO_InitTypeDef *p) { next(5); assert(g == GPIOA && p->GPIO_Pin == GPIO_Pin_0 && p->GPIO_Mode == GPIO_Mode_AIN); }
void ADC_StructInit(ADC_InitTypeDef *p) { next(6); memset(p, 0, sizeof(*p)); }
void ADC_Init(ADC_TypeDef *a, ADC_InitTypeDef *p) {
    next(7); assert(a == ADC1);
    assert(p->ADC_Mode == ADC_Mode_Independent && p->ADC_ScanConvMode == DISABLE);
    assert(p->ADC_ContinuousConvMode == DISABLE && p->ADC_ExternalTrigConv == ADC_ExternalTrigConv_None);
    assert(p->ADC_DataAlign == ADC_DataAlign_Right && p->ADC_NbrOfChannel == 1);
}
void ADC_RegularChannelConfig(ADC_TypeDef *a, uint8_t ch, uint8_t rank, uint8_t sample) {
    next(8); assert(a == ADC1 && ch == ADC_Channel_0 && rank == 1 && sample == ADC_SampleTime_239Cycles5);
}
void ADC_Cmd(ADC_TypeDef *a, FunctionalState s) { assert(a == ADC1); if (s == ENABLE) next(9); else ++disabled; }
void Com_Time_DelayMs(uint32_t ms) { next(10); assert(ms == 2U); now += ms; }
uint32_t Com_Time_GetMs(void) { return now++; }
void ADC_ResetCalibration(ADC_TypeDef *a) { next(11); assert(a == ADC1); }
FlagStatus ADC_GetResetCalibrationStatus(ADC_TypeDef *a) {
    assert(a == ADC1 && ++reset_polls < 100);
    return (mode == 1 || mode == 3 || reset_polls < 3) ? SET : RESET;
}
void ADC_StartCalibration(ADC_TypeDef *a) { next(12); assert(a == ADC1); }
FlagStatus ADC_GetCalibrationStatus(ADC_TypeDef *a) {
    assert(a == ADC1 && ++calibration_polls < 100);
    return (mode == 2 || mode == 4 || calibration_polls < 3) ? SET : RESET;
}
int main(int argc, char **argv) {
    assert(argc == 2); mode = atoi(argv[1]);
    now = mode >= 3 ? UINT32_MAX - 7U : 0U;
    ErrorStatus result = Driver_ADC1_Init();
    if (mode == 0) {
        assert(result == SUCCESS && disabled == 0 && step == 12);
        assert(reset_polls == 3 && calibration_polls == 3);
    } else if (mode == 1 || mode == 3) {
        assert(result == ERROR && disabled == 1 && step == 11);
        assert(reset_polls == 10 && calibration_polls == 0);
    } else {
        assert(result == ERROR && disabled == 1 && step == 12);
        assert(reset_polls == 3 && calibration_polls == 10);
    }
    return 0;
}
'''

LED_CASE = r'''
#include <assert.h>
#include <string.h>
#include "Interface_LED.h"
#include "stm32f10x.h"
static char calls[20]; static unsigned n;
static void add(char c) { calls[n++] = c; }
void RCC_APB2PeriphClockCmd(uint32_t m, FunctionalState s) { assert(m == RCC_APB2Periph_GPIOC && s == ENABLE); add('C'); }
void GPIO_StructInit(GPIO_InitTypeDef *p) { memset(p, 0, sizeof(*p)); add('S'); }
void GPIO_Init(GPIO_TypeDef *g, GPIO_InitTypeDef *p) {
    assert(g == GPIOC && p->GPIO_Pin == GPIO_Pin_13);
    assert(p->GPIO_Speed == GPIO_Speed_2MHz && p->GPIO_Mode == GPIO_Mode_Out_PP); add('I');
}
void GPIO_SetBits(GPIO_TypeDef *g, uint16_t p) { assert(g == GPIOC && p == GPIO_Pin_13); add('H'); }
void GPIO_ResetBits(GPIO_TypeDef *g, uint16_t p) { assert(g == GPIOC && p == GPIO_Pin_13); add('L'); }
int main(void) {
    Interface_LED_Init(); Interface_LED_Set(true); Interface_LED_Set(false);
    assert(strcmp(calls, "CSIHLH") == 0); return 0;
}
'''

APP_CASE = r'''
#include <assert.h>
#include "App_ResistorTester.h"
#include "Driver_ADC.h"
#include "Interface_LED.h"
#include "Com_Time.h"
static int step; static ErrorStatus adc_result;
void Interface_LED_Init(void) { assert(step++ == 0); }
ErrorStatus Driver_ADC1_Init(void) { assert(step++ == 1); return adc_result; }
void Interface_LED_Set(bool on) { assert(step++ == (on ? 2 : 4)); }
void Com_Time_DelayMs(uint32_t ms) { assert(ms == 500U); assert(step == 3 || step == 5); ++step; }
int main(void) {
    adc_result = SUCCESS; assert(App_ResistorTester_Init() == SUCCESS);
    App_ResistorTester_Task(); assert(step == 6);
    step = 0; adc_result = ERROR;
    assert(App_ResistorTester_Init() == ERROR && step == 2); return 0;
}
'''

TIME_CASE = r'''
#include <assert.h>
#include "Com_Time.h"
#include "stm32f10x_it.h"
/* 白盒测试回绕边界；不在生产头文件暴露设置计数值的接口。 */
#include "Com_Time.c"
uint32_t SystemCoreClock = 72000000U;
static uint32_t configured_ticks, config_result;
uint32_t SysTick_Config(uint32_t ticks) { configured_ticks = ticks; return config_result; }
int main(void) {
    assert(Com_Time_Init() == SUCCESS && configured_ticks == 72000U);
    assert(Com_Time_GetMs() == 0U);
    SysTick_Handler(); assert(Com_Time_GetMs() == 1U);
    Com_Time_DelayMs(0U); assert(Com_Time_GetMs() == 1U);
    s_ms_ticks = UINT32_MAX;
    SysTick_Handler(); assert(Com_Time_GetMs() == 0U);
    SysTick_Handler(); assert(Com_Time_GetMs() == 1U);
    config_result = 1U; assert(Com_Time_Init() == ERROR);
    return 0;
}
'''

MAIN_CASE = r'''
#include <assert.h>
#include <setjmp.h>
#include <string.h>
#include "main.h"
#include "Com_Time.h"
#include "App_ResistorTester.h"
#define main firmware_entry
#include "main.c"
#undef main
static jmp_buf escape;
static char calls[8]; static unsigned n; static int mode;
void SystemCoreClockUpdate(void) { calls[n++] = 'S'; }
ErrorStatus Com_Time_Init(void) { calls[n++] = 'T'; return mode == 1 ? ERROR : SUCCESS; }
ErrorStatus App_ResistorTester_Init(void) { calls[n++] = 'A'; return mode == 2 ? ERROR : SUCCESS; }
void App_ResistorTester_Task(void) { calls[n++] = 'L'; longjmp(escape, 1); }
void __disable_irq(void) { calls[n++] = 'E'; longjmp(escape, 2); }
int main(void) {
    for (mode = 0; mode < 3; ++mode) {
        n = 0; memset(calls, 0, sizeof(calls));
        if (setjmp(escape) == 0) firmware_entry();
        assert(strcmp(calls, mode == 0 ? "STAL" : mode == 1 ? "STE" : "STAE") == 0);
    }
    return 0;
}
'''

class LayerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cc = shlex.split(os.environ.get('CC', 'cc'))
        if not cls.cc or not shutil.which(cls.cc[0]):
            raise RuntimeError('需要可执行的主机 C 编译器；请安装 cc/gcc/clang 或设置 CC。')
        cls.temp = tempfile.TemporaryDirectory(prefix='2011g-layer-tests-')
        cls.work = Path(cls.temp.name)
        (cls.work / 'stm32f10x.h').write_text(STUB, encoding='utf-8')

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def compile_and_run(self, name, code, sources=(), args=()):
        test_file = self.work / (name + '.c')
        executable = self.work / name
        test_file.write_text(code, encoding='utf-8')
        includes = [self.work] + [FW / d for d in ('Core/Inc', 'Core/Src', 'App', 'Driver', 'Interface', 'Common')]
        cmd = self.cc + ['-std=c11', '-Wall', '-Wextra', '-Werror', '-O2']
        cmd += ['-I' + str(p) for p in includes]
        cmd += [str(test_file)] + [str(FW / p) for p in sources] + ['-o', str(executable)]
        subprocess.run(cmd, check=True, timeout=30)
        subprocess.run([str(executable), *map(str, args)], check=True, timeout=5)

    def test_adc_success(self):
        self.compile_and_run('adc_ok', ADC_CASE, ['Driver/Driver_ADC.c'], [0])

    def test_adc_reset_timeout(self):
        self.compile_and_run('adc_reset_timeout', ADC_CASE, ['Driver/Driver_ADC.c'], [1])

    def test_adc_calibration_timeout(self):
        self.compile_and_run('adc_cal_timeout', ADC_CASE, ['Driver/Driver_ADC.c'], [2])

    def test_adc_reset_timeout_across_wrap(self):
        self.compile_and_run('adc_reset_wrap', ADC_CASE, ['Driver/Driver_ADC.c'], [3])

    def test_adc_calibration_timeout_across_wrap(self):
        self.compile_and_run('adc_cal_wrap', ADC_CASE, ['Driver/Driver_ADC.c'], [4])

    def test_led_polarity_and_full_initialization(self):
        self.compile_and_run('led', LED_CASE, ['Interface/Interface_LED.c'])

    def test_app_error_propagation_and_500ms_cadence(self):
        self.compile_and_run('app', APP_CASE, ['App/App_ResistorTester.c'])

    def test_time_tick_wrap_and_irq_forwarding(self):
        self.compile_and_run('time', TIME_CASE, ['Core/Src/stm32f10x_it.c'])

    def test_main_initialization_order_and_error_paths(self):
        self.compile_and_run('entry', MAIN_CASE)

    def test_headers_are_self_contained(self):
        for i, header in enumerate(('App_ResistorTester.h', 'Driver_ADC.h', 'Interface_LED.h', 'Com_Time.h', 'stm32f10x_it.h')):
            with self.subTest(header=header):
                self.compile_and_run('header_' + str(i), f'#include "{header}"\n#include "{header}"\nint main(void) {{ return 0; }}\n')

    def test_layer_boundaries_and_single_tick_owner(self):
        sources = [p for d in ('Core', 'App', 'Driver', 'Interface', 'Common') for p in (FW / d).rglob('*.c')]
        content = '\n'.join(p.read_text(encoding='utf-8') for p in sources)
        self.assertEqual(content.count('void SysTick_Handler(void)'), 1)
        self.assertEqual(content.count('static volatile uint32_t s_ms_ticks'), 1)
        self.assertNotIn('g_ms_ticks', content)
        for directory in ('App', 'Driver', 'Interface', 'Common'):
            for path in (FW / directory).glob('*.[ch]'):
                self.assertNotIn('#include "main.h"', path.read_text(encoding='utf-8'))
        for path in (FW / 'Core/Src/main.c', FW / 'App/App_ResistorTester.c'):
            text = path.read_text(encoding='utf-8')
            self.assertNotIn('GPIO_InitTypeDef', text)
            self.assertNotIn('ADC_InitTypeDef', text)

if __name__ == '__main__':
    unittest.main(verbosity=2)
