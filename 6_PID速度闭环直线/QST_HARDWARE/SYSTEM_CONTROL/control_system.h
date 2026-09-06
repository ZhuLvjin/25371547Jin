#ifndef __CONTROL_SYSTEM_H
#define __CONTROL_SYSTEM_H
#include "sys.h"

//============== 运行模式配置 ==============
#define AUTO_RUN        1     //1:上电自动跑(不用串口)  0:需串口发START才跑
#define PRINT_TELEMETRY 1     //1:每100ms打印遥测      0:不打印

extern u8 Car_run;      //小车运行标志 1运行 0停止

void System_Control(void);

#endif
