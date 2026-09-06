/*
 * 串口端口可控舵机版 v3 (按官方写法: 显式 UartInit UART0)
 * 开机打印横幅 + uart init 结果, 便于判断
 * 指令: 0 / 45 / 90 / 135 / 180
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_uart.h"
#include "hi_io.h"
#include "hi_time.h"

#define UART_PORT  WIFI_IOT_UART_IDX_0
#define SERVO_GPIO WIFI_IOT_IO_NAME_GPIO_2

void set_angle(unsigned int duty)
{
    GpioSetDir(SERVO_GPIO, WIFI_IOT_GPIO_DIR_OUT);
    GpioSetOutputVal(SERVO_GPIO, 1);
    hi_udelay(duty);
    GpioSetOutputVal(SERVO_GPIO, 0);
    hi_udelay(20000 - duty);
}

void engine_run(unsigned int duty)
{
    for (int i = 0; i < 50; i++) {
        set_angle(duty);
    }
}

unsigned int angle2duty(int angle)
{
    switch (angle) {
        case 0:   return 500;
        case 45:  return 1000;
        case 90:  return 1500;
        case 135: return 2000;
        case 180: return 2500;
        default:  return 0;
    }
}

static void UartTask(void)
{
    unsigned char buf[16];

    printf("=== SG90 UART v3 READY, send 0/45/90/135/180 ===\r\n");

    while (1) {
        memset(buf, 0, sizeof(buf));
        int len = UartRead(UART_PORT, buf, sizeof(buf) - 1);
        if (len > 0) {
            printf("RX(%d): ", len);
            for (int i = 0; i < len; i++) {
                printf("%02X ", buf[i]);
            }
            printf("\r\n");

            int angle = atoi((char *)buf);
            unsigned int duty = angle2duty(angle);
            if (duty == 0) {
                printf("bad cmd, send 0/45/90/135/180\r\n");
            } else {
                printf("OK angle=%d\r\n", angle);
                engine_run(duty);
                printf("done\r\n");
            }
        }
        osDelay(20);
    }
}

static void AppInit(void)
{
    /* 舵机 GPIO 初始化 */
    GpioInit();
    IoSetFunc(SERVO_GPIO, WIFI_IOT_IO_FUNC_GPIO_2_GPIO);
    GpioSetDir(SERVO_GPIO, WIFI_IOT_GPIO_DIR_OUT);

    /* 显式初始化 UART0 (115200, 8N1), 官方推荐写法 */
    WifiIotUartAttribute uartAttr;
    uartAttr.baudRate = 115200;
    uartAttr.dataBits = WIFI_IOT_UART_DATA_BIT_8;
    uartAttr.stopBits = WIFI_IOT_UART_STOP_BIT_1;
    uartAttr.parity = WIFI_IOT_UART_PARITY_NONE;
    uartAttr.pad = 'M';

    WifiIotUartExtraAttr extraAttr;
    memset(&extraAttr, 0, sizeof(extraAttr));
    extraAttr.txFifoLine = WIFI_IOT_FIFO_LINE_ONE_EIGHT;
    extraAttr.rxFifoLine = WIFI_IOT_FIFO_LINE_ONE_EIGHT;
    extraAttr.flowFifoLine = WIFI_IOT_FIFO_LINE_ONE_EIGHT;
    extraAttr.txBlock = WIFI_IOT_UART_BLOCK_STATE_NONE_BLOCK;
    extraAttr.rxBlock = WIFI_IOT_UART_BLOCK_STATE_NONE_BLOCK;
    extraAttr.txBufSize = 1024;
    extraAttr.rxBufSize = 1024;
    extraAttr.txUseDma = WIFI_IOT_UART_NONE_DMA;
    extraAttr.rxUseDma = WIFI_IOT_UART_NONE_DMA;

    unsigned int ret = UartInit(UART_PORT, &uartAttr, &extraAttr);
    printf("UartInit ret=%u\r\n", ret);

    osThreadAttr_t attr;
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 1024 * 4;
    attr.name = "UartTask";
    attr.priority = 25;

    if (osThreadNew((osThreadFunc_t)UartTask, NULL, &attr) == NULL) {
        printf("failed create task\r\n");
    }
}

APP_FEATURE_INIT(AppInit);
