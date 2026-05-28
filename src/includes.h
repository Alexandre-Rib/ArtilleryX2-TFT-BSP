#ifndef INCLUDES_H
#define INCLUDES_H

#include "mks_tft28.h"          // pinout, LCD driver ID
#include "stm32f10x.h"         // MCU registers
#include "stm32f10x_conf.h"    // StdPeriph config
#include "HX8558.h"            // LCD driver

#include "lcd.h"               // LCD_WR_DATA, LCD_WR_REG, LCD_HardwareConfig
#include "delay.h"             // Delay_ms, Delay_us
#include "os_timer.h"          // OS_GetTimeMs
#include "LCD_Colors.h"        // WHITE, BLACK, RED, etc.
#include "LCD_Init.h"          // LCD_SetWindow, LCD_WR_16BITS_DATA

#endif