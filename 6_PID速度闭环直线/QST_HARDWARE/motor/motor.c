#include "motor.h"

/***********************************************
函数功能：初始化电机方向
入口参数：无
返回  值：无
***********************************************/
void Motor_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); //使能B端口时钟
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14|GPIO_Pin_13;    //端口配置
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;   //推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;  //50M
    GPIO_Init(GPIOB, &GPIO_InitStructure);      //根据设定参数初始化GPIOB

    AIN=0;
    BIN=0;
}

/***********************************************
函数功能：初始化定时器PWM
入口参数：arr:自动重装载值  psc:预分频值
返回  值：无
***********************************************/
void PWM_Init(u16 arr,u16 psc)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    TIM_OCInitTypeDef  TIM_OCInitStructure;

    Motor_Init();

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);        //使能TIM4时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);       //使能GPIOB外设时钟

    //设置该引脚为复用功能,输出TIM4 CH1/CH2的PWM波形
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6|GPIO_Pin_7; //PB6=TIM4_CH1 PB7=TIM4_CH2
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; //复用推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    TIM_TimeBaseStructure.TIM_Period = arr;    //设置在下一个更新事件装入活动的自动重装载寄存器周期的值
    TIM_TimeBaseStructure.TIM_Prescaler = psc;  //设置用来作为TIMx时钟频率除数的预分频值
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;  //设置时钟分割:TDCK=Tck_tim
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;   //TIM向上计数模式
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);  //根据TIM_TimeBaseInitStruct中指定的参数初始化TIMx的时间基数单位

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;  //选择定时器模式:TIM脉冲宽度调制模式2
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;  //比较输出使能
    TIM_OCInitStructure.TIM_Pulse = 0;   //设置待装入捕获比较寄存器的脉冲值
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;  //输出极性:TIM输出比较极性高
    TIM_OC1Init(TIM4, &TIM_OCInitStructure);   //根据TIM_OCInitStruct中指定的参数初始化外设TIMx
    TIM_OC2Init(TIM4, &TIM_OCInitStructure);   //根据TIM_OCInitStruct中指定的参数初始化外设TIMx

    //注:TIM4为通用定时器,没有MOE主输出使能位,所以不需要TIM_CtrlPWMOutputs(TIM4,ENABLE)

    TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Enable);  //CH1预装载使能
    TIM_OC2PreloadConfig(TIM4, TIM_OCPreload_Enable);  //CH2预装载使能

    TIM_ARRPreloadConfig(TIM4, ENABLE);  //使能TIMx在ARR上的预装载寄存器

    TIM_Cmd(TIM4, ENABLE);  //使能TIM4
}

/***********************************************
函数功能：绝对值(占空比计算用)
入口参数：a:带符号的占空比值
返回  值：无符号绝对值
***********************************************/
u32 myabs(long int a)
{
    u32 temp;
    if(a<0)
        temp=-a;
    else
        temp=a;
    return temp;
}

/***********************************************
函数功能：设置左右电机速度和方向
入口参数：moto1:左轮(-7199~7199)  moto2:右轮(-7199~7199)
返回  值：无
***********************************************/
void Set_Pwm(int moto1,int moto2)
{
    //AIN/BIN、PWM1/PWM2在motor.h中有定义
    if(moto2>=0) {
        AIN=0;
        PWM1=myabs(moto2);
    } else {
        AIN=1;
        PWM1=7199-myabs(moto2);
    }

    if(moto1>0) {
        BIN=0;
        PWM2=myabs(moto1);
    } else {
        BIN=1;
        PWM2=7199-myabs(moto1);
    }
}
