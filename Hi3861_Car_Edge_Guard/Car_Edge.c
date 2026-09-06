/*
 * 综合任务: 桌面防掉落小车 (组合: 电机控制 + 红外对管边缘检测)
 *
 * 分工: Hi3861 通过 UART2(GPIO11 TX/GPIO12 RX, 115200) 向 STM32 发送 0xFC 电机帧;
 *       STM32 解析后驱动 L9110S 电机 (同时保留了原 START/STOPP 命令)。
 *
 * 逻辑(状态机):
 *   直行 -> 红外对管(GPIO13/14)检测到桌边(低电平=无反射) -> 急停 -> 后退 ->
 *   原地掉头(左轮反转/右轮正转) -> 再直行 -> 循环, 保证小车不掉下桌子
 *
 * 注意: 桌面建议为浅色(白色反射强=高电平), 桌沿外无反射=低电平=边缘
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
#define SPEED_FWD   60      /* 直行速度 0~150 */
#define SPEED_BACK  45      /* 后退速度 */
#define SPEED_TURN  70      /* 转弯速度 */
#define TURN_MS     1500    /* 原地掉头时长(ms), 约180度, 按需调 */
#define BACK_MS     700     /* 后退时长(ms), 离开边缘 */
#define STOP_MS     300     /* 急停保持(ms) */

#define GPIOL 13            /* 左红外对管 */
#define GPIOR 14            /* 右红外对管 */

uint8_t uart_sendbuf[20];

/* 向 STM32 发送电机控制帧: motorA/B -150~150, 负数为反转 */
void stm32motor_control(int motorA, int motorB)
{
    uint8_t A_dir = 0;
    uint8_t B_dir = 0;
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
    if (motorA > 150) motorA = 150;
    if (motorB > 150) motorB = 150;

    uart_sendbuf[0] = 0xFC;   /* 帧头 */
    uart_sendbuf[1] = A_dir;  /* 左轮方向 0正转 1反转 */
    uart_sendbuf[2] = (uint8_t)motorA;  /* 左轮速度 */
    uart_sendbuf[3] = B_dir;  /* 右轮方向 */
    uart_sendbuf[4] = (uint8_t)motorB;  /* 右轮速度 */
    uart_sendbuf[5] = 0xFD;   /* 帧尾 */
    UartWrite(WIFI_IOT_UART_IDX_2, (unsigned char *)uart_sendbuf, 6);
}

/* 检测桌边: 左右任一对管读到低电平(无反射)即认为是边缘 */
static int IsTableEdge(void)
{
    WifiIotGpioValue l, r;
    GpioGetInputVal(GPIOL, &l);
    GpioGetInputVal(GPIOR, &r);
    return (l == WIFI_IOT_GPIO_VALUE0 || r == WIFI_IOT_GPIO_VALUE0);
}

static void CarEdgeTask(void)
{
    printf("=== table edge guard start ===\r\n");

    while (1) {
        /* 1. 直行 */
        printf("GO straight\r\n");
        stm32motor_control(SPEED_FWD, SPEED_FWD);
        usleep(100 * 1000);

        /* 2. 检测边缘(连续2次确认, 防抖动) */
        if (IsTableEdge()) {
            usleep(50 * 1000);
            if (!IsTableEdge()) {
                continue;      /* 误检, 继续直行 */
            }
            printf("EDGE detected! stop...\r\n");
            stm32motor_control(0, 0);           /* 急停 */
            usleep(STOP_MS * 1000);

            printf("back...\r\n");
            stm32motor_control(-SPEED_BACK, -SPEED_BACK);  /* 后退 */
            usleep(BACK_MS * 1000);

            printf("turn(180)...\r\n");
            stm32motor_control(-SPEED_TURN, SPEED_TURN);   /* 左反右正=原地掉头 */
            usleep(TURN_MS * 1000);

            stm32motor_control(0, 0);
            usleep(200 * 1000);
        }
    }
}

/* 初始化: UART2 + 红外输入 + 看门狗 */
static void CarEdge(void)
{
    GpioInit();
    /* 红外对管 GPIO13/14 输入 */
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_13, WIFI_IOT_IO_FUNC_GPIO_13_GPIO);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_14, WIFI_IOT_IO_FUNC_GPIO_14_GPIO);
    GpioSetDir(WIFI_IOT_IO_NAME_GPIO_13, WIFI_IOT_GPIO_DIR_IN);
    GpioSetDir(WIFI_IOT_IO_NAME_GPIO_14, WIFI_IOT_GPIO_DIR_IN);

    /* UART2 与 STM32 通信 (115200) */
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
    unsigned int ret = UartInit(WIFI_IOT_UART_IDX_2, &uart_attr, &extraAttr);
    printf("UART2 init ret=%u\r\n", ret);

    WatchDogDisable();   /* 关闭看门狗, 防长时间运行复位 */

    osThreadAttr_t attr;
    attr.name = "CarEdgeTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 1024 * 4;
    attr.priority = 25;
    if (osThreadNew((osThreadFunc_t)CarEdgeTask, NULL, &attr) == NULL) {
        printf("Failed to create CarEdgeTask!\n");
    }
}

APP_FEATURE_INIT(CarEdge);
