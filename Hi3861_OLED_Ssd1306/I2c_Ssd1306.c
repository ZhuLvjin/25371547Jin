/*
 * 任务11: OpenHarmony 系统驱动实验 - OLED 显示字符串 (SSD1306, I2C)
 * 学生作业: 将"鸿蒙先锋号"以字符形式显示在OLED上
 * 说明: 中文字符为 16x16 点阵(GB2312), 由 WQY 字体渲染生成, 见 hal_bsp_chs_font.h
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "hal_bsp_ssd1306.h"

/* 在 OLED 上显示"鸿蒙先锋号"(5个16x16汉字) */
static void ShowHongMengPioneer(void)
{
    for (uint8_t i = 0; i < 5; i++) {
        SSD1306_ShowChineseByIndex((uint8_t)(i * 16), 0, i);
    }
}

/* 任务: 清屏 -> 显示"鸿蒙先锋号" + 底部时钟 */
void Task1(void)
{
    uint8_t displayBuff[20] = {0};
    uint8_t hour = 16, min = 0, sec = 0;

    SSD1306_Init();                 /* OLED 显示屏初始化 */
    SSD1306_CLS();                  /* 清屏 */
    ShowHongMengPioneer();          /* 作业: 显示"鸿蒙先锋号" */

    while (1) {
        sec++;
        if (sec > 59) {
            sec = 0;
            min++;
        }
        if (min > 59) {
            min = 0;
            hour++;
        }
        if (hour > 23) {
            hour = 0;
        }
        memset(displayBuff, 0, sizeof(displayBuff));   /* 清除displayBuff中字符串 */
        sprintf((char *)displayBuff, "%02d:%02d:%02d", hour, min, sec);
        SSD1306_ShowStr(32, 1, (uint8_t *)displayBuff, 16);  /* 下半屏显示时钟 */
        sleep(1);    /* 1 s */
    }
}

/* 任务创建 */
static void i2c_ssd1306_demo(void)
{
    osThreadAttr_t options;
    options.name = "thread_1";
    options.attr_bits = 0;
    options.cb_mem = NULL;
    options.cb_size = 0;
    options.stack_mem = NULL;
    options.stack_size = 1024;
    options.priority = osPriorityNormal;
    osThreadId_t Task1_ID;
    Task1_ID = osThreadNew((osThreadFunc_t)Task1, NULL, &options);
    if (Task1_ID != NULL) {
        printf("ID = %p, Create Task1_ID is OK!\r\n", Task1_ID);
    }
}

APP_FEATURE_INIT(i2c_ssd1306_demo);
