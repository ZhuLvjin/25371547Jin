/*
 * 任务12: OpenHarmony 系统驱动实验 - SHT20温湿度传感器采集温度
 * 知识点: IIC 读写 + 信号量(Semaphore)多任务同步
 * 现象: Thread1每3秒释放信号量 -> Thread2(读温湿度)与Thread3抢占信号量
 */
#include <stdio.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "hal_bsp_sht20.h"

osSemaphoreId_t sem1;

/***** 任务1: 释放信号量 *****/
static void *thread1(const char *arg)
{
    (void)arg;
    while (1) {
        /* 释放sem1信号量, 让thread2与thread3获得信号量后执行 */
        osSemaphoreRelease(sem1);
        printf("\r\n");
        printf("Thread1释放信号量\r\n");
        osDelay(300);     /* 延时3秒 */
    }
    return NULL;
}

/***** 任务2: 读取温湿度 *****/
static void *thread2(const char *arg)
{
    (void)arg;
    float temperature = 0, humidity = 0;
    printf("i2c_sht20_demo()\r\n");
    SHT20_Init();       /* SHT20初始化 */

    while (1) {
        /* 等待sem1信号量 */
        osSemaphoreAcquire(sem1, osWaitForever);
        SHT20_ReadData(&temperature, &humidity);
        printf("temperature = %.2f  humidity = %.2f\r\n", temperature, humidity);
        printf("Thread2 得到信号量\r\n");
        osDelay(1);     /* 延时10ms */
    }
    return NULL;
}

/***** 任务3: 抢占信号量 *****/
static void *thread3(const char *arg)
{
    (void)arg;
    while (1) {
        /* 等待sem1信号量 */
        osSemaphoreAcquire(sem1, osWaitForever);
        printf("Thread3 得到信号量\r\n");
        osDelay(1);     /* 延时10ms */
    }
    return NULL;
}

/***** 任务创建 *****/
static void i2c_sht20_demo(void)
{
    osThreadAttr_t attr;
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 1024 * 4;

    attr.name = "thread1";
    attr.priority = 25;
    if (osThreadNew((osThreadFunc_t)thread1, NULL, &attr) == NULL) {
        printf("Failed to create thread1!\n");
    }

    attr.name = "thread2";
    attr.priority = 25;
    if (osThreadNew((osThreadFunc_t)thread2, NULL, &attr) == NULL) {
        printf("Failed to create thread2!\n");
    }

    attr.name = "thread3";
    attr.priority = 25;
    if (osThreadNew((osThreadFunc_t)thread3, NULL, &attr) == NULL) {
        printf("Failed to create thread3!\n");
    }

    sem1 = osSemaphoreNew(4, 0, NULL);    /* 创建信号量, 初始值为0, 最大计数4 */
    if (sem1 == NULL) {
        printf("Failed to create Semaphore1!\n");
    }
}

APP_FEATURE_INIT(i2c_sht20_demo);   /* 任务启动 */
