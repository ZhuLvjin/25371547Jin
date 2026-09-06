#include "speed_ctrl.h"

/***** PI参数(20ms周期,从小到大慢慢调!) *****/
#define VELOCITY_KP   10   //比例: 大了响应快但容易震荡
#define VELOCITY_KI   1    //积分: 消除稳态误差

static int Bias_L = 0, Pwm_L = 0, Last_bias_L = 0;
static int Bias_R = 0, Pwm_R = 0, Last_bias_R = 0;

/***********************************************
函数功能：清零PI内部状态(启动/切换时调用)
入口参数：无
返回  值：无
***********************************************/
void Speed_PI_Init(void)
{
    Bias_L = 0; Pwm_L = 0; Last_bias_L = 0;
    Bias_R = 0; Pwm_R = 0; Last_bias_R = 0;
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
