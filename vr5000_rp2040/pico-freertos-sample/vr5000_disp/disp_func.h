#ifndef DISP_FUNC
#define DISP_FUNC
#include <stdio.h>
#define cCMD_DATA                   0x00 //data transaction
#define cCMD_COLH                   0x01 //set column high
#define cCMD_COLL                   0x02 //set column low
#define cCMD_END_RMW                0x03 //set column low
#define cCMD_PADR                   0x05
#define cMX_ADDR                    0xA0
#define cMX_PAGE                    0x4

#define LCD_COLOR_BLACK             0x0000
#define LCD_COLOR_WHITE             0xFFFF



uint8_t parse_data_cmd(uint16_t dt16);
void init_disp_pins();
void rst_disp();
//void inline write_dt(uint32_t dt);
void write_cmd_pins(uint32_t cmd);
void lcd_write_data(uint32_t dt);
void lcd_write_cmd(uint32_t dt);
void init_lcd();
void lcd_set_rectangular_area(uint16_t x1, uint16_t y1,uint16_t x2, uint16_t y2);
void lcd_fill(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void lcd_fill_s(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void lcd_put_font(uint16_t x1, uint16_t y1, uint16_t font_s_add);
void lcd_put_font_i(uint16_t x1, uint16_t y1, uint16_t font_s_add);
void Inv_FLG_Set(void);
void update_lcd();
void force_update();
void zerro_array();



#endif 
