/**
 * @file    main.c
 * @brief   Application entry point — BSP showcase demo
 * @version 1.1
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-29
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 */

#include "mks_tft28.h"
#include "demo_app.h"

int main(void)
{
    MKS_TFT28_Init();
    DemoApp_Run();
}
