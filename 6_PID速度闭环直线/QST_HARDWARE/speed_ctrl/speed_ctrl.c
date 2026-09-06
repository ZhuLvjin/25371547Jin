#include "speed_ctrl.h"

int Target_Speed = 300;    //目标速度(脉冲/100ms),可用串口命令 Txxxx 修改(单位不变!)

/***** 起步/斜坡参数 *****/
#define TARGET_RAMP_STEP 8      //目标斜坡步进(每20ms加8 => 相当于40/100ms),起步平稳
#define START_PWM        2000   //启动瞬间给电机的初始PWM(两轮同时给,减少起步偏转)

/***** PI参数(20ms周期,从小到大慢慢调!) *****/
#define VELOCITY_KP   10   //比例: 大了响应快但容易震荡
#define VELOCITY_KI   1    //积分: 消除稳态误差

static int Bias_L = 0, Pwm_L = 0, Last_bias_L = 0;
static int Bias_R = 0, Pwm_R = 0, Last_bias_R = 0;
static int Current_Target = 0;    //当前生效的目标(斜坡过渡用,单位脉冲/100ms)

/***********************************************
函数功能：清零PI内部状态(启动/切换时调用)
入口参数：无
返回  值：无
***********************************************/
void Speed_PI_Init(void)
{
    Bias_L = 0; Pwm_L = START_PWM; Last_bias_L = 0;   //两轮同时给初始PWM
    Bias_R = 0; Pwm_R = START_PWM; Last_bias_R = 0;
    Current_Target = 0;                               //目标从0开始爬坡
}

/***********************************************
函数功能：目标值斜坡(软启动)
入口参数：无
返回  值：当前生效的目标速度(脉冲/100ms)
***********************************************/
int Soft_Target(void)
{
    if(Current_Target < Target_Speed)
    {
        Current_Target += TARGET_RAMP_STEP;
        if(Current_Target > Target_Speed) Current_Target = Target_Speed;
    }
    else if(Current_Target > Target_Speed)
    {
        Current_Target -= TARGET_RAMP_STEP;
        if(Current_Target < Target_Speed) Current_Target = Target_Speed;
    }
    return Current_Target;
}

/***********************************************
函数功能：左轮增量式PI
公式: pwm += Kp*[e(k)-e(k-1)] + Ki*e(k)
入口参数：Encoder:实测脉冲(20ms)  Target:目标脉冲(20ms)
返回  值：PWM(0~7199)
***********************************************/
int Incremental_PI_L(int Encoder, int Target)
{
    Bias_L = Target - Encoder;                                  //本次偏差
    Pwm_L += VELOCITY_KP*(Bias_L - Last_bias_L) + VELOCITY_KI*Bias_L;  //增量式PI
    if(Pwm_L > 7199) Pwm_L = 7199;                              //输出限幅
    if(Pwm_L < 0)    Pwm_L = 0;
    Last_bias_L = Bias_L;                                       //保存上次偏差
    return Pwm_L;
}

/***********************************************
函数功能：右轮增量式PI
入口参数：Encoder:实测脉冲(20ms)  Target:目标脉冲(20ms)
返回  值：PWM(0~7199)
***********************************************/
int Incremental_PI_R(int Encoder, int Target)
{
    Bias_R = Target - Encoder;                                  //本次偏差
    Pwm_R += VELOCITY_KP*(Bias_R - Last_bias_R) + VELOCITY_KI*Bias_R;  //增量式PI
    if(Pwm_R > 7199) Pwm_R = 7199;
    if(Pwm_R < 0)    Pwm_R = 0;
    Last_bias_R = Bias_R;
    return Pwm_R;
}
