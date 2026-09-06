/*
 * 任务6: OpenHarmony 系统驱动实验 - 红外对管收发 (循迹模块 TCRT)
 *
 * 学生要求(2个软件定时器):
 *   定时器1: 周期3秒, 打印 "hello QST"
 *   定时器2: 周期0.5秒, 扫描红外对管(left/right 黑/白), 观察现象
 * 硬件: TC_OUT_L -> IO13, TC_OUT_R -> IO14 (低电平=黑, 高电平=白)
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "ohos_init.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "cmsis_os2.h"
#include "hi_io.h"
#include "hi_time.h"

#define GPIOL 13
#define GPIOR 14

/* 定时器1回调: 每3秒打印 hello QST */
static void HelloTimerCallback(void *arg)
{
    (void)arg;
    printf("hello QST\r\n");
}

/* 获取红外传感器输出的电平高低 */
static void get_tcrt5000_value(void)
{
    WifiIotGpioValue id_status;

    GpioGetInputVal(GPIOL, &id_status);        /* 获取GPIO13引脚的输入电平值 */
    if (id_status == WIFI_IOT_GPIO_VALUE0) {
        printf("left black\r\n");              /* 输出低电平=左红外识别到黑色 */
    } 
	else {
        printf("left white\r\n");
    }

    GpioGetInputVal(GPIOR, &id_status);        /* 获取GPIO14引脚的输入电平值 */
    if (id_status == WIFI_IOT_GPIO_VALUE0) {
        printf("right black\r\n");             /* 输出低电平=右红外识别到黑色 */
    } 
	else {
        printf("right white\r\n");
    }
}

/* 定时器2回调: 每0.5秒扫描红外对管 */
static void TCRTScanTimerCallback(void *arg)
{
    (void)arg;
    get_tcrt5000_value();
}

static void TCRTTask(void)
{
    printf("start test tcrt5000\r\n");
    printf("Timer1: hello QST 3s, Timer2: ir scan 0.5s\r\n");

    /* 创建定时器1: 3秒打印 hello QST (Hi3861 1U=10ms, 300U=3s) */
    osTimerId_t id1 = osTimerNew((osTimerFunc_t)HelloTimerCallback,
                                 osTimerPeriodic, NULL, NULL);
    if (id1 == NULL) {
        printf("Timer1 could not be created!\n");
    } 
	else {
        if (osTimerStart(id1, 300) != osOK) {
            printf("Timer1 could not be started!\n");
        } 
		else {
            printf("Timer1 开启成功!(hello QST 3s)\r\n");
        }
    }

    /* 创建定时器2: 0.5秒扫描红外对管 (50U=0.5s) */
    osTimerId_t id2 = osTimerNew((osTimerFunc_t)TCRTScanTimerCallback,
                                 osTimerPeriodic, NULL, NULL);
    if (id2 == NULL) {
        printf("Timer2 could not be created!\n");
    } 
	else {
        if (osTimerStart(id2, 50) != osOK) {
            printf("Timer2 could not be started!\n");
        } 
		else {
            printf("Timer2 开启成功!(红外扫描 0.5s)\r\n");
        }
    }
}

/* 任务入口 */
static void TCRT(void)
{
    GpioInit();                                             /* 初始化GPIO */
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_13, WIFI_IOT_IO_FUNC_GPIO_13_GPIO);  /* 复用为普通GPIO功能 */
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_14, WIFI_IOT_IO_FUNC_GPIO_14_GPIO);

    GpioSetDir(WIFI_IOT_IO_NAME_GPIO_13, WIFI_IOT_GPIO_DIR_IN);  /* 设置GPIO13为输入 */
    GpioSetDir(WIFI_IOT_IO_NAME_GPIO_14, WIFI_IOT_GPIO_DIR_IN);  /* 设置GPIO14为输入 */

    osThreadAttr_t attr;
    attr.name = "TCRTTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 1024;
    attr.priority = 25;
    if (osThreadNew((osThreadFunc_t)TCRTTask, NULL, &attr) == NULL) {
        printf("Failed to create TCRTTask!\n");
    }
}

APP_FEATURE_INIT(TCRT);   /* 任务启动 */
