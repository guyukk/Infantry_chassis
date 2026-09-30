/**
 ***************************************(C) COPYRIGHT 2018 DJI***************************************
 * @file       oled.h
 * @brief      the head file of oled.c ,define the I2C address of oled ,declare function of oled
 * @note         
 * @Version    V1.0.0
 * @Date       Oct-7-2019      
 ***************************************(C) COPYRIGHT 2018 DJI***************************************
 */


#ifndef OLED_H
#define OLED_H
#include "struct_typedef.h"
#include "oledfont.h"


// the I2C address of oled
#define OLED_I2C_ADDRESS    0x78        //or be 0x7A 
#define OLED_I2C    I2C2


//the resolution of oled   128*64
#define MAX_COLUMN      128
#define MAX_ROW         64

#define X_WIDTH         MAX_COLUMN
#define Y_WIDTH         MAX_ROW

#define OLED_CMD        0x00
#define OLED_DATA       0x01

#define CHAR_SIZE_WIDTH 6
#define CHAR_SIZE_HIGHT 12


typedef enum
{
	PEN_CLEAR = 0x00,
	PEN_WRITE = 0x01,
	PEN_INVERSION= 0x02,
}pen_typedef;


typedef  __packed struct  
{
	uint8_t cmd_data;
	uint8_t OLED_GRAM[8][128];
}OLED_GRAM_strutct_t;


extern void OLED_com_reset(void);
extern void OLED_init(void);
extern bool_t OLED_check_ack(void);
extern void OLED_display_on(void);
extern void OLED_display_off(void);
extern void OLED_operate_gram(pen_typedef pen);
extern void OLED_set_pos(uint8_t x, uint8_t y);
extern void OLED_draw_point(uint8_t x, uint8_t y, pen_typedef pen);
extern void OLED_draw_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, pen_typedef pen);
extern void OLED_show_char(uint8_t row, uint8_t col, uint8_t chr);
extern void OLED_show_string(uint8_t row, uint8_t col, uint8_t *chr);
extern void OLED_printf(uint8_t row, uint8_t col, const char *fmt,...);
extern void OLED_refresh_gram(void);
extern void OLED_show_graphic(uint8_t x, uint8_t y, const picture_t *graphic);
extern void OLED_LOGO(void);


#endif
