#include "disp_func.h"
#include "hardware/gpio.h"
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include "disp_font.h"


#define cGLB_Y_OFFSET    -8
uint8_t col_addr = 0;
uint8_t page_addr = 0;
uint8_t page_addr_1 = 0;
uint8_t pix_buff_A[50][160];
uint8_t pix_buff_U[50][160];

uint tst = 0;
#define cN_DISP_PINS 21

uint8_t disp_pins[cN_DISP_PINS]={0,1,2,3,4,5,6,7,8,9,10,11,14,15,16,17,18,19,20,21,22};
uint8_t page2disp_pos[5]={1,0,3,2,4};

/*                                      
pin 18 - CSN, index 16 - active 0
pin 19 - WR, index 17 - active 0
pin 20 - RS, index 18 - 0 - register access, 1- data
pin 21 - RD, index 19 - active 0
pin 22 - RST, index 20 - active 0
*/

#define cCSN_PIN 16
#define cWR_PIN  17
#define cRS_PIN  18
#define cRD_PIN  19
#define cRST_PIN 20

#define cNOP     1F;

void init_disp()
{
   for (int i=0;i<40;i++)
     for (int j=0;j<160;j++)
     {
       pix_buff_A[i][j]=2;
       pix_buff_U[i][j]=0;
     }
     
}
//this function interprets NJU6575 commands and does LCD memroy buffer writes
uint8_t parse_data_cmd(uint16_t dt16)
{
  uint8_t result = 0;
  //do nothing for now
  int16_t cmd, dt;
  uint8_t indx_x, indx_y;
  uint8_t tmp, val_b;
  cmd = (dt16>>4)&0xF;//extract command;
  dt =((dt16>>4)&0xF0)|((dt16>>12)&0xF);
  switch(cmd)
  {
    case(cCMD_PADR)://unused all data are packed in cCMD_COLL command
      //page_addr_1=(dt&0x7);
      //if (page_addr>cMX_PAGE) page_addr=cMX_PAGE;
    break;
    
    case(cCMD_COLH)://called before setting low address //unused all data are packed in cCMD_COLL command
      //col_addr&=0x0F;
      //col_addr|=(dt&0xF)<<4;
    break;
    //FPGA compacts page address, low and high col addresses into single command
    case(cCMD_COLL)://this command transfers the page, and full column address from FPGA
      page_addr =  (dt16)&0x7;
      if (page_addr_1!=page_addr)
        page_addr_1=page_addr;
      if (page_addr>cMX_PAGE) page_addr=cMX_PAGE;
      col_addr=dt&0xFF;
    break;
    
    case(cCMD_DATA):
      for ( int i = 7 ; (i + 7) >= 7 ; i--) 
      {
        indx_x = page2disp_pos[page_addr] * 8 +(7 - i);
        tmp = (dt >> i)&1;
        val_b = pix_buff_A[indx_x][col_addr];
        if ((val_b&1)!=tmp) pix_buff_U[indx_x][col_addr]=1;
        pix_buff_A[indx_x][col_addr] = tmp;
      }
      if (page2disp_pos[page_addr]==3)
        col_addr = (col_addr>118)?119:col_addr+1;
      else
        col_addr = (col_addr>159)?159:col_addr+1;
    break;
    
    case(cCMD_END_RMW):
      result = 0;//not used
    break;
    
    default://do nothing
      tst = dt16;
  }
  return result;
}

void init_disp_pins()
{
  //set pins to output
  for (int i=0;i<cN_DISP_PINS;i++)
  {
    uint8_t pin_n = disp_pins[i];
    gpio_init(pin_n); gpio_set_dir(pin_n, GPIO_OUT);
  }
  for (int i=0;i<16;i++) gpio_put(disp_pins[i], 0);//set data to 0
  gpio_put(disp_pins[cRST_PIN],1);
  gpio_put(disp_pins[cCSN_PIN],1);
  gpio_put(disp_pins[cWR_PIN] ,1);
  gpio_put(disp_pins[cRD_PIN] ,1);
  gpio_put(disp_pins[cRS_PIN] ,1);
  rst_disp();
}

void rst_disp()
{
  gpio_put(disp_pins[cRST_PIN],0);
  vTaskDelay(pdMS_TO_TICKS(1)); //1ms delay
  gpio_put(disp_pins[cRST_PIN],1);
  vTaskDelay(pdMS_TO_TICKS(5)); //1ms delay
}



//bit 0 - CSN
//bit 1 - WR
//bit 2 - RS - register select
//RD and RST are not used for the interface
void inline write_cmd_pins(uint32_t cmd)
{
  uint32_t cmd_out = (cmd&0x7)<<18;//
  gpio_put_masked(0x0001C0000, cmd_out);
}

void inline lcd_write_cmd(uint32_t dt)
{
  gpio_put_masked(0x000000FF, dt);
  gpio_put_masked(0x0001C0000,0x000000000); //write_cmd_pins(0b000);
  gpio_put_masked(0x0001C0000,0x000140000); //write_cmd_pins(0b010);;
  gpio_put_masked(0x0001C0000,0x0001C0000); //write_cmd_pins(0b111);
}

void inline lcd_write_data(uint32_t dt)
{
  uint32_t dt_tmp = (dt&80)?0x0003CF00|(uint8_t)dt:(uint8_t)dt;
  gpio_put_masked(0x0003CFFF, dt_tmp);
  gpio_put_masked(0x0001C0000,0x000100000); //write_cmd_pins(0b100);
  gpio_put_masked(0x0001C0000,0x000180000); //write_cmd_pins(0b110);
  gpio_put_masked(0x0001C0000,0x0001C0000); //write_cmd_pins(0b111);
}



//skips gpio 12 and 13 mapped to UART, moves bits 12-15 to 14-17
void write_dt(uint32_t dt)
{
  uint32_t dt_tmp = (dt&80)?0x0003CF00|(uint8_t)dt:(uint8_t)dt;
  gpio_put_masked(0x0003CFFF, dt_tmp);
}
//skips gpio 12 and 13 mapped to UART, moves bits 12-15 to 14-17
void write_cmd(uint32_t dt)
{
  gpio_put_masked(0x000000FF, dt);
}

void tst_dt(uint16_t dt)
{
  uint32_t tmp_dt = ((dt&0xFF)<<8)|(dt&0xFF);
  uint32_t dt_tmp = (tmp_dt << 2)&0x0003C000;//clear bits 12 and 13, likely not needed;
  uint32_t dt_out = tmp_dt&0x00003FFF;
  dt_out|=dt_tmp;
  gpio_put_masked(0x0003CFFF, dt_out);
}

/**
 * @brief     Write LCD Register in
 * @param    reg   Register number
 * @param    data  The value to write to the register
 * @retval   none
*/

static void inline lcd_write_ram_start(void)
{
    lcd_write_cmd(0x2C);
}

static void inline lcd_write_ram(uint16_t rgb_color)
{
    lcd_write_data(rgb_color);
}

/**
 * @breif	LCD Fill a rectangular area
 * @param   x1 x Direction starting coordinates
 * @param   x2 x Direction termination coordinates
 * @param   y1 y Direction starting coordinates
 * @param   y2 y Direction termination coordinates
 * @param	color  Color
 * @retval	none
 */
void inline lcd_fill(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    uint32_t i;
    uint32_t size = (x2 - x1 + 1) * (y2 - y1 + 1);
    lcd_set_rectangular_area(x1, y1, x2, y2);
    lcd_write_ram_start();
    for(i = 0; i < size; i++)
    {
      lcd_write_ram(color);
    }
}

/**
 * @breif	LCD Fill_s a rectangular XY Size
 * @param   x1 x Direction starting coordinates
 * @param   x2 x Size
 * @param   y1 y Direction starting coordinates
 * @param   y2 y Size
 * @param	color  Color
 * @retval  none
 */
void inline  lcd_fill_s(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
  lcd_fill(x1, y1, (x1 + x2 -1), (y1 + y2 -1), color);
}

/**
 * @breif	LCD Fill_s a rectangular XY Size
 * @param   x1 x Direction starting coordinates
 * @param   x2 x Size
 * @param   y1 y Direction starting coordinates
 * @param   y2 y Size
 * @param	color  Color
 * @retval	none
 */

void inline lcd_set_rectangular_area(uint16_t x1, uint16_t y1,uint16_t x2, uint16_t y2)
{
    y1 +=cGLB_Y_OFFSET;
    y2 +=cGLB_Y_OFFSET;
    int32_t act_x1 = x1 < 0 ? 0 : x1;
    int32_t act_y1 = y1 < 0 ? 0 : y1;

    int32_t act_x2 = x2 > 480 - 1 ? 480 - 1 : x2;
    int32_t act_y2 = y2 > 320 - 1 ? 320 - 1 : y2;

    lcd_write_cmd(0x2A); //set_column_address
    lcd_write_data(act_x1 >> 8); // starts frame buffer address
    lcd_write_data(0x00FF & act_x1);
    lcd_write_data(act_x2 >> 8);
    lcd_write_data(0x00FF & act_x2);

    lcd_write_cmd(0x2B); //set_page_address
    lcd_write_data(act_y1 >> 8); // starts frame buffer address
    lcd_write_data(0x00FF & act_y1);
    lcd_write_data(act_y2 >> 8);
    lcd_write_data(0x00FF & act_y2);
}

void lcd_put_font(uint16_t x1, uint16_t y1, uint16_t font_s_add)
{
  uint16_t x2;
  uint16_t y2;
  uint16_t font_add;
  uint8_t font_data;
  uint8_t i;
  
    y1 +=cGLB_Y_OFFSET;
    x2 = x1 + 11 -1;  //   11 Width
    y2 = y1 + 16 -1;  //   16 Height
    int tmp = 0;
    if (font_s_add == 736)
      tmp = 1;
    lcd_set_rectangular_area(x1, y1-cGLB_Y_OFFSET, x2, y2-cGLB_Y_OFFSET);
    lcd_write_ram_start();
  
    for (font_add = font_s_add ; font_add < font_s_add + 30; font_add = font_add + 2 )
    {
      font_data = Font1611_Table[font_add];
      for (i=0; i < 8 ; i++)
      {
        if ((font_data >> (7-i))&1)
          lcd_write_ram(0x0000);
        else
          lcd_write_ram(0xFFFF);
      }
  
      font_data = Font1611_Table[font_add + 1];
      for (i=7; i > 4 ; i--)
      {
        if ((font_data >> i ) & 1)
          lcd_write_ram(0x0000);
        else
          lcd_write_ram(0xFFFF);
      }
    }
}

void lcd_put_font_i(uint16_t x1, uint16_t y1, uint16_t font_s_add)
{
  uint16_t x2;
  uint16_t y2;
  uint16_t font_add;
  uint8_t font_data;
  uint8_t i;
  y1 +=cGLB_Y_OFFSET;
  x2 = x1 + 11 -1;  //   11 Width
  y2 = y1 + 16 -1;  //   16 Height
    lcd_set_rectangular_area(x1, y1-cGLB_Y_OFFSET, x2, y2-cGLB_Y_OFFSET);
    lcd_write_ram_start();
    int tmp = 0;
    if (font_s_add == 736)
      tmp = 1;
    for (font_add = font_s_add ; font_add < font_s_add + 30; font_add = font_add + 2 )
    {
      font_data = Font1611_Table[font_add];
      for (i=0; i < 8 ; i++)
      {
        if ((( font_data >> (7-i) ) & 1) == 1 )
        {
          lcd_write_ram(0xFFFF);
        }
        else
        {
          lcd_write_ram(0x0000);
        }
      }
  
      font_data = Font1611_Table[font_add + 1];
      for (i=7; i > 4 ; i--)
      {
        if ((( font_data >> i ) & 1) == 1 )
        {
          lcd_write_ram(0xFFFF);
        }
        else
        {
          lcd_write_ram(0x0000);
        }
      }
    }
}
void Inv_FLG_Set(void)
{
  /*if ((*(uint8_t *)0x80E0000) == 0xFE)
    {
    eraseFlash();
    }
  else
    {
    writeFlash();
    }*/
}

int disp_poji_X ;
int disp_poji_Y ;

int Count_X ;
int Count_Y ;
int Inv_FLG = 0 ;

//const uint16_t LCD_COLOR_BLACK = 0x0000;
//const uint16_t LCD_COLOR_WHITE = 0xFFFF;

const int pix_X = 4 ;
const int pix_Y = 5 ;

const int poji_seg1 = 297;	/* STEP SEGMENT */
const int poji_seg2 = 279;
const int poji_seg3 = 261;

const int poji_segB1X = 440;	/* BANK SEGMENT */
const int poji_segB2X = 412;
const int poji_segBY = 46;

const int poji_segC1X = 442;	/* CH SEGMENT */
const int poji_segC2X = 415;
const int poji_segCY = 93;

const int icon_poji_1 = 17 ;
const int icon_poji_2 = 33 ;
const int icon_poji_3 = 49 ;



void update_lcd()
{


        for (Count_Y = 0; Count_Y < 8 ; Count_Y ++) /* Frq Main */
        {
          for (Count_X = 0; Count_X <= 88 ; Count_X ++)
          {
            if (pix_buff_U[Count_Y][Count_X])
            {
              pix_buff_U[Count_Y][Count_X] = 0;
              if (pix_buff_A[Count_Y][Count_X])
              {
                lcd_fill_s((Count_X * pix_X + 1 + 1) ,(Count_Y * 9)+65 ,1 ,9, LCD_COLOR_BLACK);
                lcd_fill_s((Count_X * pix_X + 1) ,(Count_Y * 9 +1)+65 ,pix_X-1 ,7, LCD_COLOR_BLACK);
              }
              else
              {
                lcd_fill_s((Count_X * pix_X + 1 + 1) ,(Count_Y * 9)+65 ,1 ,9, LCD_COLOR_WHITE);
                lcd_fill_s((Count_X * pix_X + 1) ,(Count_Y * 9 +1)+65 ,pix_X-1 ,7, LCD_COLOR_WHITE);
              }
            }
        
          }
        }
    
        for (Count_Y = 8; Count_Y < 16 ; Count_Y ++) /* Frq  Sub*/
        {
          for (Count_X = 0; Count_X <= 118 ; Count_X ++)
          {
            if (pix_buff_U[Count_Y][Count_X])
            {
              pix_buff_U[Count_Y][Count_X] = 0;
              if (pix_buff_A[Count_Y][Count_X])
              {
                lcd_fill_s((Count_X * pix_X)+1 ,((Count_Y -7)*8)+130 ,pix_X - 1 ,3, LCD_COLOR_BLACK);
                lcd_fill_s((Count_X * pix_X)+1 ,((Count_Y -7)*8+4)+130 ,pix_X - 1 ,3, LCD_COLOR_BLACK);
              }
              else
              {
                lcd_fill_s((Count_X * pix_X)+1 ,((Count_Y -7)*8)+130 ,pix_X - 1 ,3, LCD_COLOR_WHITE);
                lcd_fill_s((Count_X * pix_X)+1 ,((Count_Y -7)*8+4)+130 ,pix_X - 1 ,3, LCD_COLOR_WHITE);
              }
              pix_buff_U[Count_Y][Count_X] = 0;
            }
          }
        }

        for (Count_Y = 16; Count_Y < 23 ; Count_Y ++) /* Scan */
        {
          for (Count_X = 0; Count_X <= 118 ; Count_X ++)
          {
            if (pix_buff_U[Count_Y][Count_X])
            {
              pix_buff_U[Count_Y][Count_X] = 0;
              if (pix_buff_A[Count_Y][Count_X]&1 == 1)
              {
              lcd_fill_s((Count_X * pix_X)+1 ,((Count_Y - 15)*8)+194 ,pix_X - 1 ,3, LCD_COLOR_BLACK);
              lcd_fill_s((Count_X * pix_X)+1 ,((Count_Y - 15)*8+4)+194 ,pix_X - 1 ,3, LCD_COLOR_BLACK);
              }
              else
              {
                lcd_fill_s((Count_X * pix_X)+1 ,((Count_Y - 15)*8)+194 ,pix_X - 1 ,3, LCD_COLOR_WHITE);
                lcd_fill_s((Count_X * pix_X)+1 ,((Count_Y - 15)*8+4)+194 ,pix_X - 1 ,3, LCD_COLOR_WHITE);
              }
            }
          }   
        }
    
        Count_Y = 23;
        for (Count_X = 0; Count_X <= 118 ; Count_X ++)
        {
          if (pix_buff_U[Count_Y][Count_X])
          {
            pix_buff_U[Count_Y][Count_X] = 0;
            if (pix_buff_A[Count_Y][Count_X])
            {
              lcd_fill_s((Count_X * pix_X) + 1 ,(Count_Y + 235) ,pix_X - 1 ,5, LCD_COLOR_BLACK);
            }
            else
            {
              lcd_fill_s((Count_X * pix_X) + 1 ,(Count_Y + 235) ,pix_X - 1 ,5, LCD_COLOR_WHITE);
            }
            
          }
        }
      
    
        for (Count_Y = 24; Count_Y < 31 ; Count_Y ++)	/* Clock */
        {
          for (Count_X = 0; Count_X <= 118 ; Count_X ++)
          {
            if (pix_buff_U[Count_Y][Count_X])
            {
              pix_buff_U[Count_Y][Count_X] = 0;
              if (pix_buff_A[Count_Y][Count_X])
              {
                lcd_fill_s((Count_X * 4)+1 ,(Count_Y * 4)+169 ,pix_X - 1 ,3, LCD_COLOR_BLACK);
              }
              else
              {
                lcd_fill_s((Count_X * 4)+1 ,(Count_Y * 4)+169 ,pix_X - 1 ,3, LCD_COLOR_WHITE);
              }
              
            }
          }
        }

    
    
    
      /* Lock */
      if (pix_buff_U[31][9])
      {
        pix_buff_U[31][9] = 0;
        if (pix_buff_A[31][9]&1 == 1)
        {
          lcd_put_font(5, icon_poji_2+2,0);
          lcd_put_font(5 + 11, icon_poji_2+2,32);
          lcd_put_font(5 + 11 + 11, icon_poji_2+2,64);
        
          if (pix_buff_A[39][9]&1 == 1)	// F is on
          {
            Inv_FLG = 1;
          }
        }
        else
        {
          lcd_put_font_i(5, icon_poji_2+2,1056);
          lcd_put_font_i(5 + 11, icon_poji_2+2,1056);
          lcd_put_font_i(5 + 11 + 11, icon_poji_2+2,1056);
        }
      }
        /* F */
      if (pix_buff_U[39][9])
      {
        pix_buff_U[39][9] = 0;
        if (pix_buff_A[39][9]&1 == 1)
        {
          lcd_put_font(5, icon_poji_1,96);
        }
        else
        {
          lcd_put_font_i(5, icon_poji_1,1056);
      
          if (Inv_FLG == 1)
          {
            if (pix_buff_A[31][9]&1 == 1)	// Lock is on
            {
            Inv_FLG = 0;
            Inv_FLG_Set();
            }
            else
            {
            Inv_FLG = 0;
            }
          }
        }
      }
        /* BUSY */
      if (pix_buff_U[39][10])
      {
        pix_buff_U[39][10] = 0;
        if (pix_buff_A[39][10]&1 == 1)
        {
          lcd_put_font(25, icon_poji_1,128);
          lcd_put_font(25 + 11, icon_poji_1,160);
          lcd_put_font(25 + 11 + 11, icon_poji_1,192);
          lcd_put_font(25 + 11 + 11 + 11, icon_poji_1,224);
        }
        else
        {
          lcd_put_font_i(25, icon_poji_1,1056);
          lcd_put_font_i(25 + 11, icon_poji_1,1056);
          lcd_put_font_i(25 + 11 + 11, icon_poji_1,1056);
          lcd_put_font_i(25 + 11 + 11 + 11, icon_poji_1,1056);
        }
      }
        /* LSB */
      if (pix_buff_U[31][4])
      {
        pix_buff_U[31][4] = 0;
        if (pix_buff_A[31][4]&1 == 1)
        {
          lcd_put_font(5,  icon_poji_3,256);
          lcd_put_font(5 + 11, icon_poji_3,288);
          lcd_put_font(5 + 11 + 11, icon_poji_3,320);
        }
        else
        {
          lcd_put_font_i(5,  icon_poji_3,1056);
          lcd_put_font_i(5 + 11, icon_poji_3,1056);
          lcd_put_font_i(5 + 11 + 11, icon_poji_3,1056);
        }
      }
        /* USB */
      if (pix_buff_U[31][15])
      {
        pix_buff_U[31][15] = 0;
        if (pix_buff_A[31][15]&1 == 1)
        {
      
          lcd_put_font(48,  icon_poji_3,352);
          lcd_put_font(48 + 11, icon_poji_3,288);
          lcd_put_font(48 + 11 + 11, icon_poji_3,320);
        }
        else
        {
          lcd_put_font_i(48,  icon_poji_3,1056);
          lcd_put_font_i(48 + 11, icon_poji_3,1056);
          lcd_put_font_i(48 + 11 + 11, icon_poji_3,1056);
        }
      }
      /* CW */
      if (pix_buff_U[31][24])
      {
        pix_buff_U[31][24] = 0;
        if (pix_buff_A[31][24]&1 == 1)
        {
          lcd_put_font(91,  icon_poji_3,384);
          lcd_put_font(91 + 11, icon_poji_3,416);
        }
        else
        {
          lcd_put_font_i(91,  icon_poji_3,1056);
          lcd_put_font_i(91 + 11, icon_poji_3,1056);
        }
      }
      /* AM */
      if (pix_buff_U[31][39])
      {
        pix_buff_U[31][39] = 0;
        if (pix_buff_A[31][39]&1 == 1)
        {
          lcd_put_font(134,  icon_poji_3,448);
          lcd_put_font(134 + 11, icon_poji_3,480);
        }
        else
        {
          lcd_put_font_i(134,  icon_poji_3,1056);
          lcd_put_font_i(134 + 11, icon_poji_3,1056);
        }
      }
      /* W(AM) */
      if (pix_buff_U[31][34])
      {
        pix_buff_U[31][34] = 0;
        if (pix_buff_A[31][34]&1 == 1)
        {
          lcd_put_font(123,  icon_poji_3,416);
        }
        else
        {
          lcd_put_font_i(123,  icon_poji_3,1056);
        }
      }
      /* (AM)-N */
      if (pix_buff_U[31][45])
      {
        pix_buff_U[31][45] = 0;
        if (pix_buff_A[31][45])
        {
          lcd_put_font(156,  icon_poji_3,512);
          lcd_put_font(156 + 11, icon_poji_3,544);
        }
        else
        {
          lcd_put_font_i(156,  icon_poji_3,1056);
          lcd_put_font_i(156 + 11, icon_poji_3,1056);
        }
      }
      /* FM */
      if (pix_buff_U[31][58])
      {
        pix_buff_U[31][58] = 0;
        if (pix_buff_A[31][58])
        {
          lcd_put_font(190,  icon_poji_3,576);
          lcd_put_font(190 + 11, icon_poji_3,480);
        }
        else
        {
          lcd_put_font_i(190,  icon_poji_3,1056);
          lcd_put_font_i(190 + 11, icon_poji_3,1056);
        }
      }
      /* W(FM) */
      if (pix_buff_U[31][54])
      {
        pix_buff_U[31][54] = 0;
        if (pix_buff_A[31][54])
        {
          lcd_put_font(178,  icon_poji_3,416);
        }
        else
        {
          lcd_put_font_i(178,  icon_poji_3,1056);
        }
      }
    
      /* (FM)-N */
      if (pix_buff_U[31][64])
      {
        pix_buff_U[31][64] = 0;
        if (pix_buff_A[31][64]&1 == 1)
        {
        
          lcd_put_font(212,  icon_poji_3,512);
          lcd_put_font(212 + 11, icon_poji_3,544);
        }
        else
        {
          lcd_put_font_i(212,  icon_poji_3,1056);
          lcd_put_font_i(212 + 11, icon_poji_3,1056);
        }
      }
      /* ATT */
      if (pix_buff_U[39][45])
      {
        pix_buff_U[39][45] = 0;
        if (pix_buff_A[39][45])
        {
          lcd_put_font(117,  icon_poji_1,448);
          lcd_put_font(117 + 11, icon_poji_1,608);
          lcd_put_font(117 + 11 + 11, icon_poji_1,608);
        }
        else
        {
          lcd_put_font_i(117,  icon_poji_1,1056);
          lcd_put_font_i(117 + 11, icon_poji_1,1056);
          lcd_put_font_i(117 + 11 + 11, icon_poji_1,1056);
        }
      }

      /* NB */
      if (pix_buff_U[39][50])
      {
        pix_buff_U[39][50] = 0;
        if (pix_buff_A[39][50]&1 == 1)
        {
          lcd_put_font(165,  icon_poji_1,544);
          lcd_put_font(165 + 11, icon_poji_1,320);
        }
        else
        {
          lcd_put_font_i(165,  icon_poji_1,1056);
          lcd_put_font_i(165 + 11, icon_poji_1,1056);
        }
      }
      /* AUTO */
      if (pix_buff_U[31][68])
      {
        pix_buff_U[31][68] = 0;
        if (pix_buff_A[31][68]&1 == 1)
        {
          lcd_put_font(206, icon_poji_2,448);
          lcd_put_font(206 + 11, icon_poji_2,352);
          lcd_put_font(206 + 11 + 11, icon_poji_2,608);
          lcd_put_font(206 + 11 + 11 + 11, icon_poji_2,640);
        }
        else
        {
          lcd_put_font_i(206, icon_poji_2,1056);
          lcd_put_font_i(206 + 11, icon_poji_2,1056);
          lcd_put_font_i(206 + 11 + 11, icon_poji_2,1056);
          lcd_put_font_i(206 + 11 + 11 + 11, icon_poji_2,1056);
        }
      }
      /* k(Hz) */
      if (pix_buff_U[31][80])
      {
        pix_buff_U[31][80] = 0;
        if (pix_buff_A[31][80])
        {
          lcd_put_font(310, icon_poji_2,672);
        }
        else
        {
          lcd_put_font_i(310, icon_poji_2,1056);
        }
      }
      /* Hz  STEP */
      if (pix_buff_U[31][81])
      {
        pix_buff_U[31][81] = 0;
        if (pix_buff_A[31][81])
        {
          lcd_put_font(321, icon_poji_2,704);
          lcd_put_font(321 + 11, icon_poji_2,736);
          lcd_put_font(206, icon_poji_1,288);
          lcd_put_font(206 + 11, icon_poji_1,608);
          lcd_put_font(206 + 11 + 11, icon_poji_1,768);
          lcd_put_font(206 + 11 + 11 + 11, icon_poji_1,800);
        }
        else
        {
          lcd_put_font_i(321, icon_poji_2,1056);
          lcd_put_font_i(321 + 11, icon_poji_2,1056);
          lcd_put_font_i(206, icon_poji_1,1056);
          lcd_put_font_i(206 + 11, icon_poji_1,1056);
          lcd_put_font_i(206 + 11 + 11, icon_poji_1,1056);
          lcd_put_font_i(206 + 11 + 11 + 11, icon_poji_1,1056);
        }
      }
      /* DELAY */
      if (pix_buff_U[0][95])
      {
        pix_buff_U[0][95] = 0;
        if (pix_buff_A[0][95])
        {
          lcd_put_font(361, icon_poji_1,832);
          lcd_put_font(361 + 11, icon_poji_1,768);
          lcd_put_font(361 + 11 + 11, icon_poji_1,256);
          lcd_put_font(361 + 11 + 11 + 11, icon_poji_1,448);
          lcd_put_font(361 + 11 + 11 + 11 + 11, icon_poji_1,864);
        }
        else
        {
          lcd_put_font_i(361, icon_poji_1,1056);
          lcd_put_font_i(361 + 11, icon_poji_1,1056);
          lcd_put_font_i(361 + 11 + 11, icon_poji_1,1056);
          lcd_put_font_i(361 + 11 + 11 + 11, icon_poji_1,1056);
          lcd_put_font_i(361 + 11 + 11 + 11 + 11, icon_poji_1,1056);
        }
      }
      /* HOLD */
      if (pix_buff_U[39][117])
      {
        pix_buff_U[39][117] = 0;
        if (pix_buff_A[39][117])
        {
          lcd_put_font(416, icon_poji_1,704);
          lcd_put_font(416 + 11, icon_poji_1,640);
          lcd_put_font(416 + 11 + 11, icon_poji_1,256);
          lcd_put_font(416 + 11 + 11 + 11, icon_poji_1,832);
        }
        else
        {
          lcd_put_font_i(416, icon_poji_1,1056);
          lcd_put_font_i(416 + 11, icon_poji_1,1056);
          lcd_put_font_i(416 + 11 + 11, icon_poji_1,1056);
          lcd_put_font_i(416 + 11 + 11 + 11, icon_poji_1,1056);
        }
      }
      /* PAUSE */
      if (pix_buff_U[1][95])
      {
        pix_buff_U[1][95] = 0;
        if (pix_buff_A[1][95])
        {
          lcd_put_font(361, icon_poji_2,800);
          lcd_put_font(361 + 11, icon_poji_2,448);
          lcd_put_font(361 + 11 + 11, icon_poji_2,352);
          lcd_put_font(361 + 11 + 11 + 11, icon_poji_2,288);
          lcd_put_font(361 + 11 + 11 + 11 + 11, icon_poji_2,768);
        }
        else
        {
          lcd_put_font_i(361, icon_poji_2,1056);
          lcd_put_font_i(361 + 11, icon_poji_2,1056);
          lcd_put_font_i(361 + 11 + 11, icon_poji_2,1056);
          lcd_put_font_i(361 + 11 + 11 + 11, icon_poji_2,1056);
          lcd_put_font_i(361 + 11 + 11 + 11 + 11, icon_poji_2,1056);
        }
      }
      /* VCS+  can not find */
    
      /* BANK */
      if (pix_buff_U[2][95])
      {
        pix_buff_U[2][95] = 0;
        if (pix_buff_A[2][95])
        {
          lcd_put_font(362, poji_segBY+8,320);
          lcd_put_font(362 + 11, poji_segBY+8,448);
          lcd_put_font(362 + 11 + 11, poji_segBY+8,544);
          lcd_put_font(362 + 11 + 11 + 11, poji_segBY+8,896);
        }
        else
        {
          lcd_put_font_i(362, poji_segBY+8,1056);
          lcd_put_font_i(362 + 11, poji_segBY+8,1056);
          lcd_put_font_i(362 + 11 + 11, poji_segBY+8,1056);
          lcd_put_font_i(362 + 11 + 11 + 11, poji_segBY+8,1056);
        }
      }
      /* LINK */
      if (pix_buff_U[3][95])
      {
        pix_buff_U[3][95] = 0;
        if (pix_buff_A[3][95])
        {
          lcd_put_font(362, poji_segBY+26,256);
          lcd_put_font(362 + 11, poji_segBY+26,928);
          lcd_put_font(362 + 11 + 11, poji_segBY+26,544);
          lcd_put_font(362 + 11 + 11 + 11, poji_segBY+26,896);
        }
        else
        {
          lcd_put_font_i(362, poji_segBY+26,1056);
          lcd_put_font_i(362 + 11, poji_segBY+26,1056);
          lcd_put_font_i(362 + 11 + 11, poji_segBY+26,1056);
          lcd_put_font_i(362 + 11 + 11 + 11, poji_segBY+26,1056);
        }
      }
    
      /* SKIP */
      if (pix_buff_U[4][95])
      {
        pix_buff_U[4][95] = 0;
        if (pix_buff_A[4][95])
        {
          lcd_put_font(357, poji_segBY+44,288);
          lcd_put_font(357 + 11, poji_segBY+44,896);
          lcd_put_font(357 + 11 + 11, poji_segBY+44,928);
          lcd_put_font(357 + 11 + 11 + 11, poji_segBY+44,800);
          lcd_put_font(357 + 11 + 11 + 11 + 11, poji_segBY+44,960);
        }
        else
        {
          lcd_put_font_i(357, poji_segBY+44,1056);
          lcd_put_font_i(357 + 11, poji_segBY+44,1056);
          lcd_put_font_i(357 + 11 + 11, poji_segBY+44,1056);
          lcd_put_font_i(357 + 11 + 11 + 11, poji_segBY+44,1056);
          lcd_put_font_i(357 + 11 + 11 + 11 + 11, poji_segBY+44,1056);
        }
      }
      /* SEL */
      if (pix_buff_U[5][95])
      {
        pix_buff_U[5][95] = 0;
        if (pix_buff_A[5][95])
        {
          lcd_put_font(357, poji_segBY+58,288);
          lcd_put_font(357 + 11, poji_segBY+58,768);
          lcd_put_font(357 + 11 + 11, poji_segBY+58,256);
          lcd_put_font(357 + 11 + 11 + 11, poji_segBY+58,960);
        }
        else
        {
          lcd_put_font_i(357, poji_segBY+58,1056);
          lcd_put_font_i(357 + 11, poji_segBY+58,1056);
          lcd_put_font_i(357 + 11 + 11, poji_segBY+58,1056);
          lcd_put_font_i(357 + 11 + 11 + 11, poji_segBY+58,1056);
        }
      }
      /* RPI */
      if (pix_buff_U[6][95])
      {
        pix_buff_U[6][95] = 0;
        if (pix_buff_A[6][95])
        {
          lcd_put_font(362, poji_segBY+72,800);
          lcd_put_font(362 + 11, poji_segBY+72,992);
          lcd_put_font(362 + 11 + 11, poji_segBY+72,928);
          lcd_put_font(362 + 11 + 11 + 11, poji_segBY+72,960);
        }
        else
        {
          lcd_put_font_i(362, poji_segBY+72,1056);
          lcd_put_font_i(362 + 11, poji_segBY+72,1056);
          lcd_put_font_i(362 + 11 + 11, poji_segBY+72,1056);
          lcd_put_font_i(362 + 11 + 11 + 11, poji_segBY+72,1056);
        }
      }
      
      /* CH */
      if (pix_buff_A[1][117]&1 == 1)
      {
        lcd_put_font(462, poji_segCY+28,1024);
      }
      else
      {
        lcd_put_font_i(462, poji_segCY+28,1056);
      }

      /* 3a */
      if (pix_buff_A[31][70]&1 == 1)
      {
        lcd_fill_s(poji_seg3 ,icon_poji_1+4 ,10 ,2 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_seg3 ,icon_poji_1+4 ,10 ,2 ,LCD_COLOR_WHITE);
      }
      /* 3b */
      if (pix_buff_A[39][71]&1 == 1)
      {
        lcd_fill_s(poji_seg3+10 ,icon_poji_1+6 ,2 ,10 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_seg3+10 ,icon_poji_1+6 ,2 ,10 ,LCD_COLOR_WHITE);
      }
      /* 3c */
      if (pix_buff_A[31][71]&1 == 1)
      {
        lcd_fill_s(poji_seg3+10 ,icon_poji_1+18 ,2 ,10 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_seg3+10 ,icon_poji_1+18 ,2 ,10 ,LCD_COLOR_WHITE);
      }
      /* 3g,3f */
      if (pix_buff_A[39][70]&1 == 1)
      {
        lcd_fill_s(poji_seg3 ,icon_poji_1+16 ,10 ,2 ,LCD_COLOR_BLACK);
        lcd_fill_s(poji_seg3-2 ,icon_poji_1+6 ,2 ,10 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_seg3 ,icon_poji_1+16 ,10 ,2 ,LCD_COLOR_WHITE);
        lcd_fill_s(poji_seg3-2 ,icon_poji_1+6 ,2 ,10 ,LCD_COLOR_WHITE);
      }
      /* 3d */
      if (pix_buff_A[39][69]&1 == 1)
      {
        lcd_fill_s(poji_seg3 ,icon_poji_1+28 ,10 ,2 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_seg3 ,icon_poji_1+28 ,10 ,2 ,LCD_COLOR_WHITE);
      }
      /* 2a */
      if (pix_buff_A[31][74]&1 == 1)
      {
        lcd_fill_s(poji_seg2 ,icon_poji_1+4 ,10 ,2 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_seg2 ,icon_poji_1+4 ,10 ,2 ,LCD_COLOR_WHITE);
      }
      /* 2d */
      if (pix_buff_A[39][81]&1 == 1)
      {
        lcd_fill_s(poji_seg2 ,icon_poji_1+28 ,10 ,2 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_seg2 ,icon_poji_1+28 ,10 ,2 ,LCD_COLOR_WHITE);
      }
      /* 2b */
      if (pix_buff_A[39][75]&1 == 1)
      {
        lcd_fill_s(poji_seg2+10 ,icon_poji_1+6 ,2 ,10 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_seg2+10 ,icon_poji_1+6 ,2 ,10 ,LCD_COLOR_WHITE);
      }
      /* 2f */
      if (pix_buff_A[39][73]&1 == 1)
      {
        lcd_fill_s(poji_seg2-2 ,icon_poji_1+6 ,2 ,10 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_seg2-2 ,icon_poji_1+6 ,2 ,10 ,LCD_COLOR_WHITE);
      }
      /* 2c */
      if (pix_buff_A[31][75]&1 == 1)
      {
        lcd_fill_s(poji_seg2+10 ,icon_poji_1+18 ,2 ,10 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_seg2+10 ,icon_poji_1+18 ,2 ,10 ,LCD_COLOR_WHITE);
      }
      /* 2g */
      if (pix_buff_A[39][74]&1 == 1)
      {
        lcd_fill_s(poji_seg2 ,icon_poji_1+16 ,10 ,2 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_seg2 ,icon_poji_1+16 ,10 ,2 ,LCD_COLOR_WHITE);
      }
      
      /* 2e */
      if (pix_buff_A[31][73]&1 == 1)
      {
        lcd_fill_s(poji_seg2-2 ,icon_poji_1+18 ,2 ,10 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_seg2-2 ,icon_poji_1+18 ,2 ,10 ,LCD_COLOR_WHITE);
      }

      /* 1a */
      if (pix_buff_A[31][78]&1 == 1)
      {
        lcd_fill_s(poji_seg1 ,icon_poji_1+4 ,10 ,2 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_seg1 ,icon_poji_1+4 ,10 ,2 ,LCD_COLOR_WHITE);
      }
      /* 1b */
      if (pix_buff_A[39][79]&1 == 1)
      {
        lcd_fill_s(poji_seg1+10 ,icon_poji_1+6 ,2 ,10 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_seg1+10 ,icon_poji_1+6 ,2 ,10 ,LCD_COLOR_WHITE);
      }
      /* 1c */
      if (pix_buff_A[31][79]&1 == 1)
      {
        lcd_fill_s(poji_seg1+10 ,icon_poji_1+18 ,2 ,10 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_seg1+10 ,icon_poji_1+18 ,2 ,10 ,LCD_COLOR_WHITE);
      }
      /* 1f */
      if (pix_buff_A[39][77]&1 == 1)
      {
        lcd_fill_s(poji_seg1-2 ,icon_poji_1+6 ,2 ,10 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_seg1-2 ,icon_poji_1+6 ,2 ,10 ,LCD_COLOR_WHITE);
      }
      /* 1g */
      if (pix_buff_A[39][78]&1 == 1)
      {
        lcd_fill_s(poji_seg1 ,icon_poji_1+16 ,10 ,2 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_seg1 ,icon_poji_1+16 ,10 ,2 ,LCD_COLOR_WHITE);
      }
      /* 1d */
      if (pix_buff_A[39][80]&1 == 1)
      {
        lcd_fill_s(poji_seg1 ,icon_poji_1+28 ,10 ,2 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_seg1 ,icon_poji_1+28 ,10 ,2 ,LCD_COLOR_WHITE);
      }
      /* 1e */
      if (pix_buff_A[31][77]&1 == 1)
      {
        lcd_fill_s(poji_seg1-2 ,icon_poji_1+18 ,2 ,10 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_seg1-2 ,icon_poji_1+18 ,2 ,10 ,LCD_COLOR_WHITE);
      }
      /* 1DP */
      if (pix_buff_A[31][76]&1 == 1)
      {
        lcd_fill_s(poji_seg1-5 ,icon_poji_1+29 ,2 ,2 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_seg1-5 ,icon_poji_1+29 ,2 ,2 ,LCD_COLOR_WHITE);
      }
      
      /* 2DP,3e */
      if (pix_buff_A[31][69]&1 == 1)
      {
        lcd_fill_s(poji_seg2 -5 ,icon_poji_1+29 ,2 ,2 ,LCD_COLOR_BLACK);
        lcd_fill_s(poji_seg3-2 ,icon_poji_1+18 ,2 ,10 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_seg2 -5 ,icon_poji_1+29 ,2 ,2 ,LCD_COLOR_WHITE);
        lcd_fill_s(poji_seg3-2 ,icon_poji_1+18 ,2 ,10 ,LCD_COLOR_WHITE);
      }
      /* B1a */
      if (pix_buff_A[0][110]&1 == 1)
      {
        lcd_fill_s(poji_segB1X ,poji_segBY+3 ,15 ,3 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segB1X ,poji_segBY+3 ,15 ,3 ,LCD_COLOR_WHITE);
      }
      /* B1b */
      if (pix_buff_A[1][116]&1 == 1)
      {
        lcd_fill_s(poji_segB1X+15 ,poji_segBY+6 ,3 ,15 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segB1X+15 ,poji_segBY+6 ,3 ,15 ,LCD_COLOR_WHITE);
      }
      /* B1c */
      if (pix_buff_A[3][116]&1 == 1)
      {
        lcd_fill_s(poji_segB1X+15 ,poji_segBY+5+15+4 ,3 ,15 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segB1X+15 ,poji_segBY+5+15+4 ,3 ,15 ,LCD_COLOR_WHITE);
      }
      /* B1d */
      if (pix_buff_A[3][110]&1 == 1)
      {
        lcd_fill_s(poji_segB1X ,poji_segBY+5+15+15+4 ,15 ,3 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segB1X ,poji_segBY+5+15+15+4 ,15 ,3 ,LCD_COLOR_WHITE);
      }
      /* B1e */
      if (pix_buff_A[2][110]&1 == 1)
      {
        lcd_fill_s(poji_segB1X-3 ,poji_segBY+5+15+4 ,3 ,15 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segB1X-3 ,poji_segBY+5+15+4 ,3 ,15 ,LCD_COLOR_WHITE);
      }
      /* B1f */
      if (pix_buff_A[1][110]&1 == 1)
      {
        lcd_fill_s(poji_segB1X-3 ,poji_segBY+6 ,3 ,15 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segB1X-3 ,poji_segBY+6 ,3 ,15 ,LCD_COLOR_WHITE);
      }
      /* B1g */
      if (pix_buff_A[2][116]&1 == 1)
      {
        lcd_fill_s(poji_segB1X ,poji_segBY+6+15 ,15 ,3 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segB1X ,poji_segBY+6+15 ,15 ,3 ,LCD_COLOR_WHITE);
      }
      
      /* B2a */
      if (pix_buff_A[0][97]&1 == 1)
      {
        lcd_fill_s(poji_segB2X ,poji_segBY+3 ,15 ,3 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segB2X ,poji_segBY+3 ,15 ,3 ,LCD_COLOR_WHITE);
      }
      /* B2b */
      if (pix_buff_A[1][109]&1 == 1)
      {
        lcd_fill_s(poji_segB2X+15 ,poji_segBY+6 ,3 ,15 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segB2X+15 ,poji_segBY+6 ,3 ,15 ,LCD_COLOR_WHITE);
      }
      /* B2c */
      if (pix_buff_A[3][109]&1 == 1)
      {
        lcd_fill_s(poji_segB2X+15 ,poji_segBY+5+15+4 ,3 ,15 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segB2X+15 ,poji_segBY+5+15+4 ,3 ,15 ,LCD_COLOR_WHITE);
      }
      /* B2d */
      if (pix_buff_A[3][97]&1 == 1)
      {
        lcd_fill_s(poji_segB2X ,poji_segBY+5+15+15+4 ,15 ,3 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segB2X ,poji_segBY+5+15+15+4 ,15 ,3 ,LCD_COLOR_WHITE);
      }
      /* B2e */
      if (pix_buff_A[2][97]&1 == 1)
      {
        lcd_fill_s(poji_segB2X-3 ,poji_segBY+5+15+4 ,3 ,15 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segB2X-3 ,poji_segBY+5+15+4 ,3 ,15 ,LCD_COLOR_WHITE);
      }
      /* B2f */
      if (pix_buff_A[1][97]&1 == 1)
      {
        lcd_fill_s(poji_segB2X-3 ,poji_segBY+6 ,3 ,15 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segB2X-3 ,poji_segBY+6 ,3 ,15 ,LCD_COLOR_WHITE);
      }
      /* B2g */
      if (pix_buff_A[2][109]&1 == 1)
      {
        lcd_fill_s(poji_segB2X ,poji_segBY+6+15 ,15 ,3 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segB2X ,poji_segBY+6+15 ,15 ,3 ,LCD_COLOR_WHITE);
      }
      
      /* C1a */
      if (pix_buff_A[4][116]&1 == 1)
      {
        lcd_fill_s(poji_segC1X ,poji_segCY+2 ,15 ,3 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segC1X ,poji_segCY+2 ,15 ,3 ,LCD_COLOR_WHITE);
      }
      /* C1b */
      if (pix_buff_A[4][115]&1 == 1)
      {
        lcd_fill_s(poji_segC1X+15 ,poji_segCY+5 ,3 ,15 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segC1X+15 ,poji_segCY+5 ,3 ,15 ,LCD_COLOR_WHITE);
      }

      /* C1c */
      if (pix_buff_A[4][114]&1 == 1)
      {
        lcd_fill_s(poji_segC1X+15 ,poji_segCY+5+15+3 ,3 ,15 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segC1X+15 ,poji_segCY+5+15+3 ,3 ,15 ,LCD_COLOR_WHITE);
      }
      /* C1d */
      if (pix_buff_A[4][112]&1 == 1)
      {
        lcd_fill_s(poji_segC1X ,poji_segCY+5+15+15+3 ,15 ,3 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segC1X ,poji_segCY+5+15+15+3 ,15 ,3 ,LCD_COLOR_WHITE);
      }
      /* C1e */
      if (pix_buff_A[4][111]&1 == 1)
      {
        lcd_fill_s(poji_segC1X-3 ,poji_segCY+5+15+3 ,3 ,15 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segC1X-3 ,poji_segCY+5+15+3 ,3 ,15 ,LCD_COLOR_WHITE);
      }
      /* C1f */
      if (pix_buff_A[4][110]&1 == 1)
      {
        lcd_fill_s(poji_segC1X-3 ,poji_segCY+5 ,3 ,15 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segC1X-3 ,poji_segCY+5 ,3 ,15 ,LCD_COLOR_WHITE);
      }
      /* C1g1 */
      if (pix_buff_A[6][110]&1 == 1)
      {
        lcd_fill_s(poji_segC1X+8 ,poji_segCY+6+14 ,7 ,3 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segC1X+8 ,poji_segCY+6+14 ,7 ,3 ,LCD_COLOR_WHITE);
      }
      /* C1g2 */
      if (pix_buff_A[6][114]&1 == 1)
      {
        lcd_fill_s(poji_segC1X ,poji_segCY+6+14 ,7 ,3 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segC1X ,poji_segCY+6+14 ,7 ,3 ,LCD_COLOR_WHITE);
      }
      /* C1j */
      if (pix_buff_A[6][115]&1 == 1)
      {
        lcd_fill_s(poji_segC1X+11 ,poji_segCY+7 ,3 ,3 ,LCD_COLOR_BLACK);
        lcd_fill_s(poji_segC1X+10 ,poji_segCY+10 ,3 ,3 ,LCD_COLOR_BLACK);
        lcd_fill_s(poji_segC1X+9 ,poji_segCY+13 ,3 ,3 ,LCD_COLOR_BLACK);
        lcd_fill_s(poji_segC1X+8 ,poji_segCY+16 ,3 ,3 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segC1X+11 ,poji_segCY+7 ,3 ,3 ,LCD_COLOR_WHITE);
        lcd_fill_s(poji_segC1X+10 ,poji_segCY+10 ,3 ,3 ,LCD_COLOR_WHITE);
        lcd_fill_s(poji_segC1X+9 ,poji_segCY+13 ,3 ,3 ,LCD_COLOR_WHITE);
        lcd_fill_s(poji_segC1X+8 ,poji_segCY+16 ,3 ,3 ,LCD_COLOR_WHITE);
      }
      /* C1m */
      if (pix_buff_A[6][111]&1 == 1)
      {
        lcd_fill_s(poji_segC1X+5 ,poji_segCY+24 ,3 ,3 ,LCD_COLOR_BLACK);
        lcd_fill_s(poji_segC1X+4 ,poji_segCY+27 ,3 ,3 ,LCD_COLOR_BLACK);
        lcd_fill_s(poji_segC1X+3 ,poji_segCY+30 ,3 ,3 ,LCD_COLOR_BLACK);
        lcd_fill_s(poji_segC1X+2 ,poji_segCY+33 ,3 ,3 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segC1X+5 ,poji_segCY+24 ,3 ,3 ,LCD_COLOR_WHITE);
        lcd_fill_s(poji_segC1X+4 ,poji_segCY+27 ,3 ,3 ,LCD_COLOR_WHITE);
        lcd_fill_s(poji_segC1X+3 ,poji_segCY+30 ,3 ,3 ,LCD_COLOR_WHITE);
        lcd_fill_s(poji_segC1X+2 ,poji_segCY+33 ,3 ,3 ,LCD_COLOR_WHITE);
      }
      /* C2a */
      if (pix_buff_A[4][97]&1 == 1)
      {
        lcd_fill_s(poji_segC2X ,poji_segCY+2 ,15 ,3 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segC2X ,poji_segCY+2 ,15 ,3 ,LCD_COLOR_WHITE);
      }
      /* C2b */
      if (pix_buff_A[4][109]&1 == 1)
      {
        lcd_fill_s(poji_segC2X+15 ,poji_segCY+5 ,3 ,15 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segC2X+15 ,poji_segCY+5 ,3 ,15 ,LCD_COLOR_WHITE);
      }
      /* C2c */
      if (pix_buff_A[4][108]&1 == 1)
      {
        lcd_fill_s(poji_segC2X+15 ,poji_segCY+5+15+3 ,3 ,15 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segC2X+15 ,poji_segCY+5+15+3 ,3 ,15 ,LCD_COLOR_WHITE);
      }
      /* C2d */
      if (pix_buff_A[4][106]&1 == 1)
      {
        lcd_fill_s(poji_segC2X ,poji_segCY+5+15+15+3 ,15 ,3 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segC2X ,poji_segCY+5+15+15+3 ,15 ,3 ,LCD_COLOR_WHITE);
      }
      /* C2e */
      if (pix_buff_A[4][105]&1 == 1)
      {
        lcd_fill_s(poji_segC2X-3 ,poji_segCY+5+15+3 ,3 ,15 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segC2X-3 ,poji_segCY+5+15+3 ,3 ,15 ,LCD_COLOR_WHITE);
      }
      /* C2f */
      if (pix_buff_A[4][104]&1 == 1)
      {
        lcd_fill_s(poji_segC2X-3 ,poji_segCY+5 ,3 ,15 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segC2X-3 ,poji_segCY+5 ,3 ,15 ,LCD_COLOR_WHITE);
      }
      /* C2g1 */
      if (pix_buff_A[6][104]&1 == 1)
      {
        lcd_fill_s(poji_segC2X+8 ,poji_segCY+6+14 ,7 ,3 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segC2X+8 ,poji_segCY+6+14 ,7 ,3 ,LCD_COLOR_WHITE);
      }
      /* C2g2 */
      if (pix_buff_A[6][108]&1 == 1)
      {
        lcd_fill_s(poji_segC2X ,poji_segCY+6+14 ,7 ,3 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segC2X ,poji_segCY+6+14 ,7 ,3 ,LCD_COLOR_WHITE);
      }
      /* C2j */
      if (pix_buff_A[6][109]&1 == 1)
      {
        lcd_fill_s(poji_segC2X+11 ,poji_segCY+7 ,3 ,3 ,LCD_COLOR_BLACK);
        lcd_fill_s(poji_segC2X+10 ,poji_segCY+10 ,3 ,3 ,LCD_COLOR_BLACK);
        lcd_fill_s(poji_segC2X+9 ,poji_segCY+13 ,3 ,3 ,LCD_COLOR_BLACK);
        lcd_fill_s(poji_segC2X+8 ,poji_segCY+16 ,3 ,3 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segC2X+11 ,poji_segCY+7 ,3 ,3 ,LCD_COLOR_WHITE);
        lcd_fill_s(poji_segC2X+10 ,poji_segCY+10 ,3 ,3 ,LCD_COLOR_WHITE);
        lcd_fill_s(poji_segC2X+9 ,poji_segCY+13 ,3 ,3 ,LCD_COLOR_WHITE);
        lcd_fill_s(poji_segC2X+8 ,poji_segCY+16 ,3 ,3 ,LCD_COLOR_WHITE);
      }
      /* C2m */
      if (pix_buff_A[6][105]&1 == 1)
      {
        lcd_fill_s(poji_segC2X+5 ,poji_segCY+24 ,3 ,3 ,LCD_COLOR_BLACK);
        lcd_fill_s(poji_segC2X+4 ,poji_segCY+27 ,3 ,3 ,LCD_COLOR_BLACK);
        lcd_fill_s(poji_segC2X+3 ,poji_segCY+30 ,3 ,3 ,LCD_COLOR_BLACK);
        lcd_fill_s(poji_segC2X+2 ,poji_segCY+33 ,3 ,3 ,LCD_COLOR_BLACK);
      }
      else
      {
        lcd_fill_s(poji_segC2X+5 ,poji_segCY+24 ,3 ,3 ,LCD_COLOR_WHITE);
        lcd_fill_s(poji_segC2X+4 ,poji_segCY+27 ,3 ,3 ,LCD_COLOR_WHITE);
        lcd_fill_s(poji_segC2X+3 ,poji_segCY+30 ,3 ,3 ,LCD_COLOR_WHITE);
        lcd_fill_s(poji_segC2X+2 ,poji_segCY+33 ,3 ,3 ,LCD_COLOR_WHITE);
      }
}

void init_lcd()
{
  rst_disp();
  lcd_write_cmd(0x11); //exit sleep mode;
  vTaskDelay(pdMS_TO_TICKS(1)); //1ms delay
  lcd_write_cmd(0xB0);  //manufacturer command access protect
  lcd_write_data(0x04); //allow access to additional manufacturer's commands
  vTaskDelay(pdMS_TO_TICKS(1)); //1ms delay
  lcd_write_cmd(0xB3);  //Frame Memory Access and Interface Setting
  lcd_write_data(0x02); // reset start position of a window area address...
//lcd_write_data(0x00); // no reset start position of a window area address...
  lcd_write_data(0x00); //TE pin is used. TE signal is output every frame.
  lcd_write_data(0x00); // empty according to the datasheet - does nothing;
  lcd_write_data(0x00); // convert 16/18 bits to 24bits data by writing zeroes to LSBs. Sets image data write/read format(?)
  lcd_write_data(0x00); //
  vTaskDelay(pdMS_TO_TICKS(1)); //1ms delay
  lcd_write_cmd(0xB4); //Display Mode
  lcd_write_data(0x00); //Uses internal oscillator
  vTaskDelay(pdMS_TO_TICKS(1)); //1ms delay
    lcd_write_cmd(0xC0); // Panel Driving Setting;
  lcd_write_data(0x03); // Output polarity is inverted. Left/right interchanging scan. Forward scan. BGR mode (depends on other settings). S960 竊 S1 (depends)
  lcd_write_data(0xDF); // Number of lines for driver to drive - 480.
  lcd_write_data(0x40); // Scan start position - Gate1. (depend on other param);
  lcd_write_data(0x10); // Dot inversion. Dot inversion in not-lit display area. If 0x13 - both will be set to 'column inversion'.
  lcd_write_data(0x00); // settings for non-lit display area...
  lcd_write_data(0x01); // 3 frame scan interval in non-display area...
  lcd_write_data(0x00); // Source output level in retrace period...
  lcd_write_data(0x55);//54 . Internal clock divider = 5 (low and high periods).

  lcd_write_cmd(0xC1); //Display Timing Setting for Normal Mode
  lcd_write_data(0x07); // Clock devider = 12. 14MHz/12. Used by display circuit and step-up circuit.
  lcd_write_data(0x27); // These bits set the number of clocks in 1 line period. 0x27 - 39 clocks.
  lcd_write_data(0x08); // Number of back porch lines. 0x08 - 8 lines.
  lcd_write_data(0x08); // Number of front porch lines. 0x08 - 8lines.
  lcd_write_data(0x00); // Spacial configuriation mode 1 (?). 1 line inversion mode (?).

  lcd_write_cmd(0xC4); // Source/Gate Driving Timing Setting
  lcd_write_data(0x57); // falling position (stop) of gate driver - 4 clocks... gate start position - 8 clocks...
  lcd_write_data(0x00); // nothing to set up according to the datasheet
  lcd_write_data(0x05); // Source precharge period (GND) - 5 clocks.
  lcd_write_data(0x03); // source precharge period (VCI) - 3 clocks.

  lcd_write_cmd(0xC6); //DPI polarity control
  lcd_write_data(0x04); // VSYNC -Active Low. HSYNC - Active Low. DE pin enable data write in when DE=1. Reads data on the rising edge of the PCLK signal.

  //----Gamma setting start-----
  lcd_write_cmd(0xC8);
  lcd_write_data(0x03);
  lcd_write_data(0x12);
  lcd_write_data(0x1A);
  lcd_write_data(0x24);
  lcd_write_data(0x32);
  lcd_write_data(0x4B);
  lcd_write_data(0x3B);
  lcd_write_data(0x29);
  lcd_write_data(0x1F);
  lcd_write_data(0x18);
  lcd_write_data(0x12);
  lcd_write_data(0x04);

  lcd_write_data(0x03);
  lcd_write_data(0x12);
  lcd_write_data(0x1A);
  lcd_write_data(0x24);
  lcd_write_data(0x32);
  lcd_write_data(0x4B);
  lcd_write_data(0x3B);
  lcd_write_data(0x29);
  lcd_write_data(0x1F);
  lcd_write_data(0x18);
  lcd_write_data(0x12);
  lcd_write_data(0x04);

  lcd_write_cmd(0xC9);
  lcd_write_data(0x03);
  lcd_write_data(0x12);
  lcd_write_data(0x1A);
  lcd_write_data(0x24);
  lcd_write_data(0x32);
  lcd_write_data(0x4B);
  lcd_write_data(0x3B);
  lcd_write_data(0x29);
  lcd_write_data(0x1F);
  lcd_write_data(0x18);
  lcd_write_data(0x12);
  lcd_write_data(0x04);

  lcd_write_data(0x03);
  lcd_write_data(0x12);
  lcd_write_data(0x1A);
  lcd_write_data(0x24);
  lcd_write_data(0x32);
  lcd_write_data(0x4B);
  lcd_write_data(0x3B);
  lcd_write_data(0x29);
  lcd_write_data(0x1F);
  lcd_write_data(0x18);
  lcd_write_data(0x12);
  lcd_write_data(0x04);

  lcd_write_cmd(0xCA);
  lcd_write_data(0x03);
  lcd_write_data(0x12);
  lcd_write_data(0x1A);
  lcd_write_data(0x24);
  lcd_write_data(0x32);
  lcd_write_data(0x4B);
  lcd_write_data(0x3B);
  lcd_write_data(0x29);
  lcd_write_data(0x1F);
  lcd_write_data(0x18);
  lcd_write_data(0x12);
  lcd_write_data(0x04);

  lcd_write_data(0x03);
  lcd_write_data(0x12);
  lcd_write_data(0x1A);
  lcd_write_data(0x24);
  lcd_write_data(0x32);
  lcd_write_data(0x4B);
  lcd_write_data(0x3B);
  lcd_write_data(0x29);
  lcd_write_data(0x1F);
  lcd_write_data(0x18);
  lcd_write_data(0x12);
  lcd_write_data(0x04);
//---Gamma setting end--------

  lcd_write_cmd(0xD0); // Power (charge pump) settings
  lcd_write_data(0x99);//DC4~1//A5. Set up clock cycle of the internal step up controller.
  lcd_write_data(0x06);//BT // Set Voltage step up factor.
  lcd_write_data(0x08);// default according to the datasheet - does nothing.
  lcd_write_data(0x20);// VCN step up cycles.
  lcd_write_data(0x29);//VC1, VC2// VCI3 voltage = 2.70V;  VCI2 voltage = 3.8V.
  lcd_write_data(0x04);// default
  lcd_write_data(0x01);// default
  lcd_write_data(0x00);// default
  lcd_write_data(0x08);// default
  lcd_write_data(0x01);// default
  lcd_write_data(0x00);// default
  lcd_write_data(0x06);// default
  lcd_write_data(0x01);// default
  lcd_write_data(0x00);// default
  lcd_write_data(0x00);// default
  lcd_write_data(0x20);// default

  lcd_write_cmd(0xD1);//VCOM setting
  lcd_write_data(0x00);//disable write to VDC[7:0].
  lcd_write_data(0x20);//45 38 VPLVL// voltage of ﾎｳ correction registers for positive polarity
  lcd_write_data(0x20);//45 38 VNLVL// voltage of ﾎｳ correction registers for negative polarity
  lcd_write_data(0x15);//32 2A VCOMDC// VNLVL x 0.063

  lcd_write_cmd(0xE0);//NVM Access Control
  lcd_write_data(0x00);//NVM access is disabled
  lcd_write_data(0x00);//Erase operation (disabled).
  lcd_write_data(0x00);//TE pin works as tearing effect pin.
  // should be one more lcd_write_data(0x00); according to the datasheet.

  lcd_write_cmd(0xE1); //set_DDB_write_control
  lcd_write_data(0x00);
  lcd_write_data(0x00);
  lcd_write_data(0x00);
  lcd_write_data(0x00);
  lcd_write_data(0x00);
  lcd_write_data(0x00);

  lcd_write_cmd(0xE2); //NVM Load Control
  lcd_write_data(0x00); // does not execute data load from the NVM to each command

  lcd_write_cmd(0x36); //set_address_mode
//  lcd_write_data(0x00); // data is not flipped in any way?
  lcd_write_data(0x60); //

  lcd_write_cmd(0x3A); // set_pixel_format
  lcd_write_data(0x55);// 16-Bit/pixel = 55h, 24-bit/pixel = 77h

  lcd_write_cmd(0x2A); //set_column_address
  lcd_write_data(0x00); // starts from 0th frame buffer address
  lcd_write_data(0x00);
  lcd_write_data(0x01);
  lcd_write_data(0x3F);//320 - uses all columns

  lcd_write_cmd(0x2B); //set_page_address
  lcd_write_data(0x00); // starts from 0th frame buffer address
  lcd_write_data(0x00);
  lcd_write_data(0x01);
  lcd_write_data(0xDF);//480 - uses all lines in the frame buffer
  //lcd_write_cmd(0x21);
  lcd_write_cmd(0x29);//set_display_on - This command causes the display module to start displaying the image data 
}
