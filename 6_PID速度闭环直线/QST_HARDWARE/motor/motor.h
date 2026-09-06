#ifndef __MOTOR_H
#define __MOTOR_H
#include "sys.h"

//============================================================
// 电机方向与PWM端口定义(参考任务21文档电路图)
//  左电机: IA=PB7(TIM4_CH2 PWM)  IB=PB14(方向)
//  右电机: IA=PB6(TIM4_CH1 PWM)  IB=PB13(方向)
//============================================================
#define AIN  PBout(13)     //右电机方向(IB) PB13
#define BIN  PBout(14)     //左电机方向(IB) PB14
#define PWM1 TIM4->CCR1    //右电机速度 PWM (PB6)
#define PWM2 TIM4->CCR2    //左电机速度 PWM (PB7)

void Motor_Init(void);
void PWM_Init(u16 arr,u16 psc);
void Set_Pwm(int moto1,int moto2);

#endif
