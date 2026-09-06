/*
 * 任务13: OpenHarmony 系统驱动实验 - AP3216C 传感器采集光照强度
 * 定制需求:
 *   1) OLED 液晶屏显示光照/接近/红外数值与 LED 状态
 *   2) LED 灯控制:
 *      模式1: 晚上(光照低)灯亮, 白天灯关
 *      模式2: 晚上 且 有人靠近(接近值高)灯亮, 无人不亮
 * 接线(任务13原理图): LED=IO02(GPIO2), AP3216C=I2C0(SDA=GPIO10/SCL=GPIO9)
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "hal_bsp_ap3216c.h"
#include "hal_bsp_ssd1306.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"

/* ====== 可调参数 ====== */
#define LIGHT_MODE    2                 /* 1: 晚上自动亮灯; 2: 晚上+有人靠近才亮灯 */
#define LED_GPIO      WIFI_IOT_IO_NAME_GPIO_2   /* LED 接 IO02 */
#define ALS_NIGHT_TH  150                /* 光照 < 150 视为"晚上" */
#define PS_NEAR_TH    50                 /* 接近值 > 50 视为"有人靠近"(值越大越近) */
#define LED_ON_LEVEL  1                  /* 高电平点亮 */

/* LED 开关 */
static void LedSet(int on)
{
    GpioSetOutputVal(LED_GPIO, on ? LED_ON_LEVEL : !LED_ON_LEVEL);
}

void Task1(void)
{
    uint16_t ir = 0, als = 0, ps = 0;
    uint8_t buf[24] = {0};
    int ledOn = 0;

    /* 初始化: GPIO + LED + AP3216C + OLED */
    GpioInit();
    IoSetFunc(LED_GPIO, WIFI_IOT_IO_FUNC_GPIO_2_GPIO);
    GpioSetDir(LED_GPIO, WIFI_IOT_GPIO_DIR_OUT);

    AP3216C_Init();     /* 三合一传感器初始化 */

    SSD1306_Init();     /* OLED 初始化 */
    SSD1306_CLS();      /* 清屏 */

    printf("light demo start, LIGHT_MODE=%d\r\n", LIGHT_MODE);

    while (1) {
        AP3216C_ReadData(&ir, &als, &ps);

        /* LED 灯光控制逻辑 */
        if (LIGHT_MODE == 1) {
            /* 模式1: 晚上(光照低)亮, 白天关 */
            ledOn = (als < ALS_NIGHT_TH) ? 1 : 0;
        } else {
            /* 模式2: 晚上 且 有人靠近 才亮 */
            ledOn = ((als < ALS_NIGHT_TH) && (ps > PS_NEAR_TH)) ? 1 : 0;
        }
        LedSet(ledOn);

        printf("ir=%d als=%d ps=%d led=%s\r\n",
               ir, als, ps, ledOn ? "ON" : "OFF");

        /* OLED 显示: 上半行光照/接近, 下半行 LED/红外 */
        sprintf((char *)buf, "ALS:%4d PS:%4d", als, ps);
        SSD1306_ShowStr(0, 0, buf, 16);
        sprintf((char *)buf, "LED:%s IR:%4d", ledOn ? "ON " : "OFF", ir);
        SSD1306_ShowStr(0, 1, buf, 16);

        sleep(1);   /* 1s */
    }
}

/* 任务创建 */
static void i2c_ap3216c_demo(void)
{
    osThreadAttr_t options;
    options.name = "thread_1";
    options.attr_bits = 0U;
    options.cb_mem = NULL;
    options.cb_size = 0U;
    options.stack_mem = NULL;
    options.stack_size = 1024;
    options.priority = osPriorityNormal;
    osThreadId_t Task1_ID;
    Task1_ID = osThreadNew((osThreadFunc_t)Task1, NULL, &options);
    if (Task1_ID != NULL) {
        printf("Create Task1_ID is OK!\r\n");
    }
}

APP_FEATURE_INIT(i2c_ap3216c_demo);
