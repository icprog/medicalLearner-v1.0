#ifndef __GRAPHICS_H__
#define __GRAPHICS_H__

typedef enum {
  FALSE = 0,
  TRUE  = !FALSE,
} bool;


/*


        \   |    /
         \8 |1/
          \ | /
      7   \|/ 2
 ------+-------
      6   /|\  3
         / |  \
       /5 |4  \ 

*/


/*
  * arc pixel buffer size:  2*pi*r / 8 = 0.785r
  *    r_max = 64 / 2 = 32,  arc length = 25.12
  */
#define CLK_PLATE_8thARC_SIZE     26
#define CLK_POINTER_EXTRA          4

#define CLK_SEC_HAND_BSIZE        30
#define CKL_SEC_HAND_BEXTRA           4


#define PANLE_CENT_X           32
#define PANLE_CENT_Y           32
#define PANLE_HORIZONTAL       64
#define PANLE_VERTICAL         64

#define PLATE_X0_POS               32
#define PLATE_Y0_POS               32
#define PLATE_R                    30

#define PLATE_MARGIN_X              2
#define PLATE_MARGIN_Y              2

#define CIRCLE_CENT_BSZ              5  /* ������5�����ʾ,��ΰڷ�5��Ҫ������ʵ��*/
#define CIRCLE_CENT_X               32
#define CIRCLE_CENT_Y               32

#define CLK_HHAND_LEN                26
#define CLK_HHAND_WID                3
#define CLK_MHAND_LEN                22
#define CLK_MHAND_WID                2
#define CLK_MHAND_LEN                15
#define CLK_MHAND_WID                1


//const char clk_plate_eighth_buf[CLK_PLATE_8thARC_SIZE +  CLK_POINTER_EXTRA] = {0};

//const char clk_sec_hand_buf[CLK_SEC_HAND_BSIZE + CKL_SEC_HAND_BEXTRA] = {0};

struct dot_pos {
	u8 x;
	u8 y;
};

struct clk_plate_prop {
	u8 x0;		/* origin x */
	u8 y0;
	u8 r;

	u8 margin_x;	// gap between plate edge and panle edge
	u8 margin_y;

	struct timer_digital *timer;

	struct dot_pos *dots_pos;
	u8 arc_size;

	struct clk_hands_prop  *hourhand;
	struct clk_hands_prop  *minuhand;
	struct clk_hands_prop  *sechand;

	struct plate_cent_prop *platcenter;
	struct clk_scale_prop  *clkscale;

	struct clk_panle_prop  *panle;

	bool active;
};

struct timer_digital {
	u8 year;
	u8 month;
	u8 day;

	u8 hour;
	u8 minute;
	u8 second;

	struct clk_plate_prop *plate;
	bool active;
};

struct clk_hands_prop {
	u8 x0;
	u8 y0;
	int angle;

	u8 handLen;
	u8 handwide;

	u8 *Htimer;

	struct dot_pos *dots_pos;

	struct clk_plate_prop *plate;

	bool active;
};
#if 0
struct panle_dots_buf {
	struct dot_pos dotPos;
	bool data;
};
#else
//struct panle_dots_buf {
//	u8   **dotsPos;
//	bool  *dotsdat;
//};
#endif


struct clk_coordinate {
	u8 *xCoor;
	u8 *yCoor;
};

struct clk_scale_prop {
	u8  size;	/*һ���̶����ж೤*/
	int angle;
	int r;		/*�뾶*/
	struct clk_coordinate clk_coor;
	struct clk_plate_prop *plate;
	bool active;
};

struct icon_center {
	u8 *cx;
	u8 *cy;
};

struct plate_cent_prop {
	u8 *cent_buf;
	struct icon_center *plaCenter;

	struct clk_plate_prop *plate;
	bool active;
};

struct clk_panle_prop {
	//struct clk_coordinate *clkCenter;

	u8 height;
	u8 width;

	struct icon_center   *panCenter;

	//struct clk_scale_prop clkScale;

	//struct clk_hands_prop hourHand;
	//struct clk_hands_prop minuHand;
	//struct clk_hands_prop secHand;

	//struct panle_dots_buf *dots_buf;
	//struct panle_dots_buf **dots_buf;
	u8 **dots_buf;

	struct clk_plate_prop *plate;

	bool active;

};

#define TRUE                      1
#define FALSE                     0

#define EFAULT                     1
#define EINVAL                    2

#define MAP_AREA_1(x, y)    ( x, y)	// ��һ�����ϰ벿��
#define MAP_AREA_2(x, y)    ( y, x)	// ��һ�����°벿��
#define MAP_AREA_3(x, y)    ( y,-x)
#define MAP_AREA_4(x, y)    ( x,-y)
#define MAP_AREA_5(x, y)    (-x,-y)
#define MAP_AREA_6(x, y)    (-y,-x)
#define MAP_AREA_7(x, y)    (-y, x)
#define MAP_AREA_8(x, y)    (-x, y)

#ifndef NULL
#define NULL ((void *)0)
#endif

enum clkElement {
	CLK_RESERVED   = 0,
	CLK_CENTER,
	CLK_PLATE,
	CLK_TIME_SCALE,
	CLK_HOUR_HAND,
	CLK_MINT_HAND,
	CLK_SECD_HAND,
};

struct angle_xy_table {
	int angle;
	u8 x;
	u8 y;
};

#endif