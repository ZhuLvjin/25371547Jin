#ifndef __ENCODER_H
#define __ENCODER_H
#include "sys.h"

//============================================================
// 编码器参数(参考任务22文档,可通过手动测试修改!)
//============================================================
#define ENCODER_TIM_PERIOD      65535            //自动重装载值
#define ENCODER_PULSES_PER_REV  (700*4)          //电机转一圈的脉冲数(700脉冲 x 编码器模式3的四倍频)   //课程文档:电机upr=700 信频4
#define WHEEL_CIRCUMFERENCE_MM  65.0f            //车轮周长mm(需手动测量!!)

void Encoder_Init_TIM2(void);
void Encoder_Init_TIM3(void);
int  Read_Encoder(u8 TIMX);

#endif
