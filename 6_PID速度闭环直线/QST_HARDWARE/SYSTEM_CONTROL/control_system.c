#include "control_system.h"
#include "encoder.h"
#include "motor.h"
#include "speed_ctrl.h"

u8 Car_run = 0;          //小车运行标志 1运行 0停止
int OverflowTime = 20;   //速度环周期 20ms(起步修正更快,减小起步偏转)
volatile u32 millis = 0; //记录毫秒数

/***********************************************
函数功能：系统控制函数(速度闭环核心,每20ms执行)
入口参数：无
返回  值：无
***********************************************/
void System_Control(void)
{
    int encL, encR, pwmL, pwmR, target, tgt20;
    static u8 last_run = 0xFF;

    encL = Read_Encoder(2);   //读取20ms左轮脉冲
    encR = Read_Encoder(3);   //读取20ms右轮脉冲

    if(Car_run == 1)          //运行:速度闭环
    {
        if(last_run != 1) Speed_PI_Init();            //刚启动时清零PI状态并给初始PWM
        target = Soft_Target();                       //目标值斜坡(脉冲/100ms)
        tgt20  = target / 5;                          //换算成本周期(20ms)的目标
        pwmL = Incremental_PI_L(encL, tgt20);         //左轮PI
        pwmR = Incremental_PI_R(encR, tgt20);         //右轮PI
        Set_Pwm(pwmL, pwmR);                          //输出(左轮,右轮)
    }
    else                      //停止
    {
        target = 0;
        pwmL = 0;
        pwmR = 0;
        Set_Pwm(0, 0);
    }
    last_run = Car_run;

#if PRINT_TELEMETRY
    //打印遥测:当前目标 左实测 右实测 左PWM 右PWM
    printf("T=%d L=%d R=%d PL=%d PR=%d\r\n", target, encL, encR, pwmL, pwmR);
#endif
}

/**
  * @brief  系统滴答定时器中断服务函数(每1ms进入)
  * @param  None
  * @retval None
  */
void SysTick_Handler(void)
{
    millis++;                       //毫秒数加1
    if (millis % OverflowTime == 0) //满20ms
    {
        millis = 0;
        System_Control();           //执行一次速度闭环
    }
}
