#ifndef __SPEED_CTRL_H
#define __SPEED_CTRL_H
#include "sys.h"

//============================================================
// 速度闭环(增量式PI)模块
//============================================================
extern int Target_Speed;      //目标速度(脉冲/100ms),左右轮相同

void Speed_PI_Init(void);     //清零PI内部状态(启动时调用)
int  Soft_Target(void);       //目标值斜坡(软启动,返回当前生效目标)
int  Incremental_PI_L(int Encoder, int Target);   //左轮PI,返回PWM
int  Incremental_PI_R(int Encoder, int Target);   //右轮PI,返回PWM

#endif
