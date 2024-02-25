/*
 * Yaesu vr5000 display program
 */
#include "vr5000_disp.h"
void init_cmd_uart();
QueueHandle_t xQueue, xDisp_queue;
bool en_proc_loop = false;

//buffers below need to be alligned to use ring buffer
uint32_t capture_buf_0[cDMA_BUF_SZ] __attribute__((aligned(cDMA_BUF_SZ*4)));
uint32_t capture_buf_1[cDMA_BUF_SZ] __attribute__((aligned(cDMA_BUF_SZ*4)));
uint32_t *bptr[2] = {(uint32_t *)&capture_buf_0, (uint32_t *)&capture_buf_1};
int dma_chs[2];
extern struct t_cmd_fields params;

struct led_task_arg {
    int gpio;
    int delay;
};

TaskHandle_t cmd_loop_handle;

//DMA irq handler
void dma_handler_chaned() {
    portBASE_TYPE xStatus;
    uint32_t *ptr=NULL;
    if (dma_hw->ints0 & 1u << dma_chs[0])
    {
      dma_hw->ints0 = 1u << dma_chs[0]; // Clear the interrupt request.
      ptr = bptr[0];
    }
    if (dma_hw->ints0 & 1u << dma_chs[1])
    {
      dma_hw->ints0 = 1u << dma_chs[1];
      ptr = bptr[1];
    }
    for (int i=0;i<cDMA_BUF_SZ;i++) 
    {
      xStatus = xQueueSendFromISR(xDisp_queue, &(ptr[i]), NULL);//copy data into queue
      if (xStatus==pdFAIL) 
        break;
    }
}

int pio_config_dma_chained(PIO pio, uint sm, size_t capture_size_words, uint32_t ** capture_bufs)
{
  dma_channel_config d[2];
  for (int i=0;i<2;i++)
  {
    dma_chs[i]=dma_claim_unused_channel(true);//get channel
    d[i]=dma_channel_get_default_config(dma_chs[i]);
    channel_config_set_transfer_data_size(&d[i], DMA_SIZE_32); //Set control channel data transfer size to 32 bits
    channel_config_set_read_increment(&d[i], false);//we read fifo, always at the same PIO address
    channel_config_set_write_increment(&d[i], true);//writing to memory
    channel_config_set_dreq(&d[i], pio_get_dreq(pio, sm, false));
    //set buffer ring operation. This will auto-rewind pointer to the beginning of the buffer
    channel_config_set_ring(&d[i], true, CAPTURE_RING_BITS);
    dma_channel_set_irq0_enabled(dma_chs[i], true);
  }
  //set up chaining
  channel_config_set_chain_to(&d[0],dma_chs[1]);
  channel_config_set_chain_to(&d[1],dma_chs[0]);

  for (int i=0;i<2;i++)
    dma_channel_configure(
          dma_chs[i],
          &d[i],
          capture_bufs[i],    // Destination pointer
          &pio->rxf[sm],      // Source pointer
          capture_size_words, // Number of transfers
          false               // do not start yet
      );
      
  irq_set_exclusive_handler(DMA_IRQ_0, dma_handler_chaned);
  irq_set_enabled(DMA_IRQ_0, true);
  dma_start_channel_mask(1u << dma_chs[0]); //Start first DMA
}


//read DAM commands/data pass them to the parser
static void dma_rd_loop() {
  portBASE_TYPE xStatus;
  static uint32_t thread_notification;
  uint32_t data;
  uint16_t dt16;
  uint tst  =0;
  const TickType_t xTicksToWait = 100;
  for (;;) 
  {
    while( uxQueueMessagesWaiting(xDisp_queue) > 0 )
    {
      xStatus = xQueueReceive(xDisp_queue, &data, xTicksToWait);//two entries per data
      if( xStatus == pdPASS ) 
      {
        dt16 = (data&0xFFFF);
        if (parse_data_cmd(dt16)!=0) break;//parse incomming cmd, data
        dt16 = ((data>>16)&0xFFFF);
        if (parse_data_cmd(dt16)!=0) break;//parse incomming cmd, data
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

//display loop
static void disp_loop() {
  init_disp_pins();
  init_lcd();
  lcd_fill(0, 0, 480, 320, 0xFFFF);	// ALL White
  uint16_t i = 0;
  while(true)
  {
    update_lcd();
    vTaskDelay(pdMS_TO_TICKS(33));
  }
}

int main()
{
    stdio_init_all();
    //set_sys_clock_khz(250000,true);//set clk to 200MHz
    xQueue = xQueueCreate(16, sizeof( char ) );//create queue
    xDisp_queue = xQueueCreate(cDMA_QUEUE_SZ, sizeof( uint32_t ));
    float clkdiv = 1.0f;//max clk frequency for the PIO
    uint offset = pio_add_program(pio0, &dtin_sm_program);
    pio_dtin_sm_init(pio0, 0, offset, 23, 27, 28, 29, clkdiv);
    pio_config_dma_chained(pio0, 0, cDMA_BUF_SZ, bptr);
    xTaskCreate(dma_rd_loop, "DMA_RD", 256, NULL, configMAX_PRIORITIES-1, NULL);
    xTaskCreate(disp_loop, "DISPLAY TASK", 256, NULL, configMAX_PRIORITIES-2, NULL);
    vTaskStartScheduler();
    while (true);
}
