#include "delay.h"
#include "oled.h"
#include "stdio.h"
#include "Max30100.h"
#include "Max_30100_I2C.h"
#include "algorithm.h"
#include "uart.h"
#include "DS18B20.h"
#include "MLX90614.h"
#include "Adc.h"
#include "FunctionDef.h"
#include "usbio.h"
#include "hw_config.h"
#include "usb_init.h"

#include "function_oem.h"

#define CLK_PANEL_ENABLE    1
#ifdef CLK_PANEL_ENABLE
#include "graphics.h"

#endif


#define DS18B20           GPIO_Pin_7    //PA7

#define BEEP              GPIO_Pin_15

#define KEY_ECG           GPIO_Pin_8    //GPA8
#define KEY_PPG           GPIO_Pin_15   //GPA15
//#define KEY_PPG                GPIO_Pin_8       //GPB8
#define KEY_SPO2          GPIO_Pin_9    //GPB9

#if CLK_PANEL_ENABLE
extern char clk_panel_init(void);
extern int draw_panel_graphics(void);

extern int draw_panel_graphics(void);
extern volatile unsigned int dateTime[6];
extern volatile char update_panel_enable;
extern struct clk_panel_prop *get_panel_date(void);
extern int clear_pan_old_data(struct clk_panel_prop *clkPanle, enum LINE_TYPE type);
extern int draw_single_line(struct clk_panel_prop *clkPanle, char sn, char quadr, enum LINE_TYPE hand_t);
extern int digital_timer_data_init(struct timer_digital *digTime);
extern struct timer_digital digTimer;
#endif

__IO uint8_t Function_Select = 0;
__IO uint8_t Max30102_INT_Flag = 0;
TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
uint8_t      Max30102DataBuf[192];


uint16_t     IntStatus;
uint8_t      SampleLen = 0;

float        f_temp;   
int32_t      n_brightness;

void RCC_Configuration(void);
void TimerInit(void);

void InitKeyPad(void);
void BeepControl(Status_Ctl status);
void BeepPeriodOn(int enable, int Period);
void InitBeep(void);


int main(void)
{
	RCC_Configuration();                                    // 系统时钟配置函数 
	NVIC_Configuration();                                   // 嵌套向量中断控制器

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);	/* 启动 GPIOA时钟 */

	USART_Config(115200);                                   /* 串口配置*/ 

	OLED_Init();			                                //初始化OLED : SSD1306 (128 x 64 点阵)
	OLED_Clear();

#if 1
	InitBeep();	                                        /*蜂鸣器设置*/
	BeepPeriodOn(ON, 200);                                  /*  蜂鸣器响500ms */

#ifdef FUNC_OLED_QXY
	print_logo(LOGO_V1);
#endif
	delay_ms(1000);
	OLED_Clear();
#endif

#ifdef CLK_PANEL_ENABLE
	clk_panel_init();

	draw_panel_graphics();
#endif

	SetBoardTestMode(FUNCTION_SPO2);	                    /*默认为血氧模式*/

	ConfiureIoFor18b20(GPIOA, DS18B20);	                    /*配置18b20*/

	MLX90614_Init();                                        /*初始化MLX90614: Melexis MLX90614红外温度*/

	InitMax30100();                                         /*初始化Max30100:  脉搏血氧饱和、心率传感器*/
	ReadMax30100Status(&IntStatus);                         /*清除Max30100中断*/
	EXTI_ClearITPendingBit(EXTI_Line12);                    //??? 

#ifndef USB_FUNC_DISABLE
	USB_Interrupts_Config();	                            /* usb初始化*/
	Set_USBClock();
	USB_Init();
#endif

	InitKeyPad();	                                        /*按键设置*/

	TimerInit();                                            /*定时器配置*/

	ADC1_Init();                                            /*ECG、PPG、GSR初始化*/

	while(1)
	{		
		if( I2ComError_Flag == 1 ) {                         //I2C通讯错误，重新配置Max30100
			ResetI2c();
			InitMax30100();
			ReadMax30100Status(&IntStatus);
			EXTI_ClearITPendingBit(EXTI_Line6); 
		}
	}
}

void NVIC_Configuration(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;

	/* Enable the TIM2 global Interrupt */
	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

	NVIC_Init(&NVIC_InitStructure);

	NVIC_InitStructure.NVIC_IRQChannel = I2C2_ER_IRQn;

	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

void InitBeep()
{
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Pin   = BEEP  ;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;  /*  推挽输出*/

	GPIO_Init(GPIOB, &GPIO_InitStructure);
}

void BeepControl(Status_Ctl status)
{
	if( status == 1)   {  GPIO_SetBits(GPIOB ,   BEEP);  }
	else               {  GPIO_ResetBits(GPIOB , BEEP);  }
}

void BeepPeriodOn(int enable, int Period)
{
	BeepControl((Status_Ctl)enable);
	delay_ms(Period);
	BeepControl(!enable);
}

void InitKeyPad()
{
	GPIO_InitTypeDef GPIO_InitStructure;


	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);	   /* 打开GPIO时钟            */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	

	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;	           /* 上拉输入                    */
	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_8 | GPIO_Pin_15;  /* PA8:S2(ECG),  PA15: S3(PPG) */
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_8 | GPIO_Pin_9;   /* PB8:S4(GSR),  PB9: S5(x)       */
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}

#if CLK_PANEL_ENABLE
unsigned int get_max_day_for_month(unsigned int *dtime)
{
	unsigned int day, month, year;

	year  = *(dtime + 0);
	month = *(dtime + 1);
	if(month == 2) {
		/*给出一个年月,计算闰月*/
		/*闰月的定义:
		  普通年（不能被100整除的年份）能被4整除的为闰年；
		  世纪年（能被100整除的年份）能被400整除的是闰年
		 */
		if(((year % 4) == 0) && ((year % 100) != 0) || ((year % 400) == 0)) {
			day = 29;
		} else {
			day = 28;
		}
	} else if((month == 1) || (month == 3)  || (month == 5) || (month == 7) ||\
		  (month == 8) || (month == 10) || (month == 12)) {
		day = 31;
	} else {
		day = 30;
	}

	return day;
}

int update_dateTime(unsigned int *dtime)
{
	unsigned int maxday = 0;
	int ret = 0;

	(*(dateTime+5))++;

	if((*(dateTime+5)) < 60 ) {		// Second: 0~59
		return ret;
	} else {
		(*(dateTime+5)) = 0;
		(*(dateTime+4))++;
	}

	if((*(dateTime+4)) < 60) {		// minute: 0~59
		return ret;
	} else {
		(*(dateTime+4)) = 0;
		(*(dateTime+3))++;
	}

	if((*(dateTime+3)) < 24) {		// hour: 0~23
		return ret;
	} else {
		(*(dateTime+3)) = 0;
		(*(dateTime+2))++;
	}

	maxday = get_max_day_for_month(dtime);
	if((*(dateTime+2)) < maxday+1) {	// day: 1~28/29/30/31
		return ret;
	} else {
		(*(dateTime+2)) = 1;
		(*(dateTime+1))++;
	}

	if((*(dateTime+1)) < 12) {		// month: 1~12
		return ret;
	} else {
		(*(dateTime+1)) = 1;
		(*(dateTime+0))++;
	}

	if((*(dateTime+0)) < 2068) {		//year
		return ret;
	} else {
		(*(dateTime+0)) = 2018;
	}

	return ret;
}

int clear_panel_old_data(void)
{
	struct clk_panel_prop *clkPanle = NULL;
	int ret = 0;

	clkPanle = get_panel_date();
	if(clkPanle == NULL)
		return -EINVAL;

	ret = clear_pan_old_data(clkPanle, LINE_SECOND);
	ret = clear_pan_old_data(clkPanle, LINE_MINUTE);
	ret = clear_pan_old_data(clkPanle, LINE_HOUR);

//	ret = clear_pan_old_data(clkPanle, LINE_SCALE);

	return ret;
}

int update_panel_new_data(void)
{
	struct clk_panel_prop *clkPanle = NULL;
	char n = 0, sn = 0, tmp = 0, quadrant = 0; /* sn: 对应于endpoint_r30_on_plate的序号serialNum*/
	int ret=0;
	unsigned int time;

	clkPanle = get_panel_date();
	if(clkPanle == NULL)
		return -EINVAL;

#if 1
	draw_kinds_line(clkPanle, LINE_SECOND);
	draw_kinds_line(clkPanle, LINE_MINUTE);
	draw_kinds_line(clkPanle, LINE_HOUR);
#else
	while(1) {
		if(update_panel_enable) {
			update_panel_enable = 0;

		/*表盘数据不用更新*/
		/*刻度数据不用更新*/

		/*依据数字秒数据,更新秒针数据*/	/*要更新angle和象限*/ 	/*1s为6度*/
		/*	quadrant = second / 15 = 0 - 第1象限
			1 - 第4象限,关于x轴对称
			2 - 第3象限,旋转180度
			3 - 第2象限,关于y轴对称
		 */

		/*依据数字分数据,更新分针数据*/ /*要更新angle和象限*/

		/*依据数字时数据,更新时针数据*/ /*要更新angle和象限*/

		for(n = 5; n >= 3; n++) {
#if 0
			sn  = dateTime[n] % 15;
			tmp = dateTime[n] / 15;
#else
			time = dateTime[n];
			if(n == 3)     {	time *= 5;   }   /* hour jump 5 scale */

			sn  = time % 15;
			tmp = time / 15;
#endif
			switch(tmp) {
			  case 0:	quadrant = 1;  	break; /*第1象限*/
			  case 1:	quadrant = 4;  	break; /*第4象限*/
			  case 2:	quadrant = 3;  	break; /*第3象限*/
			  case 3:	quadrant = 2;  	break; /*第2象限*/
			  default:			break;
			}

			switch(n) {
			  case 5:	draw_single_line(clkPanle, sn, quadrant, LINE_SECOND);  break;
			  case 4:	draw_single_line(clkPanle, sn, quadrant, LINE_MINUTE);  break;
			  case 3:	draw_single_line(clkPanle, sn, quadrant, LINE_HOUR);	break;
			  default:								break;
			}
		}
	    }
	}
#endif

	return ret;
}
/*****************************************************************************
*                        定时中断处理函数                                                              *
*******************************************************************************/
void TIM2_IRQHandler(void)
{
#if CLK_PANEL_ENABLE
	static int cnt = 0;
	static int updatePanle = 0;

	cnt++;
	if(cnt >= 5) {
		cnt = 0;

		update_dateTime(dateTime);

		clear_panel_old_data();

		update_panel_new_data();

		//OLED_Clear();
		draw_panel_graphics();


//		digital_timer_data_init(dateTime);
		digital_timer_data_init(&digTimer);
	}


	updatePanle++;
	if(updatePanle >=20) {
		updatePanle = 0;

		update_panel_enable = 1;
	}
#endif


	if (TIM_GetITStatus(TIM2, TIM_IT_Update) == SET) {	//检测制定的中断是否发生
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);		//清除中断处理位	

#if !CLK_PANEL_ENABLE
		ShowTemperature();
		KeyPadProcess();
#endif
	}
}

void TimerInit(void)
{	//10ms
	/***772M下定时值的计算（（1+预分频TIM_Prescaler）/72*(1+定时周期TIM_Period)）*/

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);          //配置RCC，使能TIM2

	/* Time Base configuration */
	TIM_TimeBaseStructure.TIM_Prescaler     = 7199;               //时钟预分频数 例如:时钟频率=72/(时钟预分频+1)  = 72 / 7200 = 1/100
	TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up; //定时器模式 向上计数  
	TIM_TimeBaseStructure.TIM_Period        = 99;                 //自动重装载寄存器周期的值(定时时间)
                                                                  //累计 0xFFFF个频率后产生个更新或者中断(也是说定时时间到) 3s
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;                  //时间分割值  

	//TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;

	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);               //初始化定时器2

	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);                    //打开中断 溢出中断  

	TIM_Cmd(TIM2, ENABLE);                                        //打开定时器

	//TIM_CtrlPWMOutputs(TIM1, ENABLE);                                                 /* Main Output Enable */
}

/*******************************************************************************
*                           配置RCC
*******************************************************************************/
void RCC_Configuration(void)
{   
	ErrorStatus HSEStartUpStatus;

	RCC_DeInit();                                                    //复位RCC外部设备寄存器到默认值

	RCC_HSEConfig(RCC_HSE_ON);                                       //打开外部高速晶振

	HSEStartUpStatus = RCC_WaitForHSEStartUp();                      //等待外部高速时钟准备好

	if(HSEStartUpStatus == SUCCESS)                                  //外部高速时钟已经准别好
	{
		FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);    //开启FLASH的预取功能

		FLASH_SetLatency(FLASH_Latency_2);                       //FLASH延迟2个周期,等待总线同步操作

		RCC_HCLKConfig(RCC_SYSCLK_Div1);                         //配置AHB(HCLK)时钟=SYSCLK

		RCC_PCLK2Config(RCC_HCLK_Div1);                          //配置APB2(PCLK2)钟=AHB时钟

		RCC_PCLK1Config(RCC_HCLK_Div2);                          //配置APB1(PCLK1)钟=AHB 1/2时钟

		RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);     //配置PLL时钟 == 外部高速晶体时钟*9  PLLCLK = 8MHz * 9 = 72 MHz ，只能在PLL disable时使用

		RCC_PLLCmd(ENABLE);                                      //使能PLL时钟

		while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)       //等待PLL时钟就绪
		{
		}

		RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);               //配置系统时钟 = PLL时钟

		while(RCC_GetSYSCLKSource() != 0x08)                     //检查PLL时钟是否作为系统时钟
		{
		}
	}
}

void EXTI15_10_IRQHandler(void)            //???:EXTI15_10 (??????10~15??????)  
{
   if(EXTI_GetITStatus(EXTI_Line12) != RESET)    //?????????????????,??????????  
   {  
      Max30102_INT_Flag = 1;
      EXTI_ClearITPendingBit(EXTI_Line12);       //???  
      ReadMax30100Status(&IntStatus);

      if( ReadMax30100FifoLength(&SampleLen) == Success) {
         if( SampleLen > 0  ) {
            if( ReadMax30100FifoBuf(Max30102DataBuf,SampleLen) == Success ) {  //102 个字节一组
                if(Function_Select == FUNCTION_SPO2 ) {
#ifndef FOR_ANDROID
                    SendDataToPc(Max30102DataBuf,SampleLen*6);
#endif
                    Caculate_HR_SpO2(Max30102DataBuf,SampleLen);
                }
            }
         }
      }
   }
}
