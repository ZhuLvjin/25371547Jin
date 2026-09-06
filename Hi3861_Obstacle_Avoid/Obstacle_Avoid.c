/*
 * 综合任务: 超声波避障 + 红外避黑胶带
 * 传感器: HC-SR04(GPIO7触发/GPIO8回声) 前方障碍; TCRT红外(GPIO13/14) 地面黑胶带
 * 执行: UART2 0xFC 帧 -> STM32 电机
 * 逻辑优先级: 障碍(超声波<20cm) > 黑胶带(红外低电平) > 直行
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_uart.h"
#include "wifiiot_watchdog.h"
#include "hi_io.h"
#include "hi_time.h"

/* ====== 参数(可调) ====== */
#define GPIOL         13         /* 左红外 */
#define GPIOR         14         /* 右红外 */
#define OBSTACLE_CM   20         /* 障碍判定距离(cm) */
#define SPEED_FWD     60         /* 直行速度 */
#define SPEED_TURN    70         /* 转向速度 */
#define SPEED_BACK    45         /* 后退速度 */
#define AVOID_TURN_MS 800        /* 绕障/掉头转向时长 */
#define AVOID_BACK_MS 300        /* 后退时长 */

uint8_t uart_sendbuf[20];

/* 电机帧 -> STM32 */
void stm32motor_control(int motorA, int motorB)
{
    uint8_t A_dir = 0, B_dir = 0;
    if (motorA < 0) { A_dir = 1; motorA = -motorA; }
    if (motorB < 0) { B_dir = 1; motorB = -motorB; }
    if (motorA > 150) motorA = 150;
    if (motorB > 150) motorB = 150;
    uart_sendbuf[0] = 0xFC;
    uart_sendbuf[1] = A_dir;
    uart_sendbuf[2] = (uint8_t)motorA;
    uart_sendbuf[3] = B_dir;
    uart_sendbuf[4] = (uint8_t)motorB;
    uart_sendbuf[5] = 0xFD;
    UartWrite(WIFI_IOT_UART_IDX_2, (unsigned char *)uart_sendbuf, 6);
}

/* 超声波测距 (带超时保护) */
static float GetDistance(void)
{
    static unsigned long start_time = 0, time = 0;
    WifiIotGpioValue value = WIFI_IOT_GPIO_VALUE0;
    unsigned int flag = 0, cnt = 0;

    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_8, WIFI_IOT_IO_FUNC_GPIO_8_GPIO);
    GpioSetDir(WIFI_IOT_IO_NAME_GPIO_8, WIFI_IOT_GPIO_DIR_IN);
    GpioSetDir(WIFI_IOT_IO_NAME_GPIO_7, WIFI_IOT_GPIO_DIR_OUT);

    GpioSetOutputVal(WIFI_IOT_IO_NAME_GPIO_7, WIFI_IOT_GPIO_VALUE1);
    hi_udelay(20);
    GpioSetOutputVal(WIFI_IOT_IO_NAME_GPIO_7, WIFI_IOT_GPIO_VALUE0);

    while (1) {
        GpioGetInputVal(WIFI_IOT_IO_NAME_GPIO_8, &value);
        if (value == WIFI_IOT_GPIO_VALUE1 && flag == 0) {
            start_time = hi_get_us();
            flag = 1;
        }
        if (value == WIFI_IOT_GPIO_VALUE0 && flag == 1) {
            time = hi_get_us() - start_time;
            break;
        }
        if (++cnt > 100000) {      /* 超时保护(无回波) */
            time = 0;
            break;
        }
    }
    return time * 0.034f / 2;
}

/* 避障/避带主循环 */
static void ObstacleTask(void)
{
    printf("=== obstacle + black tape avoid start ===\r\n");

    while (1) {
        float d = GetDistance();
        WifiIotGpioValue l, r;
        GpioGetInputVal(GPIOL, &l);
        GpioGetInputVal(GPIOR, &r);
        int left_black = (l == WIFI_IOT_GPIO_VALUE0);
        int right_black = (r == WIFI_IOT_GPIO_VALUE0);

        if (d > 0 && d < OBSTACLE_CM) {
            /* 1. 前方障碍: 急停->后退->左转绕行 */
            printf("obstacle %.1fcm! stop->back->turn\r\n", d);
            stm32motor_control(0, 0);
            usleep(200 * 1000);
            stm32motor_control(-SPEED_BACK, -SPEED_BACK);
            usleep(AVOID_BACK_MS * 1000);
            stm32motor_control(-SPEED_TURN, SPEED_TURN);
            usleep(AVOID_TURN_MS * 1000);
            stm32motor_control(0, 0);
            usleep(200 * 1000);
        } else if (left_black && right_black) {
            /* 2. 双红外同时压胶带(车头正对): 后退->掉头 */
            printf("black tape ahead! back->turn\r\n");
            stm32motor_control(0, 0);
            usleep(200 * 1000);
            stm32motor_control(-SPEED_BACK, -SPEED_BACK);
            usleep(AVOID_BACK_MS * 1000);
            stm32motor_control(-SPEED_TURN, SPEED_TURN);
            usleep(AVOID_TURN_MS * 1000);
            stm32motor_control(0, 0);
            usleep(200 * 1000);
        } else if (left_black) {
            /* 3. 左侧压胶带: 右转避开 */
            printf("black tape left! turn right\r\n");
            stm32motor_control(SPEED_TURN, 0);
            usleep(100 * 1000);
        } else if (right_black) {
            /* 4. 右侧压胶带: 左转避开 */
            printf("black tape right! turn left\r\n");
            stm32motor_control(0, SPEED_TURN);
            usleep(100 * 1000);
        } else {
            /* 5. 无障碍无胶带: 直行 */
            stm32motor_control(SPEED_FWD, SPEED_FWD);
            usleep(100 * 1000);
        }
    }
}

/* 初始化 + 任务创建 */
static void Obstacle_Avoid(void)
{
    GpioInit();
    WatchDogDisable();

    /* 红外输入 */
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_13, WIFI_IOT_IO_FUNC_GPIO_13_GPIO);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_14, WIFI_IOT_IO_FUNC_GPIO_14_GPIO);
    GpioSetDir(WIFI_IOT_IO_NAME_GPIO_13, WIFI_IOT_GPIO_DIR_IN);
    GpioSetDir(WIFI_IOT_IO_NAME_GPIO_14, WIFI_IOT_GPIO_DIR_IN);

    /* 超声波 GPIO7 输出 */
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_7, WIFI_IOT_IO_FUNC_GPIO_7_GPIO);
    GpioSetDir(WIFI_IOT_IO_NAME_GPIO_7, WIFI_IOT_GPIO_DIR_OUT);

    /* UART2 -> STM32 */
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_11, WIFI_IOT_IO_FUNC_GPIO_11_UART2_TXD);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_12, WIFI_IOT_IO_FUNC_GPIO_12_UART2_RXD);
    WifiIotUartAttribute ua;
    ua.baudRate = 115200;
    ua.dataBits = WIFI_IOT_UART_DATA_BIT_8;
    ua.stopBits = WIFI_IOT_UART_STOP_BIT_1;
    ua.parity = WIFI_IOT_UART_PARITY_NONE;
    ua.pad = 'M';
    WifiIotUartExtraAttr ex;
    memset(&ex, 0, sizeof(ex));
    ex.txFifoLine = WIFI_IOT_FIFO_LINE_ONE_EIGHT;
    ex.rxFifoLine = WIFI_IOT_FIFO_LINE_ONE_EIGHT;
    ex.flowFifoLine = WIFI_IOT_FIFO_LINE_ONE_EIGHT;
    ex.txBlock = WIFI_IOT_UART_BLOCK_STATE_NONE_BLOCK;
    ex.rxBlock = WIFI_IOT_UART_BLOCK_STATE_NONE_BLOCK;
    ex.txBufSize = 256;
    ex.rxBufSize = 256;
    ex.txUseDma = WIFI_IOT_UART_NONE_DMA;
    ex.rxUseDma = WIFI_IOT_UART_NONE_DMA;
    UartInit(WIFI_IOT_UART_IDX_2, &ua, &ex);
    printf("UART2 init ok(115200)\r\n");

    osThreadAttr_t attr;
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 1024 * 4;
    attr.name = "ObstacleTask";
    attr.priority = 25;
    if (osThreadNew((osThreadFunc_t)ObstacleTask, NULL, &attr) == NULL) {
        printf("ObstacleTask create failed!\n");
    }
}

APP_FEATURE_INIT(Obstacle_Avoid);
