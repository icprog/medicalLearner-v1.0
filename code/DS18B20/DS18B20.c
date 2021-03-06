#include "DS18B20.h"
#include "delay.h"

uint16_t  DS18B20;

#define ds18B20_READ_ROM          0x33      // 读ROM指令
#define ds18B20_MATCH_ROM         0x55      // 匹配ROM指令
#define ds18B20_SKIP_ROM          0xCC      // 跳过ROM指令
#define ds18B20_SEARCH_ROM        0xF0      // 搜索ROM指令
#define ds18B20_ALARM_SEARCH      0xEC      // 报警搜索指令
#define ds18B20_WRITE_SCRATCHPAD  0x4E      // 写暂存寄存器指令
#define ds18B20_READ_SCRATCHPAD   0xBE      // 读暂存寄存器指令
#define ds18B20_COPY_SCRATCHPAD   0x48      // 复制暂存寄存器指令
#define ds18B20_CONVERT_T         0x44      // 启动温度转换指令
#define ds18B20_RECALL_E2         0xB8      // 重新调出E2PROM的数据
#define ds18B20_READ_POWER_SUPPLY 0xB4      // 读电源


const unsigned char  CrcTable [256]={
0,  94, 188,  226,  97,  63,  221,  131,  194,  156,  126,  32,  163,  253,  31,  65,
157,  195,  33,  127,  252,  162,  64,  30,  95,  1,  227,  189,  62,  96,  130,  220,
35,  125,  159,  193,  66,  28,  254,  160,  225,  191,  93,  3,  128,  222,  60,  98,
190,  224,  2,  92,  223,  129,  99,  61,  124,  34,  192,  158,  29,  67,  161,  255,
70,  24,  250,  164,  39,  121,  155,  197,  132,  218,  56,  102,  229,  187,  89,  7,
219,  133, 103,  57,  186,  228,  6,  88,  25,  71,  165,  251,  120,  38,  196,  154,
101,  59, 217,  135,  4,  90,  184,  230,  167,  249,  27,  69,  198,  152,  122,  36,
248,  166, 68,  26,  153,  199,  37,  123,  58,  100,  134,  216,  91,  5,  231,  185,
140,  210, 48,  110,  237,  179,  81,  15,  78,  16,  242,  172,  47,  113,  147,  205,
17,  79,  173,  243,  112,  46,  204,  146,  211,  141,  111,  49,  178,  236,  14,  80,
175,  241, 19,  77,  206,  144,  114,  44,  109,  51,  209,  143,  12,  82,  176,  238,
50,  108,  142,  208,  83,  13,  239,  177,  240,  174,  76,  18,  145,  207,  45,  115,
202,  148, 118,  40,  171,  245,  23,  73,  8,  86,  180,  234,  105,  55,  213, 139,
87,  9,  235,  181,  54,  104,  138,  212,  149,  203,  41,  119,  244,  170,  72,  22,
233,  183,  85,  11,  136,  214,  52,  106,  43,  117,  151,  201,  74,  20,  246,  168,
116,  42,  200,  150,  21,  75,  169,  247,  182,  232,  10,  84,  215,  137,  107,  53}; 

unsigned char  temp_buff[9]; //存储读取的字节，read scratchpad为9字节，read rom ID为8字节
unsigned char  id_buff[8];

unsigned char  crc_data;


/************************************************************
*函数:配置与DS18B20 通信的MCU的IO
*参数:
*返回值:
*修改:
*************************************************************/
char ConfiureIoFor18b20(GPIO_TypeDef* GPIOx,uint16_t GPIO_Pin)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	
	DS18B20 = GPIO_Pin;
	/* 配置DS18b20 通信 io */	
	GPIO_InitStructure.GPIO_Pin   = DS18B20;             //将DS18B20设为16脚
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_OD;    //开漏输出	 
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	 //输出的最大频率为50HZ
	GPIO_Init(GPIOA, &GPIO_InitStructure);               //初始化GPIOA端口
	GPIO_Write(GPIOA,0xffff);                            //将GPIOA 16个端口全部置为高电平
	return 0;
}

/************************************************************
*函数:18B20初始化
*参数:
*返回值:
*修改:
*************************************************************/
char Init18b20 (void)
{
	char ret = 0;
	
	GPIO_SetBits(GPIOA , DS18B20);       //数据线置高电平
	delay_us(2);                         //延时2微秒

	GPIO_ResetBits(GPIOA , DS18B20);     //置低
	delay_us(490);                       //delay 530 uS//80

    GPIO_SetBits(GPIOA , DS18B20);	     //置高
	delay_us(100);                       //delay 100 uS//14

	if(GPIO_ReadInputDataBit(GPIOA , DS18B20)== 0) { //detect 1820 success!
		ret = 1;
	}
	
	delay_us(480);                      //延时480微秒
	GPIO_SetBits(GPIOA , DS18B20);      //数据线置高电平
	
	return ret;
}

/************************************************************
*Function:向18B20写入一个字节
*parameter:
*Return:
*Modify:
*************************************************************/
void WriteByte (unsigned char  wr)  //单字节写入
{
	unsigned char  i;
	for (i=0;i<8;i++)
	{
		GPIO_ResetBits(GPIOA , DS18B20);	 //D18B20 = 0;
    	delay_us(2);

		//D18B20=wr&0x01;
		if(wr&0x01)	  GPIO_SetBits(GPIOA , DS18B20);
		else          GPIO_ResetBits(GPIOA , DS18B20);

		delay_us(45);   //delay 45 uS //5
		GPIO_SetBits(GPIOA , DS18B20);  //D18B20=1;
		wr >>= 1;
	}
}

/************************************************************
*Function:读18B20的一个字节
*parameter:
*Return:
*Modify:
*************************************************************/
unsigned char ReadByte (void)     //读取单字节
{
	unsigned char  i,u=0;
	for(i=0;i<8;i++)
	{		
		GPIO_ResetBits(GPIOA , DS18B20);     //D18B20 = 0;
		delay_us (2);
		u >>= 1;
	
		GPIO_SetBits(GPIOA , DS18B20);       //D18B20 = 1;
		delay_us (4);
		if(GPIO_ReadInputDataBit(GPIOA , DS18B20) == 1)
		   u |= 0x80;
		delay_us (65);
	}
	return(u);
}

/************************************************************
*Function:读18B20
*parameter:
*Return:
*Modify:
*************************************************************/
unsigned char ReadBytes(u8 *buf,u8 len)
{
	 unsigned char  i;
	 for(i=0;i<len;i++)
	 {
		  buf[i] = ReadByte();		 
	 }
}

/************************************************************
*Function:CRC校验
               查表法是对0x00~0xff这256个数依次生成与每一个数对应的CRC码所组合成的表，
               每次算一字节数据的CRC码不用经过calcrc_1byte(uchar abyte)这个函数对每个数据的
               最低位进行判断是1还是0，而直接通过查表的方式直接提取出  crc8^*p的CRC码，
               其运行效率相对按位运算方法更高，但是查表法所列的表却很占空间
*parameter:
*Return:
*Modify:
*************************************************************/
unsigned char Temp_CRC (unsigned char j)
{
   	unsigned char  i,crc_data=0;

  	for(i=0; i<j; i++)
    	crc_data = CrcTable[crc_data^temp_buff[i]];     //查表校验

    return (crc_data);
}



/************************************************************
*Function:内部配置
*parameter:
*Return:
*Modify:
*************************************************************/
void Config18b20 (void)  //重新配置报警限定值和分辨率
{
     Init18b20();
     WriteByte(0xcc);  //skip rom                 跳过ROM指令
     WriteByte(0x4e);  //write scratchpad     写暂存寄存器指令
     WriteByte(0x19);  //上限                 TH寄存器(暂存器的第2个字节)
     WriteByte(0x1a);  //下限                 TL寄存器(暂存器的第3个字节)
     WriteByte(0x7f);  //set 12 bit (0.125)


     Init18b20();
     WriteByte(0xcc);  //skip rom
     WriteByte(0x48);  //保存设定值.48h指令将TH/TL/配置寄存器(第2/3/4字节)内容拷贝到EEPROM中

     Init18b20();
     WriteByte(0xcc);  //skip rom
     WriteByte(0xb8);  //回调设定值.b8h指令将报警触发器的值(TH/TL)、配置数据从EEPROM中拷贝回暂存器
}

/************************************************************
*Function:读18B20ID
*parameter:
*Return:
*Modify:
*************************************************************/
void ReadID (void)//读取器件 id
{
	Init18b20();
	WriteByte(0x33);      // read rom   
	                      // 只有在总线上只存在单只DS18B20才能使用这条命令，
	                      // 该命令允许总线控制器在不适用搜索ROM指令的情况下读取从机的64位片序列码
	ReadBytes(id_buff,8);
}

/************************************************************
*Function:18B20ID全处理
*parameter:
*Return:
*Modify:
*************************************************************/
int ReadTemperature(void)
{
  	int temp = 0;

  	ReadID();

  	Config18b20();

	Init18b20 ();
	WriteByte(0xcc);   //skip rom
	WriteByte(0x44);   //Temperature convert 温度转换

	Init18b20 ();
	WriteByte(0xcc);   //skip rom
	WriteByte(0xbe);   //read Temperature. beh指令读取暂存器内容
	ReadBytes (temp_buff,9);

    if (Temp_CRC(9)==0) //校验正确
    {
       temp = (temp_buff[1]&0x07)*0x100 + temp_buff[0];
	   //temp /= 16;

       if((temp_buff[1]&0xf8) == 0xf8 ) {
		 temp *= -1;
	   }
       delay_us(10);
     }
	 
     return temp;
}


