/*
 * 任务8: OpenHarmony 系统驱动实验 - GPIO 驱动超声波 (HC-SR04)
 *
 * 学生要求(软件定时器版):
 *   创建2个软件定时器:
 *     定时器1: 周期3秒, 控制超声波测距并打印距离
 *     定时器2: 周期1秒, 打印当前系统tick值(hi_get_tick)
 */
#include <stdio.h>
#include <stdlib.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_watchdog.h"
#include "hi_io.h"
#include "hi_time.h"

/* HC-SR04 超声波测距模块通过GPIO7(TRIG)和GPIO8(ECHO)连接到3861 */
#define GPIO_8      8    /* ECHO 输入引脚 */
#define GPIO_7      7    /* TRIG 输出引脚 */
#define GPIO_FUNC   0

/* 测距功能实现: 触发脉冲 -> 测量回响高电平时间 -> 换算距离(cm) */
static float GetDistance(void)
{
    static unsigned long start_time = 0, time = 0;
    float distance = 0.0;
    WifiIotGpioValue value = WIFI_IOT_GPIO_VALUE0;
    unsigned int flag = 0;

    hi_io_set_func(GPIO_8, GPIO_FUNC);

    GpioSetDir(GPIO_8, WIFI_IOT_GPIO_DIR_IN);    /* GPIO8设置为输入引脚 */
    GpioSetDir(GPIO_7, WIFI_IOT_GPIO_DIR_OUT);   /* GPIO7设置为输出引脚 */

    /* GPIO_7输出一个脉冲触发信号到超声波测距模块 至少10us */
    GpioSetOutputVal(GPIO_7, WIFI_IOT_GPIO_VALUE1);
    hi_udelay(20);
    GpioSetOutputVal(GPIO_7, WIFI_IOT_GPIO_VALUE0);

    /* 超声波测距模块接收到触发信号后, 输出回响信号(高电平)到GPIO_8 */
    while (1) {
        GpioGetInputVal(GPIO_8, &value);

        /* 测量回响信号(高电平)时间 */
        if (value == WIFI_IOT_GPIO_VALUE1 && flag == 0) {
            start_time = hi_get_us();
            flag = 1;
        }
        if (value == WIFI_IOT_GPIO_VALUE0 && flag == 1) {
            time = hi_get_us() - start_time;
            start_time = 0;
            flag = 0;
            break;
        }
    }

    /* 距离=高电平时间*0.034/2 */
    distance = time * 0.034 / 2;
    return distance;
}

/* 软件定时器1回调: 每3秒测一次距离 */
static void DistanceTimerCallback(void *arg)
{
    (void)arg;
    float distance = GetDistance();
    printf("distance is %.1f (cm)\r\n", distance);
}

/* 软件定时器2回调: 打印当前tick值 */
static void TickTimerCallback(void *arg)
{
    (void)arg;
    printf("current tick: %u\r\n", hi_get_tick());
}

/* 任务入口 */
static void Hcsr04(void)
{
    WatchDogDisable();   /* 关闭看门狗 */

    /* 创建软件定时器1: 周期300个系统tick(10ms/tick)=3秒 */
    osTimerId_t timer1 = osTimerNew((osTimerFunc_t)DistanceTimerCallback,
                                    osTimerPeriodic, NULL, NULL);
    if (timer1 == NULL) {
        printf("Failed to create timer1!\n");
        return;
    }
    osTimerStart(timer1, 300);

    /* 创建软件定时器2: 周期100个系统tick=1秒, 打印当前tick值 */
    osTimerId_t timer2 = osTimerNew((osTimerFunc_t)TickTimerCallback,
                                    osTimerPeriodic, NULL, NULL);
    if (timer2 == NULL) {
        printf("Failed to create timer2!\n");
        return;
    }
    osTimerStart(timer2, 100);
}

APP_FEATURE_INIT(Hcsr04);   /* 任务启动 */
