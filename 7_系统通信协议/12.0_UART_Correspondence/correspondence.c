/*
 * 12.0_UART_Correspondence: Hi3861 -> STM32 6字节协议帧 电机控制验证
 *
 * 协议帧: 0xFC | 左轮方向(0正/1反) | 左轮速度 | 右轮方向 | 右轮速度 | 0xFD
 * 速度范围 -150~150, 负数为反转
 *
 * 学生任务: 四种动作全部验证
 *   thread1: 前进1秒 -> 后退1秒 循环
 *   thread2: 左转1秒 -> 右转1秒 循环
 *   互斥锁(mutex)保护 UART 发送, 防止两线程组帧冲突
 *
 * 接线: 3861 GPIO11(UART2_TX) -> STM32 PA10(USART1_RX)
 *       3861 GPIO12(UART2_RX) -> STM32 PA9 (USART1_TX)
 *       共地, 双方 115200
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_uart.h"
#include "hi_io.h"
#include "hi_time.h"

uint8_t uart_sendbuf[20];
osMutexId_t mutex_id;

/* 核心: 组帧并发送给 STM32 */
void stm32motor_control(int motorA, int motorB)
{
    uint8_t A_dir = 0;
    uint8_t B_dir = 0;

    /* 负数 -> 方向=1(反转) 并取绝对值 */
    if (motorA < 0) {
        A_dir = 1;
        motorA = -motorA;
    } else {
        A_dir = 0;
    }
    if (motorB < 0) {
        B_dir = 1;
        motorB = -motorB;
    } else {
        B_dir = 0;
    }

    /* -150~150 限幅 */
    if (motorA > 150) motorA = 150;
    if (motorB > 150) motorB = 150;

    /* 组帧 */
    uart_sendbuf[0] = 0xFC;   /* 帧头 */
    uart_sendbuf[1] = A_dir;  /* 左轮方向 */
    uart_sendbuf[2] = (uint8_t)motorA;  /* 左轮速度 */
    uart_sendbuf[3] = B_dir;  /* 右轮方向 */
    uart_sendbuf[4] = (uint8_t)motorB;  /* 右轮速度 */
    uart_sendbuf[5] = 0xFD;   /* 帧尾 */
    UartWrite(WIFI_IOT_UART_IDX_2, (unsigned char *)uart_sendbuf, 6);
}

/* 动作封装 */
void car_forward(void)  { stm32motor_control(100, 100); }   /* 前进 */
void car_backward(void) { stm32motor_control(-100, -100); } /* 后退 */
void car_left(void)     { stm32motor_control(50, 150); }    /* 左转 */
void car_right(void)    { stm32motor_control(150, 50); }    /* 右转 */
void car_stop(void)     { stm32motor_control(0, 0); }       /* 停止 */

/***** 线程1: 前进1秒 -> 后退1秒 *****/
static void *thread1(void *arg)
{
    (void)arg;
    usleep(1000 * 1000);   /* 等1秒 */

    while (1) {
        osMutexAcquire(mutex_id, osWaitForever);
        printf("forward 1s\r\n");
        car_forward();
        osMutexRelease(mutex_id);
        usleep(1000 * 1000);

        osMutexAcquire(mutex_id, osWaitForever);
        printf("backward 1s\r\n");
        car_backward();
        osMutexRelease(mutex_id);
        usleep(1000 * 1000);
    }
    return NULL;
}

/***** 线程2: 左转1秒 -> 右转1秒 *****/
static void *thread2(void *arg)
{
    (void)arg;
    usleep(1000 * 1000);   /* 等1秒 */

    while (1) {
        osMutexAcquire(mutex_id, osWaitForever);
        printf("left 1s\r\n");
        car_left();
        osMutexRelease(mutex_id);
        usleep(1000 * 1000);

        osMutexAcquire(mutex_id, osWaitForever);
        printf("right 1s\r\n");
        car_right();
        osMutexRelease(mutex_id);
        usleep(1000 * 1000);
    }
    return NULL;
}

/***** 初始化 + 任务创建 *****/
static void correspondence(void)
{
    GpioInit();

    /* UART2 -> STM32 */
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_11, WIFI_IOT_IO_FUNC_GPIO_11_UART2_TXD);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_12, WIFI_IOT_IO_FUNC_GPIO_12_UART2_RXD);

    WifiIotUartAttribute uart_attr;
    uart_attr.baudRate = 115200;
    uart_attr.dataBits = WIFI_IOT_UART_DATA_BIT_8;
    uart_attr.stopBits = WIFI_IOT_UART_STOP_BIT_1;
    uart_attr.parity = WIFI_IOT_UART_PARITY_NONE;
    uart_attr.pad = 'M';

    WifiIotUartExtraAttr extraAttr;
    memset(&extraAttr, 0, sizeof(extraAttr));
    extraAttr.txFifoLine = WIFI_IOT_FIFO_LINE_ONE_EIGHT;
    extraAttr.rxFifoLine = WIFI_IOT_FIFO_LINE_ONE_EIGHT;
    extraAttr.flowFifoLine = WIFI_IOT_FIFO_LINE_ONE_EIGHT;
    extraAttr.txBlock = WIFI_IOT_UART_BLOCK_STATE_NONE_BLOCK;
    extraAttr.rxBlock = WIFI_IOT_UART_BLOCK_STATE_NONE_BLOCK;
    extraAttr.txBufSize = 256;
    extraAttr.rxBufSize = 256;
    extraAttr.txUseDma = WIFI_IOT_UART_NONE_DMA;
    extraAttr.rxUseDma = WIFI_IOT_UART_NONE_DMA;
    UartInit(WIFI_IOT_UART_IDX_2, &uart_attr, &extraAttr);
    printf("UART2 -> STM32 init ok(115200)\r\n");

    osThreadAttr_t attr;
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 1024 * 4;
    attr.priority = 25;

    attr.name = "thread1";
    if (osThreadNew((osThreadFunc_t)thread1, NULL, &attr) == NULL) {
        printf("thread1 create failed!\n");
    }

    attr.name = "thread2";
    if (osThreadNew((osThreadFunc_t)thread2, NULL, &attr) == NULL) {
        printf("thread2 create failed!\n");
    }

    mutex_id = osMutexNew(NULL);
    if (mutex_id == NULL) {
        printf("mutex create failed!\n");
    }
}

APP_FEATURE_INIT(correspondence);
