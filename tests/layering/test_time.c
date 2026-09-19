#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Com_Time.h"
#include "stm32f10x_it.h"

/* 测试中直接纳入实现以设置回绕边界；生产代码仍保持计数 static 私有。 */
#include "Com_Time.c"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #expr); \
    exit(EXIT_FAILURE); } } while (0)

uint32_t SystemCoreClock = 72000000U;
static uint32_t config_result, config_ticks;
uint32_t SysTick_Config(uint32_t ticks) { config_ticks = ticks; return config_result; }

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    const char *name = argv[1];
    if (strcmp(name, "init_ok") == 0 || strcmp(name, "init_error") == 0) {
        config_result = strcmp(name, "init_error") == 0 ? 1U : 0U;
        CHECK(Com_Time_Init() == (config_result ? ERROR : SUCCESS));
        CHECK(config_ticks == 72000U && Com_Time_GetMs() == 0U);
    } else if (strcmp(name, "tick") == 0) {
        CHECK(Com_Time_GetMs() == 0U);
        Com_Time_Tick(); CHECK(Com_Time_GetMs() == 1U);
        SysTick_Handler(); CHECK(Com_Time_GetMs() == 2U);
    } else if (strcmp(name, "wrap") == 0) {
        s_ms_ticks = UINT32_MAX - 1U;
        const uint32_t start = Com_Time_GetMs();
        Com_Time_Tick(); CHECK(Com_Time_GetMs() == UINT32_MAX);
        SysTick_Handler(); CHECK(Com_Time_GetMs() == 0U);
        CHECK((uint32_t)(Com_Time_GetMs() - start) == 2U);
    } else if (strcmp(name, "delay_zero") == 0) {
        Com_Time_DelayMs(0U);
        CHECK(Com_Time_GetMs() == 0U);
    } else { CHECK(0); }
    printf("PASS: time_%s\n", name);
    return EXIT_SUCCESS;
}
