#include "FunctionDef.h"
#include "oled.h"
#include "uart.h"

#include "function_oem.h"

#ifdef FUNC_OLED_QXY
//最好是使用extern函数，不要include.h文件，这样生成的bin就小一些
#include "stdio.h"					// sprintf
#include "string.h"					// memcpy

//#include "DS18B20.h"				// ReadTemperature
extern int ReadTemperature(void);

//#include "MLX90614.h"				// SMBus_ReadTemp
extern int SMBus_ReadTemp(void);

//#include "usbio.h"					// USB_SendData
extern uint32_t USB_SendData(uint8_t *data,uint32_t dataNum);

//extern void maxim_heart_rate_and_oxygen_saturation(uint32_t *pun_ir_buffe, int32_t n_ir_buffer_length, uint32_t *pun_red_buffer, int32_t *pn_spo2, int8_t *pch_spo2_valid, int32_t *pn_heart_rate, int8_t *pch_hr_valid);
#endif

#define MAX_BRIGHTNESS      255
#define CAL_SAMPLE_SIZE     500
#define CAL_HR_SIZE         400


const u8 BMP_Buf[] = {0xF8,0xFC,0xFE,0xFE,0xFE,0xFE,0xFE,0xFC,
                      0xFC,0xFE,0xFE,0xFE,0xFE,0xFE,0xFC,0xF8,
                      0x01,0x03,0x07,0x0F,0x1F,0x3F,0x7F,0xFF,
                      0xFF,0x7F,0x3F,0x1F,0x0F,0x07,0x03,0x01};

const u8 BMP_Test_Buf[] = {0x00};

static uint8_t 	Xvalue=0;
static uint8_t 	Yvalue=0;
static uint8_t 	YvaluePre=0;
extern uint8_t 	DrawCount;

int16_t  BiaArray [CAL_HR_SIZE];	// 400

/*******************心率 SpO2计算**************************/
uint32_t aun_ir_buffer[CAL_SAMPLE_SIZE];		//IR lED传感器数据.数组大小=500
uint32_t aun_red_buffer[CAL_SAMPLE_SIZE];		//红灯传感器数据.数组大小=500
__IO int32_t  n_sp02;							//SPO2 数据
__IO int8_t   ch_spo2_valid;					// SP02 计算值有效d
__IO int32_t  n_heart_rate;						//心率计算值
__IO int8_t   ch_hr_valid;						//心率计算值有效标记
__IO uint32_t un_min, un_max, un_prev_data;		//variables to calculate the on-board LED brightness that reflects the heartbeats
__IO int  ir_red_data_count = 0;
int tmpperatureDS;
int tmpperatureMLX;

char TempDisplayBuf[16];
int16_t CaculateHR_ECG();
int16_t CaculateHR_PPG();

//显示波形
void DrawWaves(int data)
{
	OLED_ClearLine(Xvalue+1);

	if(Function_Select != FUNCTION_SPO2 ) {
		data>>=7;
	} else {
		data>>=13;
	}

	if(data>=31)   {  data=31;  }
	Yvalue=data;

	if(Yvalue>YvaluePre) {					
		while(Yvalue>YvaluePre) {
			if(YvaluePre==31)   {  YvaluePre=31;  }
			else                {  YvaluePre++;   }

			OLED_DrawPoint_Inverse(Xvalue,YvaluePre+16);
		}
	} else {
		if(Yvalue==YvaluePre) {
			OLED_DrawPoint_Inverse(Xvalue,YvaluePre+16);
		} else {
			while(Yvalue<YvaluePre){
				if(YvaluePre==0) {  YvaluePre=0;  }
				else             {  YvaluePre--;  }

				OLED_DrawPoint_Inverse(Xvalue,YvaluePre+16);
			}
		}
	}

	OLED_DrawPoint_Inverse(Xvalue,Yvalue+16);		
	Xvalue++;
	if(Xvalue==128)   {  Xvalue=0;   }

	YvaluePre=Yvalue;
}

//清除波形
void ClearWaves()
{
	Xvalue=0;
	Yvalue=0;
	YvaluePre=0;
	DrawCount=0;
	ir_red_data_count = 0;
}

//计算心率
void Caculate_HR()
{	
	int i = 0;
	int16_t HrValue;
	
	if( ir_red_data_count >= CAL_HR_SIZE )
	{
		if(Function_Select == FUNCTION_ECG )   {  HrValue = CaculateHR_ECG();  }
		else                                   {  HrValue = CaculateHR_PPG();  }
			
		sprintf(TempDisplayBuf,"%d  ",HrValue);
		OLED_ShowString(20,7, ASCII12x8, TempDisplayBuf);	
//		OLED_ShowString(0,7, ASCII12x8, TempDisplayBuf);	
//		maxim_heart_rate_and_oxygen_saturation(aun_ir_buffer, CAL_SAMPLE_SIZE, 0, &n_sp02, &ch_spo2_valid, &n_heart_rate, &ch_hr_valid); 
//		if( ch_hr_valid == 1) {
//			sprintf(TempDisplayBuf,"HR:%d   ",n_heart_rate);
//			OLED_ShowString(0,7, ASCII12x8, TempDisplayBuf);	
//		} else {
//			OLED_ShowString(0,7, ASCII12x8, "HR:   ");	
//		}

		for(i=100;i<CAL_HR_SIZE;i++) {
          aun_ir_buffer[i-100]=aun_ir_buffer[i];
        }

		ir_red_data_count =300;
	}
}

void KeyPadProcess(void)
{
	static uint8_t key_ecg_press = 0;
	static uint8_t key_ppg_press = 0;
	static uint8_t key_gsr_press = 0;
	static uint8_t key_spo2_press = 0;

	if(GPIO_ReadInputDataBit(GPIOA , GPIO_Pin_8)== 0) {//ecg key
		key_ecg_press++;
		if( key_ecg_press >= 2 ) {
			key_ecg_press = 0;
			SetBoardTestMode(FUNCTION_ECG);
		}
	} else {
		key_ecg_press = 0;
	}

	if(GPIO_ReadInputDataBit(GPIOA , GPIO_Pin_15)== 0) {//ppg key
		key_ppg_press ++;
		if( key_ppg_press >= 2 ) {
			key_ppg_press = 0;
			SetBoardTestMode(FUNCTION_PPG);
		}
	} else {
		key_ppg_press = 0;
	}

	if(GPIO_ReadInputDataBit(GPIOB , GPIO_Pin_8)== 0) {//gsr key
		key_gsr_press++;
		if( key_gsr_press >= 2 ) {
			key_gsr_press = 0;
			SetBoardTestMode(FUNCTION_GSR);
		}
	} else {
		key_gsr_press = 0;
	}

	if(GPIO_ReadInputDataBit(GPIOB , GPIO_Pin_9)== 0) {//spo2 key
		key_spo2_press ++;
		if( key_spo2_press >= 2 ) {
			key_spo2_press = 0;
			SetBoardTestMode(FUNCTION_SPO2);
		}
	} else {
		key_spo2_press = 0;
	}
}

void ShowTemperature(void)
{
	static uint8_t ds18b20_count = 0;
	static uint8_t mlx96014_count = 0;

	//DS18b20 温度测试 
	ds18b20_count++;
	if( ds18b20_count >= 200) {
		ds18b20_count = 0;
		tmpperatureDS = ReadTemperature();
		if( tmpperatureDS < 0) {
			tmpperatureDS *= -1;    // 负数转成正数
			sprintf(TempDisplayBuf,"Td:-%d.%02d",tmpperatureDS/16,(10000/(tmpperatureDS&0xf))/100);
		} else {
			sprintf(TempDisplayBuf,"Td:%d.%02d",tmpperatureDS/16,(10000/(tmpperatureDS&0xf))/100);
		}

		OLED_ShowString(72,6, ASCII12x8, TempDisplayBuf);	
	}

	//mlx96014 温度测�
	mlx96014_count++;
	if( mlx96014_count >= 200) {
		tmpperatureMLX = SMBus_ReadTemp();

		sprintf(TempDisplayBuf,"Tm:%d.%02d",tmpperatureMLX/100,tmpperatureMLX%100);
		OLED_ShowString(72,7, ASCII12x8, TempDisplayBuf);	
		mlx96014_count = 0;
	}		
}

void SendDataToPc(uint8_t * DataBuf,uint8_t dataLen)
{
#ifdef FOR_ANDROID
	USART2_SendDataDMA(DataBuf,dataLen);
#else
	if( dataLen>102)  {  dataLen = 102; }

	memcpy(&DataBuf[dataLen],(uint8_t *)&tmpperatureDS,4);
	memcpy(&DataBuf[dataLen+4],(uint8_t *)&tmpperatureMLX,4);

	DataBuf[dataLen+8] = 0x00; 
	if(Function_Select == FUNCTION_ECG )        {  DataBuf[dataLen+9] = 0x31;  }
	else if(Function_Select == FUNCTION_PPG )   {  DataBuf[dataLen+9] = 0x32;  }
	else if(Function_Select == FUNCTION_GSR )   {  DataBuf[dataLen+9] = 0x33;  }
	else                                        {  DataBuf[dataLen+9] = 0x34;  }
	
	USART1_SendDataDMA(DataBuf,112);
	USART2_SendDataDMA(DataBuf,112);
#ifndef USB_FUNC_DISABLE
	USB_SendData(DataBuf,112);
#endif

#endif
}

void Caculate_HR_SpO2(uint8_t * DataBuf,uint8_t dataLen)
{	
	int j,i = 0;
	uint32_t un_temp;

	for( j = 0; j< dataLen; j ++)
	{
		un_temp = DataBuf[j*6];
		un_temp <<= 16;
		aun_red_buffer[ir_red_data_count] = 0;
		aun_red_buffer[ir_red_data_count] += un_temp;

		un_temp = DataBuf[j*6+1];
		un_temp <<= 8;
		aun_red_buffer[ir_red_data_count] += un_temp;

		un_temp = DataBuf[j*6+2];				
		aun_red_buffer[ir_red_data_count] += un_temp;

		aun_red_buffer[ir_red_data_count] &= 0x3ffff;

		un_temp = DataBuf[j*6+3];
		un_temp <<= 16;
		aun_ir_buffer[ir_red_data_count] =0;
		aun_ir_buffer[ir_red_data_count] += un_temp;

		un_temp = DataBuf[j*6+4];
		un_temp <<= 8;
		aun_ir_buffer[ir_red_data_count] += un_temp;

		un_temp = DataBuf[j*6+5];				
		aun_ir_buffer[ir_red_data_count] += un_temp;

		aun_ir_buffer[ir_red_data_count] &= 0x3ffff;

		//DrawWaves(aun_red_buffer[ir_red_data_count]);
		if(un_min>aun_red_buffer[ir_red_data_count])  {  un_min=aun_red_buffer[ir_red_data_count];  }    //update signal min

		if(un_max<aun_red_buffer[ir_red_data_count])  {  un_max=aun_red_buffer[ir_red_data_count];  }    //update signal max

		un_prev_data=aun_red_buffer[ir_red_data_count];
		ir_red_data_count ++;
		if( ir_red_data_count >= CAL_SAMPLE_SIZE )
		{
			maxim_heart_rate_and_oxygen_saturation(aun_ir_buffer, CAL_SAMPLE_SIZE, aun_red_buffer, &n_sp02, &ch_spo2_valid, &n_heart_rate, &ch_hr_valid); 
			if( ch_spo2_valid == 1) {
				sprintf(TempDisplayBuf,"SpO2:%d  ",n_sp02);
				OLED_ShowString(0,6, ASCII12x8, TempDisplayBuf);	
			} else {
				OLED_ShowString(0,6, ASCII12x8, "SpO2:    ");	
			}

			if( ch_hr_valid == 1) {
				sprintf(TempDisplayBuf,"HR:%d   ",n_heart_rate);
				OLED_ShowString(0,7, ASCII12x8, TempDisplayBuf);	
			} else {
				OLED_ShowString(0,7, ASCII12x8, "HR:    ");	
			}

			un_min=0x3FFFF;
			un_max=0;

			//dumping the first 100 sets of samples in the memory and shift the last 400 sets of samples to the top
			for(i=100;i<500;i++) {
				aun_red_buffer[i-100]=aun_red_buffer[i];
				aun_ir_buffer[i-100]=aun_ir_buffer[i];

				//update the signal min and max
				if(un_min>aun_red_buffer[i])  {  un_min=aun_red_buffer[i];  }

				if(un_max<aun_red_buffer[i])  {  un_max=aun_red_buffer[i];  }
			}

			ir_red_data_count =400;
		}
	}
}


void print_picture(void)
{
    OLED_DrawBMP(10,1,115,8, BMP_Test_Buf);
}

void print_logo(u8 logoVer)
{
	static char runone = 1;

	if(runone) {
		runone = 0;

		OLED_Clear();
	}

	OLED_Show_logo_str(logoVer);
}

void ShowHeart(u8 show)
{
	if( show == 1 ) {    OLED_DrawBMP(0,6,16,8, BMP_Buf);  }  // u8 x0, u8 y0,u8 x1, u8 y1,const u8 BMP[]     page:6~8, cul:0~16
	else            {   OLED_ClearBMP(0,6,16,8);           } 
}

void SetBoardTestMode(uint8_t mode)
{
	Function_Select = mode;

	OLED_ShowString(0,6, ASCII12x8, "       ");	 // 擦除左下角的SPo2和HR信息
	OLED_ShowString(0,7, ASCII12x8, "       ");

	if(Function_Select == FUNCTION_ECG )      {  OLED_ShowString(48,0, ASCII16x16, "ECG ");  }  // 第0行第48列
	else if(Function_Select == FUNCTION_PPG ) {  OLED_ShowString(48,0, ASCII16x16, "PPG ");  }	// x, y, ASCII_size, string
	else if(Function_Select == FUNCTION_GSR ) {  OLED_ShowString(48,0, ASCII16x16, "GSR ");  }
	else                                      {  OLED_ShowString(48,0, ASCII16x16, "SPo2");  }

	if(Function_Select == FUNCTION_ECG || Function_Select == FUNCTION_PPG ) {  ShowHeart(1); }	
}

/***********************************计算心率************************************************/													
int16_t GetMax(int16_t * pn_buf,int16_t n_size)
{
	int16_t i,Max = 0;
	if(n_size > 0)
	{
		Max = pn_buf[0];
		for( i = 0; i < n_size; i ++)
		{
			if(pn_buf[i] > Max)
				Max = pn_buf[i];
		}
	}

	return Max;
}

int16_t GetMaxIndex(int16_t * pn_buf,int16_t n_size)
{
	int16_t i,MaxIndex = 0;
    if(n_size > 0)
    {        
        int16_t ValTemp=pn_buf[0];
        for(i=1;i<n_size;i++)
        {
            if(ValTemp<pn_buf[i])
            {
                ValTemp=pn_buf[i];
                MaxIndex=i;
            }
        }
       
    }
	 return MaxIndex ;
}

int16_t GetMin(int16_t * pn_buf,int n_size)
{
	int16_t i,Min = 0;
	if(n_size > 0)
	{	
		Min = pn_buf[0];
		for( i = 0; i < n_size; i ++)
		{
			if(pn_buf[i] < Min)
				Min = pn_buf[i];
		}
	}

	return Min;
}

int16_t GetAverage(uint32_t * pn_buf,int16_t n_size)
{
	int32_t i,sum = 0;
	if( n_size > 0)
	{
		for( i = 0; i < n_size; i ++)
		{
			sum += pn_buf[i];
		}
	}
	sum = sum/n_size;
	
	return sum;
}

int16_t GetSubArray(int16_t * pn_buf,int16_t n_size,int16_t n_start,int16_t n_len,int16_t * pn_out)
{
	int16_t i = 0;
	int16_t Len =  (n_len <= (n_size - n_start) ? n_len : (n_size - n_start));

	for(i=0;i<Len;i++)
    {
        pn_out[i] = pn_buf[i+n_start];
    }
    
	return Len;
}

/* PPG: PhotoPlethysmoGaphy 光电容积脉搏波描记法
  * 心率检测技术目前主要以光电容积脉搏波描记法PPG、
  *                                                                 心电信号法、
  *                                                                 动脉压力法、
  *                                                                 图像信号分析
  * 这几类技术应用最多*/

int16_t CaculateHR_PPG()
{
	int16_t i,j,k;
	int16_t thr,thr2;
	int16_t meanRR;

	/*
	 *  BiaArray[400]		:
	 *  aun_ir_buffer[500]	:  
	 *  aun_red_buffer[500]	:
	 */

	// 1. 将数组aun_ir_buffer中相邻数据的差值存放到BiaArray[], 并记录最大差值的7倍
	for(i=0;i<CAL_HR_SIZE-1;i++)
	{
		BiaArray[i]=aun_ir_buffer[i+1]-aun_ir_buffer[i];		//0 ~ 400个数据前后相互减,将差值数据存放到数组BiaArray
	}
	
	thr=GetMax(BiaArray,CAL_HR_SIZE-1);							// 获取差值数组中最大的数，即获取其中最大的差值
	thr2=thr*7;													// 最大差值的7倍

	// 2. 标记BiaArray[]中差值的10倍大于thr2(aun_red_buffer[]对应下标值1，否则值0.  所以,标1的aun_ir_buffer对应数据差值较大)
	for(i=0,j=0;i<CAL_HR_SIZE-1;i++)							// 0 ~ 398
	{
		if( BiaArray[i]*10>thr2) {  aun_red_buffer[j++] = 1;  }	// if 差值数组中数据a x 10 后超过最大差值的7倍，则regbuf数组增加对应下标值1
		else                     {  aun_red_buffer[j++] = 0;  }	// j记录了满足条件的数据a的个数,i&j都一直往后递增
	}

	// 3. aun_red_buffer[]前一数据为0,后一数据为1，则记下下标.所以,BiaArray[]中现在数据全是(aun_ir_buffer)相邻数据差值大的下标值
	for(i=0,k=0; i<j-1; i++)
	{
		if (aun_red_buffer[i+1] - aun_red_buffer[i] == 1) {		// red_buffer中为1表明对应下标的BiaArray的10倍超过最大差值
			BiaArray[k++] = i;									// 此时覆盖Biaarray中数据。但有隐患是BiaArrray[i] i很大的部分可能还是以前的值!!!
		}														// Biaarray现在k表面有多少个大差值，BiaArray[k-1]中记录aun_red_buffer[]邻差为1的下标
	}

	// 4. 如果BiaArray[]中有大于2个数据,则用最后一个数据- 第一个数据，再除以BiaArray中个数.这个什么意思?
	//    有点像算(f(x1) - f(x0)) / (x1 -  x0), 有点像算斜率,但这里是线性的吗????
	if( k > 1 ) {
		meanRR=(BiaArray[k-1]-BiaArray[0])/(k-1);
		return (6000 / meanRR);
	}

	return 0;
}

int16_t CaculateHR_ECG()
{
	int16_t sum1,sum2,sum3;
	int16_t CalHr = 0;
	int16_t i,j;
	int16_t thr,thr2;
 
	for(i=0;i<CAL_HR_SIZE-1;i++)
	{
		BiaArray[i]=aun_ir_buffer[i+1]-aun_ir_buffer[i];
	}
	thr=GetMax(BiaArray,CAL_HR_SIZE-1);
	thr2=thr*6;
	
	for(i=0,j=0;i<CAL_HR_SIZE-3;i++)
	{
		sum1=BiaArray[i];
		sum2=BiaArray[i+1];
		sum3=BiaArray[i+2];

		if(sum1<sum2 && sum2>sum3 && BiaArray[i+1]*10>thr2) {
			aun_red_buffer[j++] = i+1;
		}
	}

	if(j>2) {
		for(i=0;i<j-1;i++) {
			aun_red_buffer[i] = aun_red_buffer[i+1] - aun_red_buffer[i];
		}

		CalHr= 6000/GetAverage(aun_red_buffer,j-1);
	}

	return CalHr;
}
