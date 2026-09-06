/*
 * 第一阶段综合实验: 红外对管、超声波、舵机、蓝牙、UART、系统内核 多级联动
 * 要求:
 *   1. 舵机左右旋转测距 (45/90/135度 循环, 超声波测距)
 *   2. 前15秒红外对管寻线 (GPIO13/14 + 电机循迹)
 *   3. 15秒后蓝牙通信 (UART1 GPIO0/1 接蓝牙模块)
 *   4. 任务3 串口打印消息队列信息 (osMessageQueue)
 *   5. 任务1、2交替运行 (轮流往消息队列放数据)
 *
 * 电机走 UART2(GPIO11/12) 0xFC 帧 -> STM32 (需烧录打过补丁的 STM32 固件)
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

/* ====== 参数 ====== */
#define GPIOL          13     /* 左红外 */
#define GPIOR          14     /* 右红外 */
#define SERVO_IO       WIFI_IOT_IO_NAME_GPIO_2   /* 舵机 GPIO2 */
#define MQ_LEN         10
#define BLE_BAUD       115200   /* 蓝牙模块波特率, 收不到可改 9600 */
#define LINE_FOLLOW_S  15       /* 前15秒寻线 */

/* 消息类型 */
#define MSG_DIST  1
#define MSG_BLE   2

typedef struct {
    uint8_t  type;       /* MSG_DIST / MSG_BLE */
    uint16_t value;      /* 距离 x10 或 蓝牙数据长度 */
} MsgItem_t;

static osMessageQueueId_t mq;

/***** 舵机 PWM (软件模拟, 0.5~2.5ms) *****/
void servo_set(uint32_t duty)
{
    GpioSetDir(SERVO_IO, WIFI_IOT_GPIO_DIR_OUT);
    GpioSetOutputVal(SERVO_IO, 1);
    hi_udelay(duty);
    GpioSetOutputVal(SERVO_IO, 0);
    hi_udelay(20000 - duty);
}

void servo_turn(uint32_t duty)
{
    for (int i = 0; i < 50; i++) {
        servo_set(duty);
    }
}

/***** 超声波测距 (GPIO7 触发 / GPIO8 回声) *****/
float GetDistance(void)
{
    static unsigned long start_time = 0, time = 0;
    float distance = 0.0;
    WifiIotGpioValue value = WIFI_IOT_GPIO_VALUE0;
    unsigned int flag = 0;

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
            start_time = 0;
            break;
        }
    }
    distance = time * 0.034 / 2;
    return distance;
}

/***** 电机 0xFC 帧 -> STM32 (UART2) *****/
uint8_t uart_sendbuf[20];
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

/***** 任务1: 前15秒寻线, 之后舵机左右旋转测距 -> 消息队列 *****/
static void Task1(void)
{
    uint32_t start_t = 0;

    printf("[Task1] start\r\n");
    while (1) {
        /* ------- 前15秒: 红外对管寻线 ------- */
        if (start_t < LINE_FOLLOW_S) {
            WifiIotGpioValue l, r;
            GpioGetInputVal(GPIOL, &l);
            GpioGetInputVal(GPIOR, &r);
            if (l == WIFI_IOT_GPIO_VALUE0 && r == WIFI_IOT_GPIO_VALUE0) {
                stm32motor_control(0, 0);          /* 双黑 停 */
            } else if (l == WIFI_IOT_GPIO_VALUE0) {
                stm32motor_control(0, 50);         /* 左黑 左转 */
            } else if (r == WIFI_IOT_GPIO_VALUE0) {
                stm32motor_control(50, 0);         /* 右黑 右转 */
            } else {
                stm32motor_control(60, 60);        /* 全白 直行 */
            }
            usleep(100 * 1000);
            start_t++;
            continue;
        }

        /* ------- 15秒后: 舵机左右旋转测距 -> 队列 ------- */
        stm32motor_control(0, 0);                  /* 停车 */
        uint32_t angles[3] = {1000, 1500, 2000};   /* 45/90/135度 */
        for (int i = 0; i < 3; i++) {
            servo_turn(angles[i]);
            usleep(300 * 1000);
            float d = GetDistance();
            MsgItem_t msg = {MSG_DIST, (uint16_t)(d * 10)};
            osMessageQueuePut(mq, &msg, 0, osWaitForever);
            printf("[Task1] servo angle=%d dist=%.1fcm put queue\r\n",
                   (i == 0 ? 45 : (i == 1 ? 90 : 135)), d);
            usleep(500 * 1000);
        }
    }
}

/***** 任务2: 15秒后蓝牙通信 -> 消息队列 *****/
static void Task2(void)
{
    unsigned char buf[32];

    /* 等待15秒寻线阶段结束 */
    uint32_t t = 0;
    while (t < LINE_FOLLOW_S) {
        usleep(1000 * 1000);
        t++;
    }
    printf("[Task2] BLE start\r\n");

    while (1) {
        memset(buf, 0, sizeof(buf));
        int len = UartRead(WIFI_IOT_UART_IDX_1, buf, sizeof(buf) - 1);
        if (len > 0) {
            MsgItem_t msg = {MSG_BLE, (uint16_t)len};
            osMessageQueuePut(mq, &msg, 0, osWaitForever);
            printf("[Task2] BLE rx %d bytes: %.32s\r\n", len, buf);
        }
        usleep(100 * 1000);
    }
}

/***** 任务3: 每0.5秒消费消息队列并打印 *****/
static void Task3(void)
{
    printf("[Task3] start\r\n");
    while (1) {
        MsgItem_t msg;
        if (osMessageQueueGet(mq, &msg, NULL, 0) == osOK) {
            if (msg.type == MSG_DIST) {
                printf("[Task3] queue: DIST=%.1fcm\r\n", msg.value / 10.0);
            } else if (msg.type == MSG_BLE) {
                printf("[Task3] queue: BLE rx len=%d\r\n", msg.value);
            }
            printf("[Task3] queue count=%d space=%d\r\n",
                   (int)osMessageQueueGetCount(mq), (int)osMessageQueueGetSpace(mq));
        }
        usleep(500 * 1000);
    }
}

/* 初始化 + 任务创建 */
static void Sum_Experiment_First(void)
{
    GpioInit();
    WatchDogDisable();

    /* 红外对管输入 */
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_13, WIFI_IOT_IO_FUNC_GPIO_13_GPIO);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_14, WIFI_IOT_IO_FUNC_GPIO_14_GPIO);
    GpioSetDir(WIFI_IOT_IO_NAME_GPIO_13, WIFI_IOT_GPIO_DIR_IN);
    GpioSetDir(WIFI_IOT_IO_NAME_GPIO_14, WIFI_IOT_GPIO_DIR_IN);

    /* 舵机 GPIO2 输出 */
    IoSetFunc(SERVO_IO, WIFI_IOT_IO_FUNC_GPIO_2_GPIO);
    GpioSetDir(SERVO_IO, WIFI_IOT_GPIO_DIR_OUT);

    /* UART2 -> STM32 电机 (115200) */
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

    /* UART1 -> 蓝牙模块 (115200) */
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_0, WIFI_IOT_IO_FUNC_GPIO_0_UART1_TXD);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_1, WIFI_IOT_IO_FUNC_GPIO_1_UART1_RXD);
    ua.baudRate = BLE_BAUD;
    UartInit(WIFI_IOT_UART_IDX_1, &ua, &ex);
    printf("UART2/STM32 + UART1/BLE init ok\r\n");

    /* 消息队列 */
    mq = osMessageQueueNew(MQ_LEN, sizeof(MsgItem_t), NULL);
    if (mq == NULL) {
        printf("message queue create failed!\n");
    }

    osThreadAttr_t attr;
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 1024 * 4;
    attr.priority = 25;

    attr.name = "Task1";
    if (osThreadNew((osThreadFunc_t)Task1, NULL, &attr) == NULL) {
        printf("Task1 create failed!\n");
    }
    attr.name = "Task2";
    if (osThreadNew((osThreadFunc_t)Task2, NULL, &attr) == NULL) {
        printf("Task2 create failed!\n");
    }
    attr.name = "Task3";
    if (osThreadNew((osThreadFunc_t)Task3, NULL, &attr) == NULL) {
        printf("Task3 create failed!\n");
    }
}

APP_FEATURE_INIT(Sum_Experiment_First);
