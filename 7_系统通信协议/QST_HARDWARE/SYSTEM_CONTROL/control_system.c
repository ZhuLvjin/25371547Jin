#include "control_system.h"
#include "encoder.h"
#include "motor.h"
#include "speed_ctrl.h"

//============================================================
// 协议解析结果: 目标转速(转/s),由数据帧更新
//============================================================
float Target_MotorA = 0.0;   //左轮目标转速(转/s)
float Target_MotorB = 0.0;   //右轮目标转速(转/s)

//速度环目标(脉冲/100ms)
int Target_PulseA = 0;
int Target_PulseB = 0;

int OverflowTime = 20;       //速度环周期 20ms
volatile u32 millis = 0;     //记录毫秒数

/***********************************************
函数功能：把转速(转/s)设置成速度环的目标脉冲(脉冲/100ms)
说明：电机一圈 700*4=2800 脉冲, 换算: 转/s * 2800/(1000/100) = 转/s * 280
入口参数：mA:左轮转速(转/s)  mB:右轮转速(转/s)
返回  值：无
***********************************************/
void CalculateAndControlMotors(float mA, float mB)
{
    Target_PulseA = mA * 280;   //左轮目标(脉冲/100ms)
    Target_PulseB = mB * 280;   //右轮目标(脉冲/100ms)
}

/***********************************************
函数功能：系统控制函数(每20ms执行: 解析协议帧 + 速度闭环)
入口参数：无
返回  值：无
***********************************************/
void System_Control(void)
{
    int encL, encR, pwmL, pwmR;

    if(uart_rec_flag)   //收到一帧数据 → 解析方向/速度
    {
        Target_MotorA = CAR_buff[1]/100.00;   //左轮转速绝对值(转/s)
        Target_MotorB = CAR_buff[3]/100.00;   //右轮转速绝对值(转/s)

        //方向还原: 1=反转 → 取负
        if(CAR_buff[0]==1) Target_MotorA = -Target_MotorA;
        if(CAR_buff[2]==1) Target_MotorB = -Target_MotorB;

        //倒车灯: 两轮都倒车 → 尾灯亮
        if(CAR_buff[0]==1 && CAR_buff[2]==1) R_led_mode();
        else R_led_CLC();

        uart_rec_flag = 0;
        CalculateAndControlMotors(Target_MotorA, Target_MotorB);   //设置转速, PID介入
    }

    //每20ms: 读编码器 → 增量式PI → 输出(左右独立目标)
    encL = Read_Encoder(2);   //左轮实测脉冲(20ms)
    encR = Read_Encoder(3);   //右轮实测脉冲(20ms)

    pwmL = Incremental_PI_L(encL, Target_PulseA/5);   //脉冲/100ms → 每20ms目标
    pwmR = Incremental_PI_R(encR, Target_PulseB/5);

    Set_Pwm(pwmL, pwmR);      //输出(左轮,右轮)

#if PRINT_TELEMETRY
    //打印遥测: 左目标 右目标 左实测 右实测 左PWM 右PWM(目标单位转/s)
    printf("MA=%.1f MB=%.1f L=%d R=%d PL=%d PR=%d\r\n", Target_MotorA, Target_MotorB, encL, encR, pwmL, pwmR);
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
        System_Control();           //执行一次(解析帧+速度闭环)
    }
}
